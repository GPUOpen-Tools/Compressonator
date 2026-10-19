//=====================================================================
// Copyright (c), Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================

/**
 * @file dll_security_check.h
 * @brief Runtime DLL proxy attack detection for Compressonator applications
 * 
 * This header provides runtime security validation to detect DLL proxy attacks
 * and suspicious DLL replacements. It should be called early in application
 * startup after SecureDllLoading() to validate DLL integrity.
 * 
 * Detection Methods:
 * - Invalid or missing digital signatures for all DLLs
 * - Untrusted or tampered DLL signatures
 *
 * Scope and limits:
 * - The scan runs from main(), so it cannot prevent a substituted import-table DLL
 *   (Qt5, glew, OpenCV, ...) from having already been mapped and run its DllMain. It
 *   detects the substitution after the fact; it is not a pre-startup enforcement
 *   mechanism. See SecureDllLoading() for what is and is not covered.
 * - DLLs shipped with Compressonator - including redistributed third-party libraries -
 *   are re-signed by AMD and are required to carry a valid signature from an AMD
 *   code-signing certificate.
 * - The documented user extension point \\plugins\\imageformats (see
 *   docs/source/gui_tool/user_guide/imageview.rst) holds third-party Qt image-format
 *   plugins that cannot be AMD-signed. A separate, explicit trust policy applies there:
 *   an unsigned plugin is accepted, but a present signature must still be valid and
 *   chain to a trusted root.
 * 
 */

#ifndef DLL_SECURITY_CHECK_H
#define DLL_SECURITY_CHECK_H

#ifdef _WIN32

#include <windows.h>
#ifdef DLL_SECURITY_CHECK_ALL_DLLS
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#endif
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>  // for std::transform
#include <cwctype>    // for towlower/iswalnum
#include <cwchar>     // for wcslen
#include <mutex>

// Defined by newer SDKs only; declared here so the header still builds against older ones.
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#endif
#ifndef LOAD_LIBRARY_SEARCH_USER_DIRS
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400
#endif
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

namespace DllSecurity
{

// Bring std::wstring and std::vector into the DllSecurity namespace scope
using std::wstring;
using std::vector;

// Publisher pinning: a valid trusted-CA chain alone does not prove identity, since a proxy DLL
// signed with an unrelated (but valid and unrevoked) cert would otherwise pass. Every DLL shipped
// with Compressonator is AMD-signed — including redistributed third-party libraries — so the check
// is applied to all of them. DLLs under the documented user plugin directory are exempt and use
// the separate policy described in the file header.

// WinVerifyTrust/CertGetNameStringW dependent code; only compiled (and only needs wintrust/crypt32
// at link time) when the runtime DLL security check is enabled.
#ifdef DLL_SECURITY_CHECK_ALL_DLLS

/**
 * @brief Reduce a certificate subject to lower-case alphanumerics for comparison.
 *
 * Dropping punctuation/whitespace absorbs harmless cert-reissue differences ("Advanced Micro
 * Devices" vs "Advanced Micro Devices, Inc.") while keeping the comparison exact, so a longer
 * lookalike subject such as "Advanced Micro Devices, Inc. Reseller" still fails to match.
 * Also used to canonicalise configured key digests, which may contain colons or spaces.
 */
static wstring NormalizeSubject(const wstring& name)
{
    wstring normalized;
    normalized.reserve(name.size());
    for (wchar_t c : name)
    {
        if (iswalnum(static_cast<wint_t>(c)))
            normalized.push_back(static_cast<wchar_t>(towlower(static_cast<wint_t>(c))));
    }
    return normalized;
}

/**
 * @brief Helper function to check if a wstring ends with a given suffix (C++17 compatible)
 * @param str The string to check
 * @param suffix The suffix to look for
 * @return true if str ends with suffix
 */
static bool EndsWith(const wstring& str, const wstring& suffix)
{
    if (str.length() < suffix.length())
        return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

/**
 * @brief Results from DLL security scan
 */
struct SecurityScanResult
{
    bool            isSecure;           ///< Overall security status
    bool            scanFailed;         ///< Directory could not be enumerated/verified completely
    vector<wstring> suspiciousFiles;    ///< List of suspicious DLLs found
    vector<wstring> warnings;           ///< Non-critical warnings
    int             invalidSignatures;  ///< Count of invalid signatures
    int             dllsScanned;        ///< Count of .dll files actually examined
};

/**
 * @brief Whether the leaf signing certificate belongs to one of the accepted AMD identities.
 * @param stateData The WINTRUST_DATA::hWVTStateData handle from a VERIFY call that returned 0.
 *
 * This is certificate subject matching, not a cryptographic identity pin: any certificate that
 * chains to a trusted root and carries a matching subject would satisfy it.
 */
static bool SignerIsAcceptedAmdPublisher(HANDLE stateData)
{
    CRYPT_PROVIDER_DATA* pProvData = WTHelperProvDataFromStateData(stateData);
    if (!pProvData)
        return false;

    CRYPT_PROVIDER_SGNR* pSigner = WTHelperGetProvSignerFromChain(pProvData, 0, FALSE, 0);
    if (!pSigner || pSigner->csCertChain == 0)
        return false;

    PCCERT_CONTEXT pCertContext = pSigner->pasCertChain[0].pCert;
    if (!pCertContext)
        return false;

    // Both the suffixed and unsuffixed spellings are listed because the corporate suffix in the
    // certificate subject has varied between reissues.
    static const wchar_t* const kAcceptedAmdSubjects[] = {L"Advanced Micro Devices", L"Advanced Micro Devices, Inc."};

    // Both the simple display name (usually CN) and the Organization attribute are examined, so
    // the check survives a reissue that moves the company name between those fields.
    const DWORD nameTypes[] = {CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ATTR_TYPE};
    for (DWORD nameType : nameTypes)
    {
        void* pvTypePara = (nameType == CERT_NAME_ATTR_TYPE) ? (void*)szOID_ORGANIZATION_NAME : NULL;

        // Query the required length first: a subject longer than a fixed buffer would otherwise be
        // silently truncated into a value that can never match.
        DWORD needed = CertGetNameStringW(pCertContext, nameType, 0, pvTypePara, NULL, 0);
        if (needed <= 1)
            continue;

        vector<wchar_t> nameBuf(needed);
        if (CertGetNameStringW(pCertContext, nameType, 0, pvTypePara, nameBuf.data(), needed) <= 1)
            continue;

        wstring candidate = NormalizeSubject(wstring(nameBuf.data()));
        for (const wchar_t* accepted : kAcceptedAmdSubjects)
        {
            if (candidate == NormalizeSubject(accepted))
                return true;
        }
    }

    return false;
}

/**
 * @brief Whether a WinVerifyTrust status means "revocation state could not be determined".
 *
 * Distinct from "revoked": an empty local CRL/OCSP cache or an offline machine produces these,
 * and treating them as tampering would stop the application from starting at all.
 */
static bool IsRevocationUnavailable(LONG status)
{
    return status == CRYPT_E_REVOCATION_OFFLINE || status == CERT_E_REVOCATION_FAILURE || status == CRYPT_E_NO_REVOCATION_CHECK ||
           status == CRYPT_E_NO_REVOCATION_DLL;
}

/**
 * @brief Run a single WinVerifyTrust pass and, on success, apply the publisher pin.
 * @param filePath Full path to the file to check.
 * @param revocationChecks Value for WINTRUST_DATA::fdwRevocationChecks.
 * @param pinToAmdPublisher If true, the signer must be an accepted AMD identity.
 * @param pinSatisfied Set to true only when the chain is trusted and the pin (if any) matched.
 * @return The WinVerifyTrust status code.
 */
static LONG RunTrustVerification(const wchar_t* filePath, DWORD revocationChecks, bool pinToAmdPublisher, bool& pinSatisfied)
{
    pinSatisfied = false;

    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct           = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath      = filePath;
    fileInfo.hFile              = NULL;
    fileInfo.pgKnownSubject     = NULL;

    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData       = {};
    trustData.cbStruct            = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice          = WTD_UI_NONE;
    trustData.fdwRevocationChecks = revocationChecks;
    trustData.dwUnionChoice       = WTD_CHOICE_FILE;
    trustData.pFile               = &fileInfo;
    trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
    // Keeps revocation lookups on the local cache so the per-DLL startup scan never stalls on a
    // network fetch; a cold cache is handled by the caller's retry rather than by blocking.
    trustData.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL;

    LONG result = WinVerifyTrust(NULL, &guidAction, &trustData);

    if (result == 0)
        pinSatisfied = !pinToAmdPublisher || SignerIsAcceptedAmdPublisher(trustData.hWVTStateData);

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &guidAction, &trustData);

    return result;
}

/**
 * @brief Check if a file has a valid digital signature from the expected publisher.
 * @param filePath Full path to the file to check
 * @param requireSigned If true, an unsigned file is treated as invalid.
 * @param pinToAmdPublisher If true, the signer's certificate must be an accepted AMD identity;
 *        a trusted chain from an unrelated publisher is rejected.
 * @return true if signature is valid (or unsigned when requireSigned is false); false otherwise.
 */
static bool VerifyFileSignature(const wchar_t* filePath, bool requireSigned = false, bool pinToAmdPublisher = false)
{
    bool pinSatisfied = false;

    // Check the whole chain for revocation; a revoked-but-not-yet-expired cert must not be
    // treated as trustworthy.
    LONG result = RunTrustVerification(filePath, WTD_REVOKE_WHOLECHAIN, pinToAmdPublisher, pinSatisfied);

    // "Revocation unknown" is not "revoked", so fall back to a chain-only verification instead of
    // failing an offline or cold-cache machine closed.
    if (IsRevocationUnavailable(result))
        result = RunTrustVerification(filePath, WTD_REVOKE_NONE, pinToAmdPublisher, pinSatisfied);

    // TRUST_E_NOSIGNATURE means file is not signed (acceptable for some DLLs)
    // pinSatisfied means a trusted (and, if pinned, correctly-attributed) signature
    // Anything else means invalid/tampered/revoked/unpinned signature
    if (result == 0)
    {
        return pinSatisfied;
    }
    else if (result == TRUST_E_NOSIGNATURE)
    {
        return !requireSigned;  // Unsigned: acceptable for 3rd-party, failure for 1st-party
    }
    else
    {
        return false;  // Invalid, tampered, revoked, or wrong-publisher signature
    }
}

#endif  // DLL_SECURITY_CHECK_ALL_DLLS

/**
 * @brief Directory containing the running executable.
 *
 * The buffer is grown instead of being fixed at MAX_PATH so an install under a long path
 * resolves normally rather than being reported as a failure.
 *
 * @return false only if the path could not be queried at all.
 */
static bool GetApplicationDirectory(wstring& outDirectory)
{
    vector<wchar_t> buffer(MAX_PATH);

    for (;;)
    {
        const DWORD length = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return false;

        if (length < buffer.size() - 1)
        {
            outDirectory.assign(buffer.data(), length);
            break;
        }

        if (buffer.size() >= 32768)  // Windows maximum extended path length
            return false;

        buffer.resize(buffer.size() * 2);
    }

    const size_t separator = outDirectory.find_last_of(L"\\/");
    if (separator == wstring::npos)
        return false;

    outDirectory.resize(separator);
    return true;
}

// Remainder of the WinVerifyTrust-based scan; see the matching guard above VerifyFileSignature().
#ifdef DLL_SECURITY_CHECK_ALL_DLLS

/**
 * @brief Whether @p path is @p root or lies beneath it (case-insensitive).
 */
static bool IsWithinDirectory(const wstring& root, const wstring& path)
{
    if (root.empty() || path.length() < root.length())
        return false;

    wstring lowerRoot = root;
    wstring lowerPath = path;
    std::transform(lowerRoot.begin(), lowerRoot.end(), lowerRoot.begin(), ::towlower);
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);

    if (lowerPath.compare(0, lowerRoot.length(), lowerRoot) != 0)
        return false;

    return lowerPath.length() == lowerRoot.length() || lowerPath[lowerRoot.length()] == L'\\' || lowerPath[lowerRoot.length()] == L'/';
}

/**
 * @brief Resolves a path through any reparse points to the location it ultimately refers to.
 * @return The resolved path without its \\?\ prefix, or an empty string if it cannot be opened.
 */
static wstring ResolveFinalPath(const wstring& path)
{
    HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return wstring();

    wstring     resolved;
    const DWORD length = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length > 0)
    {
        resolved.resize(length);
        const DWORD written = GetFinalPathNameByHandleW(handle, &resolved[0], length, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (written > 0 && written < length)
            resolved.resize(written);
        else
            resolved.clear();
    }

    CloseHandle(handle);

    if (resolved.compare(0, 4, L"\\\\?\\") == 0)
        resolved.erase(0, 4);

    return resolved;
}

/**
 * @brief Recursively scan directory for DLL security issues
 * @param directoryPath Directory to scan
 * @param result Output results structure
 * @param userPluginRoot Root of the documented user plugin directory, or empty to apply the
 *        first-party policy everywhere.
 * @param scanRoot Resolved root of the tree being scanned, used to decide whether a reparse
 *        point leads somewhere this scan already covers.
 *
 * @note Any directory that cannot be enumerated sets result.scanFailed, so the caller can fail
 *       closed instead of mistaking "not scanned" for "scanned and clean".
 */
static void ScanDirectory(const wchar_t* directoryPath, SecurityScanResult& result, const wstring& userPluginRoot, const wstring& scanRoot, int depth = 0)
{
    if (depth > 10)
    {
        result.scanFailed = true;
        result.warnings.push_back(wstring(L"Directory nesting too deep to scan: ") + directoryPath);
        return;
    }

    WIN32_FIND_DATAW findData;

    // std::wstring instead of a MAX_PATH buffer: a DLL under a long install path must still be
    // verified rather than silently skipped.
    const wstring basePath(directoryPath);
    const wstring searchPath = basePath + L"\\*";

    // Documented extension point: users may drop their own Qt image-format plugins here
    // (docs/source/gui_tool/user_guide/imageview.rst), so those cannot be AMD-signed.
    const bool inUserPluginTree = IsWithinDirectory(userPluginRoot, basePath);

    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return;

        result.scanFailed = true;
        result.warnings.push_back(wstring(L"Unable to enumerate directory: ") + basePath);
        return;
    }

    do
    {
        // Skip . and ..
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
        {
            continue;
        }

        const wstring fullPath = basePath + L"\\" + findData.cFileName;

        // Recurse into subdirectories (skip reparse points to avoid symlink loops)
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                // Not following the link keeps the scan terminating. A link that stays inside
                // the tree reaches DLLs this scan verifies anyway; one that leaves it, or that
                // cannot be resolved, hides files the scan never saw.
                const wstring resolved = ResolveFinalPath(fullPath);
                if (resolved.empty() || !IsWithinDirectory(scanRoot, resolved))
                {
                    result.scanFailed = true;
                    result.warnings.push_back(wstring(L"Skipped reparse point leaving the scanned tree, contents not verified: ") + fullPath);
                }
                else
                    result.warnings.push_back(wstring(L"Skipped reparse point, target already covered by this scan: ") + fullPath);
            }
            else
                ScanDirectory(fullPath.c_str(), result, userPluginRoot, scanRoot, depth + 1);
            continue;
        }

        wstring fileName(findData.cFileName);
        wstring lowerName = fileName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

        // Check DLLs for signature validity or checksum
        if (EndsWith(lowerName, L".dll"))
        {
            result.dllsScanned++;

            if (inUserPluginTree)
            {
                // User plugin trust policy: an unsigned third-party plugin is accepted, but a
                // present signature must still be valid and chain to a trusted root, so a
                // tampered or revoked plugin is still rejected.
                if (!VerifyFileSignature(fullPath.c_str(), false, false))
                {
                    result.suspiciousFiles.push_back(wstring(L"Invalid signature (user plugin): ") + fileName);
                    result.invalidSignatures++;
                }
            }
            else
            {
                // Digital signature verification — every shipped DLL must be signed, trusted, and
                // attributed to AMD, so a proxy DLL signed with an unrelated (but validly-chained)
                // cert cannot impersonate one of the shipped libraries.
                if (!VerifyFileSignature(fullPath.c_str(), true, true))
                {
                    result.suspiciousFiles.push_back(wstring(L"Invalid or missing signature: ") + fileName);
                    result.invalidSignatures++;
                }
            }
        }

    } while (FindNextFileW(hFind, &findData));

    const DWORD enumStatus = GetLastError();

    FindClose(hFind);

    // A truncated enumeration means some DLLs were never verified.
    if (enumStatus != ERROR_NO_MORE_FILES)
    {
        result.scanFailed = true;
        result.warnings.push_back(wstring(L"Directory enumeration ended unexpectedly: ") + basePath);
    }
}

/**
 * @brief Perform comprehensive DLL security scan
 * @param logToConsole If true, print results to console
 * @return SecurityScanResult with findings
 *
 * @note The scan is performed once per process; later calls reuse the first result, because
 *       verifying every shipped DLL is the dominant cost of application start-up.
 *
 * @warning This is a start-up integrity report, not a load time control. Statically imported
 *          DLLs are already mapped before it runs, and a plugin can be replaced between this
 *          scan and the PluginManager's LoadLibrary call. Write protection on the install
 *          directory remains the control that actually prevents DLL substitution.
 *
 */
static SecurityScanResult CheckDllSecurity(bool logToConsole = true)
{
    static ::std::mutex       scanMutex;
    static bool               scanned = false;
    static SecurityScanResult cached  = {};

    ::std::lock_guard< ::std::mutex> scanLock(scanMutex);

    if (!scanned)
    {
        cached.isSecure          = true;
        cached.scanFailed        = false;
        cached.invalidSignatures = 0;
        cached.dllsScanned       = 0;

        wstring appDirectory;
        if (!GetApplicationDirectory(appDirectory))
        {
            // Being unable to locate the application directory means no DLL was verified, which
            // is indistinguishable from a tampered install, so fail closed.
            cached.scanFailed = true;
            cached.warnings.push_back(L"Failed to get application path - DLL scan could not be performed");
        }
        else
        {
            // Documented user extension point; scanned under the relaxed plugin policy.
            const wstring userPluginRoot = appDirectory + L"\\plugins\\imageformats";
            const wstring resolvedRoot   = ResolveFinalPath(appDirectory);
            ScanDirectory(appDirectory.c_str(), cached, userPluginRoot, resolvedRoot.empty() ? appDirectory : resolvedRoot);
        }

        // A real install always ships signed DLLs, so finding none is indistinguishable from a
        // scan that was silently pointed at the wrong (e.g. empty) directory - treat it as a
        // failed scan rather than an implicit pass with zero findings.
        if (!cached.scanFailed && cached.dllsScanned == 0)
        {
            cached.scanFailed = true;
            cached.warnings.push_back(L"No DLLs found during security scan - treating as incomplete");
        }

        if (!cached.suspiciousFiles.empty() || cached.scanFailed)
            cached.isSecure = false;

        scanned = true;
    }

    SecurityScanResult result = cached;

    // Log results
    if (logToConsole)
    {
        if (!result.isSecure)
        {
            ::std::wcout << L"[DLL Security] FAIL - Security issues detected:" << ::std::endl;
            ::std::wcout << L"  Suspicious DLLs: " << result.suspiciousFiles.size() << ::std::endl;
            ::std::wcout << L"  Invalid signatures: " << result.invalidSignatures << ::std::endl;

            for (const auto& suspicious : result.suspiciousFiles)
            {
                ::std::wcout << L"  - " << suspicious << ::std::endl;
            }

            if (result.scanFailed)
                ::std::wcout << L"[DLL Security] The DLL scan could not be completed - treating as a failure." << ::std::endl;

            ::std::wcout << L"[DLL Security] WARNING: Potential DLL tampering or corruption detected!" << ::std::endl;
            ::std::wcout << L"[DLL Security] Please reinstall the application or contact support." << ::std::endl;
        }
        else
        {
            ::std::wcout << L"[DLL Security] PASS - All DLL signatures verified successfully" << ::std::endl;
        }

        for (const auto& warning : result.warnings)
        {
            ::std::wcout << L"[DLL Security] Warning: " << warning << ::std::endl;
        }
    }

    return result;
}

/**
 * @brief Simple boolean check for DLL security with a dialog on failure (for GUI startup).
 * @return true if no security issues detected
 */
static bool ValidateDllSecurity()
{
    SecurityScanResult result = CheckDllSecurity(false);

    if (!result.isSecure)
    {
        // Show error dialog on security failure
        wstring message = L"DLL Security Check Failed!\n\n";

        if (result.scanFailed)
        {
            message += L"The DLL integrity scan could not be completed:\n";
            for (size_t i = 0; i < result.warnings.size() && i < 5; i++)
            {
                message += L"- " + result.warnings[i] + L"\n";
            }
            message += L"\n";
        }

        if (!result.suspiciousFiles.empty())
        {
            message += L"Suspicious files detected:\n";

            for (size_t i = 0; i < result.suspiciousFiles.size() && i < 5; i++)
            {
                message += L"- " + result.suspiciousFiles[i] + L"\n";
            }

            if (result.suspiciousFiles.size() > 5)
            {
                message += L"... and " + std::to_wstring(result.suspiciousFiles.size() - 5) + L" more\n";
            }
        }

        message += L"\nThis may indicate a security compromise.\n";
        message += L"Please reinstall the application or contact support.\n\n";
        message += L"The application will now exit.";

        MessageBoxW(NULL, message.c_str(), L"Security Warning", MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }

    return result.isSecure;
}

#endif  // DLL_SECURITY_CHECK_ALL_DLLS

/**
 * @brief Secure DLL loading configuration to prevent DLL hijacking attacks.
 *
 * Implements defense-in-depth against DLL hijacking for DLLs loaded via
 * LoadLibrary() after this call:
 * 1. Removes the current directory from the DLL search path (Vista+)
 * 2. Restricts the default search to System32 only (Windows 8+)
 * 3. Explicitly adds the application and plugins directories
 *
 * @note The anti-proxy guarantee of this function is limited to runtime-loaded DLLs.
 *       DLLs listed in the PE import table (Qt5, glew, OpenCV, ...) are resolved and their
 *       DllMain run by the Windows loader before main() is entered, so neither this function
 *       nor CheckDllSecurity() can prevent a substituted copy from executing - CheckDllSecurity()
 *       only reports it afterwards. Preventing that requires enforcement before process start
 *       (installer ACLs on the install directory, or an OS application-control policy such as
 *       WDAC/AppLocker). Portable or user-writable deployments are not covered.
 * @note Must be the first call in main() to protect any runtime LoadLibrary
 *       calls made by the application and its plugins.
 *
 * @return false if the restrictive search order could not be applied, in which case runtime
 *         LoadLibrary calls are still using an unsafe search configuration.
 */
static bool SecureDllLoading()
{
    // Remove current directory from DLL search path
    // This works on Windows Vista and later
    // Use the W variant explicitly to avoid TCHAR/Unicode ambiguity
    bool hardened = (SetDllDirectoryW(L"") != FALSE);

    // For Windows 8 and later, use more restrictive search
    // typedef to avoid compilation issues on older SDKs
    typedef BOOL(WINAPI * SetDefaultDllDirectoriesFunc)(DWORD);
    typedef DLL_DIRECTORY_COOKIE(WINAPI * AddDllDirectoryFunc)(PCWSTR);

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32)
        return false;

    SetDefaultDllDirectoriesFunc pSetDefaultDllDirectories = (SetDefaultDllDirectoriesFunc)GetProcAddress(hKernel32, "SetDefaultDllDirectories");
    AddDllDirectoryFunc          pAddDllDirectory          = (AddDllDirectoryFunc)GetProcAddress(hKernel32, "AddDllDirectory");

    if (!pSetDefaultDllDirectories || !pAddDllDirectory)
        return false;

    // Drop the current/PATH directories from the search order. LOAD_LIBRARY_SEARCH_USER_DIRS
    // must be included or the AddDllDirectory() paths below would never be searched and
    // runtime plugin loading would fail.
    if (!pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS))
        return false;

    // Get application directory
    wstring appDirectory;
    if (!GetApplicationDirectory(appDirectory))
        return false;

    // Add application directory to safe search path
    if (!pAddDllDirectory(appDirectory.c_str()))
        return false;

    // Add plugins directory
    const wstring pluginPath = appDirectory + L"\\plugins";
    pAddDllDirectory(pluginPath.c_str());  // Best effort, may not exist yet

    return hardened;
}

}  // namespace DllSecurity

#endif  // _WIN32
#endif  // DLL_SECURITY_CHECK_H
