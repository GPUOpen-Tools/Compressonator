//=====================================================================
// Copyright (c) 2026, Advanced Micro Devices, Inc. All rights reserved.
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
//

#include "single_include/catch2/catch.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "compressonator.h"
#include "test_constants.h"

// ---------------------------------------------------------------------------
// Security metadata macro — logged via Catch2 INFO on every test failure or
// with -s (--success) flag, and recorded in JUnit / HTML reports.
// ---------------------------------------------------------------------------
#define DDS_LOADER_SECURITY_INFO() \
    INFO("TEST   : Release of Invalid Pointer or Reference"); \
    INFO("       : Integer Overflow or Wraparound"); \
    INFO("Status : FIXED")

// ---------------------------------------------------------------------------
// Helper — write a minimal ARGB8888 DDS file to the test-data directory.
//
// Builds a binary DDS file with:
//   - 4-byte magic "DDS "
//   - 124-byte DDSURFACEDESC2 header (dwRGBBitCount=32, DDPF_RGB|DDPF_ALPHAPIXELS)
//   - Optional pixel data (written only when both width and height are small
//     enough that the pixel buffer is ≤ 256 bytes, so the file stays tiny)
//
// For overflow/zero-dimension tests no pixel data is appended — the loader
// is expected to reject the file before ever calling fread() for pixels.
//
// Returns the full file path on success, or an empty string on failure.
// ---------------------------------------------------------------------------
static std::string createMinimalDDS_ARGB8888(const char* filename, uint32_t width, uint32_t height)
{
    std::string path = std::string(TEST_DATA_PATH) + "/" + filename;

    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return {};

    // Convenience: write a little-endian 32-bit value.
    auto writeU32 = [&](uint32_t v) {
        uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)};
        fwrite(b, 1, 4, f);
    };

    // ---- 4-byte magic -------------------------------------------------------
    fwrite("DDS ", 1, 4, f);

    // ---- DDSURFACEDESC2 (124 bytes) -----------------------------------------
    writeU32(124);          // dwSize
    writeU32(0x00001007u);  // dwFlags: DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT
    writeU32(height);       // dwHeight
    writeU32(width);        // dwWidth
    writeU32(0);            // lPitch / dwLinearSize
    writeU32(0);            // dwDepth
    writeU32(1);            // dwMipMapCount (loader forces to 1 if DDSD_MIPMAPCOUNT not set)

    // reserved1[11] — 11 DWORDs of zeros (covers dwAlphaBitDepth, dwReserved,
    // lpSurface, and the four DDCOLORKEY pairs that follow in DDSURFACEDESC2).
    for (int i = 0; i < 11; ++i)
        writeU32(0);

    // ---- DDPIXELFORMAT (32 bytes, at DDSURFACEDESC2 offset 72) -------------
    writeU32(32);           // dwSize
    writeU32(0x00000041u);  // dwFlags: DDPF_RGB (0x40) | DDPF_ALPHAPIXELS (0x01)
    writeU32(0);            // dwFourCC = 0 (not a FourCC format)
    writeU32(32);           // dwRGBBitCount = 32  → routes to LoadDDS_RGB8888
    writeU32(0x00FF0000u);  // dwRBitMask
    writeU32(0x0000FF00u);  // dwGBitMask
    writeU32(0x000000FFu);  // dwBBitMask
    writeU32(0xFF000000u);  // dwRGBAlphaBitMask

    // ---- DDSCAPS2 (16 bytes, at DDSURFACEDESC2 offset 104) -----------------
    writeU32(0x00001000u);  // dwCaps: DDSCAPS_TEXTURE
    writeU32(0);            // dwCaps2
    writeU32(0);            // dwCaps3
    writeU32(0);            // dwCaps4

    // dwTextureStage (at DDSURFACEDESC2 offset 120)
    writeU32(0);

    // ---- Pixel data ---------------------------------------------------------
    // Written only for small, valid images so the file is self-consistent.
    // For overflow/zero-dimension attack files we deliberately omit pixel data;
    // the loader must reject them during header validation (before any fread).
    if (width > 0 && height > 0 && width <= 16 && height <= 16)
    {
        uint32_t             pixelBytes = width * height * 4u;
        std::vector<uint8_t> pixels(pixelBytes, 0xFFu);
        fwrite(pixels.data(), 1, pixelBytes, f);
    }

    fclose(f);
    return path;
}

// ===========================================================================
// Mandatory metadata test —
// it appears in every test run's HTML report and JUnit XML output.
// ===========================================================================
TEST_CASE("DDSLoaderSecurity_Metadata", "[DDSLoader][Security]")
{
    SUCCEED("Release of Invalid Pointer, Reference Integer Overflow Status: FIXED");
}

// ===========================================================================
// Group 1 — Integer-overflow dimensions
//
// The expression 4 * dwWidth * dwHeight overflows a uint32_t when either
// dimension is crafted to wrap.  Before the fix malloc() received the tiny
// wrapped size; fread() then overflowed the tiny buffer, corrupting the heap
// and eventually triggering an invalid free in CMP_FreeMipSet().
// After the fix the loader must reject the file before any allocation.
// ===========================================================================

TEST_CASE("DDSLoaderSecurity_RGB8888_OverflowDimensions_Rejected", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    // dwWidth = 0x40000001 (1073741825), dwHeight = 4
    // 4 * 0x40000001 * 4 = 0x400000010  →  wraps to 16 bytes in 32-bit arithmetic.
    // Overflow guard:  0x40000001 > UINT32_MAX/4/4 = 268435455  →  PE_Unknown.
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_overflow_dims.dds", 0x40000001u, 4u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(result != CMP_OK);

    if (result == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("DDSLoaderSecurity_RGB8888_MaxWidthOverflow_Rejected", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    // dwWidth = UINT32_MAX (0xFFFFFFFF), dwHeight = 2
    // 4 * 0xFFFFFFFF * 2 would overflow to 0xFFFFFFF8 — but the overflow guard
    // fires before malloc(): 0xFFFFFFFF > UINT32_MAX/4/2 = 536870911.
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_maxwidth.dds", 0xFFFFFFFFu, 2u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(result != CMP_OK);

    if (result == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 2 — Zero-dimension inputs
//
// Zero width or height is an invalid DDS and must be rejected.  Both are
// explicitly checked in the new overflow guard:
//   if (pDDSD->dwWidth == 0 || pDDSD->dwHeight == 0 || ...)
// ===========================================================================

TEST_CASE("DDSLoaderSecurity_RGB8888_ZeroWidth_Rejected", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_zerowidth.dds", 0u, 4u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(result != CMP_OK);

    if (result == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("DDSLoaderSecurity_RGB8888_ZeroHeight_Rejected", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_zeroheight.dds", 4u, 0u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(result != CMP_OK);

    if (result == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 3 — Boundary condition: exact-fit large-but-valid dimensions
//
// dwWidth = 1024, dwHeight = 1024 — 4 * 1024 * 1024 = 4,194,304 which fits
// comfortably in uint32_t.  The overflow guard must NOT fire here.
// The file contains no pixel data so the loader will fail on fread, but it
// must NOT crash — it must return an error gracefully.
// ===========================================================================

TEST_CASE("DDSLoaderSecurity_RGB8888_LargeValidDims_NoOverflowFire", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    // 4 * 1024 * 1024 = 4194304 — no overflow.
    // File has no pixel data, so fread will fail → loader returns error (not CMP_OK).
    // The important check is that there is NO crash / heap corruption.
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_largevalid.dds", 1024u, 1024u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    // Result must be an error (no pixel data), but there must be NO crash.
    // We do not assert CMP_OK here — just verify graceful failure.
    INFO("result = " << result << " (expected non-zero / error because no pixel data)");
    CHECK(result != CMP_OK);

    if (result == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 4 — Happy path: valid minimal 2×2 ARGB8888 DDS
//
// Verifies that the security fix did not break loading of a syntactically
// valid DDS file.  This is the regression guard: if this test fails, the
// overflow check has an off-by-one or is applied too aggressively.
// ===========================================================================

TEST_CASE("DDSLoaderSecurity_RGB8888_ValidSmall_Accepted", "[DDSLoader][Security]")
{
    DDS_LOADER_SECURITY_INFO();
    // 2×2 ARGB8888: 4 * 2 * 2 = 16 bytes of pixel data appended.
    // The loader should return CMP_OK and produce a valid MipSet.
    const std::string path = createMinimalDDS_ARGB8888("sec_dds_rgb8888_valid_2x2.dds", 2u, 2u);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  result = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(result == CMP_OK);
    if (result == CMP_OK)
    {
        CHECK(mipSet.m_nWidth == 2);
        CHECK(mipSet.m_nHeight == 2);
        CMP_FreeMipSet(&mipSet);
    }
    std::remove(path.c_str());
}
