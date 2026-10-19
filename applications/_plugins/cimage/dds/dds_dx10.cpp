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

// Windows Header Files:
#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#ifdef _WIN32
#include "ddraw.h"
#include "dxgiformat.h"
#include "d3d10.h"
#endif

#include "common.h"
#include "compressonator.h"
#include "tc_pluginapi.h"
#include "dds_dx10.h"
#include "dds_helpers.h"
#include "version.h"
#include "texture.h"

TC_PluginError LoadDDS_DX10_RGBA_32F(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_RGBA32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_RGBA_16F(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_RGBA16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_RG32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R10G10B10A2(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R9G9B9E5_SHAREDEXP(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R11G11B10F(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R8G8B8A8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R16G16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R8G8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_R8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet);
TC_PluginError LoadDDS_DX10_FourCC(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet, CMP_DWORD dwFourCC);

extern int CMP_MaxFacesOrSlices(const MipSet* pMipSet, int nMipLevel);

typedef struct
{
    DXGI_FORMAT              dxgiFormat;
    D3D10_RESOURCE_DIMENSION resourceDimension;
    uint32_t                 miscFlag;  // Used for D3D10_RESOURCE_MISC_FLAG
    uint32_t                 arraySize;
    uint32_t                 reserved;  // Currently unused
} DDS_HEADER_DDS10;

// TODO: This function doesn't set pMipSet->m_format for all loaded DDS images
// this is mostly fine because we assume RGBA8888 by default, and most of these functions convert to that format
// but this isn't the case for everything, so a better solution should probably be sought out
TC_PluginError LoadDDS_DX10(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    DDS_HEADER_DDS10 HeaderDDS10;
    ;
    fread(&HeaderDDS10, sizeof(HeaderDDS10), 1, pFile);

    TC_PluginError err = PE_Unknown;

    switch (HeaderDDS10.dxgiFormat)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        err = LoadDDS_DX10_RGBA_32F(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        err = LoadDDS_DX10_RGBA32(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
        err = LoadDDS_DX10_RGBA16(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
        err = LoadDDS_DX10_RG32(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
        pMipSet->m_format = CMP_FORMAT_RGBA_1010102;
        err               = LoadDDS_DX10_R10G10B10A2(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        err = LoadDDS_DX10_R9G9B9E5_SHAREDEXP(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R11G11B10_FLOAT:
        err = LoadDDS_DX10_R11G11B10F(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
        err = LoadDDS_DX10_R8G8B8A8(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
        err = LoadDDS_DX10_R16G16(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
        err = LoadDDS_DX10_R32(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
        err = LoadDDS_DX10_R8G8(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
        err = LoadDDS_DX10_R16(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
        err = LoadDDS_DX10_R8(pFile, pDDSD, pMipSet);
        break;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC1;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC1);
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC2;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC2);
        break;

    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC3;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC3);
        break;

    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC4;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC4);
        break;

    case DXGI_FORMAT_BC4_SNORM:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC4_S;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC4S);
        break;

    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
        pMipSet->m_format     = CMP_FORMAT_BC5;
        pMipSet->m_compressed = true;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC5);
        break;

    case DXGI_FORMAT_BC5_SNORM:
        pMipSet->m_format     = CMP_FORMAT_BC5_S;
        pMipSet->m_compressed = true;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_BC5S);
        break;

    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC6H;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_DX10);
        break;
    case DXGI_FORMAT_BC6H_SF16:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC6H_SF;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_DX10);
        break;

    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        pMipSet->m_compressed = true;
        pMipSet->m_format     = CMP_FORMAT_BC7;
        err                   = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_DX10);
        break;

    // case DXGI_FORMAT_???:
    //    pMipSet->m_compressed = true;
    //    pMipSet->m_format     = CMP_FORMAT_ASTC;
    //    err = LoadDDS_DX10_FourCC(pFile, pDDSD, pMipSet, CMP_FOURCC_DX10);
    //    pMipSet->m_swizzle    = ???;
    //    break;
    //
    default:
        assert(0);
    }

    fclose(pFile);

    return err;
}

TC_PluginError LoadDDS_DX10_RGBA_32F(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_Float32, TDT_ARGB, PreLoopABGR32F, LoopABGR32F, PostLoopABGR32F);
}

TC_PluginError LoadDDS_DX10_RGBA32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_32bit, TDT_ARGB, PreLoopABGR32F, LoopABGR32F, PostLoopABGR32F);
}

TC_PluginError LoadDDS_DX10_RGBA_16F(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_Float16, TDT_ARGB, PreLoopABGR16F, LoopABGR16F, PostLoopABGR16F);
}

TC_PluginError LoadDDS_DX10_RGBA16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_Float16, TDT_ARGB, PreLoopABGR16F, LoopABGR16F, PostLoopABGR16F);
}

TC_PluginError LoadDDS_DX10_RG32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_32bit, TDT_XRGB, PreLoopABGR32, LoopR32G32, PreLoopABGR32);
}

TC_PluginError LoadDDS_DX10_R10G10B10A2(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    pMipSet->m_TextureDataType   = TDT_ARGB;
    ChannelFormat channelFormat  = CF_1010102;
    void*         pChannelFormat = &channelFormat;
    return GenericLoadFunction(
        pFile, pDDSD, pMipSet, pChannelFormat, channelFormat, pMipSet->m_TextureDataType, PreLoopDefault, LoopR10G10B10A2, PostLoopDefault);
}

TC_PluginError LoadDDS_DX10_R9G9B9E5_SHAREDEXP(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    pMipSet->m_TextureDataType   = TDT_XRGB;
    ChannelFormat channelFormat  = CF_Float9995E;
    void*         pChannelFormat = &channelFormat;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, pChannelFormat, channelFormat, pMipSet->m_TextureDataType, PreLoopDefault, LoopR9G9B9E5, PostLoopDefault);
}

TC_PluginError LoadDDS_DX10_R11G11B10F(FILE* /*pFile*/, DDSD2* /*pDDSD*/, MipSet* /*pMipSet*/)
{
    return PE_Unknown;
    /*
        void* extra;
        return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_Float16, TDT_ARGB, PreLoopABGR16F, LoopABGR16F, PostLoopABGR16F);
    */
}

TC_PluginError LoadDDS_DX10_R8G8B8A8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    ARGB8888Struct* pARGB8888Struct = (ARGB8888Struct*)calloc(sizeof(ARGB8888Struct), 1);
    if (!pARGB8888Struct)
        return PE_Unknown;
    void* extra = pARGB8888Struct;

    pARGB8888Struct->nFlags |= EF_UseBitMasks;
    // prevent integer overflow in temporary pixel buffer size calculation
    if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || pDDSD->dwWidth > UINT32_MAX / 4 / pDDSD->dwHeight)
    {
        free(pARGB8888Struct);
        return PE_Unknown;
    }
    pARGB8888Struct->pMemory = malloc((size_t)4 * pDDSD->dwWidth * pDDSD->dwHeight);
    if (!pARGB8888Struct->pMemory)
    {
        free(pARGB8888Struct);
        return PE_Unknown;
    }
    pARGB8888Struct->nRMask  = 0x000000ff;
    pARGB8888Struct->nGMask  = 0x0000ff00;
    pARGB8888Struct->nBMask  = 0x00ff0000;
    pARGB8888Struct->nRShift = 0;
    pARGB8888Struct->nGShift = 8;
    pARGB8888Struct->nBShift = 16;

    pMipSet->m_TextureDataType = TDT_ARGB;

    TC_PluginError err = GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_8bit, pMipSet->m_TextureDataType, PreLoopRGB8888, LoopRGB8888, PostLoopRGB8888);
    // pFile is closed by the LoadDDS_DX10() dispatcher.
    return err;
}

TC_PluginError LoadDDS_DX10_R16G16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_16bit, TDT_XRGB, PreLoopABGR16, LoopR16G16, PreLoopABGR16);
}

TC_PluginError LoadDDS_DX10_R8G8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_8bit, TDT_XRGB, PreLoopRGB8888, LoopR8G8, PreLoopRGB8888);
}

TC_PluginError LoadDDS_DX10_R32(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_32bit, TDT_XRGB, PreLoopABGR32, LoopR32, PostLoopABGR32);
}

TC_PluginError LoadDDS_DX10_R16(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_16bit, TDT_XRGB, PreLoopABGR16, LoopR16, PostLoopABGR16);
}

TC_PluginError LoadDDS_DX10_R8(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_8bit, TDT_XRGB, PreLoopRGB8888, LoopR8, PreLoopRGB8888);
}

TC_PluginError LoadDDS_DX10_FourCC(FILE* pFile, DDSD2* pDDSD, MipSet* pMipSet, CMP_DWORD /*dwFourCC*/)
{
    void* extra = NULL;
    return GenericLoadFunction(pFile, pDDSD, pMipSet, extra, CF_Compressed, TDT_XRGB, PreLoopFourCC, LoopFourCC, PostLoopFourCC);
}

DXGI_FORMAT GetDXGIFormat(const MipSet* pMipSet)
{
    if (pMipSet->m_dwFourCC == CMP_FOURCC_DX10)
    {
        switch (pMipSet->m_format)
        {
        case CMP_FORMAT_BC6H:
            return DXGI_FORMAT_BC6H_UF16;
        case CMP_FORMAT_BC6H_SF:
            return DXGI_FORMAT_BC6H_SF16;
        case CMP_FORMAT_BC1:
            return DXGI_FORMAT_BC1_UNORM;
        case CMP_FORMAT_BC2:
            return DXGI_FORMAT_BC2_UNORM;
        case CMP_FORMAT_BC3:
            return DXGI_FORMAT_BC3_UNORM;
        case CMP_FORMAT_BC4:
            return DXGI_FORMAT_BC4_UNORM;
        case CMP_FORMAT_BC5:
            return DXGI_FORMAT_BC5_UNORM;
        case CMP_FORMAT_BC7:
            return DXGI_FORMAT_BC7_UNORM;
            // case CMP_FORMAT_ASTC:        return DXGI_FORMAT_????;  Not yet supported as of Jun 24 2015
        }
    }
    else
        switch (pMipSet->m_dwFourCC)
        {  // legacy dont add anything here use above
        case CMP_FOURCC_BC1:
            return DXGI_FORMAT_BC1_UNORM;
        case CMP_FOURCC_BC2:
            return DXGI_FORMAT_BC2_UNORM;
        case CMP_FOURCC_BC3:
            return DXGI_FORMAT_BC3_UNORM;
        case CMP_FOURCC_BC4:
            return DXGI_FORMAT_BC4_UNORM;
        case CMP_FOURCC_BC4S:
            return DXGI_FORMAT_BC4_SNORM;
        case CMP_FOURCC_BC4U:
            return DXGI_FORMAT_BC4_UNORM;
        case CMP_FOURCC_BC5:
            return DXGI_FORMAT_BC5_UNORM;
        case CMP_FOURCC_BC5S:
            return DXGI_FORMAT_BC5_SNORM;
        }
    return DXGI_FORMAT_UNKNOWN;
}

bool SetupDDSD10(DDS_HEADER_DDS10& HeaderDDS10, const MipSet* pMipSet)
{
    memset(&HeaderDDS10, 0, sizeof(HeaderDDS10));

    assert(pMipSet);
    if (pMipSet == NULL)
        return false;

    HeaderDDS10.dxgiFormat = GetDXGIFormat(pMipSet);

    if (pMipSet->m_TextureType == TT_2D)
    {
        HeaderDDS10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
        HeaderDDS10.miscFlag          = 0;
        HeaderDDS10.arraySize         = 1;
    }
    else if (pMipSet->m_TextureType == TT_CubeMap)
    {
        HeaderDDS10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
        HeaderDDS10.miscFlag          = D3D10_RESOURCE_MISC_TEXTURECUBE;
        HeaderDDS10.arraySize         = 1;
    }
    else
    {
        HeaderDDS10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
        HeaderDDS10.miscFlag          = 0;
        HeaderDDS10.arraySize         = pMipSet->m_nDepth;
    }

    HeaderDDS10.reserved = 0;

    return true;
}

TC_PluginError SaveDDS_DX10(FILE* pFile, const MipSet* pMipSet)
{
    assert(pFile);
    assert(pMipSet);

    DDSD2 ddsd2;
    SetupDDSD_DX10(ddsd2, pMipSet, true);

    ddsd2.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    //    if(pMipSet->m_TextureDataType == TDT_ARGB)
    //        ddsd2.ddpfPixelFormat.dwFlags |= DDPF_ALPHAPIXELS;
    //    if(pMipSet->m_Flags & MS_AlphaPremult)
    //        ddsd2.ddpfPixelFormat.dwFlags |= DDPF_ALPHAPREMULT;

    ddsd2.ddpfPixelFormat.dwFourCC = CMP_MAKEFOURCC('D', 'X', '1', '0');

    switch (pMipSet->m_format)
    {
    case CMP_FORMAT_BC6H:
    case CMP_FORMAT_BC6H_SF:
        ddsd2.lPitch = ddsd2.dwWidth * 4;
        break;
    case CMP_FORMAT_BC7:
    default:
        ddsd2.lPitch = ddsd2.dwWidth * 4;
        break;
    }

    // Write the data
    fwrite(&ddsd2, sizeof(DDSD2), 1, pFile);

    DDS_HEADER_DDS10 HeaderDDS10;
    SetupDDSD10(HeaderDDS10, pMipSet);

    fwrite(&HeaderDDS10, sizeof(HeaderDDS10), 1, pFile);

    int nSlices = (pMipSet->m_TextureType == TT_2D) ? 1 : CMP_MaxFacesOrSlices(pMipSet, 0);
    for (int nSlice = 0; nSlice < nSlices; nSlice++)
        for (int nMipLevel = 0; nMipLevel < pMipSet->m_nMipLevels; nMipLevel++)
            fwrite(DDS_CMips->GetMipLevel(pMipSet, nMipLevel, nSlice)->m_pbData, DDS_CMips->GetMipLevel(pMipSet, nMipLevel)->m_dwLinearSize, 1, pFile);

    fclose(pFile);

    return PE_OK;
}
