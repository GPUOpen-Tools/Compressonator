//=====================================================================
// Copyright (c) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.
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

#include "codec_common.h"

#include <assert.h>

CodecType GetCodecType(CMP_FORMAT format)
{
    switch (format)
    {
    case CMP_FORMAT_RGBA_1010102:
    case CMP_FORMAT_ARGB_2101010:
        return CT_None;
    case CMP_FORMAT_RGBA_8888:
        return CT_None;
    case CMP_FORMAT_BGRA_8888:
        return CT_None;
    case CMP_FORMAT_RGBA_8888_S:
    case CMP_FORMAT_ARGB_8888:
        return CT_None;
    case CMP_FORMAT_BGR_888:
        return CT_None;
    case CMP_FORMAT_RGB_888:
        return CT_None;
    case CMP_FORMAT_RG_8:
        return CT_None;
    case CMP_FORMAT_R_8:
        return CT_None;
    case CMP_FORMAT_ARGB_16:
        return CT_None;
    case CMP_FORMAT_RG_16:
        return CT_None;
    case CMP_FORMAT_R_16:
        return CT_None;
    case CMP_FORMAT_ABGR_16F:
    case CMP_FORMAT_ARGB_16F:
    case CMP_FORMAT_RGBA_16F:
        return CT_None;
    case CMP_FORMAT_RG_16F:
        return CT_None;
    case CMP_FORMAT_R_16F:
        return CT_None;
    case CMP_FORMAT_ARGB_32F:
    case CMP_FORMAT_RGBA_32F:
        return CT_None;
    case CMP_FORMAT_RG_32F:
        return CT_None;
    case CMP_FORMAT_R_32F:
        return CT_None;
    case CMP_FORMAT_RGBE_32F:
        return CT_None;
    case CMP_FORMAT_DXT1:
        return CT_DXT1;
    case CMP_FORMAT_DXT3:
        return CT_DXT3;
    case CMP_FORMAT_DXT5:
        return CT_DXT5;
    case CMP_FORMAT_DXT5_xGBR:
        return CT_DXT5_xGBR;
    case CMP_FORMAT_DXT5_RxBG:
        return CT_DXT5_RxBG;
    case CMP_FORMAT_DXT5_RBxG:
        return CT_DXT5_RBxG;
    case CMP_FORMAT_DXT5_xRBG:
        return CT_DXT5_xRBG;
    case CMP_FORMAT_DXT5_RGxB:
        return CT_DXT5_RGxB;
    case CMP_FORMAT_DXT5_xGxR:
        return CT_DXT5_xGxR;
    case CMP_FORMAT_ATI1N:
        return CT_ATI1N;
    case CMP_FORMAT_ATI2N:
        return CT_ATI2N;
    case CMP_FORMAT_ATI2N_XY:
        return CT_ATI2N_XY;
    case CMP_FORMAT_ATI2N_DXT5:
        return CT_ATI2N_DXT5;
    case CMP_FORMAT_BC1:
        return CT_DXT1;
    case CMP_FORMAT_BC2:
        return CT_DXT3;
    case CMP_FORMAT_BC3:
        return CT_DXT5;
    case CMP_FORMAT_BC4:
        return CT_ATI1N;
    case CMP_FORMAT_BC4_S:
        return CT_ATI1N_S;
    case CMP_FORMAT_BC5:
        return CT_ATI2N_XY;  // Red & Green channels
    case CMP_FORMAT_BC5_S:
        return CT_ATI2N_XY_S;  // Red & Green channels
    case CMP_FORMAT_BC6H:
        return CT_BC6H;
    case CMP_FORMAT_BC6H_SF:
        return CT_BC6H_SF;
    case CMP_FORMAT_BC7:
        return CT_BC7;
#if (OPTION_BUILD_ASTC == 1)
    case CMP_FORMAT_ASTC:
        return CT_ASTC;
#endif
    case CMP_FORMAT_ATC_RGB:
        return CT_ATC_RGB;
    case CMP_FORMAT_ATC_RGBA_Explicit:
        return CT_ATC_RGBA_Explicit;
    case CMP_FORMAT_ATC_RGBA_Interpolated:
        return CT_ATC_RGBA_Interpolated;
    case CMP_FORMAT_ETC_RGB:
        return CT_ETC_RGB;
    case CMP_FORMAT_ETC2_RGB:
        return CT_ETC2_RGB;
    case CMP_FORMAT_ETC2_SRGB:
        return CT_ETC2_SRGB;
    case CMP_FORMAT_ETC2_RGBA:
        return CT_ETC2_RGBA;
    case CMP_FORMAT_ETC2_RGBA1:
        return CT_ETC2_RGBA1;
    case CMP_FORMAT_ETC2_SRGBA:
        return CT_ETC2_SRGBA;
    case CMP_FORMAT_ETC2_SRGBA1:
        return CT_ETC2_SRGBA1;
#ifdef USE_APC
    case CMP_FORMAT_APC:
        return CT_APC;
#endif
#ifdef USE_GTC
    case CMP_FORMAT_GTC:
        return CT_GTC;
#endif
    case CMP_FORMAT_BROTLIG:
        return CT_BRLG;
#ifdef USE_BASIS
    case CMP_FORMAT_BASIS:
        return CT_BASIS;
#endif
    case CMP_FORMAT_BINARY:
        return CT_None;
    default:
        return CT_Unknown;
    }
}

CMP_DWORD CalcBufferSize(CodecType nCodecType, CMP_DWORD dwWidth, CMP_DWORD dwHeight, CMP_BYTE nBlockWidth, CMP_BYTE nBlockHeight)
{
    nBlockWidth;
    nBlockHeight;

    CMP_DWORD dwChannels;
    CMP_DWORD dwBitsPerChannel;
    CMP_DWORD buffsize = 0;

    switch (nCodecType)
    {
    // Block size is 4x4 and 64 bits per block
    case CT_DXT1:
    case CT_ATI1N:
    case CT_ATI1N_S:
    case CT_ATC_RGB:
    case CT_ETC_RGB:
    case CT_ETC2_RGB:
    case CT_ETC2_SRGB:
    case CT_ETC2_RGBA1:
    case CT_ETC2_SRGBA1:
        dwChannels       = 1;
        dwBitsPerChannel = 4;
        dwWidth          = ((dwWidth + 3) / 4) * 4;
        dwHeight         = ((dwHeight + 3) / 4) * 4;
        buffsize         = (dwWidth * dwHeight * dwChannels * dwBitsPerChannel) / 8;
        break;

    // Block size is 4x4 and 128 bits per block
    case CT_ETC2_RGBA:
    case CT_ETC2_SRGBA:
        // dwChannels = 2;
        // dwBitsPerChannel = 4;
        // dwWidth = ((dwWidth + 3) / 4) * 4;
        // dwHeight = ((dwHeight + 3) / 4) * 4;
        // buffsize = (dwWidth * dwHeight * dwChannels * dwBitsPerChannel) / 8;
        dwWidth  = ((dwWidth + 3) / 4) * 4;
        dwHeight = ((dwHeight + 3) / 4) * 4;
        buffsize = dwWidth * dwHeight;
        if (buffsize < BC_BLOCK_PIXELS)
            buffsize = BC_BLOCK_PIXELS;
        break;

    // Block size is 4x4 and 128 bits per block
    case CT_DXT3:
    case CT_DXT5:
    case CT_DXT5_xGBR:
    case CT_DXT5_RxBG:
    case CT_DXT5_RBxG:
    case CT_DXT5_xRBG:
    case CT_DXT5_RGxB:
    case CT_DXT5_xGxR:
    case CT_ATI2N:
    case CT_ATI2N_S:
    case CT_ATI2N_XY:
    case CT_ATI2N_XY_S:
    case CT_ATI2N_DXT5:
    case CT_ATC_RGBA_Explicit:
    case CT_ATC_RGBA_Interpolated:
        dwChannels       = 2;
        dwBitsPerChannel = 4;
        dwWidth          = ((dwWidth + 3) / 4) * 4;
        dwHeight         = ((dwHeight + 3) / 4) * 4;
        buffsize         = (dwWidth * dwHeight * dwChannels * dwBitsPerChannel) / 8;
        break;

    // Block size is 4x4 and 128 bits per block
    case CT_BC6H:
    case CT_BC6H_SF:
        dwWidth  = ((dwWidth + 3) / 4) * 4;
        dwHeight = ((dwHeight + 3) / 4) * 4;
        buffsize = dwWidth * dwHeight;
        if (buffsize < BC_BLOCK_PIXELS)
            buffsize = BC_BLOCK_PIXELS;
        break;

    // Block size is 4x4 and 128 bits per block
    case CT_BC7:
        dwWidth  = ((dwWidth + 3) / 4) * 4;
        dwHeight = ((dwHeight + 3) / 4) * 4;
        buffsize = dwWidth * dwHeight;
        if (buffsize < BC_BLOCK_PIXELS)
            buffsize = BC_BLOCK_PIXELS;
        break;
#if (OPTION_BUILD_ASTC == 1)
    // Block size ranges from 4x4 to 12x12 and 128 bits per block
    case CT_ASTC:
        // Guard against divide-by-zero from an unset/invalid block size
        if (nBlockWidth <= 0)
            nBlockWidth = 4;
        if (nBlockHeight <= 0)
            nBlockHeight = 4;
        dwWidth  = ((dwWidth + nBlockWidth - 1) / nBlockWidth) * 4;
        dwHeight = ((dwHeight + nBlockHeight - 1) / nBlockHeight) * 4;
        buffsize = dwWidth * dwHeight;
        break;
#endif
#ifdef _WIN32
#ifdef USE_APC
    case CT_APC:
        if (nBlockWidth <= 0)
            nBlockWidth = 4;
        if (nBlockHeight <= 0)
            nBlockHeight = 4;
        dwWidth  = ((dwWidth + nBlockWidth - 1) / nBlockWidth) * 4;
        dwHeight = ((dwHeight + nBlockHeight - 1) / nBlockHeight) * 4;
        buffsize = dwWidth * dwHeight;
        break;
#endif
        // Block size is 4x4 and 128 bits per block. in future releases its will vary in Block Sizes and bits per block may change to 256
#ifdef USE_GTC
    case CT_GTC:
        dwWidth  = ((dwWidth + 3) / 4) * 4;
        dwHeight = ((dwHeight + 3) / 4) * 4;
        buffsize = dwWidth * dwHeight;
        if (buffsize < (4 * 4))
            buffsize = 4 * 4;
        break;
#endif
#ifdef USE_LOSSLESS_COMPRESSION
    case CT_BRLG:
        buffsize = dwWidth * dwHeight;
        break;
#endif
#ifdef USE_BASIS
    // Block size is 4x4 and 128 bits per block, needs conformation!!
    case CT_BASIS:
        dwWidth  = ((dwWidth + 3) / 4) * 4;
        dwHeight = ((dwHeight + 3) / 4) * 4;
        buffsize = dwWidth * dwHeight;
        if (buffsize < (4 * 4))
            buffsize = 4 * 4;
        break;
#endif
#endif
    default:
        return 0;
    }

    return buffsize;
}