//=====================================================================
// Copyright 2016-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
// Copyright 2008 (c), ATI Technologies Inc. All rights reserved.
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

// DDS_Helpers.cpp : Defines the entry point for the DLL application.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits>

#include "dds.h"
#include "dds_file.h"
#include "dds_helpers.h"
#include "tc_pluginapi.h"
#include "version.h"

extern int CMP_MaxFacesOrSlices(const MipSet* pMipSet, int nMipLevel);
typedef TC_PluginError(PreLoopFunction)(FILE*& pFile, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra);
typedef TC_PluginError(
    LoopFunction)(FILE*& pFile, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight);
typedef TC_PluginError(PostLoopFunction)(FILE*& pFile, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra);
TC_PluginError GenericLoadFunction(FILE*&           pFile,
                                   DDSD2*&          pDDSD,
                                   MipSet*&         pMipSet,
                                   void*&           extra,
                                   ChannelFormat    channelFormat,
                                   TextureDataType  textureDataType,
                                   PreLoopFunction  fnPreLoop,
                                   LoopFunction     fnLoop,
                                   PostLoopFunction fnPostLoop);

#ifndef _WIN32
#define _UI32_MAX std::numeric_limits<uint32_t>::max()
#define _UI16_MAX std::numeric_limits<uint16_t>::max()
#endif

bool IsD3D10Format(const MipSet* pMipSet)
{
    assert(pMipSet);

    if (!pMipSet)
        return false;

    if (pMipSet->m_dwFourCC == CMP_FOURCC_DX10)
        return true;
    else
        return false;
}

void DetermineTextureType(const DDSD2* pDDSD, MipSet* pMipSet)
{
    if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP)
    {
        pMipSet->m_nDepth       = 0;
        pMipSet->m_TextureType  = TT_CubeMap;
        pMipSet->m_CubeFaceMask = MS_CF_None;

        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_PositiveX;
        }
        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_PositiveY;
        }
        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_PositiveZ;
        }
        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_NegativeX;
        }
        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_NegativeY;
        }
        if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ)
        {
            pMipSet->m_nDepth++;
            pMipSet->m_CubeFaceMask |= MS_CF_NegativeZ;
        }
    }
    else if (pDDSD->ddsCaps.dwCaps2 & DDSCAPS2_VOLUME && pDDSD->dwFlags & DDSD_DEPTH)
    {
        pMipSet->m_nDepth      = pDDSD->dwDepth;
        pMipSet->m_TextureType = TT_VolumeTexture;
    }
    else
    {
        pMipSet->m_nDepth      = 1;
        pMipSet->m_TextureType = TT_2D;
    }
}

// Runs fnLoop over every face/slice/mip level. Ownership of 'extra' stays with the post-loop.
static TC_PluginError GenericLoadLoops(FILE*& pFile, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra, LoopFunction fnLoop)
{
    CMP_DWORD      dwWidth, dwHeight;
    TC_PluginError err;

    if (pMipSet->m_dwFourCC)
        return fnLoop(pFile, pDDSD, pMipSet, extra, 0, 0, pDDSD->dwWidth, pDDSD->dwHeight);

    //pMipSet now allocated
    if (pMipSet->m_TextureType == TT_2D || pMipSet->m_TextureType == TT_CubeMap)
    {
        for (int nFace = 0; nFace < pMipSet->m_nDepth; nFace++)
        {
            dwWidth  = pDDSD->dwWidth;
            dwHeight = pDDSD->dwHeight;
            for (int nMipLevel = 0; nMipLevel < pMipSet->m_nMipLevels; nMipLevel++)
            {
                err = fnLoop(pFile, pDDSD, pMipSet, extra, nMipLevel, nFace, dwWidth, dwHeight);
                if (err != PE_OK)
                    return err;
                dwWidth  = (dwWidth > 1) ? (dwWidth >> 1) : 1;
                dwHeight = (dwHeight > 1) ? (dwHeight >> 1) : 1;
            }
        }
        return PE_OK;
    }

    if (pMipSet->m_TextureType == TT_VolumeTexture)
    {
        dwWidth  = pDDSD->dwWidth;
        dwHeight = pDDSD->dwHeight;
        for (int nMipLevel = 0; nMipLevel < pMipSet->m_nMipLevels; nMipLevel++)
        {
            int nMaxSlices = CMP_MaxFacesOrSlices(pMipSet, nMipLevel);
            for (int nSlice = 0; nSlice < nMaxSlices; nSlice++)
            {
                err = fnLoop(pFile, pDDSD, pMipSet, extra, nMipLevel, nSlice, dwWidth, dwHeight);
                if (err != PE_OK)
                    return err;
            }
            dwWidth  = dwWidth > 1 ? dwWidth >> 1 : 1;
            dwHeight = dwHeight > 1 ? dwHeight >> 1 : 1;
        }
        return PE_OK;
    }

    return PE_Unknown;
}

TC_PluginError GenericLoadFunction(FILE*&           pFile,
                                   DDSD2*&          pDDSD,
                                   MipSet*&         pMipSet,
                                   void*&           extra,
                                   ChannelFormat    channelFormat,
                                   TextureDataType  textureDataType,
                                   PreLoopFunction  fnPreLoop,
                                   LoopFunction     fnLoop,
                                   PostLoopFunction fnPostLoop)
{
    TC_PluginError err;

    DetermineTextureType(pDDSD, pMipSet);

    // 'extra' may already hold a caller-allocated context, so the post-loop owns it from the
    // first statement onwards and must run on every exit path below.
    if (!DDS_CMips->AllocateMipSet(pMipSet, channelFormat, textureDataType, pMipSet->m_TextureType, pDDSD->dwWidth, pDDSD->dwHeight, pMipSet->m_nDepth))
    {
        fnPostLoop(pFile, pDDSD, pMipSet, extra);
        return PE_Unknown;
    }

    pMipSet->m_nMipLevels = pDDSD->dwMipMapCount;

    if (pMipSet->m_nMipLevels < 1)
        pMipSet->m_nMipLevels = 1;
    else if (pMipSet->m_nMipLevels > pMipSet->m_nMaxMipLevels)
        pMipSet->m_nMipLevels = pMipSet->m_nMaxMipLevels;

    err = fnPreLoop(pFile, pDDSD, pMipSet, extra);
    if (err != PE_OK)
    {
        fnPostLoop(pFile, pDDSD, pMipSet, extra);
        return err;
    }

    err = GenericLoadLoops(pFile, pDDSD, pMipSet, extra, fnLoop);

    TC_PluginError postErr = fnPostLoop(pFile, pDDSD, pMipSet, extra);

    return (err != PE_OK) ? err : postErr;
}

TC_PluginError PreLoopDefault(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError LoopDefault(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);

    ChannelFormat channelFormat = *reinterpret_cast<ChannelFormat*>(extra);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, channelFormat, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopDefault(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopFourCC(FILE*& pFile, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra)
{
    if (pDDSD->ddpfPixelFormat.dwFourCC == CMP_FOURCC_DXT1 && !(pDDSD->ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS))
        pMipSet->m_TextureDataType = TDT_XRGB;
    else
        pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC = pDDSD->ddpfPixelFormat.dwFourCC;
    if (pDDSD->ddpfPixelFormat.dwPrivateFormatBitCount > 8)
        pMipSet->m_dwFourCC2 = pDDSD->ddpfPixelFormat.dwPrivateFormatBitCount;

    // Get Data Size
    long nCurrPos = ftell(pFile);
    fseek(pFile, 0, SEEK_END);
    long nSize = ftell(pFile) - nCurrPos;
    fseek(pFile, nCurrPos, SEEK_SET);

    CMP_DWORD dwWidth;
    CMP_DWORD dwHeight;
    CMP_DWORD dwDepth;
    CMP_DWORD dwPixels = 0;
    switch (pMipSet->m_TextureType)
    {
    case TT_2D:
    case TT_CubeMap:
        dwWidth  = pMipSet->m_nWidth;
        dwHeight = pMipSet->m_nHeight;
        for (int i = 0; i < pMipSet->m_nMipLevels; i++)
        {
            dwPixels += dwWidth * dwHeight * pMipSet->m_nDepth;
            dwWidth  = dwWidth > 1 ? dwWidth >> 1 : 1;
            dwHeight = dwHeight > 1 ? dwHeight >> 1 : 1;
        }
        break;
    case TT_VolumeTexture:
        dwWidth  = pMipSet->m_nWidth;
        dwHeight = pMipSet->m_nHeight;
        dwDepth  = pMipSet->m_nDepth;
        for (int i = 0; i < pMipSet->m_nMipLevels; i++)
        {
            dwPixels += dwWidth * dwHeight * dwDepth;
            dwWidth  = dwWidth > 1 ? dwWidth >> 1 : 1;
            dwHeight = dwHeight > 1 ? dwHeight >> 1 : 1;
            dwDepth  = dwDepth > 1 ? dwDepth >> 1 : 1;
        }
        break;
    default:
        assert(0);
        fclose(pFile);
        return PE_Unknown;
    }
    //make a DWORD, then cast to void*
    CMP_DWORD block = ((nSize * 8) / dwPixels);
    extra           = (void*)reinterpret_cast<CMP_DWORD_PTR>(&block);

    return PE_OK;
}

TC_PluginError LoopFourCC(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& /*extra*/, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);

    if (!pMipLevel)
    {
        return PE_Unknown;
    }

    // Get Data Size
    // We need to read everything that we can as we don't know how big each mip-level is
    long nCurrPos = ftell(pFile);
    fseek(pFile, 0, SEEK_END);
    long nSize = ftell(pFile) - nCurrPos;
    fseek(pFile, nCurrPos, SEEK_SET);

    if (!DDS_CMips->AllocateCompressedMipLevelData(pMipLevel, dwWidth, dwHeight, nSize))
    {
        return PE_Unknown;
    }

    //read in the data....
    if (fread(pMipLevel->m_pbData, nSize, 1, pFile) != 1)
    {
        //Error(PLUGIN_NAME, EL_Error, IDS_ERROR_FILE_OPEN, g_pszFilename);
        return PE_Unknown;
    }
    return PE_OK;
}

TC_PluginError PostLoopFourCC(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopRGB565(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra)
{
    extra                = nullptr;
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 2-bytes-per-pixel buffer size would wrap CMP_DWORD
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > (dwMax / 2) / pDDSD->dwWidth)
        return PE_Unknown;

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD dwTempSize = pDDSD->dwWidth * pDDSD->dwHeight * 2;
    extra                = malloc(dwTempSize);
    return extra ? PE_OK : PE_Unknown;
}

TC_PluginError LoopRGB565(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    // 'extra' is owned by PostLoopRGB565, which runs even when this loop fails: do not free it here.
    // Allocate the permanent buffer and unpack the bitmap data into it
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    if (!pMipLevel || !DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, TDT_XRGB))
    {
        return PE_Unknown;
    }

    if (fread(extra, pMipLevel->m_dwLinearSize / 2, 1, pFile) != 1)
    {
        return PE_Unknown;
    }

    CMP_BYTE* pData    = pMipLevel->m_pbData;
    CMP_WORD* pTempPtr = (CMP_WORD*)extra;
    CMP_WORD* pEnd     = (CMP_WORD*)extra + (pMipLevel->m_dwLinearSize / 4);
    while (pTempPtr < pEnd)
    {
        *pData++ = static_cast<CMP_BYTE>((*pTempPtr & 0x001f) << 3);
        *pData++ = static_cast<CMP_BYTE>((*pTempPtr & 0x07e0) >> 3);
        *pData++ = static_cast<CMP_BYTE>((*pTempPtr & 0xf800) >> 8);
        *pData++ = 0xff;
        pTempPtr++;
    }

    return PE_OK;
}

TC_PluginError PostLoopRGB565(FILE*&, DDSD2*&, MipSet*&, void*& extra)
{
    free(extra);
    extra = nullptr;
    return PE_OK;
}

TC_PluginError PreLoopRGB888(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*& extra)
{
    extra                = nullptr;
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 3-bytes-per-pixel buffer size would wrap CMP_DWORD
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > (dwMax / 3) / pDDSD->dwWidth)
        return PE_Unknown;

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD dwTempSize = pDDSD->dwWidth * pDDSD->dwHeight * 3;
    extra                = malloc(dwTempSize);
    return extra ? PE_OK : PE_Unknown;
}

TC_PluginError LoopRGB888(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    // 'extra' is owned by PostLoopRGB888, which runs even when this loop fails: do not free it here.
    if (fread(extra, dwWidth * dwHeight * 3, 1, pFile) != 1)
    {
        return PE_Unknown;
    }
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);

    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!pMipLevel)
    {
        return PE_Unknown;
    }

    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, TDT_XRGB))
    {
        return PE_Unknown;
    }

    CMP_BYTE* pData    = pMipLevel->m_pbData;
    CMP_BYTE* pTempPtr = (CMP_BYTE*)extra;
    CMP_BYTE* pEnd     = pData + pMipLevel->m_dwLinearSize;
    while (pData < pEnd)
    {
        *pData++ = *pTempPtr++;
        *pData++ = *pTempPtr++;
        *pData++ = *pTempPtr++;
        *pData++ = 0xff;
    }

    return PE_OK;
}

TC_PluginError PostLoopRGB888(FILE*&, DDSD2*&, MipSet*&, void*& extra)
{
    free(extra);
    extra = nullptr;
    return PE_OK;
}

TC_PluginError PreLoopRGB8888(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopRGB8888(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);

    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }
    ARGB8888Struct* pARGB8888Struct = reinterpret_cast<ARGB8888Struct*>(extra);
    if (!(pARGB8888Struct->nFlags & EF_UseBitMasks))
    {
        //not using bitmasks
        if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        {
            return PE_Unknown;
        }
    }
    else
    {
        //using bitmasks
        if (fread(pARGB8888Struct->pMemory, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        {
            return PE_Unknown;
        }
        CMP_BYTE   alpha;
        CMP_BYTE*  pData    = pMipLevel->m_pbData;
        CMP_DWORD* pTempPtr = (CMP_DWORD*)pARGB8888Struct->pMemory;
        CMP_DWORD* pEnd     = (CMP_DWORD*)pARGB8888Struct->pMemory + (pMipLevel->m_dwLinearSize / 4);
        while (pTempPtr < pEnd)
        {
            *pData++ = static_cast<CMP_BYTE>((*pTempPtr & pARGB8888Struct->nRMask) >> pARGB8888Struct->nRShift);
            *pData++ = static_cast<CMP_BYTE>((*pTempPtr & pARGB8888Struct->nGMask) >> pARGB8888Struct->nGShift);
            *pData++ = static_cast<CMP_BYTE>((*pTempPtr & pARGB8888Struct->nBMask) >> pARGB8888Struct->nBShift);
            //take alpha whether or if its not used set it to a default 255
            alpha    = static_cast<CMP_BYTE>((*pTempPtr & 0xFF000000) >> 24);
            *pData++ = (pMipSet->m_TextureDataType == TDT_ARGB ? alpha : 255);
            pTempPtr++;
        }
    }
    return PE_OK;
}

TC_PluginError LoopRGB8888_S(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*& extra, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);

    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }
    ARGB8888Struct* pARGB8888Struct = reinterpret_cast<ARGB8888Struct*>(extra);
    if (!(pARGB8888Struct->nFlags & EF_UseBitMasks))
    {
        //not using bitmasks
        if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        {
            return PE_Unknown;
        }
    }
    else
    {
        //using bitmasks
        if (fread(pARGB8888Struct->pMemory, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        {
            return PE_Unknown;
        }
        CMP_SBYTE* pData    = pMipLevel->m_psbData;
        CMP_DWORD* pTempPtr = (CMP_DWORD*)pARGB8888Struct->pMemory;
        CMP_DWORD* pEnd     = (CMP_DWORD*)pARGB8888Struct->pMemory + (pMipLevel->m_dwLinearSize / 4);
        CMP_SBYTE  R, G, B, A;
        while (pTempPtr < pEnd)
        {
            R        = static_cast<CMP_SBYTE>((*pTempPtr & pARGB8888Struct->nRMask) >> pARGB8888Struct->nRShift);
            G        = static_cast<CMP_SBYTE>((*pTempPtr & pARGB8888Struct->nGMask) >> pARGB8888Struct->nGShift);
            B        = static_cast<CMP_SBYTE>((*pTempPtr & pARGB8888Struct->nBMask) >> pARGB8888Struct->nBShift);
            A        = static_cast<CMP_SBYTE>((*pTempPtr & 0xFF000000) >> 24);
            A        = (pMipSet->m_TextureDataType == TDT_ARGB ? A : 127);
            *pData++ = R;
            *pData++ = G;
            *pData++ = B;
            *pData++ = A;
            // printf("[%d,%d,%d,%d]\n",R,G,B,A);
            pTempPtr++;
        }
    }
    return PE_OK;
}

TC_PluginError PostLoopRGB8888(FILE*&, DDSD2*&, MipSet*&, void*& extra)
{
    if (extra)
    {
        ARGB8888Struct* pStruct = reinterpret_cast<ARGB8888Struct*>(extra);
        free(pStruct->pMemory);  // free temporary pixel buffer before releasing struct
        pStruct->pMemory = nullptr;
        free(pStruct);
        extra = nullptr;
    }
    return PE_OK;
}

TC_PluginError PreLoopABGR32F(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopABGR32F(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float32, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopABGR32F(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError LoopGR32F(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float32, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 2;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    float* pSrc  = (float*)pTempData;
    float* pEnd  = (float*)(pTempData + dwSize);
    float* pDest = pMipLevel->m_pfData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = *pSrc++;
        *pDest++ = 0.0f;
        *pDest++ = 1.0f;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR32F(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float32, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 4;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    float* pSrc  = (float*)pTempData;
    float* pEnd  = (float*)(pTempData + dwSize);
    float* pDest = pMipLevel->m_pfData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = 0.0;
        *pDest++ = 0.0;
        *pDest++ = 1.0f;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR16F(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float16, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 4;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    size_t dwBytesRead = fread(pTempData, 1, dwSize, pFile);
    if (dwBytesRead != dwSize)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_WORD* pSrc  = (CMP_WORD*)pTempData;
    CMP_WORD* pEnd  = (CMP_WORD*)(pTempData + dwSize);
    CMP_WORD* pDest = pMipLevel->m_pwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 0;
        *pDest++ = 0;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError PreLoopABGR16F(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopABGR16F(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float16, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopABGR16F(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopG8(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = CMP_FOURCC_G8;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 1-bytes-per-pixel buffer size would wrap CMP_DWORD. LoopG8 sizes its allocation as dwWidth * dwHeight. These loaders report CF_Compressed,
    // so the mip-level size check in CMP_ProcessTexture is skipped and a wrapped size reaches the encoder.
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > dwMax / pDDSD->dwWidth)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError LoopG8(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateCompressedMipLevelData(pMipLevel, dwWidth, dwHeight, dwWidth * dwHeight))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopG8(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopAG8(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = CMP_FOURCC_AG8;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 2-bytes-per-pixel buffer size would wrap CMP_DWORD in LoopAG8.
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > (dwMax / 2) / pDDSD->dwWidth)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError LoopAG8(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateCompressedMipLevelData(pMipLevel, dwWidth, dwHeight, dwWidth * dwHeight * 2))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopAG8(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopG16(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = CMP_FOURCC_G16;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 2-bytes-per-pixel buffer size would wrap CMP_DWORD in LoopG16.
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > (dwMax / 2) / pDDSD->dwWidth)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError LoopG16(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateCompressedMipLevelData(pMipLevel, dwWidth, dwHeight, dwWidth * dwHeight * 2))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopG16(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopA8(FILE*&, DDSD2*& pDDSD, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = CMP_FOURCC_A8;
    pMipSet->m_dwFourCC2 = 0;

    // Reject dimensions whose 1-byte-per-pixel buffer size would wrap CMP_DWORD in LoopA8.
    const CMP_DWORD dwMax = (std::numeric_limits<CMP_DWORD>::max)();
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwHeight > dwMax / pDDSD->dwWidth)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError LoopA8(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateCompressedMipLevelData(pMipLevel, dwWidth, dwHeight, dwWidth * dwHeight))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopA8(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopABGR16(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopABGR16(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_16bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopABGR16(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError PreLoopG16R16(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopG16R16(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_16bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 2;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_WORD* pSrc  = (CMP_WORD*)pTempData;
    CMP_WORD* pEnd  = (CMP_WORD*)(pTempData + dwSize);
    CMP_WORD* pDest = pMipLevel->m_pwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 65535;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError PostLoopG16R16(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

bool SetupDDSD(DDSD2& ddsd2, const MipSet* pMipSet, bool bCompressed)
{
    assert(pMipSet);
    if (pMipSet == NULL)
        return false;

    memset(&ddsd2, 0, sizeof(DDSD2));
    ddsd2.dwSize        = sizeof(DDSD2);
    ddsd2.dwWidth       = pMipSet->m_nWidth;
    ddsd2.dwHeight      = pMipSet->m_nHeight;
    ddsd2.dwMipMapCount = pMipSet->m_nMipLevels;
    ddsd2.dwFlags       = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;

    ddsd2.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    ddsd2.ddsCaps.dwCaps         = DDSCAPS_TEXTURE;

    if (bCompressed)
    {
        ddsd2.dwFlags |= DDSD_LINEARSIZE;
        ddsd2.dwLinearSize = DDS_CMips->GetMipLevel(pMipSet, 0)->m_dwLinearSize;
    }
    else
        ddsd2.dwFlags |= DDSD_PITCH;

    if (pMipSet->m_nMipLevels > 1)
    {
        ddsd2.dwFlags |= DDSD_MIPMAPCOUNT;
        ddsd2.ddsCaps.dwCaps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
    }

    if (pMipSet->m_TextureType == TT_CubeMap)
    {
        ddsd2.ddsCaps.dwCaps2 |= DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;
    }
    else if (pMipSet->m_TextureType == TT_VolumeTexture)
    {
        ddsd2.dwFlags |= DDSD_DEPTH;
        ddsd2.dwDepth = pMipSet->m_nDepth;
        ddsd2.ddsCaps.dwCaps2 |= DDSCAPS2_VOLUME;
    }

    return true;
}

bool SetupDDSD_DX10(DDSD2& ddsd2, const MipSet* pMipSet, bool /*bCompressed*/)
{
    memset(&ddsd2, 0, sizeof(DDSD2));
    ddsd2.dwSize = sizeof(DDSD2);

    assert(pMipSet);
    if (pMipSet == NULL)
        return false;

    ddsd2.dwWidth       = pMipSet->m_nWidth;
    ddsd2.dwHeight      = pMipSet->m_nHeight;
    ddsd2.dwMipMapCount = pMipSet->m_nMipLevels;
    ddsd2.dwFlags       = DDSD_WIDTH | DDSD_HEIGHT;

    ddsd2.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    ddsd2.ddsCaps.dwCaps         = DDSCAPS_TEXTURE;

    if (pMipSet->m_TextureType == TT_CubeMap)
    {
        ddsd2.ddsCaps.dwCaps2 |= DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;
    }
    else if (pMipSet->m_TextureType == TT_VolumeTexture)
    {
        ddsd2.dwFlags |= DDSD_DEPTH;
        ddsd2.dwDepth = pMipSet->m_nDepth;
        ddsd2.ddsCaps.dwCaps2 |= DDSCAPS2_VOLUME;
    }

    if (pMipSet->m_nMipLevels > 1)
    {
        ddsd2.dwFlags |= DDSD_MIPMAPCOUNT;
        ddsd2.ddsCaps.dwCaps |= DDSCAPS_MIPMAP;
    }

    return true;
}

TC_PluginError PreLoopABGR32(FILE*&, DDSD2*&, MipSet*& pMipSet, void*&)
{
    pMipSet->m_dwFourCC  = 0;
    pMipSet->m_dwFourCC2 = 0;
    return PE_OK;
}

TC_PluginError LoopABGR32(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_32bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pbData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError PostLoopABGR32(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError LoopR32(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_32bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 4;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    size_t dwBytesRead = fread(pTempData, 1, dwSize, pFile);
    if (dwBytesRead != dwSize)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_DWORD* pSrc  = (CMP_DWORD*)pTempData;
    CMP_DWORD* pEnd  = (CMP_DWORD*)(pTempData + dwSize);
    CMP_DWORD* pDest = (CMP_DWORD*)pMipLevel->m_pwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 0;
        *pDest++ = _UI32_MAX;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR8G8(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 2;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_BYTE* pSrc  = pTempData;
    CMP_BYTE* pEnd  = (pTempData + dwSize);
    CMP_BYTE* pDest = pMipLevel->m_pbData;
    while (pSrc < pEnd)
    {
        *pDest++      = 0;
        CMP_BYTE bRed = *pSrc++;
        *pDest++      = *pSrc++;
        *pDest++      = bRed;
        *pDest++      = 255;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR32G32(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_32bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD  dwSize    = pMipLevel->m_dwLinearSize / 2;
    CMP_DWORD* pTempData = (CMP_DWORD*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_DWORD* pSrc  = pTempData;
    CMP_DWORD* pEnd  = (pTempData + (dwSize / sizeof(CMP_DWORD)));
    CMP_DWORD* pDest = pMipLevel->m_pdwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 0;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR10G10B10A2(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_1010102, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD  dwSize    = pMipLevel->m_dwLinearSize;
    CMP_DWORD* pTempData = (CMP_DWORD*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_DWORD* pSrc  = pTempData;
    CMP_DWORD* pEnd  = (pTempData + (dwSize / sizeof(CMP_DWORD)));
    CMP_DWORD* pDest = pMipLevel->m_pdwData;
    while (pSrc < pEnd)
    {
        CMP_DWORD dwSrc  = *pSrc++;
        CMP_DWORD dwDest = dwSrc;
        *pDest++         = dwDest;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoopR9G9B9E5(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_Float9995E, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    if (fread(pMipLevel->m_pfData, pMipLevel->m_dwLinearSize, 1, pFile) != 1)
        return PE_Unknown;

    return PE_OK;
}

TC_PluginError LoopR16G16(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_16bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 2;
    CMP_WORD* pTempData = (CMP_WORD*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_WORD* pSrc  = pTempData;
    CMP_WORD* pEnd  = (pTempData + (dwSize / sizeof(CMP_WORD)));
    CMP_WORD* pDest = pMipLevel->m_pwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 0;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError PreLoopR16(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError LoopR16(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it, for CMP we are always using RGBA buffer
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_16bit, TDT_ARGB))  // pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    pMipSet->m_TextureDataType = TDT_ARGB;
    CMP_DWORD dwSize           = pMipLevel->m_dwLinearSize / 4;

    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    size_t dwBytesRead = fread(pTempData, 1, dwSize, pFile);
    if (dwBytesRead != dwSize)
    {
        free(pTempData);
        return PE_Unknown;
    }

    // Load as gray scale
    CMP_WORD* pSrc  = (CMP_WORD*)pTempData;
    CMP_WORD* pEnd  = (CMP_WORD*)(pTempData + dwSize);
    CMP_WORD* pDest = pMipLevel->m_pwData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc;
        *pDest++ = *pSrc;
        *pDest++ = *pSrc++;
        *pDest++ = _UI16_MAX;
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError PostLoopR16(FILE*&, DDSD2*&, MipSet*&, void*&)
{
    return PE_OK;
}

TC_PluginError LoopR8(FILE*& pFile, DDSD2*&, MipSet*& pMipSet, void*&, int nMipLevel, int nFaceOrSlice, CMP_DWORD dwWidth, CMP_DWORD dwHeight)
{
    MipLevel* pMipLevel = DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nFaceOrSlice);
    // Allocate the permanent buffer and unpack the bitmap data into it
    if (!DDS_CMips->AllocateMipLevelData(pMipLevel, dwWidth, dwHeight, CF_8bit, pMipSet->m_TextureDataType))
    {
        return PE_Unknown;
    }

    CMP_DWORD dwSize    = pMipLevel->m_dwLinearSize / 4;
    CMP_BYTE* pTempData = (CMP_BYTE*)malloc(dwSize);
    assert(pTempData);
    if (!pTempData)
        return PE_Unknown;

    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        return PE_Unknown;
    }

    CMP_BYTE* pSrc  = pTempData;
    CMP_BYTE* pEnd  = (pTempData + dwSize);
    CMP_BYTE* pDest = pMipLevel->m_pbData;
    while (pSrc < pEnd)
    {
        *pDest++ = *pSrc++;
        *pDest++ = 0;
        *pDest++ = 0;
        *pDest++ = 255;
    }

    free(pTempData);

    return PE_OK;
}
