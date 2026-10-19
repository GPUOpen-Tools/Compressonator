//=====================================================================
// Copyright 2020-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

#include "cmp_fileio.h"

#ifdef _WIN32
#pragma warning(disable : 4244)
#pragma warning(disable : 4996)  // 'fopen': This function or variable may be unsafe.
#pragma warning(disable : 4100)  // unreferenced formal parameter
#include "windows.h"
#include <shlobj.h>
#include <sddl.h>
#include <stdio.h>
#include <direct.h>
#include <iostream>
#include <iterator>
#include <string>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include <cstdlib>
#include <fstream>

// For dev work on older C++ code
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
//#undef _CMP_CPP17_
//#undef _CMP_CPP14_
#endif

#if defined _CMP_CPP17_  // Build code using std::c++17
#include <filesystem>
namespace sfs = std::filesystem;
#else
#if defined _CMP_CPP14_  // Build code using std::c++14
#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif
#include <experimental/filesystem>
namespace sfs = std::experimental::filesystem;
#endif
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>

std::string CMP_GetJustFileName(const std::string& filePath)
{
    std::size_t dotPos = filePath.rfind('.');
    std::size_t sepPos = filePath.find_last_of(FILE_SPLIT_PATH);

    if (sepPos == std::string::npos)
        return filePath.substr(0, dotPos);
    else
        return filePath.substr(sepPos + 1, dotPos - sepPos - 1);
}

std::string CMP_GetFileName(const std::string& filePath)
{
    std::size_t dirPos = filePath.find_last_of(FILE_SPLIT_PATH);

    if (dirPos == std::string::npos)
        return filePath;
    else
        return filePath.substr(dirPos + 1);
}

uintmax_t CMP_GetFileSize(const std::string& fileName)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    return sfs::file_size(fileName);
#else
    std::ifstream fstrm(fileName.c_str(), std::ifstream::ate | std::ifstream::binary);
    fstrm.seekg(0, std::ifstream::end);
    return fstrm.tellg();
#endif
}

void CMP_FileCopy(std::string& source_file, std::string& destination_file)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    sfs::copy(source_file, destination_file);
#else
    FILE* src = fopen(source_file.c_str(), "rb");
    if (src == NULL)
        return;

    FILE* dst = fopen(destination_file.c_str(), "wb");
    if (dst == NULL)
    {
        fclose(src);
        return;
    }

    for (int i = getc(src); i != EOF; i = getc(src))
    {
        putc(i, dst);
    }

    fclose(dst);
    fclose(src);
#endif
}

bool CMP_DirExists(const std::string& directory)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    if (sfs::exists(directory))
        return sfs::is_directory(directory);
    else
        return false;
#else
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesA(directory.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    DIR* dir = opendir(directory.c_str());
    if (dir)
    {
        closedir(dir);
        return true;
    }
    return (false);
#endif
#endif
}

bool CMP_FileExists(const std::string& fileName)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    return sfs::exists(fileName) && !sfs::is_directory(fileName);
#else
    bool  ret = false;
    FILE* fp;
#ifdef _WIN32
    errno_t err = fopen_s(&fp, fileName.c_str(), "rb");
    if (err != 0)
        return false;
#else
    fp = fopen(fileName.c_str(), "rb");
#endif
    if (fp)
    {
        ret = true;
        fclose(fp);
    }
    return ret;
#endif
}

bool CMP_CreateDir(std::string dirName)
{
    bool success = true;

#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    try
    {
        success = sfs::create_directory(sfs::absolute(dirName));
    }
    catch (std::exception exception)
    {
        success = false;
    }
#else
    int error = 0;
#ifdef _WIN32
    error = _mkdir(dirName.c_str());
#else
    mode_t nMode = 0733;  // UNIX style permissions
    error        = mkdir(dirName.c_str(), nMode);
#endif
    success = error == 0;
#endif

    return success;
}

std::string CMP_GetFileNameAndExt(const std::string& filePathName)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    return sfs::path(filePathName).filename().string();
#else
    //std::size_t dotPos = filePathName.rfind('.');
    std::size_t sepPos = filePathName.rfind('/');
    if (sepPos == 0)
        sepPos = filePathName.rfind('\\');
    if (sepPos != std::string::npos)
    {
        return filePathName.substr(sepPos + 1, filePathName.size() - 1);
    }
    return "";
#endif
}

std::string CMP_GetJustFileExt(const std::string& sourceFile)
{
    std::string fileExt = "";

#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    sfs::path fp(sourceFile);
    fileExt = fp.extension().string();
#else
    // Alternate Code
    fileExt = sourceFile.substr(sourceFile.find_last_of('.') + 1);
#endif

    return fileExt;
}

std::string CMP_GetPath(const std::string& sourceFile)
{
    std::string strpath = "";
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    sfs::path fp(sourceFile);
    strpath = fp.parent_path().string();
    return strpath;
#else
    size_t found;
    found   = sourceFile.find_last_of("/\\");
    strpath = sourceFile.substr(0, found);
    return strpath;
#endif
}

bool CMP_IsHidden(const std::string& fullPath)
{
#ifdef _WIN32
    bool  IsHidden = false;
    DWORD Result   = GetFileAttributesA(fullPath.c_str());
    if (Result != 0xFFFFFFFF)
    {
        IsHidden = !!(Result & FILE_ATTRIBUTE_HIDDEN);
    }

    return IsHidden;
#else
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    sfs::path path(fullPath);
    if (path.filename().string().find(".") == 0)
        return true;
#else
    return false;
#endif
#endif
}

std::string CMP_GetFullPath(std::string file)
{
#if defined _CMP_CPP17_ || defined _CMP_CPP14_
    sfs::path p(file);
    sfs::path fullpath = sfs::absolute(p);
    return fullpath.generic_string();
    //return sfs::absolute(file).string();
#else
    return file;
#endif
}

static std::vector<std::string> GetFilesInSubdirectory(const std::string& directory, const std::string& filter)
{
    std::vector<std::string> resultFiles;

#if defined(_CMP_CPP17_) || defined(_CMP_CPP14_)
    for (const sfs::directory_entry& entry : sfs::directory_iterator(directory))
    {
        if (CMP_IsHidden(entry.path().string()))
            continue;

        if (sfs::is_directory(entry.path()))
        {
            std::vector<std::string> subDirFiles = GetFilesInSubdirectory(entry.path().string(), filter);
            resultFiles.insert(resultFiles.end(), subDirFiles.begin(), subDirFiles.end());
        }
        else if (sfs::is_regular_file(entry.path()))
        {
            if (filter.length() > 0)
            {
                std::string extension = CMP_GetFileExtension(entry.path().string().c_str(), false, true);

                if (filter.find(extension) != std::string::npos)
                    resultFiles.push_back(entry.path().string());
            }
            else
                resultFiles.push_back(entry.path().string());
        }
    }
#elif defined(_WIN32)
    std::string path = directory + "\\*";

    WIN32_FIND_DATAA fileData = {};

    HANDLE fileHandle = FindFirstFileA(path.c_str(), &fileData);

    if (fileHandle == INVALID_HANDLE_VALUE)
        return {};

    do
    {
        std::string fullPath = directory + "\\" + fileData.cFileName;

        bool isHidden    = (fileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        bool isDirectory = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (fileData.cFileName[0] == '.' && fileData.cFileName[1] == 0)
            continue;
        if (fileData.cFileName[0] == '.' && fileData.cFileName[1] == '.' && fileData.cFileName[2] == 0)
            continue;

        if (isHidden)
            continue;

        if (isDirectory)
        {
            std::string subDir = directory + "\\" + fileData.cFileName;

            std::vector<std::string> subDirFiles = GetFilesInSubdirectory(subDir, filter);
            resultFiles.insert(resultFiles.end(), subDirFiles.begin(), subDirFiles.end());
        }
        else
        {
            if (filter.length() > 0)
            {
                std::string extension = CMP_GetFileExtension(fileData.cFileName, false, true);

                if (filter.find(extension) != std::string::npos)
                    resultFiles.push_back(fullPath);
            }
            else
                resultFiles.push_back(fullPath);
        }
    } while (FindNextFileA(fileHandle, &fileData) != 0);

    FindClose(fileHandle);
#endif

    return resultFiles;
}

std::vector<std::string> CMP_GetAllFilesInDirectory(const std::string& directory, const std::string& filter)
{
    if (!CMP_DirExists(directory))
        return {};

    return GetFilesInSubdirectory(directory, filter);
}

//=================== Generic for all platforms and c++ standards ===============================
std::string CMP_ExpandFilePath(const std::string& filepath)
{
#ifdef _WIN32
    DWORD len = ExpandEnvironmentStringsA(filepath.c_str(), NULL, 0);
    char* str = new char[len];
    ExpandEnvironmentStringsA(filepath.c_str(), str, len);

    std::string s(str);

    delete[] str;

    return s;
#else

#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR) || defined(__ANDROID__)
    // no expansion
    std::string s = filepath;
#else
    std::string s;
    wordexp_t   p;

    if (filepath.empty())
    {
        return "";
    }

    // char** w;
    int ret = wordexp(filepath.c_str(), &p, 0);
    if (ret)
    {
        // err
        s = filepath;
        return s;
    }

    // Use first element only.
    if (p.we_wordv)
    {
        s = std::string(p.we_wordv[0]);
        wordfree(&p);
    }
    else
    {
        s = filepath;
    }

#endif

    return s;
#endif
}

std::string CMP_JoinPath(const std::string& path0, const std::string& path1)
{
    if (path0.empty())
        return path1;
    if (path1.empty())
        return path0;

    // check '/'
    char lastChar  = *path0.rbegin();
    char firstChar = path1[0];

    if (lastChar != '/' && firstChar != '/')
        return path0 + std::string("/") + path1;

    else if (lastChar == '/' && firstChar == '/')
        return path0 + path1.substr(1, path1.size() - 1);

    else
        return path0 + path1;
}

std::string CMP_FindFile(const std::vector<std::string>& paths, const std::string& filepath)
{
    for (size_t i = 0; i < paths.size(); i++)
    {
        std::string absPath = CMP_ExpandFilePath(CMP_JoinPath(paths[i], filepath));
        if (CMP_FileExists(absPath))
        {
            return absPath;
        }
    }
    return std::string();
}

std::string CMP_GetFilePathExtension(const std::string& fileName)
{
    std::size_t extIndex = fileName.find_last_of('.');

    if (extIndex != std::string::npos)
    {
        // check the special case where the '.' character is part of a relative path
        std::size_t slashIndex = fileName.find_last_of(FILE_SPLIT_PATH);
        if (slashIndex != std::string::npos && slashIndex > extIndex)
        {
            return "";
        }

        return fileName.substr(extIndex + 1);
    }

    return "";
}

std::string CMP_GetBaseDir(const std::string& srcfileDirpath)
{
    if (srcfileDirpath.find_last_of(FILE_SPLIT_PATH) != std::string::npos)
        return srcfileDirpath.substr(0, srcfileDirpath.find_last_of(FILE_SPLIT_PATH));
    return "";
}

std::string CMP_GetFileExtension(const char* fileName, bool includeDot, bool upperCase)
{
    std::string fileExt = CMP_GetJustFileExt(fileName);

    if (upperCase)
    {
        for (char& c : fileExt)
            c = (char)(toupper(c));
    }
    else
    {
        for (char& c : fileExt)
            c = (char)(tolower(c));
    }

    if (!includeDot)
    {
        fileExt.erase(std::remove(fileExt.begin(), fileExt.end(), '.'), fileExt.end());
    }

    return fileExt;
}

void CMP_CreateTextFile(std::string& sourceFile)
{
    FILE* fp;

#ifdef _WIN32
    fopen_s(&fp, sourceFile.c_str(), "a");
#else
    fp = fopen(sourceFile.c_str(), "a");
#endif

    if (fp)
        fclose(fp);
}

FILE* CMP_OpenTextFile(char* SourceFile, const char* mode)
{
    FILE* fp;
#ifdef _WIN32
    fopen_s(&fp, SourceFile, mode);
#else
    fp = fopen(SourceFile, mode);
#endif
    return fp;
}

void CMP_CloseTextFile(FILE* fp)
{
    if (fp)
        fclose(fp);
}

CMP_PATHTYPES CMP_PathType(const char* path)
{
    std::string pathString = path;

    std::string basedir  = CMP_GetBaseDir(pathString);
    std::string ext      = CMP_GetFilePathExtension(pathString);
    std::string filename = CMP_GetFileName(pathString);

    if (ext.length() > 0)
        return CMP_PATH_IS_FILE;

    // checking for the following three cases: '*/test/', '*/.', and '*/..'
    if (filename.length() == 0 || filename.back() == '.' || filename.compare(filename.length() - 2, 2, "..") == 0)
        return CMP_PATH_IS_DIR;
    else  // no extension, but still a valid file
        return CMP_PATH_IS_FILE;

    return CMP_PATHTYPES::CMP_PATH_IS_UNKNOWN;
}

void CMP_ReplaceExt(std::string& s, const std::string& newExt)
{
    std::string::size_type i = s.rfind('.', s.length());

    if (i != std::string::npos)
    {
        s.replace(i + 1, newExt.length(), newExt);
    }
}

//---------------------------------------------------------------------
// Install (read only) and per user (writable) locations
//---------------------------------------------------------------------

static char CMP_NativeSeparator()
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

// Length of the root prefix of path ("C:\", "\\", "/", ...), or 0 when the path is relative.
static std::size_t CMP_RootPrefixLength(const std::string& path)
{
    if (path.length() >= 2 && path[1] == ':')
        return (path.length() > 2 && (path[2] == '/' || path[2] == '\\')) ? 3 : 2;

    if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
    {
        // UNC style "\\server\share" has no creatable component before the share.
        if (path.length() > 1 && (path[1] == '/' || path[1] == '\\'))
        {
            std::size_t server = path.find_first_of(FILE_SPLIT_PATH, 2);
            if (server == std::string::npos)
                return path.length();
            std::size_t share = path.find_first_of(FILE_SPLIT_PATH, server + 1);
            return (share == std::string::npos) ? path.length() : share + 1;
        }
        return 1;
    }

    return 0;
}

bool CMP_CreateDirTree(const std::string& directoryName)
{
    if (directoryName.empty())
        return false;

    if (CMP_DirExists(directoryName))
        return true;

    // Create each missing parent before the leaf, starting past any root prefix so the
    // root itself is never passed to mkdir. Relative paths have no prefix to skip.
    std::size_t pos = directoryName.find_first_of(FILE_SPLIT_PATH, CMP_RootPrefixLength(directoryName));

    while (pos != std::string::npos)
    {
        std::string parent = directoryName.substr(0, pos);
        if (!parent.empty() && !CMP_DirExists(parent))
        {
            if (!CMP_CreateDir(parent))
                return false;
        }
        pos = directoryName.find_first_of(FILE_SPLIT_PATH, pos + 1);
    }

    if (!CMP_CreateDir(directoryName))
        return CMP_DirExists(directoryName);

    return true;
}

#ifdef _WIN32
// Narrow paths reach this file in two different encodings: UTF-8 from Qt (QString::toStdString())
// and the active ANSI code page from the CLI's narrow main(argv) and getenv(). Decoding with the
// wrong one turns a valid non-ASCII path into a different (usually nonexistent) path. Strict UTF-8
// is tried first and CP_ACP is used only when the bytes are not valid UTF-8, which is unambiguous
// for both sources: pure ASCII decodes identically either way, and ANSI text carrying non-ASCII
// bytes is almost never also well-formed UTF-8.
static bool CMP_NarrowPathToWide(const std::string& path, std::wstring& wide)
{
    for (UINT codePage : {CP_UTF8, CP_ACP})
    {
        const DWORD flags  = (codePage == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;
        const int   length = MultiByteToWideChar(codePage, flags, path.c_str(), -1, NULL, 0);
        if (length <= 0)
            continue;

        // length counts the terminating null, which MultiByteToWideChar writes, so the buffer
        // has to hold it during the call and is trimmed afterwards.
        wide.assign(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(codePage, flags, path.c_str(), -1, &wide[0], length) > 0)
        {
            wide.resize(static_cast<size_t>(length) - 1);
            return true;
        }
    }

    return false;
}
#endif

bool CMP_IsDirWritable(const std::string& directory)
{
    if (directory.empty())
        return false;

#ifdef _WIN32
    // The *A Win32 APIs (and CMP_DirExists' std::filesystem narrow conversion) decode using the
    // ANSI code page only, so a UTF-8 path from the GUI would be mis-decoded and a valid, writable
    // directory reported as inaccessible. Convert once and use the wide APIs throughout instead.
    std::wstring wideDir;
    if (!CMP_NarrowPathToWide(directory, wideDir))
        return false;

    DWORD dwAttrib = GetFileAttributesW(wideDir.c_str());
    if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
        return false;

    // _access() reports only the read-only attribute for directories and ignores the
    // ACL, so probe for real write access by creating a file. FILE_FLAG_DELETE_ON_CLOSE
    // removes the probe even if this process is killed before CloseHandle().
    std::wstring probePath = wideDir;
    if (!probePath.empty() && probePath.back() != L'\\' && probePath.back() != L'/')
        probePath += L'\\';
    probePath += L"cmp_wtest_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount()) + L".tmp";

    HANDLE probe = CreateFileW(probePath.c_str(),
                               GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL,
                               CREATE_NEW,  // never truncate a pre-existing file on an unlucky name collision
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                               NULL);
    if (probe == INVALID_HANDLE_VALUE)
        return false;

    CloseHandle(probe);
    return true;
#else
    if (!CMP_DirExists(directory))
        return false;

    return access(directory.c_str(), W_OK) == 0;
#endif
}

// Creates filePath only if nothing already exists at that name, so a pre-planted file,
// symlink or hardlink at a predictable temp path cannot be written through or later deleted.
// This only reserves the name at call time: the handle is closed before any caller reopens
// filePath by path, so a shared/world-writable directory still allows another process to
// unlink and replace it in that window. Callers must pass a path inside a private directory
// (not writable by other users, e.g. a per-user app-data directory) to actually close the race.
bool CMP_CreateFileExclusive(const std::string& filePath)
{
    if (filePath.empty())
        return false;

#ifdef _WIN32
    // filePath reaches here in either UTF-8 (GUI, via ProcessCMDLine) or the active ANSI code
    // page (CLI argv) -- see CMP_NarrowPathToWide's comment. CreateFileA alone would mis-decode
    // whichever caller doesn't match the active code page, most often just failing outright on
    // a non-ASCII path (ERROR_PATH_NOT_FOUND) rather than creating the intended file.
    std::wstring wideFilePath;
    if (!CMP_NarrowPathToWide(filePath, wideFilePath))
        return false;

    HANDLE handle =
        CreateFileW(wideFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    CloseHandle(handle);
    return true;
#else
    int fd = open(filePath.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (fd < 0)
        return false;

    close(fd);
    return true;
#endif
}

std::string CMP_CreatePrivateTempDirectory()
{
#ifdef _WIN32
    PWSTR appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &appData)))
        return {};
    std::wstring directory(appData);
    CoTaskMemFree(appData);

    GUID    identifier = {};
    wchar_t suffix[40] = {};
    if (FAILED(CoCreateGuid(&identifier)) || StringFromGUID2(identifier, suffix, 40) == 0)
        return {};
    directory += L"\\cmp_transcode_";
    directory += suffix;

    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    DWORD tokenSize = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &tokenSize);
    std::vector<unsigned char> tokenData(tokenSize);
    const bool                 haveUser = GetTokenInformation(token, TokenUser, tokenData.data(), tokenSize, &tokenSize) != FALSE;
    CloseHandle(token);
    if (!haveUser)
        return {};

    LPWSTR userSid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(tokenData.data())->User.Sid, &userSid))
        return {};
    const std::wstring descriptor = std::wstring(L"O:") + userSid + L"D:P(A;OICI;FA;;;" + userSid + L")";
    LocalFree(userSid);
    PSECURITY_DESCRIPTOR security = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(descriptor.c_str(), SDDL_REVISION_1, &security, nullptr))
        return {};
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), security, FALSE};
    const bool          created    = CreateDirectoryW(directory.c_str(), &attributes) != FALSE;
    LocalFree(security);
    if (!created)
        return {};

    const int   length = WideCharToMultiByte(CP_UTF8, 0, directory.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length), '\0');
    if (length <= 1 || !WideCharToMultiByte(CP_UTF8, 0, directory.c_str(), -1, &result[0], length, nullptr, nullptr))
    {
        RemoveDirectoryW(directory.c_str());
        return {};
    }
    result.pop_back();
    return result;
#else
    std::string directory = "/tmp/cmp_transcode_XXXXXX";
    if (!mkdtemp(&directory[0]))
        return {};
    return directory;
#endif
}

bool CMP_RemovePath(const std::string& path)
{
    if (path.empty())
        return false;
#ifdef _WIN32
    std::wstring widePath;
    if (!CMP_NarrowPathToWide(path, widePath))
        return false;
    return DeleteFileW(widePath.c_str()) != FALSE || RemoveDirectoryW(widePath.c_str()) != FALSE;
#else
    return unlink(path.c_str()) == 0 || rmdir(path.c_str()) == 0;
#endif
}

static std::string CMP_QueryExecutableDir()
{
    std::string executableDir;

#ifdef _WIN32
    char  modulePath[MAX_PATH] = {0};
    DWORD length               = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
        executableDir = CMP_GetPath(std::string(modulePath, length));
#elif defined(__APPLE__)
    char     modulePath[4096] = {0};
    uint32_t size             = sizeof(modulePath);
    if (_NSGetExecutablePath(modulePath, &size) == 0)
        executableDir = CMP_GetPath(std::string(modulePath));
#else
    char    modulePath[4096] = {0};
    ssize_t length           = readlink("/proc/self/exe", modulePath, sizeof(modulePath) - 1);
    if (length > 0)
        executableDir = CMP_GetPath(std::string(modulePath, length));
#endif

    return executableDir;
}

std::string CMP_GetExecutableDir()
{
    // Initialised once, under the compiler's guard: the GPU backends resolve install
    // paths from more than one thread and a lazy assignment here would be a data race.
    static const std::string executableDir = CMP_QueryExecutableDir();
    return executableDir;
}

std::string CMP_ResolveInstallPath(const std::string& relativePath)
{
    if (relativePath.empty())
        return relativePath;

    // Already absolute. A bare "C:foo" is drive-relative, not absolute, so also require the
    // separator after the drive letter.
    if (relativePath[0] == '/' || relativePath[0] == '\\' ||
        (relativePath.length() > 2 && relativePath[1] == ':' && (relativePath[2] == '/' || relativePath[2] == '\\')))
        return relativePath;

    std::string trimmed = relativePath;
    while (trimmed.length() > 1 && trimmed[0] == '.' && (trimmed[1] == '/' || trimmed[1] == '\\'))
        trimmed = trimmed.substr(2);

    const std::string executableDir = CMP_GetExecutableDir();
    if (executableDir.empty())
        return relativePath;

    std::string resolved = executableDir + CMP_NativeSeparator() + trimmed;
    if (CMP_FileExists(resolved) || CMP_DirExists(resolved))
        return resolved;

    // Keep the original path so existing working-directory based layouts still work.
    return relativePath;
}

std::string CMP_GetSampleDataDir()
{
    const char               separator = CMP_NativeSeparator();
    std::vector<std::string> candidates;
    const std::string        executableDir = CMP_GetExecutableDir();

#ifdef _WIN32
    PWSTR programData = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, NULL, &programData)) && programData != NULL)
    {
        const int length = WideCharToMultiByte(CP_UTF8, 0, programData, -1, NULL, 0, NULL, NULL);
        if (length > 1)
        {
            std::string utf8(static_cast<size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, programData, -1, &utf8[0], length, NULL, NULL);
            utf8.resize(static_cast<size_t>(length) - 1);
            candidates.push_back(utf8 + separator + CMP_USER_DATA_VENDOR_DIR + separator + CMP_USER_DATA_PRODUCT_DIR);
        }
    }
    CoTaskMemFree(programData);
#elif defined(__APPLE__)
    if (!executableDir.empty())
        candidates.push_back(executableDir + "/../Resources");
#else
    candidates.push_back("/usr/share/compressonator");
    candidates.push_back("/usr/local/share/compressonator");
    if (!executableDir.empty())
        candidates.push_back(executableDir + "/share/compressonator");
#endif

    if (!executableDir.empty())
        candidates.push_back(executableDir);

    for (const std::string& candidate : candidates)
    {
        if (CMP_DirExists(candidate + separator + "images"))
            return candidate;
    }

    return "";
}

static std::string CMP_GetUserRootDir(const char* leafDir)
{
    std::string root;

#ifdef _WIN32
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData == NULL || localAppData[0] == '\0')
        localAppData = getenv("APPDATA");
    if (localAppData == NULL || localAppData[0] == '\0')
        return "";
    root = localAppData;
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return "";
    root = std::string(home) + "/Library/Application Support";
#else
    const char* dataHome = getenv("XDG_DATA_HOME");
    if (dataHome != NULL && dataHome[0] != '\0')
        root = dataHome;
    else
    {
        const char* home = getenv("HOME");
        if (home == NULL || home[0] == '\0')
            return "";
        root = std::string(home) + "/.local/share";
    }
#endif

    const char separator = CMP_NativeSeparator();

    root += separator;
    root += CMP_USER_DATA_VENDOR_DIR;
    root += separator;
    root += CMP_USER_DATA_PRODUCT_DIR;

    if (leafDir != NULL && leafDir[0] != '\0')
    {
        root += separator;
        root += leafDir;
    }

    // CMP_IsDirWritable() reports non-existent directories as unwritable, so on a first run
    // (root not created yet) checking writability before creating the tree would always fail
    // and permanently disable the user data dir. Create the tree first, then verify it.
    if (!CMP_CreateDirTree(root) || !CMP_IsDirWritable(root))
        return "";

    return root;
}

static std::string CMP_AppendUserSubDir(const std::string& root, const std::string& subDir)
{
    if (root.empty() || subDir.empty())
        return root;

    std::string path = root;
    path += CMP_NativeSeparator();
    path += subDir;

    if (!CMP_CreateDirTree(path))
        return root;

    return path;
}

std::string CMP_GetUserDataDir(const std::string& subDir)
{
    return CMP_AppendUserSubDir(CMP_GetUserRootDir(NULL), subDir);
}

std::string CMP_GetUserCacheDir(const std::string& subDir)
{
    return CMP_AppendUserSubDir(CMP_GetUserRootDir("cache"), subDir);
}

std::string CMP_GetUserResultsDir()
{
    const std::string path = CMP_GetUserDataDir("results");

    // CMP_CreateDirTree (via CMP_GetUserDataDir) succeeds on an existing directory whose ACL
    // denies file creation, so probe for write access too rather than handing back a fallback
    // the caller cannot write to.
    if (path.empty() || !CMP_IsDirWritable(path))
        return "";

    return path;
}

// FNV-1a over the whole source path. The shader cache is a single flat folder, so the file
// name on its own would let two same-named sources in different folders share one entry.
static std::string CMP_PathHashPrefix(const std::string& path)
{
    uint32_t hash = 2166136261u;

    for (std::size_t i = 0; i < path.length(); ++i)
    {
        unsigned char character = (unsigned char)path[i];
#ifdef _WIN32
        // Windows paths are case insensitive and accept either separator, so the same file
        // spelled differently must still hash to the same cache entry.
        if (character >= 'A' && character <= 'Z')
            character = (unsigned char)(character - 'A' + 'a');
        if (character == '/')
            character = '\\';
#endif
        hash ^= character;
        hash *= 16777619u;
    }

    char prefix[16] = {0};
    snprintf(prefix, sizeof(prefix), "%08x_", hash);
    return prefix;
}

std::string CMP_GetShaderCacheFilePath(const std::string& sourceFilePath, const std::string& suffix)
{
    const std::string cacheDir = CMP_GetUserCacheDir("shaders");

    // No writable per user location: fall back to the legacy behaviour of keeping
    // the binary next to its source so portable/dev builds keep working.
    if (cacheDir.empty())
        return sourceFilePath + suffix;

    return cacheDir + CMP_NativeSeparator() + CMP_PathHashPrefix(sourceFilePath) + CMP_GetFileName(sourceFilePath) + suffix;
}

bool CMP_ShaderCacheIsStale(const std::string& sourceFilePath, const std::string& cacheFilePath)
{
    struct stat cacheAttrib;
    if (stat(cacheFilePath.c_str(), &cacheAttrib) != 0)
        return true;

    struct stat sourceAttrib;
    if (stat(sourceFilePath.c_str(), &sourceAttrib) != 0)
        return true;

    // st_mtime has one second resolution, so a source edited in the same second the cache was
    // written is indistinguishable from one edited just before it: rebuild rather than risk
    // running a stale kernel.
    return sourceAttrib.st_mtime >= cacheAttrib.st_mtime;
}
