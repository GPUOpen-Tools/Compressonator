//=====================================================================
// Copyright 2026 (c), Advanced Micro Devices, Inc. All rights reserved.
//=====================================================================
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

#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include "cmp_fileio.h"

// Tracks paths handed out via Add() and removes every one of them when the guard goes out of
// scope, so a caller with many early-return paths can't leave per-call temp files on disk.
class ScopedTempFiles
{
public:
    ScopedTempFiles() = default;

    ~ScopedTempFiles()
    {
        for (const std::string& path : m_paths)
            CMP_RemovePath(path);
        if (!m_directory.empty())
            CMP_RemovePath(m_directory);
    }

    ScopedTempFiles(const ScopedTempFiles&)            = delete;
    ScopedTempFiles& operator=(const ScopedTempFiles&) = delete;

    std::string Add(const std::string& path)
    {
        m_paths.push_back(path);
        return path;
    }

    std::string Create()
    {
        if (m_directory.empty())
            m_directory = CMP_CreatePrivateTempDirectory();
        if (m_directory.empty())
            return {};
        const std::string path = CMP_JoinPath(m_directory, std::to_string(m_paths.size()) + ".dds");
        if (!CMP_CreateFileExclusive(path))
            return {};
        return Add(path);
    }

private:
    std::vector<std::string> m_paths;
    std::string              m_directory;
};
