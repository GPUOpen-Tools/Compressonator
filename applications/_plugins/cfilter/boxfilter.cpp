//=============================================================================
// Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2008, ATI Technologies Inc. All rights reserved.
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
//==============================================================================
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
#include "boxfilter.h"

// Windows Header Files:
#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "tc_pluginapi.h"
#include "tc_plugininternal.h"
#include "compressonator.h"
#include "texture.h"
#include "cmp_boxfilter.h"

#ifndef _WIN32
#include "textureio.h"
#endif

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_BoxFilter)
SET_PLUGIN_TYPE("FILTERS")
SET_PLUGIN_NAME("MIPMAP")
#else
void* make_Plugin_BoxFilter()
{
    return new Plugin_BoxFilter;
}
#endif

CMIPS* CFilterMips = NULL;

Plugin_BoxFilter::Plugin_BoxFilter()
{
}

Plugin_BoxFilter::~Plugin_BoxFilter()
{
    CFilterMips = NULL;
}

// Not used return error!
int Plugin_BoxFilter::TC_PluginSetSharedIO(void* SharedCMips)
{
    if (SharedCMips && CFilterMips == NULL)
    {
        CFilterMips     = reinterpret_cast<CMIPS*>(SharedCMips);
        PrintStatusLine = CFilterMips->PrintLine;
        return CMP_OK;
    }
    else
        CFilterMips = NULL;

    return CMP_ERR_GENERIC;
}

int Plugin_BoxFilter::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
#ifdef _WIN32
    pPluginVersion->guid = g_GUID;
#endif
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

//nMinSize : The size in pixels used to determine how many mip levels to generate. Once all dimensions are less than or equal to nMinSize your mipper should generate no more mip levels.
int Plugin_BoxFilter::TC_CFilter(MipSet* pMipSet, CMP_MipSet* pMipSetDst, CMP_CFilterParams* pCFilterParams)
{
    (pMipSetDst);
    assert(pMipSet);
    assert(pMipSet->m_nMipLevels);
    int result = CMP_OK;
    if (!CFilterMips)
        return CMP_ERR_PLUGIN_SHAREDIO_NOT_SET;

    if (pCFilterParams->fGammaCorrection < 0)
        return CMP_ERR_GAMMA_OUTOFRANGE;

    if (pCFilterParams->nFilterType == 0)
    {
        pMipSet->m_nMipLevels = 1;

        int nWidth  = pMipSet->m_nWidth;
        int nHeight = pMipSet->m_nHeight;

        while ((nWidth > pCFilterParams->nMinSize || nHeight > pCFilterParams->nMinSize))
        {
            nWidth               = (std::max)(nWidth >> 1, 1);
            nHeight              = (std::max)(nHeight >> 1, 1);
            int nCurMipLevel     = pMipSet->m_nMipLevels;
            int maxFacesOrSlices = (std::max)((pMipSet->m_TextureType == TT_VolumeTexture) ? (CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1) >> 1)
                                                                                           : CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1),
                                              1);
            for (int nFaceOrSlice = 0; nFaceOrSlice < maxFacesOrSlices; nFaceOrSlice++)
            {
                MipLevel* pThisMipLevel = CFilterMips->GetMipLevel(pMipSet, nCurMipLevel, nFaceOrSlice);
                if (!pThisMipLevel)
                    continue;
                assert(CFilterMips->GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice)->m_pbData);  //prev miplevel ok

                if (pThisMipLevel->m_pbData)
                {  // Space for mip level already allocated ?
                    if (pThisMipLevel->m_nWidth != nWidth || pThisMipLevel->m_nHeight != nHeight)
                    {
                        // Wrong size - release & reallocate
                        //CFilterMips->FreeMipLevelData(pThisMipLevel);
                        if (CFilterMips->AllocateMipLevelData(pThisMipLevel, nWidth, nHeight, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType) == NULL)
                        {
                            return PE_Unknown;
                        }
                    }
                }
                else if (CFilterMips->AllocateMipLevelData(pThisMipLevel, nWidth, nHeight, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType) == NULL)
                {
                    return PE_Unknown;
                }

                assert(pThisMipLevel->m_pbData);

                if (pMipSet->m_TextureType == TT_VolumeTexture && CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1) > 1)
                {
                    //prev miplevel had 2 or more slices, so avg together slices

                    MipLevel* prevMipLevels[] = {CFilterMips->GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice * 2),
                                                 CFilterMips->GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice * 2 + 1)};

                    GenerateMipmapLevel(pThisMipLevel, prevMipLevels, 2, pMipSet->m_format);
                }
                else
                {
                    MipLevel* prevMipLevel = CFilterMips->GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice);
                    GenerateMipmapLevel(pThisMipLevel, &prevMipLevel, 1, pMipSet->m_format);
                }
            }

            if (pMipSet->m_nMipLevels < MAX_MIPLEVEL_SUPPORTED)
                ++pMipSet->m_nMipLevels;
            else
                break;
            if (nWidth == 1 && nHeight == 1)
                break;
        }
    }
    else
    {
#ifdef _WIN32
        result = GenMipLevelsUsingDXTex(pMipSet, pCFilterParams);
#else
        result = CMP_ERR_UNSUPPORTED_DEST_FORMAT;
#endif
    }

    if (pCFilterParams->fGammaCorrection != 1.0f && pCFilterParams->fGammaCorrection != 0.0f)
        CMP_SetMipSetGamma(pMipSet, pCFilterParams->fGammaCorrection);

    return result;
}

//--------------------------------------------------------------------------------------------
// DirectX MipMap Filter
//--------------------------------------------------------------------------------------------
void Plugin_BoxFilter::Error(CMP_CHAR* pszCaption, CMP_UINT nErrorNo, CMP_CHAR* errMsg)
{
    // Add code to print message to caller
    // printf("Error [%x]: %s %s\n", nErrorNo, pszCaption,errMsg);
    if (CFilterMips)
    {
        CFilterMips->PrintError("Error [%x]: %s %s", nErrorNo, pszCaption, errMsg);
    }
}

#ifdef _WIN32

#define HANDLE_ERROR_BREAK(err, msg)                            \
    {                                                           \
        Plugin_BoxFilter::Error("D3DXFilter Plugin", err, msg); \
        break;                                                  \
    }

void PrintDX(const char* Format, ...)
{
    // define a pointer to save argument list
    va_list args;
    char    buff[1024];
    // process the arguments into our debug buffer
    va_start(args, Format);
    vsprintf_s(buff, Format, args);
    va_end(args);

    if (CFilterMips)
    {
        CFilterMips->Print(buff);
    }
    else
    {
        printf(buff);
    }
}

#ifdef _WIN32
DWORD Plugin_BoxFilter::CalcMipLevels(MipSet* pMipSet, CMP_CFilterParams* pD3DXFilterParams)
{
    DWORD dwMipLevels = 1;
    int   nWidth      = pMipSet->m_nWidth;
    int   nHeight     = pMipSet->m_nHeight;

    while (nWidth > pD3DXFilterParams->nMinSize || nHeight > pD3DXFilterParams->nMinSize)
    {
        nWidth  = max(nWidth >> 1, 1);
        nHeight = max(nHeight >> 1, 1);
        dwMipLevels++;
        if ((nWidth <= pD3DXFilterParams->nMinSize) && (nHeight <= pD3DXFilterParams->nMinSize))
            break;
        if (dwMipLevels == pMipSet->m_nMaxMipLevels)
            break;
    }

    return dwMipLevels;
}

int Plugin_BoxFilter::GenMipLevelsUsingDXTex(MipSet* pMipSet, CMP_CFilterParams* pD3DXFilterParams)
{
    CMP_ERROR retVal = CMP_OK;
    HWND      hWnd   = NULL;  //
    HRESULT   hRes;
    DWORD     dwLevel;

    DXGI_FORMAT d3dformat;
    bool        extra_mirror_filtering = false;
    switch (pMipSet->m_ChannelFormat)
    {
    case CF_Float32:
        d3dformat = DXGI_FORMAT_R32G32B32A32_FLOAT;
        break;
    case CF_Float16:
        d3dformat = DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case CF_16bit:
        d3dformat = DXGI_FORMAT_R16G16B16A16_UNORM;
        break;
    case CF_8bit:
        d3dformat = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    default:
        if (CFilterMips)
            CFilterMips->PrintError("Feature is not supported for the images channel type!");
        return PE_Unknown;
        break;
    }

    DirectX::TEX_FILTER_FLAGS filter;
    switch (pD3DXFilterParams->dwMipFilterOptions & 0xF)
    {
    case CMP_D3DX_FILTER_POINT:
        filter = DirectX::TEX_FILTER_FLAGS::TEX_FILTER_POINT;
        break;
    case CMP_D3DX_FILTER_LINEAR:
        filter                 = DirectX::TEX_FILTER_FLAGS::TEX_FILTER_LINEAR;
        extra_mirror_filtering = true;
        break;
    case CMP_D3DX_FILTER_TRIANGLE:
        filter                 = DirectX::TEX_FILTER_FLAGS::TEX_FILTER_TRIANGLE;
        extra_mirror_filtering = true;
        break;
    default:  // BOX
        filter = DirectX::TEX_FILTER_FLAGS::TEX_FILTER_BOX;
        break;
    }

    if (pD3DXFilterParams->dwMipFilterOptions & CMP_D3DX_FILTER_DITHER)
        filter |= DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DITHER;

    if (extra_mirror_filtering)
    {
        if (pD3DXFilterParams->dwMipFilterOptions & CMP_D3DX_FILTER_MIRROR)
            filter |= DirectX::TEX_FILTER_FLAGS::TEX_FILTER_MIRROR;
    }

    DWORD dwMipLevels = CalcMipLevels(pMipSet, pD3DXFilterParams);

    // Create ScratchImage from loaded image
    DirectX::Image inputPixels = {(size_t)pMipSet->m_nWidth,
                                  (size_t)pMipSet->m_nHeight,
                                  d3dformat,
                                  1,  // rowPitch
                                  1,  // slicePitch
                                  CFilterMips->GetMipLevel(pMipSet, 0)->m_pbData};

    DirectX::ScratchImage mipMaps;
    DirectX::ScratchImage scratchImage;
    DirectX::Rect         imputRec     = {0, 0, (size_t)pMipSet->m_nWidth, (size_t)pMipSet->m_nHeight};
    DirectX::Image        outputPixels = {};

    do
    {
        if (DirectX::ComputePitch(d3dformat, pMipSet->m_nWidth, pMipSet->m_nHeight, inputPixels.rowPitch, inputPixels.slicePitch, DirectX::CP_FLAGS_NONE) !=
            S_OK)
            HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed ComputePitch");

        if (scratchImage.Initialize2D(d3dformat, pMipSet->m_nWidth, pMipSet->m_nHeight, dwMipLevels, 1) != S_OK)
            HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed Initialize2D");

        outputPixels = *scratchImage.GetImage(0, 0, 0);
        if (DirectX::CopyRectangle(inputPixels, imputRec, outputPixels, DirectX::TEX_FILTER_DEFAULT, 0, 0) != S_OK)
            HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed CopyRectangle");

        hRes = DirectX::GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), scratchImage.GetMetadata(), filter, 0, mipMaps);
        if (hRes != S_OK)
            HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed GenerateMipMaps");

        for (dwLevel = 0; dwLevel < dwMipLevels; dwLevel++)
        {
            DirectX::Image mipPixel = *mipMaps.GetImage(dwLevel, 0, 0);

            // Space for mip level already allocated
            if (CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_pbData)
            {
                if (CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_nWidth != (int)mipPixel.width ||
                    CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_nHeight != (int)mipPixel.height)
                {
                    // Wrong size - release & reallocate
                    CFilterMips->FreeMipLevelData(CFilterMips->GetMipLevel(pMipSet, dwLevel));
                    if (CFilterMips->AllocateMipLevelData(CFilterMips->GetMipLevel(pMipSet, dwLevel),
                                                          mipPixel.width,
                                                          mipPixel.height,
                                                          pMipSet->m_ChannelFormat,
                                                          pMipSet->m_TextureDataType) == NULL)
                        HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, "Failed Existing AllocateMipLevelData");
                }
            }
            else if (CFilterMips->AllocateMipLevelData(
                         CFilterMips->GetMipLevel(pMipSet, dwLevel), mipPixel.width, mipPixel.height, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType) ==
                     NULL)
                HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed New AllocateMipLevelData");

            if (CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_dwLinearSize == mipPixel.slicePitch)
                memcpy(CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_pbData, mipPixel.pixels, CFilterMips->GetMipLevel(pMipSet, dwLevel)->m_dwLinearSize);
            else
                HANDLE_ERROR_BREAK(retVal = CMP_ERR_GENERIC, " Failed GetMipLevel");
        }

        // Increment nMipLevel to point to next (empty) miplevel
        while (++dwLevel < dwMipLevels)
            CFilterMips->FreeMipLevelData(CFilterMips->GetMipLevel(pMipSet, dwLevel));

        pMipSet->m_nMipLevels = dwMipLevels;

    } while (0);

    scratchImage.Release();
    mipMaps.Release();
    return retVal;
}
#endif

#endif
