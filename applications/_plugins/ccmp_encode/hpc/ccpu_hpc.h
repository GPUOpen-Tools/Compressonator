//=====================================================================
// Copyright (c) 2020-2024    Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef _Plugin_COPENCL_H
#define _Plugin_COPENCL_H

#pragma once

#ifdef _WIN32
#include <tchar.h>
#endif
#include <assert.h>

#include "compute_base.h"
#include "compute_cpu_hpc.h"
#include "compressonator.h"
#include "plugininterface.h"
#include "cmp_plugininterface.h"
#include "cmp_math_common.h"
//#include "debug.h"

// {E8378561-F1CE-45D7-BD3C-695D4FB5CE44}
#ifdef _WIN32
static const GUID g_GUID_CPU_HPC = {0xe8378561, 0xf1ce, 0x45d7, {0xbd, 0x3c, 0x69, 0x5d, 0x4f, 0xb5, 0xce, 0x44}};
#else
static const GUID g_GUID_CPU_HPC = {0};
#endif

#define TC_PLUGIN_VERSION_MAJOR 1
#define TC_PLUGIN_VERSION_MINOR 0

using namespace CMP_Compute_Base;

class Plugin_CCPU_HPC : public PluginInterface_Pipeline
{
public:
    Plugin_CCPU_HPC();
    virtual ~Plugin_CCPU_HPC();
    int       TC_PluginGetVersion(TC_PluginVersion* pPluginVersion);
    int       TC_Init(void* kernel_options);
    int       TC_PluginSetSharedIO(void* Shared);
    CMP_ERROR TC_Compress(void* kernel_options, MipSet& SrcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2);
    void      TC_SetComputeOptions(void* options);
    char*     TC_ComputeSourceFile();
    CMP_ERROR TC_GetPerformanceStats(void* pPerfStats);
    CMP_ERROR TC_GetDeviceInfo(void* pDeviceInfo);
    int       TC_Close();

private:
    ComputeBase* m_pComputeBase;
};

#endif