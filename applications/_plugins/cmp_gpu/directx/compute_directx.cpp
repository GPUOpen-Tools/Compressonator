//=========================================================================
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
// \file Compute_DirectX.cpp
//
//=====================================================================
//--------------------------------------------------------------------------------------
//                                The MIT License (MIT)
//
// Copyright (c) 2004-2020 Microsoft Corp
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "cdirectx.h"
#include "cmp_fileio.h"
#include "stdio.h"
#include "minwindef.h"  // max,min
#include <comdef.h>
#include <d3dcommon.h>

using namespace std;

#pragma comment(lib, "dxguid.lib")

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#define USE_ALL_BC7_MODES
#define USE_ALL_BC6_MODES

extern CMIPS* GPU_DXMips;

void PrintDX(const char* Format, ...)
{
    // define a pointer to save argument list
    va_list args;
    char    buff[1024];
    // process the arguments into our debug buffer
    va_start(args, Format);
    vsprintf_s(buff, Format, args);
    va_end(args);

    if (GPU_DXMips)
    {
        GPU_DXMips->Print(buff);
    }
    else
    {
        printf(buff);
    }
}

//======================================  Direct Compute Interfaces ==========================================

void CDirectX::QueryDispatchBegin()
{
}

void CDirectX::QueryDispatchEnd(unsigned int numBlocks)
{
    m_totalnumBlocks += numBlocks;
}

void CDirectX::QueryProcessBegin(int miplevel)
{
    if (m_getGPUPerfStats && m_initQueryOk)
    {
        m_cmpTimer.Start(miplevel);
        m_totalnumBlocks = 0;
        m_pContext->Begin(m_pQueryDisjoint);  // Begin Disjoint (used to get current GPU frequency)
        m_pContext->End(m_pQueryBegin);       // Start Time Stamp to profile current Dispatch call
    }
}

void CDirectX::QueryProcessEnd(int miplevel)
{
    if (m_getGPUPerfStats && m_initQueryOk)
    {
        m_pContext->End(m_pQueryEnd);
        m_pContext->End(m_pQueryDisjoint);
        m_pContext->Flush();  // May not be need for more recent DX versions

        // Wait for data to become available
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
        while (m_pContext->GetData(m_pQueryDisjoint, &tsDisjoint, sizeof(tsDisjoint), 0) == S_FALSE)
        {
            Sleep(0);  // Give other threads time to process
        }

        m_cmpTimer.Stop(miplevel);

        // disgard this block analysis if GPU is not valid!
        if (!tsDisjoint.Disjoint)
        {
            // Get GPU counter increments in Hz.
            m_GPUFrequency = (float)tsDisjoint.Frequency;
            if (m_GPUFrequency <= m_GPUFrequencyMin)
                m_GPUFrequencyMin = m_GPUFrequency;
            if (m_GPUFrequency > m_GPUFrequencyMax)
                m_GPUFrequencyMax = m_GPUFrequency;
        }
        else
        {
            PrintDX("Query disjoint failed!\n");
        }

        if (m_GPUFrequency > 0)
        {
            UINT64 beginCSTimeStamp;
            UINT64 endCSTimeStamp;
            UINT64 diffCSTimeStamp;
            while (m_pContext->GetData(m_pQueryBegin, &beginCSTimeStamp, sizeof(UINT64), 0) == S_FALSE)
            {
            }
            while (m_pContext->GetData(m_pQueryEnd, &endCSTimeStamp, sizeof(UINT64), 0) == S_FALSE)
            {
            }

            // get GPU count
            diffCSTimeStamp = endCSTimeStamp - beginCSTimeStamp;

            // Profile data available to store
            if ((diffCSTimeStamp > 0) && (m_totalnumBlocks))
            {
                float ElapseHz     = float(diffCSTimeStamp) / m_GPUFrequency;
                float ElapseHzToMs = ElapseHz * 1E3f;
                // time to process a single block (4x4) which is 16 texels in ms
                m_computeShaderElapsedMS  = ElapseHzToMs / m_totalnumBlocks;
                float ElapsedSeconds      = m_computeShaderElapsedMS / 1E3f;
                float ElapsedSecondsPerTx = ElapsedSeconds / 16;
                float TxPerSec            = 1 / ElapsedSecondsPerTx;
                // time to process a 1M texels in a second
                m_CmpMTxPerSec = TxPerSec / 1E6f;
            }
        }
    }
}

void CDirectX::ResetContext()
{
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    m_pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    ID3D11ShaderResourceView* nullSRV[3] = {nullptr, nullptr, nullptr};
    m_pContext->CSSetShaderResources(0, 3, nullSRV);

    ID3D11Buffer* nullBuffer[1] = {nullptr};
    m_pContext->CSSetConstantBuffers(0, 1, nullBuffer);
}

void CDirectX::RunComputeShader(ID3D11ComputeShader*       pComputeShader,
                                ID3D11ShaderResourceView** pShaderResourceViews,
                                UINT                       uNumSRVs,
                                ID3D11Buffer*              pCBCS,
                                ID3D11UnorderedAccessView* pUnorderedAccessView,
                                UINT                       X,
                                UINT                       Y,
                                UINT                       Z,
                                UINT                       numBlocks)
{
    // Force UAV to nullptr before setting SRV since we are swapping buffers
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    m_pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    m_pContext->CSSetShader(pComputeShader, nullptr, 0);
    m_pContext->CSSetShaderResources(0, uNumSRVs, pShaderResourceViews);
    m_pContext->CSSetUnorderedAccessViews(0, 1, &pUnorderedAccessView, nullptr);
    m_pContext->CSSetConstantBuffers(0, 1, &pCBCS);

    QueryDispatchBegin();
    m_pContext->Dispatch(X, Y, Z);
    QueryDispatchEnd(numBlocks);

    ResetContext();
}

//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
//--------------------------------------------------------------------------------------
ID3D11Buffer* CreateAndCopyToCPUBuf(ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer)
{
    ID3D11Buffer* cpubuf = nullptr;

    D3D11_BUFFER_DESC desc = {};
    pBuffer->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.BindFlags      = 0;
    desc.MiscFlags      = 0;
    if (SUCCEEDED(pDevice->CreateBuffer(&desc, nullptr, &cpubuf)))
    {
        pd3dImmediateContext->CopyResource(cpubuf, pBuffer);
    }

    return cpubuf;
}

//====================================== Framework Common Interfaces : Direct Compute  ==========================================

CDirectX::CDirectX(void* kerneloptions)
{
    m_initDeviceOk  = false;
    m_kernelOptions = (KernelOptions*)kerneloptions;
    Init();
}

CDirectX::~CDirectX()
{
    // Clear any old states
    if (m_pContext)
    {
        m_pContext->ClearState();
        m_pContext->Flush();
    }

    Cleanup();

#if defined(_DEBUG)
    // Final cleanup check on live objects
    if (m_pDebug != nullptr)
        m_pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
#endif
}

void CDirectX::GetErrorMessages()
{
}

void CDirectX::Init()
{
    m_programRun   = false;
    m_codecFormat  = CMP_FORMAT_Unknown;
    m_deviceName   = "";
    m_initDeviceOk = false;

    m_num_blocks             = 0;
    m_CmpMTxPerSec           = 0;
    m_computeShaderElapsedMS = 0.0f;
    m_activeEncoder          = 0;  // BC1

    m_pTexture2DSourceTexture = nullptr;
    m_pDevice                 = nullptr;
    m_pContext                = nullptr;
    m_pQueryDisjoint          = nullptr;
    m_pQueryBegin             = nullptr;
    m_pQueryEnd               = nullptr;
#if defined(_DEBUG)
    m_pDebug = nullptr;
#endif

    m_bc7_mode02  = true;  // uses 3 subsets
    m_bc7_mode137 = true;  // uses 2 subsets

    m_BC7_pTryMode456CS = nullptr;
    m_BC7_pTryMode137CS = nullptr;
    m_BC7_pTryMode02CS  = nullptr;

    m_BC6H_pTryModeG10CS  = nullptr;
    m_BC6H_pTryModeLE10CS = nullptr;

    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC1] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC2] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC3] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC4] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC5] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC6] = nullptr;
    m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC7] = nullptr;

    m_GPUFrequency    = 0;
    m_GPUFrequencyMin = 1E9f;
    m_GPUFrequencyMax = 0;
    m_getGPUPerfStats = true;
}

void CDirectX::SetComputeOptions(ComputeOptions* CLOptions)
{
}

float CDirectX::GetProcessElapsedTimeMS()
{
    return m_computeShaderElapsedMS;
}

float CDirectX::GetMTxPerSec()
{
    return m_CmpMTxPerSec;
}

int CDirectX::GetBlockSize()
{
    return m_num_blocks;
}

int CDirectX::GetMaxUCores()
{
    return m_maxUCores;
}

const char* CDirectX::GetDeviceName()
{
    return m_deviceName.c_str();
}

const char* CDirectX::GetVersion()
{
    return m_version.c_str();
}

void CDirectX::CleanupEncoders()
{
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC1]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC2]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC3]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC4]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC5]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC6]);
    SAFE_RELEASE(m_BCn_pEncodeBlockCS[ACTIVE_ENCODER_BC7]);

    // BC7 codec
    SAFE_RELEASE(m_BC7_pTryMode456CS);
    SAFE_RELEASE(m_BC7_pTryMode137CS);
    SAFE_RELEASE(m_BC7_pTryMode02CS);

    // BC6H
    SAFE_RELEASE(m_BC6H_pTryModeG10CS);
    SAFE_RELEASE(m_BC6H_pTryModeLE10CS);
}

void CDirectX::Cleanup()
{
    CleanupEncoders();

    // Compute
    SAFE_RELEASE(m_pTexture2DSourceTexture);
    SAFE_RELEASE(m_pContext);
    SAFE_RELEASE(m_pDevice);

    // Query Performance Monitors
    SAFE_RELEASE(m_pQueryDisjoint);
    SAFE_RELEASE(m_pQueryBegin);
    SAFE_RELEASE(m_pQueryEnd);
}

bool CDirectX::CreateDevice()
{
    //printf("CreateDevice\n");

    Init();

    HRESULT hr = S_OK;

    UINT uCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(_DEBUG)
    uCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL flOut = D3D_FEATURE_LEVEL_9_1;

    hr = D3D11CreateDevice(nullptr,                   // Use default graphics card to support printf
                           D3D_DRIVER_TYPE_HARDWARE,  // Try to create a hardware accelerated device
                           nullptr,                   // Do not use external software rasterizer module
                           uCreationFlags,            // Device creation flags
                           nullptr,                   // Try to get greatest feature level available
                           0,                         // # of elements in the previous array
                           D3D11_SDK_VERSION,         // SDK version
                           &m_pDevice,                // Device out
                           &flOut,                    // Actual feature level created
                           &m_pContext);              // Context out

    if (FAILED(hr))
    {
        // Failure on creating a hardware device, try to create a ref device
        SAFE_RELEASE(m_pDevice);
        SAFE_RELEASE(m_pContext);

        hr = D3D11CreateDevice(nullptr,                    // Use default graphics card
                               D3D_DRIVER_TYPE_REFERENCE,  // Try to create a hardware accelerated device
                               nullptr,                    // Do not use external software rasterizer module
                               uCreationFlags,             // Device creation flags
                               nullptr,                    // Try to get greatest feature level available
                               0,                          // # of elements in the previous array
                               D3D11_SDK_VERSION,          // SDK version
                               &m_pDevice,                 // Device out
                               &flOut,                     // Actual feature level created
                               &m_pContext);               // Context out
        if (FAILED(hr))
        {
            _com_error err(hr);
            LPCTSTR    errMsg = err.ErrorMessage();
            PrintDX("Error %s\n", errMsg);
            return false;
        }
    }
    else
    {
        if (FAILED(hr))
        {
            _com_error err(hr);
            LPCTSTR    errMsg = err.ErrorMessage();
            PrintDX("Error %s\n", errMsg);
            return false;
        }
    }

#if defined(_DEBUG)
    hr = m_pDevice->QueryInterface(IID_PPV_ARGS(&m_pDebug));
    if (FAILED(hr))
    {
        _com_error err(hr);
        LPCTSTR    errMsg = err.ErrorMessage();
        PrintDX("Error %s\n", errMsg);
        return false;
    }
#endif

    GetDeviceInfo();

    return true;
}

bool CDirectX::GetDeviceInfo()
{
    IDXGIDevice1* pDXGIDevice = NULL;
    HRESULT       hr          = m_pDevice->QueryInterface(&pDXGIDevice);

    IDXGIAdapter* pAdapter = NULL;
    pDXGIDevice->GetAdapter(&pAdapter);

    DXGI_ADAPTER_DESC desc;
    hr = pAdapter->GetDesc(&desc);

    WCHAR adapterDesc[128];
    swprintf(adapterDesc, 128, L"%s, DeviceID:0x%x", desc.Description, desc.DeviceId);
    wstring ws(adapterDesc);
    string  str(ws.begin(), ws.end());
    m_deviceName = str;
    m_maxUCores  = 0;
    m_version    = "";

    SAFE_RELEASE(pAdapter);
    return (hr == S_OK);
}

bool CDirectX::CheckCS4Suppot()
{
    //printf("CheckCS4Suppot\n");

    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
    m_pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
    if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
    {
        PrintDX("GPU does not support DirectCompute 4.x\n");
        Cleanup();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------
// Initialize the encoder(s)
//--------------------------------------------------------------------------------------
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

HRESULT CDirectX::CompileComputeShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _Outptr_ ID3DBlob** blob)
{
    if (!srcFile || !entryPoint || !blob)
        return E_INVALIDARG;

    if (!m_pDevice)
        return E_POINTER;

    *blob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif
    LPCSTR                 profile   = (m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";
    const D3D_SHADER_MACRO defines[] = {"ASPM_HLSL", "1", NULL, NULL};

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob  = nullptr;
    HRESULT   hr         = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, profile, flags, 0, &shaderBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            PrintDX((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }

        if (shaderBlob)
            shaderBlob->Release();

        return hr;
    }

    *blob = shaderBlob;

    return hr;
}

void CDirectX::csBlobCleanUp()
{
    //---------------
    // Clean up temps
    //---------------
    if (m_csBlob != nullptr)
        m_csBlob->Release();
    for (int i = 0; i < 3; i++)
    {
        if (m_csBlobEx[i] != nullptr)
            m_csBlobEx[i]->Release();
    }
}

HRESULT CDirectX::BuildBCnEncoder()
{
    //printf("BuildBCnEncoder\n");
    HRESULT hr = S_OK;
    bool    rebuild;
    FILE*   p_file_bin = NULL;
    errno_t fopen_result;

    // Compiled shaders are cached under the user's application data folder: the
    // install directory is read only for standard users.
    auto shaderCacheFile = [this](const char* suffix) { return CMP_GetShaderCacheFilePath(m_sourceShaderFile, suffix); };

    string compiledShaderFile = shaderCacheFile(".cmp");

    //-------------------------------
    // Load Compiled Compute Shaders
    //-------------------------------
    std::wstring stemp;
    LPCWSTR      sw;

    m_csBlob = nullptr;
    for (int i = 0; i < 3; i++)
    {
        if (m_csBlobEx[i])
            m_csBlobEx[i] = nullptr;
    }

    // Check build configuration of the shader, has it been modified since last use
    rebuild = cmp_recompile_shader(m_sourceShaderFile, compiledShaderFile);

    if (!rebuild)
    {
        fopen_result = fopen_s(&p_file_bin, compiledShaderFile.c_str(), "rb");
        if (fopen_result != 0)
            rebuild = true;
    }

    //------------------------------
    // compiled (.cmp) file exists
    //------------------------------
    if (!rebuild)
    {
        fclose(p_file_bin);
        // Load the .cmp binary
        stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
        sw    = stemp.c_str();
        hr    = D3DReadFileToBlob(sw, &m_csBlob);
        if (FAILED(hr))
        {
            PrintDX("Failed reading shader binary %08X\n", hr);
            return -1;
        }

        if (m_activeEncoder == ACTIVE_ENCODER_BC6)
        {
#ifdef USE_ALL_BC6_MODES
            compiledShaderFile = shaderCacheFile(".0.cmp");
            stemp              = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw                 = stemp.c_str();
            hr                 = D3DReadFileToBlob(sw, &m_csBlobEx[0]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                PrintDX("Failed reading shader binary %08X\n", hr);
                return -1;
            }
            compiledShaderFile = shaderCacheFile(".1.cmp");
            stemp              = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw                 = stemp.c_str();
            hr                 = D3DReadFileToBlob(sw, &m_csBlobEx[1]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                PrintDX("Failed reading shader binary %08X\n", hr);
                return -1;
            }
#endif
        }
#ifdef USE_ALL_BC7_MODES
        else if (m_activeEncoder == ACTIVE_ENCODER_BC7)
        {
            compiledShaderFile = shaderCacheFile(".0.cmp");
            stemp              = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw                 = stemp.c_str();
            hr                 = D3DReadFileToBlob(sw, &m_csBlobEx[0]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                PrintDX("Failed reading shader binary %08X\n", hr);
                return -1;
            }

            compiledShaderFile = shaderCacheFile(".1.cmp");
            stemp              = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw                 = stemp.c_str();
            hr                 = D3DReadFileToBlob(sw, &m_csBlobEx[1]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                PrintDX("Failed reading shader binary %08X\n", hr);
                return -1;
            }

            compiledShaderFile = shaderCacheFile(".2.cmp");
            stemp              = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw                 = stemp.c_str();
            hr                 = D3DReadFileToBlob(sw, &m_csBlobEx[2]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                PrintDX("Failed reading shader binary %08X\n", hr);
                return -1;
            }
        }
#endif
    }
    //-------------------------------------
    // Compile the codec for first time use
    //-------------------------------------
    else
    {
        stemp = std::wstring(m_sourceShaderFile.begin(), m_sourceShaderFile.end());
        sw    = stemp.c_str();
        hr    = CompileComputeShader(sw, "EncodeBlocks", &m_csBlob);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed compiling shader %08X\n", hr);
            return -1;
        }

        // Additional Shaders Needed for larger codecs
        if (m_activeEncoder == ACTIVE_ENCODER_BC6)
        {
#ifdef USE_ALL_BC6_MODES
            hr = CompileComputeShader(sw, "TryModeG10CS", &m_csBlobEx[0]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed compiling shader %08X\n", hr);
                return -1;
            }
            hr = CompileComputeShader(sw, "TryModeLE10CS", &m_csBlobEx[1]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed compiling shader %08X\n", hr);
                return -1;
            }
#endif
        }
#ifdef USE_ALL_BC7_MODES
        else if (m_activeEncoder == ACTIVE_ENCODER_BC7)
        {
            hr = CompileComputeShader(sw, "TryMode456CS", &m_csBlobEx[0]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed compiling shader %08X\n", hr);
                return -1;
            }
            hr = CompileComputeShader(sw, "TryMode137CS", &m_csBlobEx[1]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed compiling shader %08X\n", hr);
                return -1;
            }
            hr = CompileComputeShader(sw, "TryMode02CS", &m_csBlobEx[2]);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed compiling shader %08X\n", hr);
                return -1;
            }
        }
#endif
        // Save the compiled version for use on future runs
        stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
        sw    = stemp.c_str();
        hr    = D3DWriteBlobToFile(m_csBlob, sw, true);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to save compiled shader %08X\n", hr);
            return -1;
        }

        if (m_activeEncoder == ACTIVE_ENCODER_BC6)
        {
#ifdef USE_ALL_BC6_MODES
            compiledShaderFile = shaderCacheFile(".0.cmp");

            // Save the compiled version for use on future runs
            stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw    = stemp.c_str();
            hr    = D3DWriteBlobToFile(m_csBlobEx[0], sw, true);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed to save compiled shader %08X\n", hr);
                return -1;
            }

            compiledShaderFile = shaderCacheFile(".1.cmp");
            // Save the compiled version for use on future runs
            stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw    = stemp.c_str();
            hr    = D3DWriteBlobToFile(m_csBlobEx[1], sw, true);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed to save compiled shader %08X\n", hr);
                return -1;
            }
#endif
        }
#ifdef USE_ALL_BC7_MODES
        else if (m_activeEncoder == ACTIVE_ENCODER_BC7)
        {
            compiledShaderFile = shaderCacheFile(".0.cmp");

            // Save the compiled version for use on future runs
            stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw    = stemp.c_str();
            hr    = D3DWriteBlobToFile(m_csBlobEx[0], sw, true);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed to save compiled shader %08X\n", hr);
                return -1;
            }

            compiledShaderFile = shaderCacheFile(".1.cmp");
            // Save the compiled version for use on future runs
            stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw    = stemp.c_str();
            hr    = D3DWriteBlobToFile(m_csBlobEx[1], sw, true);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed to save compiled shader %08X\n", hr);
                return -1;
            }

            compiledShaderFile = shaderCacheFile(".2.cmp");
            // Save the compiled version for use on future runs
            stemp = std::wstring(compiledShaderFile.begin(), compiledShaderFile.end());
            sw    = stemp.c_str();
            hr    = D3DWriteBlobToFile(m_csBlobEx[2], sw, true);
            if (FAILED(hr))
            {
                csBlobCleanUp();
                Cleanup();
                PrintDX("Failed to save compiled shader %08X\n", hr);
                return -1;
            }
        }
#endif
    }

    //------------------------------------------------------------------
    // clean up prior encoders first and then Create the codecs for use
    //-------------------------------------------------------------------
    CleanupEncoders();
    hr = m_pDevice->CreateComputeShader(m_csBlob->GetBufferPointer(), m_csBlob->GetBufferSize(), nullptr, &m_BCn_pEncodeBlockCS[m_activeEncoder]);
    if (FAILED(hr))
    {
        csBlobCleanUp();
        Cleanup();
        PrintDX("Failed to create shader %08X\n", hr);
        return -1;
    }

    if (m_activeEncoder == ACTIVE_ENCODER_BC6)
    {
#ifdef USE_ALL_BC6_MODES
        hr = m_pDevice->CreateComputeShader(m_csBlobEx[0]->GetBufferPointer(), m_csBlobEx[0]->GetBufferSize(), nullptr, &m_BC6H_pTryModeG10CS);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to create shader %08X\n", hr);
            return -1;
        }

        hr = m_pDevice->CreateComputeShader(m_csBlobEx[1]->GetBufferPointer(), m_csBlobEx[1]->GetBufferSize(), nullptr, &m_BC6H_pTryModeLE10CS);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to create shader %08X\n", hr);
            return -1;
        }
#endif
    }
#ifdef USE_ALL_BC7_MODES
    else if (m_activeEncoder == ACTIVE_ENCODER_BC7)
    {
        hr = m_pDevice->CreateComputeShader(m_csBlobEx[0]->GetBufferPointer(), m_csBlobEx[0]->GetBufferSize(), nullptr, &m_BC7_pTryMode456CS);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to create shader %08X\n", hr);
            return -1;
        }

        hr = m_pDevice->CreateComputeShader(m_csBlobEx[1]->GetBufferPointer(), m_csBlobEx[1]->GetBufferSize(), nullptr, &m_BC7_pTryMode137CS);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to create shader %08X\n", hr);
            return -1;
        }

        hr = m_pDevice->CreateComputeShader(m_csBlobEx[2]->GetBufferPointer(), m_csBlobEx[2]->GetBufferSize(), nullptr, &m_BC7_pTryMode02CS);
        if (FAILED(hr))
        {
            csBlobCleanUp();
            Cleanup();
            PrintDX("Failed to create shader %08X\n", hr);
            return -1;
        }
    }
#endif

    //---------------
    // Clean up temps
    //---------------
    csBlobCleanUp();

    return hr;
}

HRESULT CDirectX::GPU_Encode(ID3D11Buffer** ppDstTextureAsBufOut, int miplevel)
{
    if (!ppDstTextureAsBufOut)
        return false;

    HRESULT hr = S_OK;

    //----------------------------------------------------------------------------------------------------------------
    // Encode the source texture to BCn and store the result in a buffer
    //-----------------------------------------------------------------------------------------------------------------
    {
        ID3D11Buffer*              pCBCS                 = nullptr;
        ID3D11Buffer*              pErrBestModeBuffer[2] = {nullptr, nullptr};
        ID3D11ShaderResourceView*  pInputsourceSRV       = nullptr;
        ID3D11UnorderedAccessView* pCompressedBlockUAV   = nullptr;
        ID3D11UnorderedAccessView* pErrBestModeUAV[2]    = {nullptr, nullptr};
        ID3D11ShaderResourceView*  pErrBestModeSRV[2]    = {nullptr, nullptr};
        // check for supported formats
        switch (m_fmtEncode)
        {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            break;
        default:
            return false;
            break;
        }

        D3D11_TEXTURE2D_DESC texSrcDesc;
        m_pTexture2DSourceTexture->GetDesc(&texSrcDesc);

        CGU_UINT32 NumOfBlocks = (texSrcDesc.Height * texSrcDesc.Width) / (4 * 4);

        // Init Query Performance Monitors
        if ((!m_initQueryOk) && m_getGPUPerfStats)
        {
            D3D11_QUERY_DESC m_queryDisjointDesc;
            D3D11_QUERY_DESC m_queryDesc;

            // destroy prior setting
            SAFE_RELEASE(m_pQueryDisjoint);
            SAFE_RELEASE(m_pQueryBegin);
            SAFE_RELEASE(m_pQueryEnd);

            m_queryDisjointDesc.Query     = D3D11_QUERY_TIMESTAMP_DISJOINT;
            m_queryDisjointDesc.MiscFlags = 0;
            hr                            = m_pDevice->CreateQuery(&m_queryDisjointDesc, &m_pQueryDisjoint);
            if (FAILED(hr))
            {
                _com_error err(hr);
                LPCTSTR    errMsg = err.ErrorMessage();
                PrintDX("Query Error %s\n", errMsg);
            }

            m_queryDesc.Query     = D3D11_QUERY_TIMESTAMP;
            m_queryDesc.MiscFlags = 0;
            hr                    = m_pDevice->CreateQuery(&m_queryDesc, &m_pQueryBegin);
            if (FAILED(hr))
            {
                _com_error err(hr);
                LPCTSTR    errMsg = err.ErrorMessage();
                PrintDX("Query Error %s\n", errMsg);
            }

            hr = m_pDevice->CreateQuery(&m_queryDesc, &m_pQueryEnd);
            if (FAILED(hr))
            {
                _com_error err(hr);
                LPCTSTR    errMsg = err.ErrorMessage();
                PrintDX("Query Error %s\n", errMsg);
            }
            m_initQueryOk = true;
        }

        //===================================================================================================================================================================
        // Create a Shader Resource View (SRV) for input texture
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels             = texSrcDesc.MipLevels;
            SRVDesc.Texture2D.MostDetailedMip       = 0;
            SRVDesc.Format                          = texSrcDesc.Format;
            V_GOTO(m_pDevice->CreateShaderResourceView(m_pTexture2DSourceTexture, &SRVDesc, &pInputsourceSRV));
        }

        //========================================================================================
        // Create output buffer with a size mathing the Width and Height of the source texture
        // For BC6H & BC7 Block Size = 128 bits (16 Bytes) per texel
        //========================================================================================
        D3D11_BUFFER_DESC sbOutDesc;
        {
            sbOutDesc.BindFlags      = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            sbOutDesc.CPUAccessFlags = 0;
            sbOutDesc.Usage          = D3D11_USAGE_DEFAULT;
            sbOutDesc.MiscFlags      = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

            switch (m_fmtEncode)
            {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC4_UNORM:
                sbOutDesc.StructureByteStride = sizeof(OutCompressedStruct64Bits);  // 8 Bytes
                sbOutDesc.ByteWidth           = NumOfBlocks * sizeof(OutCompressedStruct64Bits);
                break;
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
            case DXGI_FORMAT_BC7_UNORM:
            default:
                sbOutDesc.StructureByteStride = sizeof(OutCompressedStruct128Bits);  // 16 Bytes
                sbOutDesc.ByteWidth           = NumOfBlocks * sizeof(OutCompressedStruct128Bits);
                break;
            }

            V_GOTO(m_pDevice->CreateBuffer(&sbOutDesc, nullptr, ppDstTextureAsBufOut));
        }

        // Create UAV of the output resources
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
            UAVDesc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
            UAVDesc.Format                           = DXGI_FORMAT_UNKNOWN;
            UAVDesc.Buffer.FirstElement              = 0;
            UAVDesc.Buffer.NumElements               = NumOfBlocks;
#pragma warning(push)
#pragma warning(disable : 6387)
            V_GOTO(m_pDevice->CreateUnorderedAccessView(*ppDstTextureAsBufOut, &UAVDesc, &pCompressedBlockUAV));
#pragma warning(pop)
        }

        //===================================================================================================================================================================
        D3D11_BUFFER_DESC sbOutTempDesc;
        {
            sbOutTempDesc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
            sbOutTempDesc.CPUAccessFlags      = 0;
            sbOutTempDesc.Usage               = D3D11_USAGE_DEFAULT;
            sbOutTempDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            sbOutTempDesc.StructureByteStride = sizeof(SharedIOData);
            sbOutTempDesc.ByteWidth           = NumOfBlocks * sizeof(SharedIOData);

            V_GOTO(m_pDevice->CreateBuffer(&sbOutTempDesc, nullptr, &pErrBestModeBuffer[0]));
            V_GOTO(m_pDevice->CreateBuffer(&sbOutTempDesc, nullptr, &pErrBestModeBuffer[1]));
            _Analysis_assume_(pErrBestModeBuffer[0] != 0);
        }

        // Create UAV of the output resources
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
            UAVDesc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
            UAVDesc.Format                           = DXGI_FORMAT_UNKNOWN;
            UAVDesc.Buffer.FirstElement              = 0;
            UAVDesc.Buffer.NumElements               = NumOfBlocks;
#pragma warning(push)
#pragma warning(disable : 6387)
            V_GOTO(m_pDevice->CreateUnorderedAccessView(pErrBestModeBuffer[0], &UAVDesc, &pErrBestModeUAV[0]));
            V_GOTO(m_pDevice->CreateUnorderedAccessView(pErrBestModeBuffer[1], &UAVDesc, &pErrBestModeUAV[1]));
#pragma warning(pop)
        }

        // Create SRV of the pErrBestModeBuffer
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
            SRVDesc.Format                          = DXGI_FORMAT_UNKNOWN;
            SRVDesc.Buffer.FirstElement             = 0;
            SRVDesc.Buffer.NumElements              = NumOfBlocks;
#pragma warning(push)
#pragma warning(disable : 6387)
            V_GOTO(m_pDevice->CreateShaderResourceView(pErrBestModeBuffer[0], &SRVDesc, &pErrBestModeSRV[0]));
            V_GOTO(m_pDevice->CreateShaderResourceView(pErrBestModeBuffer[1], &SRVDesc, &pErrBestModeSRV[1]));
#pragma warning(pop)
        }

        //===================================================================================================================================================================
        int const    MAX_BLOCK_BATCH = 64;
        unsigned int start_block_id  = 0;

        m_num_blocks = NumOfBlocks;

        // BCn options to pass down to shaders
        // this should match cbuffer cbCS : register( b0 ) in the shader code
        struct ShaderOptions
        {
            unsigned int tex_width;         // param[0]
            unsigned int num_block_x;       // param[1]
            unsigned int format;            // param[2]
            unsigned int mode_id;           // param[3]
            unsigned int start_block_id;    // param[4]
            unsigned int num_total_blocks;  // param[5]
            float        alpha_weight;      // param[6]
            float        quality;           // param[7]
        } options;

        options.tex_width        = texSrcDesc.Width;
        options.num_block_x      = texSrcDesc.Width / BLOCK_SIZE_X;
        options.format           = m_fmtEncode;
        options.mode_id          = 0;
        options.start_block_id   = start_block_id;
        options.num_total_blocks = NumOfBlocks;
        options.alpha_weight     = m_fAlphaWeight;
        options.quality          = m_fquality;

        // Create constant buffer for BCn options
        {
            D3D11_BUFFER_DESC cbDesc;
            cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            cbDesc.MiscFlags      = 0;
            cbDesc.ByteWidth      = sizeof(ShaderOptions);
            V_GOTO(m_pDevice->CreateBuffer(&cbDesc, nullptr, &pCBCS));
        }

        //================================================
        // Run the BC1 to BC5, New BC7 Code Shaders
        //================================================
        if ((m_fmtEncode == DXGI_FORMAT_BC1_UNORM) || (m_fmtEncode == DXGI_FORMAT_BC2_UNORM) || (m_fmtEncode == DXGI_FORMAT_BC3_UNORM) ||
            (m_fmtEncode == DXGI_FORMAT_BC4_UNORM) || (m_fmtEncode == DXGI_FORMAT_BC5_UNORM))
        {
            ID3D11ShaderResourceView* pSRVs[] = {pInputsourceSRV, nullptr};
            m_pContext->CSSetShader(m_BCn_pEncodeBlockCS[m_activeEncoder], nullptr, 0);
            m_pContext->CSSetShaderResources(0, 1, pSRVs);
            m_pContext->CSSetUnorderedAccessViews(0, 1, &pCompressedBlockUAV, nullptr);

            ID3D11Buffer* ppBufferNULL[1] = {nullptr};

            UINT uThreadGroupCount;
            int  n;

            QueryProcessBegin(miplevel);
            while (NumOfBlocks > 0)
            {
                if (NumOfBlocks >= MAX_BLOCK_BATCH)
                {
                    n                 = MAX_BLOCK_BATCH;
                    uThreadGroupCount = 16;
                }
                else
                {
                    n                 = NumOfBlocks;
                    uThreadGroupCount = __max(n / 4, 1);
                    if ((uThreadGroupCount * 4) < NumOfBlocks)
                        uThreadGroupCount++;
                }

                // Set user options and processing info to pass down to GPU
                {
                    D3D11_MAPPED_SUBRESOURCE cbMapped;
                    m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                    options.start_block_id = start_block_id;
                    memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                    m_pContext->Unmap(pCBCS, 0);
                    m_pContext->CSSetConstantBuffers(0, 1, &pCBCS);
                }

                QueryDispatchBegin();
                m_pContext->Dispatch(uThreadGroupCount, 1, 1);
                QueryDispatchEnd(n);

                m_pContext->CSSetConstantBuffers(0, 1, ppBufferNULL);

                start_block_id += n;
                NumOfBlocks -= n;
            }  // while num_blocks
            QueryProcessEnd(miplevel);

        }  // BC
        else
        {
            //=======================
            // Run the BC6H Shaders
            //=======================
            if ((m_fmtEncode == DXGI_FORMAT_BC6H_UF16) || (m_fmtEncode == DXGI_FORMAT_BC6H_SF16))
            {
                unsigned int              n;
                UINT                      uThreadGroupCount4;
                UINT                      uThreadGroupCount2;
                D3D11_MAPPED_SUBRESOURCE  cbMapped;
                ID3D11ShaderResourceView* pSRVs[] = {pInputsourceSRV, nullptr};
                QueryProcessBegin(miplevel);
                while (NumOfBlocks > 0)
                {
                    if (NumOfBlocks >= MAX_BLOCK_BATCH)
                    {
                        n                  = MAX_BLOCK_BATCH;
                        uThreadGroupCount2 = 32;
                        uThreadGroupCount4 = 16;
                    }
                    else
                    {
                        n                  = NumOfBlocks;
                        uThreadGroupCount2 = __max(n / 2, 1);
                        if ((uThreadGroupCount2 * 2) < n)
                            uThreadGroupCount2++;
                        uThreadGroupCount4 = __max(n / 4, 1);
                        if ((uThreadGroupCount4 * 4) < n)
                            uThreadGroupCount4++;
                    }

                    m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                    options.mode_id        = 0;
                    options.start_block_id = start_block_id;
                    memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                    m_pContext->Unmap(pCBCS, 0);

#ifdef USE_ALL_BC6_MODES
                    RunComputeShader(m_BC6H_pTryModeG10CS, pSRVs, 2, pCBCS, pErrBestModeUAV[0], uThreadGroupCount4, 1, 1, n);

                    for (INT modeID = 0; modeID < 10; ++modeID)
                    {
                        {
                            m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                            options.mode_id        = modeID;
                            options.start_block_id = start_block_id;
                            memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                            m_pContext->Unmap(pCBCS, 0);
                        }

                        pSRVs[1] = pErrBestModeSRV[modeID & 1];
                        RunComputeShader(m_BC6H_pTryModeLE10CS, pSRVs, 2, pCBCS, pErrBestModeUAV[!(modeID & 1)], uThreadGroupCount2, 1, 1, n);
                    }
#endif
                    pSRVs[1] = pErrBestModeSRV[0];
                    RunComputeShader(m_BCn_pEncodeBlockCS[m_activeEncoder], pSRVs, 2, pCBCS, pCompressedBlockUAV, uThreadGroupCount2, 1, 1, n);

                    start_block_id += n;
                    NumOfBlocks -= n;
                }  // while num_blocks
                QueryProcessEnd(miplevel);
                ResetContext();
            }  // BC6H
#ifdef USE_ALL_BC7_MODES
            else
                //====================
                // Run the BC7 Shaders
                //====================
                if ((m_fmtEncode == DXGI_FORMAT_BC7_UNORM) || (m_fmtEncode == DXGI_FORMAT_BC7_UNORM_SRGB))
                {
                    int                      modes137[3] = {1, 3, 7};
                    int                      modes02[2]  = {0, 2};
                    D3D11_MAPPED_SUBRESOURCE cbMapped;
                    unsigned int             n;
                    UINT                     uThreadGroupCount4;
                    UINT                     uThreadGroupCount;

                    QueryProcessBegin(miplevel);

                    while (NumOfBlocks > 0)
                    {
                        if (NumOfBlocks >= MAX_BLOCK_BATCH)
                        {
                            n                  = MAX_BLOCK_BATCH;
                            uThreadGroupCount  = n;
                            uThreadGroupCount4 = 16;
                        }
                        else
                        {
                            n                  = NumOfBlocks;
                            uThreadGroupCount  = n;
                            uThreadGroupCount4 = __max(n / 4, 1);
                            if ((uThreadGroupCount4 * 4) < n)
                                uThreadGroupCount4++;
                        }

                        ID3D11ShaderResourceView* pSRVs[] = {pInputsourceSRV, nullptr};
                        {
                            m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                            options.mode_id        = 6;
                            options.start_block_id = start_block_id;
                            memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                            m_pContext->Unmap(pCBCS, 0);
                        }

                        RunComputeShader(m_BC7_pTryMode456CS, pSRVs, 2, pCBCS, pErrBestModeUAV[0], uThreadGroupCount4, 1, 1, n);

                        if (m_bc7_mode137)
                        {
                            for (int i = 0; i < 3; ++i)
                            {
                                m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                                options.mode_id        = modes137[i];
                                options.start_block_id = start_block_id;
                                memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                                m_pContext->Unmap(pCBCS, 0);

                                // Mode 1: err1 -> err2
                                // Mode 3: err2 -> err1
                                // Mode 7: err1 -> err2
                                pSRVs[1] = (i & 1) ? pErrBestModeSRV[1] : pErrBestModeSRV[0];
                                RunComputeShader(m_BC7_pTryMode137CS, pSRVs, 2, pCBCS, pErrBestModeUAV[!(i & 1)], uThreadGroupCount, 1, 1, n);
                            }
                        }

                        if (m_bc7_mode02)
                        {
                            for (int i = 0; i < 2; ++i)
                            {
                                m_pContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped);
                                options.mode_id        = modes02[i];
                                options.start_block_id = start_block_id;
                                memcpy(cbMapped.pData, &options, sizeof(ShaderOptions));
                                m_pContext->Unmap(pCBCS, 0);

                                // Mode 0: err2 -> err1
                                // Mode 2: err1 -> err2
                                pSRVs[1] = (i & 1) ? pErrBestModeSRV[0] : pErrBestModeSRV[1];
                                RunComputeShader(m_BC7_pTryMode02CS, pSRVs, 2, pCBCS, pErrBestModeUAV[i & 1], uThreadGroupCount, 1, 1, n);
                            }
                        }

                        pSRVs[1] = (m_bc7_mode02 || m_bc7_mode137) ? pErrBestModeSRV[1] : pErrBestModeSRV[0];
                        RunComputeShader(m_BCn_pEncodeBlockCS[m_activeEncoder], pSRVs, 2, pCBCS, pCompressedBlockUAV, uThreadGroupCount4, 1, 1, n);

                        start_block_id += n;
                        NumOfBlocks -= n;
                    }  // while num_blocks

                    QueryProcessEnd(miplevel);

                }  // BC7
#endif
        }
    quit:

        SAFE_RELEASE(pInputsourceSRV);
        SAFE_RELEASE(pCompressedBlockUAV);

        SAFE_RELEASE(pErrBestModeSRV[0]);
        SAFE_RELEASE(pErrBestModeSRV[1]);
        SAFE_RELEASE(pErrBestModeUAV[0]);
        SAFE_RELEASE(pErrBestModeUAV[1]);
        SAFE_RELEASE(pErrBestModeBuffer[0]);
        SAFE_RELEASE(pErrBestModeBuffer[1]);
        SAFE_RELEASE(pCBCS);
    }

    return (hr == S_OK ? true : false);
    // ------------------ END OF BCn Encoders
}

HRESULT CDirectX::Create2DTexture()
{
    //printf("Create2DTexture\n");

    HRESULT hr;

    unsigned int                              miscFlags = 0;
    std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[1]);
    if (!initData)
        return E_OUTOFMEMORY;

    initData[0].pSysMem = m_SrcTexture.pData;  // pixel data

    if (m_activeEncoder == ACTIVE_ENCODER_BC6)
        initData[0].SysMemPitch = m_SrcTexture.dwWidth * 8;
    else
        initData[0].SysMemPitch = m_SrcTexture.dwWidth * 4;

    initData[0].SysMemSlicePitch = m_SrcTexture.dwDataSize;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width                = m_SrcTexture.dwWidth;
    desc.Height               = m_SrcTexture.dwHeight;
    desc.MipLevels            = 1;
    desc.ArraySize            = 1;
    if (m_activeEncoder == ACTIVE_ENCODER_BC6)
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    else
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = miscFlags & ~static_cast<uint32_t>(D3D11_RESOURCE_MISC_TEXTURECUBE);

    hr = m_pDevice->CreateTexture2D(&desc, initData.get(), reinterpret_cast<ID3D11Texture2D**>(&m_pTexture2DSourceTexture));

    return (hr);
}

HRESULT CDirectX::GPU_Process()
{
    //printf("GPU_Process\n");

    bool isprocessed = true;

    D3D11_TEXTURE2D_DESC srcDesc;
    m_pTexture2DSourceTexture->GetDesc(&srcDesc);

    D3D11_TEXTURE2D_DESC desc = srcDesc;
    desc.MipLevels            = 1;
    desc.ArraySize            = 1;
    desc.MiscFlags            = 0;

    std::vector<ID3D11Buffer*> buffers;

    UINT srcW = srcDesc.Width, srcH = srcDesc.Height;
    UINT w, h;

    for (UINT item = 0; (item < srcDesc.ArraySize) && isprocessed; ++item)
    {
        w = desc.Width = srcW;
        h = desc.Height = srcH;

        if ((desc.Width % 4) != 0 || (desc.Height % 4) != 0)
            break;

        for (UINT level = 0; (level < srcDesc.MipLevels) && isprocessed; ++level)
        {
            ID3D11Texture2D* pMipLevel = nullptr;
            m_pDevice->CreateTexture2D(&desc, nullptr, &pMipLevel);

            for (UINT x = 0; x < desc.Width; x += w)
            {
                for (UINT y = 0; y < desc.Height; y += h)
                {
                    m_pContext->CopySubresourceRegion(pMipLevel, 0, x, y, 0, m_pTexture2DSourceTexture, item * srcDesc.MipLevels + level, nullptr);
                }
            }

            ID3D11Buffer* pBufferMipLevel = nullptr;
            if (GPU_Encode(&pBufferMipLevel, level))
            {
                buffers.push_back(pBufferMipLevel);
            }
            else
                isprocessed = false;

            SAFE_RELEASE(pMipLevel);

            desc.Width >>= 1;
            if (desc.Width < 4)
                desc.Width = 4;
            desc.Height >>= 1;
            if (desc.Height < 4)
                desc.Height = 4;
            w >>= 1;
            if (w < 1)
                w = 1;
            h >>= 1;
            if (h < 1)
                h = 1;
        }
    }

    // buffers contains the compressed data to save to disk
    if (isprocessed)
        GPU_CompressedBuffer(buffers);

    for (UINT i = 0; i < buffers.size(); ++i)
        SAFE_RELEASE(buffers[i]);

    return (isprocessed);
}

HRESULT CDirectX::GPU_CompressedBuffer(std::vector<ID3D11Buffer*>& subTextureAsBufs)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC desc;
    m_pTexture2DSourceTexture->GetDesc(&desc);

    if ((desc.ArraySize * desc.MipLevels) != (UINT)subTextureAsBufs.size())
        return E_INVALIDARG;

    UINT srcW = desc.Width, srcH = desc.Height;
    for (UINT item = 0; item < desc.ArraySize; ++item)
    {
        desc.Width  = srcW;
        desc.Height = srcH;
        for (UINT level = 0; level < desc.MipLevels; ++level)
        {
            ID3D11Buffer* pReadbackbuf = CreateAndCopyToCPUBuf(m_pDevice, m_pContext, subTextureAsBufs[item * desc.MipLevels + level]);
            if (!pReadbackbuf)
            {
                hr = E_OUTOFMEMORY;
                return hr;
            }

            D3D11_MAPPED_SUBRESOURCE mappedSrc;
#pragma warning(push)
#pragma warning(disable : 6387)
            m_pContext->Map(pReadbackbuf, 0, D3D11_MAP_READ, 0, &mappedSrc);
            memcpy(m_DstTexture.pData, mappedSrc.pData, m_DstTexture.dwDataSize);
            // printf("CMP %x %x %x %x %x %x %x %x\n",
            //        m_DstTexture.pData[0],
            //        m_DstTexture.pData[1],
            //        m_DstTexture.pData[2],
            //        m_DstTexture.pData[3],
            //        m_DstTexture.pData[4],
            //        m_DstTexture.pData[5],
            //        m_DstTexture.pData[6],
            //        m_DstTexture.pData[7]);
            m_pContext->Unmap(pReadbackbuf, 0);
#pragma warning(pop)

            SAFE_RELEASE(pReadbackbuf);

            desc.Width >>= 1;
            if (desc.Width < 4)
                desc.Width = 4;
            desc.Height >>= 1;
            if (desc.Height < 4)
                desc.Height = 4;
        }
    }

    return hr;
}

//-----------------------------------------------------------------
#ifdef USE_COMMON_PIPELINE_API
void CDirectX::CleanUpKernelAndIOBuffers()
{
}

void CDirectX::CleanUpProgramEncoder()
{
}

bool CDirectX::CreateIOBuffers()
{
    return true;
}

long CDirectX::file_size(FILE* p_file)
{
    // Get the size of the program.
    if (fseek(p_file, 0, SEEK_END) != 0)
        return 0;
    long program_size = ftell(p_file);
    fseek(p_file, 0, SEEK_SET);

    return program_size;
}

bool CDirectX::load_file()
{
    return true;
}

bool CDirectX::Create_Program_File()
{
    return true;
}

bool CDirectX::CreateProgramEncoder()
{
    // Create the program.
    // if (!Create_Program_File())
    // {
    //     return false;
    // }
    return true;
}

bool CDirectX::RunKernel()
{
    return true;
}

bool CDirectX::GetResults()
{
    return true;
}
#endif

//------------------------------------------------------------------
CMP_ERROR CDirectX::Compress(KernelOptions* KernelOptions, MipSet& srcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback = NULL)
{
    bool newFormat = false;
    if (m_codecFormat != destTexture.m_format)
    {
        m_codecFormat = destTexture.m_format;
        newFormat     = true;
    }

    m_sourceShaderFile = KernelOptions->srcfile;
    if (m_sourceShaderFile.length() == 0)
        return CMP_ERR_NOSHADER_CODE_DEFINED;

    // Get perf data only on top most miplevels
    m_getPerfStats = KernelOptions->getPerfStats && (destTexture.m_nIterations < 1);

    m_fAlphaWeight           = 1.0f;  // Need to add to kerneloptions!
    m_fquality               = KernelOptions->fquality;
    m_kernelOptions->data    = KernelOptions->data;
    m_kernelOptions->size    = KernelOptions->size;
    m_kernelOptions->format  = KernelOptions->format;
    m_kernelOptions->dataSVM = KernelOptions->dataSVM;
    m_SrcTexture             = srcTexture;
    m_DstTexture             = destTexture;

    bool ok = true;
    if (!m_initDeviceOk)
    {
        if (!CreateDevice())
            return CMP_ERROR::CMP_ERR_FAILED_HOST_SETUP;
        if (!CheckCS4Suppot())
            return CMP_ERROR::CMP_ERR_GPU_DOESNOT_SUPPORT_COMPUTE;
        m_initDeviceOk = true;
        m_initQueryOk  = false;
    }

    // Check for supported formats!
    switch (m_DstTexture.m_format)
    {
    case CMP_FORMAT_BC1:
        m_fmtEncode     = DXGI_FORMAT_BC1_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC1;
        break;
    case CMP_FORMAT_BC2:
        m_fmtEncode     = DXGI_FORMAT_BC2_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC2;
        break;
    case CMP_FORMAT_BC3:
        m_fmtEncode     = DXGI_FORMAT_BC3_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC3;
        break;
    case CMP_FORMAT_BC4:
        m_fmtEncode     = DXGI_FORMAT_BC4_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC4;
        break;
    case CMP_FORMAT_BC5:
        m_fmtEncode     = DXGI_FORMAT_BC5_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC5;
        break;
    case CMP_FORMAT_BC6H:
        m_fmtEncode     = DXGI_FORMAT_BC6H_UF16;
        m_activeEncoder = ACTIVE_ENCODER_BC6;
        break;
    case CMP_FORMAT_BC6H_SF:
        m_fmtEncode     = DXGI_FORMAT_BC6H_SF16;
        m_activeEncoder = ACTIVE_ENCODER_BC6;
        break;
    default:
    case CMP_FORMAT_BC7:
        m_fmtEncode     = DXGI_FORMAT_BC7_UNORM;
        m_activeEncoder = ACTIVE_ENCODER_BC7;
        break;
    }

#ifdef USE_COMMON_PIPELINE_API
    if (m_programRun)
    {
        CleanUpKernelAndIOBuffers();
        if (newFormat)
            CleanUpProgramEncoder();
        m_programRun = false;
    }
    if (newFormat)
    {
        if (ok && (CreateProgramEncoder() == false))
            ok = false;
    }
    if (ok && (CreateIOBuffers() == false))
        ok = false;
    if (ok && (RunKernel() == false))
        ok = false;
    if (ok && (GetResults() == false))
        ok = false;

    if (ok)
    {
        m_programRun = true;
    }
#endif

    if (FAILED(BuildBCnEncoder()))
    {
        Cleanup();
        return CMP_ERROR::CMP_ERR_UNABLE_TO_CREATE_ENCODER;
    }

    // Create input Texture to process
    SAFE_RELEASE(m_pTexture2DSourceTexture);
    if (FAILED(Create2DTexture()))
    {
        Cleanup();
        return CMP_ERROR::CMP_ERR_UNABLE_TO_CREATE_ENCODER;
    }

    if (FAILED(GPU_Process()))
    {
        Cleanup();
        return CMP_ERROR::CMP_ERR_UNABLE_TO_CREATE_ENCODER;
    }

    m_programRun = true;
    //printf("DXC Done\n");
    return CMP_ERROR::CMP_OK;
}
