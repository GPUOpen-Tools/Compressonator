//=====================================================================
// Copyright 2023-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef _CMP_FILEIO_H_
#define _CMP_FILEIO_H_

#include <cstdint>
#include <string>
#include <vector>

enum CMP_PATHTYPES
{
    CMP_PATH_IS_UNKNOWN,
    CMP_PATH_IS_DIR,
    CMP_PATH_IS_FILE,
};

#define FILE_SPLIT_PATH "/\\"

// Given a path string, determine the type of the path.
// This is done by purely through string processing, so the path doesn't need to actually exist
// NOTE: Paths that don't end in a slash ('/' or '\') are assumed to be files, even if they don't have extensions
CMP_PATHTYPES CMP_PathType(const char* path);

void  CMP_CreateTextFile(std::string& source_file);
FILE* CMP_OpenTextFile(char* SourceFile, const char* mode);
void  CMP_CloseTextFile(FILE* fp);

uintmax_t CMP_GetFileSize(const std::string& fileName);
bool      CMP_FileExists(const std::string& abs_filename);
void      CMP_FileCopy(std::string& source_file, std::string& destination_file);
bool      CMP_DirExists(const std::string& directory);
bool      CMP_CreateDir(std::string directoryName);
bool      CMP_IsHidden(const std::string& fullPath);

// Returns a list of all files in the directory and any subdirectories
std::vector<std::string> CMP_GetAllFilesInDirectory(const std::string& directory, const std::string& filter);

// This function assumes the input is a file path and returns the parent directory of SourceFile
std::string CMP_GetPath(const std::string& SourceFile);

std::string CMP_FindFile(const std::vector<std::string>& paths, const std::string& filepath);
std::string CMP_ExpandFilePath(const std::string& filepath);
std::string CMP_JoinPath(const std::string& path0, const std::string& path1);

std::string CMP_GetFilePathExtension(const std::string& fileName);
std::string CMP_GetFileExtension(const char* file, bool incDot, bool upperCase);
std::string CMP_GetJustFileExt(const std::string& SourceFile);

std::string CMP_GetBaseDir(const std::string& srcfileDirpath);

// Returns the file name and extension without any preceeding directory paths
std::string CMP_GetFileName(const std::string& filePath);
// Same as above, but also removes the file extension from the result
std::string CMP_GetJustFileName(const std::string& filePath);

std::string CMP_GetFileNameAndExt(const std::string& filePathName);

std::string CMP_GetFullPath(std::string file);

void CMP_ReplaceExt(std::string& s, const std::string& newExt);

// Creates directoryName and every missing parent directory.
bool CMP_CreateDirTree(const std::string& directoryName);

// True when the current process can create files in directory.
bool        CMP_IsDirWritable(const std::string& directory);
bool        CMP_CreateFileExclusive(const std::string& filePath);
std::string CMP_CreatePrivateTempDirectory();
// Removes a file or empty directory without throwing; returns false if nothing was removed.
bool CMP_RemovePath(const std::string& path);

//---------------------------------------------------------------------
// Install (read only) and per user (writable) locations.
//
// Application binaries and shipped resources live under the install
// directory, which is read only for standard users when the product is
// installed to %ProgramFiles%. Anything the application generates at run
// time must go to the per user locations returned below.
//---------------------------------------------------------------------

// Directory containing the running executable.
std::string CMP_GetExecutableDir();

// Resolves a resource shipped with the application, such as
// "./plugins/Compute/BC7_Encode_kernel.cpp", against the executable
// directory. Returns relativePath unchanged when it is already absolute
// or cannot be located next to the executable.
std::string CMP_ResolveInstallPath(const std::string& relativePath);

#define CMP_USER_DATA_VENDOR_DIR "GPUOpen.com"
#define CMP_USER_DATA_PRODUCT_DIR "Compressonator"

// Placeholder the sample .cprj files use for both source and destination paths.
#define CMP_PROJECT_SAMPLES_TOKEN "$(CMP_SAMPLES)"

// Shared, all-users writable folder holding the sample content (the images/ and projects/ sub folders).
// Windows: %ProgramData%/GPUOpen.com/Compressonator (installer)
// Linux  : /usr/share/compressonator, /usr/local/share/compressonator or <exe dir>/share/compressonator
// macOS  : <bundle>/Contents/Resources
// Falls back to the executable directory, which is where developer builds deploy the samples.
// Returns an empty string when no sample content could be found.
std::string CMP_GetSampleDataDir();

// Per user writable root for application data, creating it if needed.
// Windows: %LOCALAPPDATA%/GPUOpen.com/Compressonator
// Linux  : $XDG_DATA_HOME/GPUOpen.com/Compressonator or ~/.local/share/GPUOpen.com/Compressonator
// macOS  : ~/Library/Application Support/GPUOpen.com/Compressonator
// Returns an empty string when no writable location could be created.
std::string CMP_GetUserDataDir(const std::string& subDir = "");

// Per user writable root for regenerable artifacts (compiled kernels and shaders).
std::string CMP_GetUserCacheDir(const std::string& subDir = "");

// Per user writable location for processed output when the requested destination folder is
// read only, such as the sample content installed under the install directory.
// Returns an empty string when no writable location could be created.
std::string CMP_GetUserResultsDir();

// Maps a kernel or shader source path in the install directory to the path of
// its compiled binary in the per user shader cache, for example
// "%LOCALAPPDATA%\GPUOpen.com\Compressonator\cache\shaders\0123abcd_BC7_Encode_kernel.cpp.cmp".
std::string CMP_GetShaderCacheFilePath(const std::string& sourceFilePath, const std::string& suffix);

// True when cacheFilePath is missing, unreadable or older than sourceFilePath.
bool CMP_ShaderCacheIsStale(const std::string& sourceFilePath, const std::string& cacheFilePath);

#endif
