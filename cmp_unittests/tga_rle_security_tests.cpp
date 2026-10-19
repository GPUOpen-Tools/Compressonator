//=====================================================================
// Copyright (c) 2024, Advanced Micro Devices, Inc. All rights reserved.
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

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "compressonator.h"
#include "test_constants.h"

// ---------------------------------------------------------------------------
// TGA image-type constants (from tga.h)
// ---------------------------------------------------------------------------
static const unsigned char TGA_IMAGETYPE_RLE_RGB  = 10;  // RLE RGB  (32 or 24 bpp)
static const unsigned char TGA_IMAGETYPE_RLE_GREY = 11;  // RLE grey (8 bpp)

// ---------------------------------------------------------------------------
// Helper - write a 4x4 malformed TGA file to the test-data directory.
//
// The 18-byte header is fully initialised with the supplied imageType,
// colorDepth, width, height and the top-to-bottom orientation flag (0x20).
// pixelData is appended verbatim after the header; it is intentionally
// incomplete, which is what triggers the bounds-check paths under test.
//
// Returns the full file path on success, or an empty string on failure.
// ---------------------------------------------------------------------------
static std::string createMalformedTGA(const char*                       filename,
                                      unsigned char                     imageType,
                                      unsigned char                     colorDepth,
                                      short                             width,
                                      short                             height,
                                      const std::vector<unsigned char>& pixelData)
{
    std::string path = std::string(TEST_DATA_PATH) + "/" + filename;

    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return {};

    // 18-byte TGA header - all fields zero-initialised then filled selectively.
    unsigned char header[18];
    memset(header, 0, sizeof(header));

    header[2] = imageType;
    // bytes 3-11: color-map spec (5 bytes) and origin (4 bytes) - all zero
    header[12] = static_cast<unsigned char>(width & 0xFF);
    header[13] = static_cast<unsigned char>((width >> 8) & 0xFF);
    header[14] = static_cast<unsigned char>(height & 0xFF);
    header[15] = static_cast<unsigned char>((height >> 8) & 0xFF);
    header[16] = colorDepth;
    header[17] = 0x20;  // image descriptor: top-to-bottom scan order

    fwrite(header, 1, sizeof(header), f);

    if (!pixelData.empty())
        fwrite(pixelData.data(), 1, pixelData.size(), f);

    fclose(f);
    return path;
}

// ---------------------------------------------------------------------------
// Security metadata logged via Catch2 INFO - visible with -s (--success) or
// on any test failure in the JUnit/HTML report Message column.
// ---------------------------------------------------------------------------
#define TGA_SECURITY_INFO()                \
    INFO("Test     : Out-of-Bounds Read"); \
    INFO("Status   : FIXED");              \
    INFO("Component: libImage_TGA -- applications/_plugins/cimage/tga/tga.cpp")

// ---------------------------------------------------------------------------
// Security context test - always visible in HTML report and with -s flag.
// This is not a functional test; it records the CVE/CVSS metadata so that
// every test run (pass or fail) includes the security context in the log.
// ---------------------------------------------------------------------------
TEST_CASE("TGARLESecurity_Metadata", "[TGA][Security]")
{
    SUCCEED("Out-of-Bounds Read | Status: FIXED");
}

// ===========================================================================
// Group 1  -  LoadTGA_ARGB8888_RLE  (imageType=10, colorDepth=32)
//
// PSIRT test cases 1-4 from Table 7 of the assessment document.
// ===========================================================================

TEST_CASE("TGARLESecurity_ARGB8888_EmptyPixelData", "[TGA][Security]")
{
    TGA_SECURITY_INFO();
    // Test 1: Zero bytes of pixel data after the 18-byte header.
    // pTempPtr immediately equals pTempEnd, so the RLE-header-byte guard
    // fires on the very first iteration of the decode loop.
    const std::string path = createMalformedTGA("sec_tga_argb32_rle_empty.tga", TGA_IMAGETYPE_RLE_RGB, 32, 4, 4, {});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_ARGB8888_RLEHeaderOnly_NoColourBytes", "[TGA][Security]")
{
    // Test 2: One RLE run-length packet header byte (0x80 = run of 1 pixel)
    // with no colour bytes following.  The decoder needs 4 bytes (BGRA) but
    // only 0 remain after consuming the header byte.
    const std::string path = createMalformedTGA("sec_tga_argb32_rle_hdronly.tga", TGA_IMAGETYPE_RLE_RGB, 32, 4, 4, {0x80});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_ARGB8888_RLEHeader_TwoColourBytes", "[TGA][Security]")
{
    // Test 3: RLE run-length packet header (0x80) followed by only 2 of the
    // required 4 colour bytes (BGRA needs 4).  The 4-byte colour guard
    // (pTempPtr + 4 > pTempEnd) must fire before the partial read.
    const std::string path = createMalformedTGA("sec_tga_argb32_rle_partial.tga", TGA_IMAGETYPE_RLE_RGB, 32, 4, 4, {0x80, 0xAA, 0xBB});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_ARGB8888_RawBlock_TruncatedPixelData", "[TGA][Security]")
{
    // Test 4: Raw block packet header (0x01 = 2 raw pixels, requiring 8 bytes)
    // followed by exactly 4 bytes (one complete pixel).  The per-pixel guard
    // (pTempPtr + 4 > pTempEnd) fires when the decoder attempts to read the
    // second pixel.
    const std::string path = createMalformedTGA("sec_tga_argb32_raw_trunc.tga", TGA_IMAGETYPE_RLE_RGB, 32, 4, 4, {0x01, 0xAA, 0xBB, 0xCC, 0xDD});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 2  -  LoadTGA_RGB888_RLE  (imageType=10, colorDepth=24)
//
// PSIRT test cases 5-8 from Table 7 of the assessment document.
// ===========================================================================

TEST_CASE("TGARLESecurity_RGB888_EmptyPixelData", "[TGA][Security]")
{
    TGA_SECURITY_INFO();
    // Test 5: 24-bit RLE - zero bytes of pixel data after the header.
    const std::string path = createMalformedTGA("sec_tga_rgb24_rle_empty.tga", TGA_IMAGETYPE_RLE_RGB, 24, 4, 4, {});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_RGB888_RLEHeaderOnly_NoColourBytes", "[TGA][Security]")
{
    // Test 6: RLE run-length packet header (0x80) with no colour bytes.
    // The decoder needs 3 bytes (BGR) but only 0 remain.
    const std::string path = createMalformedTGA("sec_tga_rgb24_rle_hdronly.tga", TGA_IMAGETYPE_RLE_RGB, 24, 4, 4, {0x80});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_RGB888_RLEHeader_OneColourByte", "[TGA][Security]")
{
    // Test 7: RLE run-length packet header (0x80) followed by only 1 of the
    // required 3 colour bytes (BGR needs 3).
    const std::string path = createMalformedTGA("sec_tga_rgb24_rle_onebyte.tga", TGA_IMAGETYPE_RLE_RGB, 24, 4, 4, {0x80, 0xAA});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_RGB888_RawBlock_TruncatedPixelData", "[TGA][Security]")
{
    // Test 8: Raw block packet header (0x01 = 2 raw pixels, requiring 6 bytes)
    // followed by 3 bytes (one complete 24-bit pixel, second pixel absent).
    // The per-pixel guard fires when the decoder attempts to read the second pixel.
    const std::string path = createMalformedTGA("sec_tga_rgb24_raw_trunc.tga", TGA_IMAGETYPE_RLE_RGB, 24, 4, 4, {0x01, 0xAA, 0xBB, 0xCC});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 3  -  LoadTGA_G8_RLE  (imageType=11, colorDepth=8)
//
// PSIRT test cases 9-12 from Table 7 of the assessment document.
// ===========================================================================

TEST_CASE("TGARLESecurity_G8_EmptyPixelData", "[TGA][Security]")
{
    TGA_SECURITY_INFO();
    // Test 9: 8-bit greyscale RLE - zero bytes of pixel data after the header.
    const std::string path = createMalformedTGA("sec_tga_g8_rle_empty.tga", TGA_IMAGETYPE_RLE_GREY, 8, 4, 4, {});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_G8_RLEHeaderOnly_NoGreyByte", "[TGA][Security]")
{
    // Test 10: 8-bit greyscale RLE run-length packet header (0x80) with no
    // following grey byte.  The 1-byte colour guard fires before the read.
    const std::string path = createMalformedTGA("sec_tga_g8_rle_hdronly.tga", TGA_IMAGETYPE_RLE_GREY, 8, 4, 4, {0x80});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_G8_RawBlockHeader_NoGreyBytes", "[TGA][Security]")
{
    // Test 11: 8-bit greyscale raw block packet header (0x01 = 2 raw pixels)
    // with no grey bytes following.  The per-pixel guard fires on the first
    // pixel read attempt inside the raw-block loop.
    const std::string path = createMalformedTGA("sec_tga_g8_raw_nopixels.tga", TGA_IMAGETYPE_RLE_GREY, 8, 4, 4, {0x01});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_G8_RawBlock_PartialGreyBytes", "[TGA][Security]")
{
    // Test 12: 8-bit greyscale raw block packet header (0x03 = 4 raw pixels)
    // followed by only 2 grey bytes.  The first 2 pixels decode successfully;
    // the per-pixel guard fires when the decoder attempts to read the 3rd pixel.
    // This exercises the per-iteration bounds check added to the raw-block loop
    // as part of the Defence 4 (logic-bug fix) in LoadTGA_G8_RLE.
    const std::string path = createMalformedTGA("sec_tga_g8_raw_partial.tga", TGA_IMAGETYPE_RLE_GREY, 8, 4, 4, {0x03, 0x55, 0x66});

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error != CMP_OK);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Happy path - regression guard
//
// Verifies that the bounds-check sentinels added by the fix do NOT reject
// well-formed TGA files.  If this test fails, a guard is triggering on valid
// data and normal texture loading is broken.
// ===========================================================================

TEST_CASE("TGARLESecurity_HappyPath_ValidARGB32RLE", "[TGA][Security]")
{
    TGA_SECURITY_INFO();
    // Regression guard: a well-formed 2x2 ARGB8888 RLE TGA must load without
    // error after the fix is applied.
    //
    // Packet state (type, remaining count) persists across scanlines in
    // LoadTGA_ARGB8888_RLE, so this is encoded as two independent raw packets
    // (one per row) purely as a readable/simple valid encoding -- not because
    // a packet is required to start at a row boundary. See
    // TGARLESecurity_ARGB8888_RawPacket_SpansScanlines below for a packet that
    // deliberately crosses the row boundary.  Packet byte 0x01 = count-1 = 2
    // raw pixels.  Total payload: 2 rows x (1 + 2x4) = 18 bytes.
    const std::vector<unsigned char> pixelData = {
        0x01,  // row 0: raw packet, 2 pixels
        0xFF,
        0x00,
        0x00,
        0xFF,  // pixel 0: B=255, G=0,   R=0,   A=255 (blue)
        0x00,
        0xFF,
        0x00,
        0xFF,  // pixel 1: B=0,   G=255, R=0,   A=255 (green)
        0x01,  // row 1: raw packet, 2 pixels
        0x00,
        0x00,
        0xFF,
        0xFF,  // pixel 2: B=0,   G=0,   R=255, A=255 (red)
        0x80,
        0x80,
        0x80,
        0xFF  // pixel 3: B=128, G=128, R=128, A=255 (grey)
    };

    const std::string path = createMalformedTGA("sec_tga_argb32_rle_valid.tga", TGA_IMAGETYPE_RLE_RGB, 32, 2, 2, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error == CMP_OK);

    if (error == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_HappyPath_ValidRGB24RLE", "[TGA][Security]")
{
    // Regression guard: well-formed 2x2 RGB888 RLE TGA must load successfully.
    //
    // Encoding: one raw packet (header byte 0x03 = 4 pixels) followed by
    // four 3-byte BGR samples totalling 13 bytes of pixel data.
    const std::vector<unsigned char> pixelData = {
        0x03,  // raw packet: 4 pixels
        0xFF,
        0x00,
        0x00,  // pixel 0: B=255, G=0,   R=0   (blue)
        0x00,
        0xFF,
        0x00,  // pixel 1: B=0,   G=255, R=0   (green)
        0x00,
        0x00,
        0xFF,  // pixel 2: B=0,   G=0,   R=255 (red)
        0x80,
        0x80,
        0x80  // pixel 3: grey
    };

    const std::string path = createMalformedTGA("sec_tga_rgb24_rle_valid.tga", TGA_IMAGETYPE_RLE_RGB, 24, 2, 2, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    CHECK(error == CMP_OK);

    if (error == CMP_OK)
        CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_HappyPath_ValidG8RLE", "[TGA][Security]")
{
    // Regression guard: well-formed 2x2 greyscale RLE TGA must load successfully
    // AND decode to the correct per-sample pixel values. A CMP_OK-only check
    // here would still pass if the raw-packet decode regressed (e.g. back to
    // replicating the packet's first grey byte across every sample), so this
    // pins the actual decoded output -- see Group 5 below for the dedicated
    // raw-packet-vs-run-length-packet decode tests this guards against.
    //
    // Single raw packet (header 0x03 = count-1 => 4 samples) spanning both
    // rows of the 2x2 image -- a legal TGA encoding since packets are not
    // required to start or end at a scanline boundary.
    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    const std::vector<unsigned char> pixelData = {
        0x03,  // raw packet: 4 samples
        0x40,  // pixel (0,0): dark grey
        0x80,  // pixel (1,0): mid grey
        0xA0,  // pixel (0,1): grey
        0xC0   // pixel (1,1): light grey
    };

    const std::string path = createMalformedTGA("sec_tga_g8_rle_valid.tga", TGA_IMAGETYPE_RLE_GREY, 8, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);
    REQUIRE(mipSet.dwDataSize >= static_cast<CMP_DWORD>(kWidth * kHeight * 4));

    // header[17] = 0x20 (top-to-bottom) in createMalformedTGA, so file row order
    // matches MipSet row order and this is a direct row-major comparison.
    const unsigned char expectedGrey[kWidth * kHeight] = {0x40, 0x80, 0xA0, 0xC0};

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        CHECK(px[0] == expectedGrey[i]);
        CHECK(px[1] == expectedGrey[i]);
        CHECK(px[2] == expectedGrey[i]);
        CHECK(px[3] == 0xFF);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 5  -  Decoded-sample verification for LoadTGA_G8_RLE
//
// The Defence 4 logic fix in LoadTGA_G8_RLE changed the raw-packet branch so
// that it consumes ONE grey byte per sample (correct TGA semantics) instead of
// replicating the packet's first grey byte across every sample in the packet.
// That is a deliberate behavioural change: greyscale RLE TGAs containing raw
// packets previously decoded to the wrong pixel values.
//
// These tests pin the decoded output so the fix cannot silently regress in
// either direction:
//   * a raw packet must yield DISTINCT samples (one source byte each);
//   * an RLE packet must still yield REPLICATED samples.
//
// LoadTGA_G8_RLE reads a fresh packet header at the start of every row, so the
// test data supplies one packet per row. header[17] = 0x20 (top-to-bottom), so
// file row order matches MipSet row order. Output is RGBA8888 with the grey
// value replicated into R, G and B and alpha forced to 0xFF.
// ===========================================================================

TEST_CASE("TGARLESecurity_G8_RawPacket_DecodesDistinctSamples", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    const std::vector<unsigned char> pixelData = {
        // Row 0: RAW packet, count-1 = 1 => 2 samples, each with its own byte.
        0x01,
        0x40,  // pixel (0,0): dark grey
        0x80,  // pixel (1,0): mid grey
        // Row 1: RLE packet, 0x81 => nRepeat = 0x81 - 0x7F = 2 replicated samples.
        0x81,
        0xC0  // pixels (0,1) and (1,1): light grey
    };

    const std::string path = createMalformedTGA("sec_tga_g8_rle_values.tga", TGA_IMAGETYPE_RLE_GREY, 8, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);
    REQUIRE(mipSet.dwDataSize >= static_cast<CMP_DWORD>(kWidth * kHeight * 4));

    // Expected grey level for each pixel, in row-major order.
    const unsigned char expectedGrey[kWidth * kHeight] = {0x40, 0x80, 0xC0, 0xC0};

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        CHECK(px[0] == expectedGrey[i]);
        CHECK(px[1] == expectedGrey[i]);
        CHECK(px[2] == expectedGrey[i]);
        CHECK(px[3] == 0xFF);
    }

    // Explicit guard against the pre-fix behaviour, where the raw packet
    // replicated its first grey byte and pixel 1 would have decoded as 0x40.
    CHECK(mipSet.pData[4] != mipSet.pData[0]);

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

// ===========================================================================
// Group 6  -  Packet state must persist across scanline boundaries
//
// Per the TGA spec, an RLE packet is not required to start or end at a
// scanline boundary -- its remaining count (and, for a run-length packet,
// the repeated sample value) must carry over into the next row.  Before the
// fix, LoadTGA_ARGB8888_RLE and LoadTGA_G8_RLE re-read a fresh packet header
// at the start of every row, silently discarding any remaining count from a
// packet that spanned the row boundary and misinterpreting the next pixel
// byte as a new header.  LoadTGA_RGB888_RLE persisted the header/count
// correctly but re-read the run-length packet's colour bytes from the
// stream on every row it crossed, corrupting the decode and desynchronising
// the packet stream.
//
// These tests use a single packet whose count exceeds the image width, so
// the packet necessarily spans both rows of a 2x2 image, and assert the
// fully decoded pixel values (not just CMP_OK) for both raw and run-length
// packet types.
// ===========================================================================

TEST_CASE("TGARLESecurity_G8_RawPacket_SpansScanlines", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    // Single raw packet, count-1 = 3 => 4 distinct samples, one per pixel,
    // spanning both rows of the 2x2 image.
    const std::vector<unsigned char> pixelData = {0x03, 0x10, 0x20, 0x30, 0x40};

    const std::string path = createMalformedTGA("sec_tga_g8_raw_spans_rows.tga", TGA_IMAGETYPE_RLE_GREY, 8, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);

    const unsigned char expectedGrey[kWidth * kHeight] = {0x10, 0x20, 0x30, 0x40};

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        CHECK(px[0] == expectedGrey[i]);
        CHECK(px[1] == expectedGrey[i]);
        CHECK(px[2] == expectedGrey[i]);
        CHECK(px[3] == 0xFF);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_G8_RunLengthPacket_SpansScanlines", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    // Single run-length packet: 0x83 = 0x80 | (count-1 = 3) => 4 replicated
    // samples of the single following grey byte, spanning both rows.
    const std::vector<unsigned char> pixelData = {0x83, 0xAA};

    const std::string path = createMalformedTGA("sec_tga_g8_rle_spans_rows.tga", TGA_IMAGETYPE_RLE_GREY, 8, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        CHECK(px[0] == 0xAA);
        CHECK(px[1] == 0xAA);
        CHECK(px[2] == 0xAA);
        CHECK(px[3] == 0xFF);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_ARGB8888_RawPacket_SpansScanlines", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    // Single raw packet, count-1 = 3 => 4 distinct BGRA samples, one per
    // pixel, spanning both rows of the 2x2 image.
    const std::vector<unsigned char> pixelData = {
        0x03,
        0xFF,
        0x00,
        0x00,
        0xFF,  // pixel (0,0): B=255,G=0,  R=0,  A=255 (blue)
        0x00,
        0xFF,
        0x00,
        0xFF,  // pixel (1,0): B=0,  G=255,R=0,  A=255 (green)
        0x00,
        0x00,
        0xFF,
        0xFF,  // pixel (0,1): B=0,  G=0,  R=255,A=255 (red)
        0x80,
        0x80,
        0x80,
        0x7F  // pixel (1,1): B=128,G=128,R=128,A=127 (grey, translucent)
    };

    const std::string path = createMalformedTGA("sec_tga_argb32_raw_spans_rows.tga", TGA_IMAGETYPE_RLE_RGB, 32, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);

    const CMP_BYTE expectedRGBA[kWidth * kHeight][4] = {
        {0x00, 0x00, 0xFF, 0xFF},
        {0x00, 0xFF, 0x00, 0xFF},
        {0xFF, 0x00, 0x00, 0xFF},
        {0x80, 0x80, 0x80, 0x7F},
    };

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        CHECK(px[0] == expectedRGBA[i][0]);
        CHECK(px[1] == expectedRGBA[i][1]);
        CHECK(px[2] == expectedRGBA[i][2]);
        CHECK(px[3] == expectedRGBA[i][3]);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_ARGB8888_RunLengthPacket_SpansScanlines", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    // Single run-length packet: 0x83 = 0x80 | (count-1 = 3) => 4 replicated
    // BGRA samples of the single following pixel, spanning both rows.
    const std::vector<unsigned char> pixelData = {0x83, 0x11, 0x22, 0x33, 0x44};

    const std::string path = createMalformedTGA("sec_tga_argb32_rle_spans_rows.tga", TGA_IMAGETYPE_RLE_RGB, 32, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        // Source bytes are B,G,R,A (0x11,0x22,0x33,0x44); decoded output order is R,G,B,A.
        CHECK(px[0] == 0x33);
        CHECK(px[1] == 0x22);
        CHECK(px[2] == 0x11);
        CHECK(px[3] == 0x44);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}

TEST_CASE("TGARLESecurity_RGB888_RunLengthPacket_SpansScanlines", "[TGA][Security]")
{
    TGA_SECURITY_INFO();

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    // Single run-length packet: 0x83 = 0x80 | (count-1 = 3) => 4 replicated
    // BGR samples of the single following pixel, spanning both rows.  This
    // pins the fix for the colour bytes being re-read from the stream when a
    // run-length packet is re-entered on a new row.
    const std::vector<unsigned char> pixelData = {0x83, 0x55, 0x66, 0x77};

    const std::string path = createMalformedTGA("sec_tga_rgb24_rle_spans_rows.tga", TGA_IMAGETYPE_RLE_RGB, 24, kWidth, kHeight, pixelData);

    REQUIRE_FALSE(path.empty());

    CMP_MipSet mipSet = {};
    CMP_ERROR  error  = CMP_LoadTexture(path.c_str(), &mipSet);

    REQUIRE(error == CMP_OK);
    REQUIRE(mipSet.pData != nullptr);
    REQUIRE(mipSet.m_nWidth == kWidth);
    REQUIRE(mipSet.m_nHeight == kHeight);

    for (int i = 0; i < kWidth * kHeight; ++i)
    {
        INFO("pixel index " << i);
        const CMP_BYTE* px = mipSet.pData + (static_cast<size_t>(i) * 4);
        // Source bytes are B,G,R (0x55,0x66,0x77); decoded output order is R,G,B,A(=0xFF).
        CHECK(px[0] == 0x77);
        CHECK(px[1] == 0x66);
        CHECK(px[2] == 0x55);
        CHECK(px[3] == 0xFF);
    }

    CMP_FreeMipSet(&mipSet);
    std::remove(path.c_str());
}
