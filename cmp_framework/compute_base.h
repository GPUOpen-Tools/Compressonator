//=====================================================================
// Copyright (c) 2016-2024    Advanced Micro Devices, Inc. All rights reserved.
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
/// \file Compute_Base.h
//
//=====================================================================

#ifndef COMPUTE_BASE_H
#define COMPUTE_BASE_H

#include "compressonator.h"
#include "common.h"
#include "common_kerneldef.h"
#include "texture.h"

namespace CMP_Compute_Base
{

class RenderWindow
{
public:
    RenderWindow() {};
    virtual ~RenderWindow() {};
};

class ComputeBase : public RenderWindow
{
public:
    ComputeBase() {};
    virtual ~ComputeBase() {};

    virtual CMP_ERROR   Compress(KernelOptions* Options, MipSet& SrcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback) = 0;
    virtual void        SetComputeOptions(ComputeOptions* options)                                                             = 0;
    virtual float       GetProcessElapsedTimeMS()                                                                              = 0;
    virtual float       GetMTxPerSec()                                                                                         = 0;
    virtual int         GetBlockSize()                                                                                         = 0;
    virtual const char* GetDeviceName()                                                                                        = 0;
    virtual const char* GetVersion()                                                                                           = 0;
    virtual int         GetMaxUCores()                                                                                         = 0;

private:
};
}  // namespace CMP_Compute_Base

const CMP_CHAR* GetEncodeWithDesc(CMP_Compute_type nFormat);

// True when compiledShaderFile is missing or older than sourceShaderFile.
bool cmp_recompile_shader(const std::string& sourceShaderFile, const std::string& compiledShaderFile);

#endif  // !COMPUTE_BASE_H
