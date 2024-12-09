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
#include <stdio.h>
#include <stdlib.h>
#include "tc_pluginapi.h"
#include "tc_plugininternal.h"
#include "compressonator.h"
#include "format_conversion.h"
#include "ccpu_hpc.h"

CMIPS* CMips = NULL;

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_CCPU_HPC)
SET_PLUGIN_TYPE("PIPELINE")
SET_PLUGIN_NAME("HPC")
#else
void* make_Plugin_HPC()
{
    return new Plugin_CCPU_HPC;
}
#endif

Plugin_CCPU_HPC::Plugin_CCPU_HPC()
{
    m_pComputeBase = NULL;
}

Plugin_CCPU_HPC::~Plugin_CCPU_HPC()
{
    if (m_pComputeBase)
        delete m_pComputeBase;
}

int Plugin_CCPU_HPC::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_CCPU_HPC::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
#ifdef _WIN32
    pPluginVersion->guid = g_GUID_CPU_HPC;
#endif
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

int Plugin_CCPU_HPC::TC_Init(void* kernel_options)
{
    m_pComputeBase = (ComputeBase*)new CCPU_HPC(kernel_options);
    if (m_pComputeBase == NULL)
        return -1;
    return 0;
}

CMP_ERROR Plugin_CCPU_HPC::TC_GetPerformanceStats(void* pPerfStats)
{
    CMP_ERROR result = CMP_ERR_NOPERFSTATS;
    if (m_pComputeBase)
    {
        KernelPerformanceStats* PerfStats   = reinterpret_cast<KernelPerformanceStats*>(pPerfStats);
        PerfStats->m_num_blocks             = m_pComputeBase->GetBlockSize();
        PerfStats->m_computeShaderElapsedMS = m_pComputeBase->GetProcessElapsedTimeMS();
        PerfStats->m_CmpMTxPerSec           = m_pComputeBase->GetMTxPerSec();
        result                              = CMP_OK;
    }
    return result;
}

CMP_ERROR Plugin_CCPU_HPC::TC_GetDeviceInfo(void* pDeviceInfo)
{
    CMP_ERROR result = CMP_ERR_NOPERFSTATS;
    if (m_pComputeBase)
    {
        KernelDeviceInfo* DeviceInfo = reinterpret_cast<KernelDeviceInfo*>(pDeviceInfo);
        snprintf(DeviceInfo->m_deviceName, sizeof(DeviceInfo->m_deviceName), "%s", m_pComputeBase->GetDeviceName());
        snprintf(DeviceInfo->m_version, sizeof(DeviceInfo->m_version), "%s", m_pComputeBase->GetVersion());
        DeviceInfo->m_maxUCores = m_pComputeBase->GetMaxUCores();
        result                  = CMP_OK;
    }
    return result;
}

// TODO: For the future, this can be simplified if srcTexture was passed by value instead of reference
CMP_ERROR Plugin_CCPU_HPC::TC_Compress(void* options, MipSet& srcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    CMP_ERROR result = CMP_OK;

#ifdef ENABLE_MAKE_COMPATIBLE_API
    ConvertedBuffer tempBuffer = CreateCompatibleBuffer(destTexture.m_format, &srcTexture);

    CMP_FORMAT prevFormat   = srcTexture.m_format;
    CMP_BYTE*  prevData     = srcTexture.pData;
    CMP_DWORD  prevDataSize = srcTexture.dwDataSize;

    srcTexture.m_format   = tempBuffer.format;
    srcTexture.pData      = (CMP_BYTE*)tempBuffer.data;
    srcTexture.dwDataSize = tempBuffer.dataSize;
#endif

    if (m_pComputeBase)
        result = m_pComputeBase->Compress((KernelOptions*)options, srcTexture, destTexture, pFeedback, pUser1, pUser2);

#ifdef ENABLE_MAKE_COMPATIBLE_API
    if (tempBuffer.isBufferNew)
    {
        // restore original data
        srcTexture.m_format   = prevFormat;
        srcTexture.pData      = prevData;
        srcTexture.dwDataSize = prevDataSize;
    }
#endif

    return result;
}

void Plugin_CCPU_HPC::TC_SetComputeOptions(void* options)
{
    if (m_pComputeBase)
        m_pComputeBase->SetComputeOptions((ComputeOptions*)options);
}

char* Plugin_CCPU_HPC::TC_ComputeSourceFile()
{
    return NULL;
}

int Plugin_CCPU_HPC::TC_Close()
{
    if (m_pComputeBase)
    {
        delete m_pComputeBase;
        m_pComputeBase = NULL;
    }
    return 0;
}