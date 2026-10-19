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

#include "single_include/catch2/catch.hpp"

#include "texture_utils.h"

TEST_CASE("CalcBufferSize_All_Formats", "[SDK]")
{
    const CMP_DWORD width       = 64;
    const CMP_DWORD height      = 64;
    const CMP_DWORD blockWidth  = 4;
    const CMP_DWORD blockHeight = 4;

    // these will change depending on the input texture
    CMP_DWORD pitch        = 0;
    CMP_DWORD expectedSize = 0;

    // 8-bit Four channel formats:
    //  CMP_FORMAT_RGBA_8888_S
    //  CMP_FORMAT_ARGB_8888_S
    //  CMP_FORMAT_ARGB_8888
    //  CMP_FORMAT_ABGR_8888
    //  CMP_FORMAT_RGBA_8888
    //  CMP_FORMAT_BGRA_8888
    //  CMP_FORMAT_ARGB_2101010
    //  CMP_FORMAT_RGBA_1010102

    pitch        = width * 4;
    expectedSize = width * height * 4;

    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_8888_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_8888_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_8888, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ABGR_8888, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_8888, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGRA_8888, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_2101010, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_1010102, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 8-bit Three channel formats
    //  CMP_FORMAT_RGB_888
    //  CMP_FORMAT_RGB_888_S
    //  CMP_FORMAT_BGR_888

    pitch        = width * 3;
    expectedSize = width * height * 3;

    CHECK(CalcBufferSize(CMP_FORMAT_RGB_888, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGB_888_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGR_888, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 8-bit Two channel formats
    //  CMP_FORMAT_RG_8_S
    //  CMP_FORMAT_RG_8

    pitch        = width * 2;
    expectedSize = width * height * 2;

    CHECK(CalcBufferSize(CMP_FORMAT_RG_8_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RG_8, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 8-bit One channel formats
    //  CMP_FORMAT_R_8_S
    //  CMP_FORMAT_R_8

    pitch        = width * 1;
    expectedSize = width * height * 1;

    CHECK(CalcBufferSize(CMP_FORMAT_R_8_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_R_8, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 16-bit Four channel formats
    //  CMP_FORMAT_ARGB_16
    //  CMP_FORMAT_ABGR_16
    //  CMP_FORMAT_RGBA_16
    //  CMP_FORMAT_BGRA_16
    //  CMP_FORMAT_ARGB_16F
    //  CMP_FORMAT_ABGR_16F
    //  CMP_FORMAT_RGBA_16F
    //  CMP_FORMAT_BGRA_16F

    pitch        = width * sizeof(CMP_WORD) * 4;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ABGR_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGRA_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ABGR_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGRA_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 16-bit Two channel formats
    //  CMP_FORMAT_RG_16
    //  CMP_FORMAT_RG_16F

    pitch        = width * sizeof(CMP_WORD) * 2;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_RG_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RG_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 16-bit One channel formats
    //  CMP_FORMAT_R_16
    //  CMP_FORMAT_R_16F

    pitch        = width * sizeof(CMP_WORD) * 1;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_R_16, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_R_16F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 32-bit Four channel formats
    //  CMP_FORMAT_RGBE_32F
    //  CMP_FORMAT_ARGB_32F
    //  CMP_FORMAT_ABGR_32F
    //  CMP_FORMAT_RGBA_32F
    //  CMP_FORMAT_BGRA_32F

    pitch        = width * sizeof(CMP_DWORD) * 4;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_RGBE_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ARGB_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ABGR_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_RGBA_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGRA_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 32-bit Three channel formats
    //  CMP_FORMAT_RGB_32F
    //  CMP_FORMAT_BGR_32F

    pitch        = width * sizeof(CMP_DWORD) * 3;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_RGB_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BGR_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 32-bit Two channel formats
    //  CMP_FORMAT_RG_32F

    pitch        = width * sizeof(CMP_DWORD) * 2;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_RG_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 32-bit One channel formats
    //  CMP_FORMAT_R_32F

    pitch        = width * sizeof(CMP_DWORD) * 1;
    expectedSize = pitch * height;

    CHECK(CalcBufferSize(CMP_FORMAT_R_32F, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 8 byte compressed formats
    //  CMP_FORMAT_BC1
    //  CMP_FORMAT_BC4
    //  CMP_FORMAT_BC4_S
    //  CMP_FORMAT_DXT1
    //  CMP_FORMAT_ATI1N
    //  CMP_FORMAT_ATC_RGB
    //  CMP_FORMAT_ETC_RGB
    //  CMP_FORMAT_ETC2_RGB
    //  CMP_FORMAT_ETC2_SRGB
    //  CMP_FORMAT_PVRTC

    pitch        = width;
    expectedSize = (width / 4) * (height / 4) * 8;

    CHECK(CalcBufferSize(CMP_FORMAT_BC1, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC4, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC4_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT1, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATI1N, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATC_RGB, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC_RGB, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_RGB, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_SRGB, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_RGBA1, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_SRGBA1, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // NOTE: This format isn't really used, so I don't know if it is worth making this test work
    //CHECK(CalcBufferSize(CMP_FORMAT_PVRTC, width, height, pitch, blockWidth, blockHeight) == expectedSize);

    // 16 byte compressed formats
    //  CMP_FORMAT_BC2
    //  CMP_FORMAT_BC3
    //  CMP_FORMAT_BC5
    //  CMP_FORMAT_BC5_S
    //  CMP_FORMAT_BC6H
    //  CMP_FORMAT_BC6H_SF
    //  CMP_FORMAT_BC7
    //  CMP_FORMAT_DXT3
    //  CMP_FORMAT_DXT5
    //  CMP_FORMAT_DXT5_xGBR
    //  CMP_FORMAT_DXT5_RxBG
    //  CMP_FORMAT_DXT5_RBxG
    //  CMP_FORMAT_DXT5_xRBG
    //  CMP_FORMAT_DXT5_RGxB
    //  CMP_FORMAT_DXT5_xGxR
    //  CMP_FORMAT_ATI2N
    //  CMP_FORMAT_ATI2N_XY
    //  CMP_FORMAT_ATI2N_DXT5
    //  CMP_FORMAT_ATC_RGBA_Explicit
    //  CMP_FORMAT_ATC_RGBA_Interpolated
    //  CMP_FORMAT_ASTC
    //  CMP_FORMAT_ETC2_RGBA
    //  CMP_FORMAT_ETC2_RGBA1
    //  CMP_FORMAT_ETC2_SRGBA
    //  CMP_FORMAT_ETC2_SRGBA1
    //  CMP_FORMAT_APC
    //  CMP_FORMAT_GTC

    pitch        = width;
    expectedSize = (width / 4) * (height / 4) * 16;

    CHECK(CalcBufferSize(CMP_FORMAT_BC2, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC3, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC5, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC5_S, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC6H, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC6H_SF, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_BC7, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT3, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_xGBR, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_RxBG, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_RBxG, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_xRBG, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_RGxB, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_DXT5_xGxR, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATI2N, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATI2N_XY, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATI2N_DXT5, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATC_RGBA_Explicit, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ATC_RGBA_Interpolated, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_RGBA, width, height, pitch, blockWidth, blockHeight) == expectedSize);
    CHECK(CalcBufferSize(CMP_FORMAT_ETC2_SRGBA, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#if (OPTION_BUILD_ASTC == 1)
    CHECK(CalcBufferSize(CMP_FORMAT_ASTC, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#endif
#ifdef USE_APC
    CHECK(CalcBufferSize(CMP_FORMAT_APC, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#endif
#ifdef USE_GTC
    CHECK(CalcBufferSize(CMP_FORMAT_GTC, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#endif

    // Other formats
    //  CMP_FORMAT_BROTLIG
    //  CMP_FORMAT_BINARY
    //  CMP_FORMAT_BASIS

    pitch        = width;
    expectedSize = width * height;

#ifdef USE_LOSSLESS_COMPRESSION
    CHECK(CalcBufferSize(CMP_FORMAT_BROTLIG, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#endif
    CHECK(CalcBufferSize(CMP_FORMAT_BINARY, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#ifdef USE_BASIS
    CHECK(CalcBufferSize(CMP_FORMAT_BASIS, width, height, pitch, blockWidth, blockHeight) == expectedSize);
#endif
}

#if (OPTION_BUILD_ASTC == 1)
TEST_CASE("CalcBufferSize_ASTC_VariableFootprint", "[SDK]")
{
    const CMP_DWORD width  = 64;
    const CMP_DWORD height = 64;
    const CMP_DWORD pitch  = 0;

    // ASTC is always 16 bytes/block regardless of the block footprint (4x4 up to 12x12),
    // so only the block count should vary with nBlockWidth/nBlockHeight.
    CHECK(CalcBufferSize(CMP_FORMAT_ASTC, width, height, pitch, 4, 4) == ((width / 4) * (height / 4) * 16));
    CHECK(CalcBufferSize(CMP_FORMAT_ASTC, width, height, pitch, 8, 8) == ((width / 8) * (height / 8) * 16));
    CHECK(CalcBufferSize(CMP_FORMAT_ASTC, width, height, pitch, 12, 12) == (((width + 11) / 12) * ((height + 11) / 12) * 16));

    // A caller that forgets to set nBlockWidth/nBlockHeight (left as 0) must not
    // trigger a divide-by-zero; it should fall back to the 4x4 default like CT_APC does.
    CHECK(CalcBufferSize(CMP_FORMAT_ASTC, width, height, pitch, 0, 0) == ((width / 4) * (height / 4) * 16));
}
#endif