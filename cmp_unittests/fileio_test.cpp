//=====================================================================
// Copyright (c) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.
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

#include <string>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#include "single_include/catch2/catch.hpp"

#include "compressonator.h"
#include "cmp_fileio.h"
#include "crc32.h"
#include "test_constants.h"
#include "stdio.h"
#ifdef _WIN32
#include "direct.h"
#include <windows.h>
#endif

// setenv() with an empty value keeps the variable defined but empty, which is what the
// per user directory resolution treats as "not set"; _putenv_s() with "" removes it.
static void CMP_SetEnvForTest(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    if (value && value[0] != '\0')
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

static std::string CMP_GetEnvForTest(const char* name)
{
#ifdef _WIN32
    char*  buffer = nullptr;
    size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr)
        return std::string();
    std::string value(buffer);
    free(buffer);
    return value;
#else
    const char* value = getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

TEST_CASE("CreateDirectory_EmptyName", "[fileio]")
{
    const std::string outputFolder = TEST_DATA_PATH + std::string("/ ");
    bool              status       = CMP_CreateDir(outputFolder);
    REQUIRE(status == false);
}

TEST_CASE("GetFileName_Basic", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    SECTION("Get Just File Name")
    {
        const std::string result = CMP_GetJustFileName(filePath);
        REQUIRE(result == "mipmap_128x512");
    }

    SECTION("Get File Name")
    {
        const std::string result = CMP_GetFileName(filePath);
        REQUIRE(result == "mipmap_128x512.png");
    }
}

TEST_CASE("GetFileName_EmptyName", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("/ ");
    SECTION("Get Just File Name")
    {
        const std::string result = CMP_GetJustFileName(filePath);
        REQUIRE(result == " ");
    }

    SECTION("Get File Name")
    {
        const std::string result = CMP_GetFileName(filePath);
        REQUIRE(result == " ");
    }
}

TEST_CASE("GetFileName_GivenWeirdName", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("/mipmap_128x512./.png");
    SECTION("Get Just File Name")
    {
        const std::string result = CMP_GetJustFileName(filePath);
        REQUIRE(result == "");
    }

    SECTION("Get File Name")
    {
        const std::string result = CMP_GetFileName(filePath);
        REQUIRE(result == ".png");
    }
}

TEST_CASE("GetFileName_NoExt", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("testCreateDir");
    SECTION("Get Just File Name")
    {
        const std::string result = CMP_GetJustFileName(filePath);
        REQUIRE(result == "test_datatestCreateDir");
    }

    SECTION("Get File Name")
    {
        const std::string result = CMP_GetFileName(filePath);
        REQUIRE(result == "test_datatestCreateDir");
    }
}

TEST_CASE("GetFileSize", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    uintmax_t         result   = CMP_GetFileSize(filePath);
    REQUIRE(result == 1419);
}

TEST_CASE("GetFileSize_FileDoesNotExist", "[fileio]")
{
    const std::string filePath = TEST_DATA_PATH + std::string("/hello.txt");
    uintmax_t         result   = 0;
    REQUIRE_THROWS(result = CMP_GetFileSize(filePath));
}

bool CompareFiles(std::string& sourceFile, std::string& destinationFile)
{
    FILE*   fpSrc;
    errno_t err = fopen_s(&fpSrc, sourceFile.c_str(), "rb");
    if (err != 0)
        return false;

    fseek(fpSrc, 0, SEEK_END);
    int sourceSize = ftell(fpSrc);
    rewind(fpSrc);

    FILE* fpDest;
    err = fopen_s(&fpDest, destinationFile.c_str(), "rb");
    if (err != 0)
    {
        fclose(fpSrc);
        return false;
    }

    fseek(fpDest, 0, SEEK_END);
    int destSize = ftell(fpDest);
    rewind(fpDest);

    if (sourceSize != destSize)
    {
        fclose(fpSrc);
        fclose(fpDest);
        return false;
    }

    int  c1, c2;
    bool filesEqual = true;
    while (1)
    {
        c1 = getc(fpSrc);
        c2 = getc(fpDest);

        if (c1 != c2)
        {
            filesEqual = false;
            break;
        }

        if ((c1 == EOF) || (c2 == EOF))
            break;
    }

    fclose(fpSrc);
    fclose(fpDest);

    return filesEqual;
}

TEST_CASE("FileCopy", "[fileio]")
{
    std::string sourceFile = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    std::string destFile   = TEST_DATA_PATH + std::string("/copy_mipmap_128x512.png");

    if (CMP_FileExists(destFile))
        remove(destFile.c_str());

    CMP_FileCopy(sourceFile, destFile);
    bool compare = CompareFiles(sourceFile, destFile);
    REQUIRE(compare == true);
}

TEST_CASE("FileCopy_SourceDoesNotExist", "[fileio]")
{
    std::string sourceFile = TEST_DATA_PATH + std::string("/cat.png");
    std::string destFile   = TEST_DATA_PATH + std::string("/copy_mipmap_128x512.png");

    if (CMP_FileExists(destFile))
        remove(destFile.c_str());

    REQUIRE_THROWS(CMP_FileCopy(sourceFile, destFile));
    //bool compare = CompareFiles(sourceFile, destFile);
    //REQUIRE_FALSE(compare);
}

TEST_CASE("DirExists_Basic", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/testCreateDir");
    int               error     = 0;
    if (CMP_DirExists(directory))
        error = _rmdir(directory.c_str());
    REQUIRE(error == 0);

    error = _mkdir(directory.c_str());
    REQUIRE(error == 0);

    bool dirExist = CMP_DirExists(directory);

    REQUIRE(dirExist == true);
}

TEST_CASE("DirExists_DirDoesNotExist", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/hello");
    bool              dirExist  = CMP_DirExists(directory);
    REQUIRE(dirExist == false);
}

TEST_CASE("DirExists_GivenFile", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/createTextFile.txt");
    bool              dirExist  = CMP_DirExists(directory);
    REQUIRE(dirExist == false);
}

TEST_CASE("FileExists_Basic", "[fileio]")
{
    std::string file   = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    bool        isFile = CMP_FileExists(file);
    REQUIRE(isFile == true);
}

TEST_CASE("FileExists_FileDoesNotExist", "[fileio]")
{
    std::string file      = TEST_DATA_PATH + std::string("/hello.png");
    bool        fileExist = CMP_FileExists(file);
    REQUIRE(fileExist == false);
}

TEST_CASE("CreateDir_ValidName", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/testCreateDir");
    int               error     = 0;

    bool dirExist = CMP_DirExists(directory);
    if (dirExist == true)
        error = _rmdir(directory.c_str());

    REQUIRE(error == 0);

    bool dirCreated = CMP_CreateDir(directory);
    REQUIRE(dirCreated == true);
}

TEST_CASE("CreateDir_InvalidName", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/testCreateDir?bad:");
    int               error     = 0;

    bool dirExist = CMP_DirExists(directory);
    if (dirExist == true)
        error = _rmdir(directory.c_str());

    REQUIRE(error == 0);

    bool dirCreated = CMP_CreateDir(directory);
    REQUIRE(dirCreated == false);
}

TEST_CASE("GetFileNameAndExt", "[fileio]")
{
    std::string path       = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    std::string nameAndExt = CMP_GetFileNameAndExt(path);
    REQUIRE(nameAndExt == "mipmap_128x512.png");
}

TEST_CASE("GetFileNameAndExt_EmptyString", "[fileio]")
{
    std::string path       = TEST_DATA_PATH + std::string(" ");
    std::string nameAndExt = CMP_GetFileNameAndExt(path);
    REQUIRE(nameAndExt == "test_data ");
}

TEST_CASE("FileExt", "[fileio]")
{
    const std::string path = TEST_DATA_PATH + std::string("/mipmap_128x512.png");

    SECTION("GetJustFileExt")
    {
        std::string extention = CMP_GetJustFileExt(path);
        REQUIRE(extention == ".png");
    }

    SECTION("GetFilePathExtension")
    {
        std::string extention = CMP_GetFilePathExtension(path);
        REQUIRE(extention == "png");
    }

    SECTION("GetFileExtension")
    {
        bool        incDot        = true;
        bool        upperCase     = true;
        const char* pathChar      = path.c_str();
        std::string fileExtension = CMP_GetFileExtension(pathChar, incDot, upperCase);
        REQUIRE(fileExtension == ".PNG");
    }
}

TEST_CASE("FileExt_GivenDir", "[fileio]")
{
    const std::string path = TEST_DATA_PATH + std::string("testCreateDir");

    SECTION("GetJustFileExt")
    {
        std::string extention = CMP_GetJustFileExt(path);
        REQUIRE(extention == "");
    }

    SECTION("GetFilePathExtension")
    {
        std::string extention = CMP_GetFilePathExtension(path);
        REQUIRE(extention == "");
    }

    SECTION("GetFileExtension")
    {
        bool        incDot        = true;
        bool        upperCase     = true;
        const char* pathChar      = path.c_str();
        std::string fileExtension = CMP_GetFileExtension(pathChar, incDot, upperCase);
        REQUIRE(fileExtension == "");
    }
}

TEST_CASE("GetPath", "[fileio]")
{
    std::string path = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    std::string returnedPath;

    SECTION("GetParentPath")
    {
        returnedPath = CMP_GetPath(path);
        REQUIRE(returnedPath == "./test_data");
    }

    SECTION("GetFullPath")
    {
        // TO-DO
        // Cannot test absolute path since the test will only work on author's local device
    }

    SECTION("GetBaseDir")
    {
        returnedPath = CMP_GetBaseDir(path);
        REQUIRE(returnedPath == "./test_data");
    }
}

TEST_CASE("IsHidden_Basic", "[fileio]")
{
    std::string path     = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    bool        IsHidden = CMP_IsHidden(path);
    REQUIRE(IsHidden == false);
}

TEST_CASE("IsHidden_FileDoesNotExist", "[fileio]")
{
    std::string path     = TEST_DATA_PATH + std::string("/hello.png");
    bool        IsHidden = CMP_IsHidden(path);
    REQUIRE(IsHidden == false);
}

TEST_CASE("GetAllDirFilesList", "[fileio]")
{
    std::string path = TEST_DATA_PATH;
    // The current format of file path is needed because of the nature of the functions called. CMP_GetAllFilesInDirectory manually adds "\" to the file path name.
    std::vector<std::string> files = {"./test_data\\16bit_image.png",
                                      "./test_data\\mipmap_128x128.png",
                                      "./test_data\\mipmap_128x512.png",
                                      "./test_data\\mipmap_512x128.png",
                                      "./test_data\\ruby.bmp"};

    std::string filter;

    SECTION("CMP_GetAllFilesInDirectory")
    {
        std::vector<std::string> returnedFiles = CMP_GetAllFilesInDirectory(path, filter);

        for (std::string& expectedFile : files)
        {
            CHECK(std::find(returnedFiles.begin(), returnedFiles.end(), expectedFile) != returnedFiles.end());
        }
    }
}

//=================== Generic for all platforms and c++ standards ===============================

TEST_CASE("CMP_ExpandFilePath", "[fileio]")
{
    std::string path   = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    std::string result = CMP_ExpandFilePath(path);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("JoinPath_OnlyP1Slash", "[fileio]")
{
    std::string path0  = TEST_DATA_PATH;
    std::string path1  = "/mipmap_128x512.png";
    std::string result = CMP_JoinPath(path0, path1);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("JoinPath_BothNoSlash", "[fileio]")
{
    std::string path0  = TEST_DATA_PATH;
    std::string path1  = "mipmap_128x512.png";
    std::string result = CMP_JoinPath(path0, path1);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("JoinPath_BothSlash", "[fileio]")
{
    std::string path0  = TEST_DATA_PATH + std::string("/");
    std::string path1  = "/mipmap_128x512.png";
    std::string result = CMP_JoinPath(path0, path1);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("JoinPath_OnlyP0Slash", "[fileio]")
{
    std::string path0  = TEST_DATA_PATH + std::string("/");
    std::string path1  = "mipmap_128x512.png";
    std::string result = CMP_JoinPath(path0, path1);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("FindFile", "[fileio]")
{
    const std::vector<std::string> paths    = {"./images", "./plugins", "./projects", "./resources", "./test_data"};
    std::string                    filepath = "/mipmap_128x512.png";
    std::string                    result   = CMP_FindFile(paths, filepath);
    REQUIRE(result == "./test_data/mipmap_128x512.png");
}

TEST_CASE("FindFile_FileDoesNotExist", "[fileio]")
{
    const std::vector<std::string> paths    = {"./images", "./plugins", "./projects", "./resources", "./test_data"};
    std::string                    filepath = "/hello.png";
    std::string                    result   = CMP_FindFile(paths, filepath);
    REQUIRE(result == "");
}

TEST_CASE("CMP_CreateTextFile", "[fileio]")
{
    std::string sourceFile = TEST_DATA_PATH + std::string("/createTextFile.txt");
    int         error      = 0;
    bool        exist      = CMP_FileExists(sourceFile);
    if (exist == true)
        error = remove(sourceFile.c_str());

    REQUIRE(error == 0);

    CMP_CreateTextFile(sourceFile);
    bool fileCreated = CMP_FileExists(sourceFile);
    REQUIRE(fileCreated == true);
}

TEST_CASE("CMP_OpenTextFile", "[fileio]")
{
    std::string sourceFile     = TEST_DATA_PATH + std::string("/createTextFile.txt");
    char*       sourceFileChar = &sourceFile[0];
    FILE*       fp             = CMP_OpenTextFile(sourceFileChar, "r");
    REQUIRE(fp != NULL);
    CMP_CloseTextFile(fp);
}

TEST_CASE("CMP_PathType_Basic", "[fileio]")
{
    std::string   path     = TEST_DATA_PATH + std::string("/createTextFile.txt");
    const char*   pathChar = path.c_str();
    CMP_PATHTYPES pathtype = CMP_PathType(pathChar);
    REQUIRE(pathtype == CMP_PATH_IS_FILE);
}

TEST_CASE("CMP_PathType_RelativePath", "[fileio]")
{
    std::string   path     = "..\..\testing.txt";
    const char*   pathChar = path.c_str();
    CMP_PATHTYPES pathtype = CMP_PathType(pathChar);
    REQUIRE(pathtype == CMP_PATH_IS_FILE);
}

TEST_CASE("CMP_PathType_NoExtensionFile", "[fileio]")
{
    std::string   path     = TEST_DATA_PATH + std::string("/createTextFile");
    const char*   pathChar = path.c_str();
    CMP_PATHTYPES pathtype = CMP_PathType(pathChar);
    REQUIRE(pathtype == CMP_PATH_IS_FILE);
}

TEST_CASE("CMP_ReplaceExt", "[fileio]")
{
    std::string file   = TEST_DATA_PATH + std::string("/createTextFile.txt");
    std::string newExt = "pdf";
    CMP_ReplaceExt(file, newExt);
    std::string newFile = TEST_DATA_PATH + std::string("/createTextFile.pdf");
    REQUIRE(file == newFile);
}

//=====================================================================================
// Read-only source/destination redirect support.
//
// These tests cover the primitives that the CLI destination gate in cmdline.cpp and
// the GUI compress/mesh/project-save paths rely on to redirect processed output (and
// compiled GPU shader/kernel binaries) to a per user writable location whenever the
// requested destination is a read only install or source folder. See
// applications/_plugins/common/cmdline.cpp (destination folder gate) and
// applications/_plugins/common/cmp_fileio.cpp (CMP_IsDirWritable, CMP_GetUserDataDir,
// CMP_GetUserCacheDir, CMP_GetShaderCacheFilePath).
//=====================================================================================

TEST_CASE("IsDirWritable_ExistingWritableDir", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH;
    REQUIRE(CMP_IsDirWritable(directory) == true);
}

TEST_CASE("IsDirWritable_DirDoesNotExist", "[fileio]")
{
    const std::string directory = TEST_DATA_PATH + std::string("/no_such_dir_xyz");
    REQUIRE(CMP_IsDirWritable(directory) == false);
}

TEST_CASE("IsDirWritable_EmptyPath", "[fileio]")
{
    REQUIRE(CMP_IsDirWritable("") == false);
}

TEST_CASE("IsDirWritable_NonAsciiUtf8Dir", "[fileio]")
{
    // Qt's QString::toStdString() (used by the GUI to call CMP_IsDirWritable()) yields UTF-8, which
    // can contain bytes outside the current Windows ANSI code page. Regression guard for
    // mis-decoding that UTF-8 path with an ANSI Win32 call and misreporting it as unwritable.
    // The directory itself is created via CreateDirectoryW from a wide-string literal (bypassing
    // CMP_CreateDirTree/std::filesystem's own narrow-to-wide conversion) so the test is isolated
    // to CMP_IsDirWritable's own UTF-8 handling.
#ifdef _WIN32
    const std::wstring wideName = L"./test_data/\u00E9\u00E9_\u6D4B\u8BD5_dir";
    const std::string  utf8Dir  = std::string(TEST_DATA_PATH) + "/\xC3\xA9\xC3\xA9_\xE6\xB5\x8B\xE8\xAF\x95_dir";

    CreateDirectoryW(wideName.c_str(), NULL);
    REQUIRE(CMP_IsDirWritable(utf8Dir) == true);
    RemoveDirectoryW(wideName.c_str());
#endif
}

TEST_CASE("IsDirWritable_NonAsciiAnsiDir", "[fileio]")
{
    // The CLI reaches CMP_IsDirWritable() with active-code-page bytes, not UTF-8: it uses a narrow
    // main(argv) and CMP_GetUserDataDir() builds on getenv("LOCALAPPDATA"). Regression guard for
    // decoding those bytes as UTF-8, which would misreport a non-ASCII profile/destination folder
    // as unwritable and silently redirect CLI output to the fallback results folder.
#ifdef _WIN32
    const std::wstring wideName = L"./test_data/\u00E9\u00E9_ansi_dir";

    BOOL usedDefaultChar    = FALSE;
    char ansiName[MAX_PATH] = {0};
    int  ansiLen            = WideCharToMultiByte(CP_ACP, 0, wideName.c_str(), -1, ansiName, MAX_PATH, NULL, &usedDefaultChar);

    // Skip on a code page that cannot represent the name (the lossy conversion would not round trip).
    if (ansiLen > 0 && !usedDefaultChar)
    {
        CreateDirectoryW(wideName.c_str(), NULL);
        REQUIRE(CMP_IsDirWritable(std::string(ansiName)) == true);
        RemoveDirectoryW(wideName.c_str());
    }
#endif
}

TEST_CASE("GetUserDataDir_ResultsSubdir_IsWritable", "[fileio]")
{
    // Both the CLI (cmdline.cpp destination gate) and the GUI (CMP_GetUserResultsFolder)
    // redirect processed image/mesh output here when the requested destination folder is
    // not writable. Regression guard for that fallback location actually existing and
    // being writable.
    std::string resultsDir = CMP_GetUserDataDir("results");
    REQUIRE_FALSE(resultsDir.empty());
    REQUIRE(CMP_DirExists(resultsDir));
    REQUIRE(CMP_IsDirWritable(resultsDir));
}

TEST_CASE("GetUserCacheDir_ShadersSubdir_IsWritable", "[fileio]")
{
    std::string cacheDir = CMP_GetUserCacheDir("shaders");
    REQUIRE_FALSE(cacheDir.empty());
    REQUIRE(CMP_DirExists(cacheDir));
    REQUIRE(CMP_IsDirWritable(cacheDir));
}

TEST_CASE("GetShaderCacheFilePath_RedirectsToUserCacheDir", "[fileio]")
{
    // The OpenCL and DirectX GPU compute backends (compute_opencl.cpp, compute_directx.cpp)
    // always resolve their compiled kernel/shader binary cache through this helper, so
    // shader compilation/caching keeps working even when the source shader lives in a
    // read only install location.
    const std::string sourceShader = TEST_DATA_PATH + std::string("/mipmap_128x512.png");  // stand-in path, contents unused
    std::string       cacheFile    = CMP_GetShaderCacheFilePath(sourceShader, ".cmp");
    std::string       cacheDir     = CMP_GetUserCacheDir("shaders");

    REQUIRE_FALSE(cacheDir.empty());
    REQUIRE(cacheFile.rfind(cacheDir, 0) == 0);   // cacheFile starts with the per user cache dir
    REQUIRE(cacheFile != sourceShader + ".cmp");  // must not fall back to the source-adjacent legacy path
}

TEST_CASE("GetShaderCacheFilePath_SameNameDifferentDirs_DoNotCollide", "[fileio]")
{
    // The cache is a single flat folder, so the path - not just the file name - has to drive
    // the cache entry, otherwise two same-named shaders overwrite each other's binaries.
    const std::string first  = CMP_GetShaderCacheFilePath("/shaders/a/bcn_encode.cl", ".cmp");
    const std::string second = CMP_GetShaderCacheFilePath("/shaders/b/bcn_encode.cl", ".cmp");

    REQUIRE(first != second);
    REQUIRE(CMP_GetShaderCacheFilePath("/shaders/a/bcn_encode.cl", ".cmp") == first);
}

TEST_CASE("GetShaderCacheFilePath_FallsBackToSourceAdjacent_WhenNoUserDir", "[fileio]")
{
    // Simulate an environment where no per user writable location can be resolved (e.g. a
    // sandboxed/portable build with no LOCALAPPDATA/APPDATA) by clearing the environment
    // variables CMP_GetUserRootDir() depends on, then confirm the legacy behaviour of
    // keeping the binary next to its source still works so those builds keep functioning.
#ifdef _WIN32
    static const char* const varNames[] = {"LOCALAPPDATA", "APPDATA"};
#elif defined(__APPLE__)
    static const char* const varNames[] = {"HOME"};
#else
    static const char* const varNames[] = {"XDG_DATA_HOME", "HOME"};
#endif
    const size_t varCount = sizeof(varNames) / sizeof(varNames[0]);

    std::string saved[sizeof(varNames) / sizeof(varNames[0])];
    for (size_t i = 0; i < varCount; ++i)
    {
        saved[i] = CMP_GetEnvForTest(varNames[i]);
        CMP_SetEnvForTest(varNames[i], "");
    }

    const std::string sourceShader = TEST_DATA_PATH + std::string("/mipmap_128x512.png");
    std::string       cacheFile    = CMP_GetShaderCacheFilePath(sourceShader, ".cmp");

    // Restore the environment before asserting so a failed REQUIRE never leaves the
    // process (and any subsequent test cases) without those variables.
    for (size_t i = 0; i < varCount; ++i)
        CMP_SetEnvForTest(varNames[i], saved[i].c_str());

    REQUIRE(cacheFile == sourceShader + ".cmp");
}

TEST_CASE("CreateFileExclusive_ClaimsAnUnusedName", "[fileio]")
{
    const std::string path = TEST_DATA_PATH + std::string("/cmp_exclusive_new.tmp");
    std::remove(path.c_str());

    REQUIRE(CMP_CreateFileExclusive(path) == true);
    REQUIRE(CMP_FileExists(path));

    std::remove(path.c_str());
}

TEST_CASE("CreateFileExclusive_RefusesAnExistingName", "[fileio]")
{
    // The transcoding intermediate file (cmdline.cpp GetTranscodeTempFilePath) uses this to
    // avoid writing through, and later deleting, whatever a hostile process pre-planted at a
    // guessable temp path in a shared working directory.
    const std::string path = TEST_DATA_PATH + std::string("/cmp_exclusive_existing.tmp");
    std::remove(path.c_str());
    std::string seedPath = path;
    CMP_CreateTextFile(seedPath);
    REQUIRE(CMP_FileExists(path));

    const bool claimed = CMP_CreateFileExclusive(path);

    std::remove(path.c_str());
    REQUIRE(claimed == false);
}

TEST_CASE("CreateFileExclusive_EmptyPath", "[fileio]")
{
    REQUIRE(CMP_CreateFileExclusive("") == false);
}

TEST_CASE("CRC32_TableInitDoesNotOverrunItsBuffer", "[fileio]")
{
    // InitCRC32Table() used a <= bound over a 256 entry table, writing one element past the
    // end of m_crc32Table and into the neighbouring member. Two instances must therefore
    // agree, and the digest must match the known CRC-32 of a fixed input.
    unsigned char data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

    CRC32 first;
    first.ProcessBuffer(data, (int)sizeof(data), 1);

    CRC32 second;
    second.ProcessBuffer(data, (int)sizeof(data), 1);

    REQUIRE(first.GetCRC() == second.GetCRC());
    REQUIRE(first.GetCRC() == 0xCBF43926u);
}
