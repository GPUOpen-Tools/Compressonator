//=====================================================================
// Copyright 2016-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include "compressonator.h"
#include "common.h"
#include "format_conversion.h"
#include "tc_pluginapi.h"
#include "tc_plugininternal.h"
#include "cdirectx.h"

CMIPS* GPU_DXMips = nullptr;

#define BUILD_AS_PLUGIN_DLL

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_CDirectX)
SET_PLUGIN_TYPE("PIPELINE")
SET_PLUGIN_NAME("GPU_DXC")
#else
void* make_Plugin_Compute_DirectX()
{
    return new Plugin_CDirectX;
}
#endif

#pragma comment(lib, "advapi32.lib")  // for RegCloseKey and other Reg calls ...

Plugin_CDirectX::Plugin_CDirectX()
{
    m_pComputeBase = NULL;
}

Plugin_CDirectX::~Plugin_CDirectX()
{
    if (m_pComputeBase)
        delete m_pComputeBase;
}

int Plugin_CDirectX::TC_PluginSetSharedIO(void* Shared)
{
    GPU_DXMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_CDirectX::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID_DIREXTX;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

int Plugin_CDirectX::TC_Init(void* kernel_options)
{
    m_pComputeBase = (ComputeBase*)new CDirectX(kernel_options);
    if (m_pComputeBase == NULL)
        return -1;
    return 0;
}

// TODO: For the future, this can be simplified if srcTexture was passed by value instead of reference
CMP_ERROR Plugin_CDirectX::TC_Compress(void* options, MipSet& srcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback)
{
    CMP_ERROR result = CMP_OK;

#ifdef ENABLE_MAKE_COMPATIBLE_API
    ConvertedBuffer tempBuffer = CreateCompatibleBuffer(destTexture.m_format, &srcTexture);

    // A failed conversion returns an empty buffer; passing its null data on would crash the encoder.
    if (tempBuffer.data == NULL)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;

    CMP_FORMAT prevFormat   = srcTexture.m_format;
    CMP_BYTE*  prevData     = srcTexture.pData;
    CMP_DWORD  prevDataSize = srcTexture.dwDataSize;

    srcTexture.m_format   = tempBuffer.format;
    srcTexture.pData      = (CMP_BYTE*)tempBuffer.data;
    srcTexture.dwDataSize = tempBuffer.dataSize;
#endif

    if (m_pComputeBase)
        result = m_pComputeBase->Compress((KernelOptions*)options, srcTexture, destTexture, pFeedback);

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

CMP_ERROR Plugin_CDirectX::TC_GetPerformanceStats(void* pPerfStats)
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

CMP_ERROR Plugin_CDirectX::TC_GetDeviceInfo(void* pDeviceInfo)
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

void Plugin_CDirectX::TC_SetComputeOptions(void* options)
{
    if (m_pComputeBase)
        m_pComputeBase->SetComputeOptions((ComputeOptions*)options);
}

char* Plugin_CDirectX::TC_ComputeSourceFile()
{
    return NULL;
}

int Plugin_CDirectX::TC_Close()
{
    if (m_pComputeBase)
    {
        delete m_pComputeBase;
        m_pComputeBase = NULL;
    }
    return 0;
}