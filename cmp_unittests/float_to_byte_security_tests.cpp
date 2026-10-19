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

#include "single_include/catch2/catch.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include "compressonator.h"
#include "format_conversion.h"

// ---------------------------------------------------------------------------
// Security metadata macro — logged via Catch2 INFO, visible with -s or on
// failure in the JUnit/HTML report Message column.
// ---------------------------------------------------------------------------
#define FLOATTOBYTE_SECURITY_INFO()                                     \
    INFO("Test     : Out-of-Bounds Write / Improper Input Validation"); \
    INFO("Status   : FIXED");                                           \
    INFO("Component: format_conversion.cpp -- applications/_plugins/common/format_conversion.cpp")

// ---------------------------------------------------------------------------
// Per-channel-format input bytes per pixel (matches the constants used in
// the production bounds check so that tests stay in sync with the fix).
// ---------------------------------------------------------------------------
static const CMP_DWORD BYTES_PER_PIXEL_FLOAT16    = 4u * 2u;  // 4 channels × sizeof(CMP_HALF)
static const CMP_DWORD BYTES_PER_PIXEL_FLOAT32    = 4u * 4u;  // 4 channels × sizeof(CMP_FLOAT)
static const CMP_DWORD BYTES_PER_PIXEL_FLOAT9995E = 1u * 4u;  // 1 packed CMP_DWORD per pixel
static const CMP_DWORD BYTES_PER_PIXEL_OUT        = 4u;       // RGBA8888: always 4 bytes/pixel

// ===========================================================================
// Security metadata record — always passes, logs CVE context in every report.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_Metadata", "[FloatToByte][Security]")
{
    SUCCEED("Out-of-Bounds Write Status: FIXED");
}

// ===========================================================================
// Group 1 — Inflated-dimension attack (core CVE scenario)
//
// A DDS file header claiming large dimensions (e.g. 100×100 = 10 000 pixels)
// but backed by only 4 pixels of real data was the attack vector.  The
// pre-fix code iterated all 10 000 positions, reading past the end of the
// input buffer and writing 40 000 bytes into a 16-byte output buffer.
//
// After the fix the input-buffer size check fires first, returning
// CMP_ERR_INVALID_SOURCE_TEXTURE before any pixel is processed.
// ===========================================================================

TEST_CASE("FloatToByteSecurity_CF_Float16_InflatedDimensions_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Simulates DDS header claiming 100x100 (10000 pixels) but only 4 pixels of CF_Float16 data supplied.");
    INFO("Pre-fix: reads 80000 bytes from a 32-byte input; writes 40000 bytes into a 16-byte output.");
    INFO("Post-fix: CMP_ERR_INVALID_SOURCE_TEXTURE returned before the pixel loop.");

    const CMP_DWORD realPixels    = 4u;
    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;  // claimed 10 000 pixels vs 4 real

    std::vector<uint8_t> inBuf(realPixels * BYTES_PER_PIXEL_FLOAT16, 0);
    std::vector<uint8_t> outBuf(realPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result != CMP_OK);
}

TEST_CASE("FloatToByteSecurity_CF_Float32_InflatedDimensions_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Simulates DDS header claiming 100x100 (10000 pixels) but only 4 pixels of CF_Float32 data supplied.");

    const CMP_DWORD realPixels    = 4u;
    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;

    std::vector<uint8_t> inBuf(realPixels * BYTES_PER_PIXEL_FLOAT32, 0);
    std::vector<uint8_t> outBuf(realPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float32,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result != CMP_OK);
}

TEST_CASE("FloatToByteSecurity_CF_Float9995E_InflatedDimensions_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Simulates DDS header claiming 100x100 (10000 pixels) but only 4 pixels of CF_Float9995E data supplied.");

    const CMP_DWORD realPixels    = 4u;
    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;

    std::vector<uint8_t> inBuf(realPixels * BYTES_PER_PIXEL_FLOAT9995E, 0);
    std::vector<uint8_t> outBuf(realPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float9995E,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result != CMP_OK);
}

// ===========================================================================
// Group 2 — Output-buffer-only undersized
//
// Input buffer is allocated large enough for the claimed dimensions; only the
// output buffer is undersized.
// Pre-fix: 10 000 × 4 = 40 000 bytes written into a 16-byte output buffer.
// Post-fix: CMP_ERR_INVALID_DEST_TEXTURE.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_CF_Float16_OutputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Input buffer is sized for the full 100x100 claim; output is only 4 pixels.");
    INFO("Pre-fix: writes 40000 bytes into a 16-byte output buffer.");
    INFO("Post-fix: CMP_ERR_INVALID_DEST_TEXTURE.");

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    // Input is deliberately large enough so the input check passes.
    std::vector<uint8_t> inBuf(claimedPixels * BYTES_PER_PIXEL_FLOAT16, 0);
    // Output holds only 4 pixels — intentionally undersized.
    std::vector<uint8_t> outBuf(4u * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_DEST_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float32_OutputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Input buffer is sized for the full 100x100 claim; output is only 4 pixels.");

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    std::vector<uint8_t> inBuf(claimedPixels * BYTES_PER_PIXEL_FLOAT32, 0);
    std::vector<uint8_t> outBuf(4u * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float32,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_DEST_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float9995E_OutputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    std::vector<uint8_t> inBuf(claimedPixels * BYTES_PER_PIXEL_FLOAT9995E, 0);
    std::vector<uint8_t> outBuf(4u * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float9995E,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_DEST_TEXTURE);
}

// ===========================================================================
// Group 3 — Input-buffer-only undersized
//
// Output buffer is large enough for the claimed dimensions; only the input
// buffer is undersized.  Exercises the OOB-read path independently.
//
// Pre-fix: reads past the end of the input buffer for every pixel beyond the
// few real pixels supplied.
// Post-fix: CMP_ERR_INVALID_SOURCE_TEXTURE.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_CF_Float16_InputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("Output buffer sized for the full 100x100 claim; input holds only 4 pixels.");
    INFO("Pre-fix: reads past the end of the 32-byte input buffer.");
    INFO("Post-fix: CMP_ERR_INVALID_SOURCE_TEXTURE.");

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    // Input is intentionally small — only 4 pixels.
    std::vector<uint8_t> inBuf(4u * BYTES_PER_PIXEL_FLOAT16, 0);
    // Output is large enough for the full claimed size.
    std::vector<uint8_t> outBuf(claimedPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float32_InputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    std::vector<uint8_t> inBuf(4u * BYTES_PER_PIXEL_FLOAT32, 0);
    std::vector<uint8_t> outBuf(claimedPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float32,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float9995E_InputBufferTooSmall_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();

    const CMP_DWORD claimedWidth  = 100u;
    const CMP_DWORD claimedHeight = 100u;
    const CMP_DWORD claimedPixels = claimedWidth * claimedHeight;

    std::vector<uint8_t> inBuf(4u * BYTES_PER_PIXEL_FLOAT9995E, 0);
    std::vector<uint8_t> outBuf(claimedPixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float9995E,
                                   claimedWidth,
                                   claimedHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 4 — Integer overflow in width * height
//
// Choosing width and height so that width * height wraps a CMP_DWORD (uint32)
// could bypass a naive pixel-count check.  The fix catches this before any
// multiplication, returning CMP_ERR_INVALID_SOURCE_TEXTURE.
//
// Example: width=0x10000 (65536), height=0x10001 (65537) gives a product of
// 0x100010000 which truncates to 0x10000 in a 32-bit multiply — far smaller
// than the true pixel count.  The pre-fix code would then allocate a
// ~262 KB output buffer believing it holds 65536 pixels, while the loop
// itself ran 65537 × 65536 ≈ 4 billion iterations.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_WidthHeightOverflow_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("width=0x10000 height=0x10001: product overflows a uint32, bypassing a naive pixel-count check.");
    INFO("Post-fix: CMP_ERR_INVALID_SOURCE_TEXTURE returned from the overflow guard.");

    const CMP_DWORD overflowWidth  = 0x10000u;  // 65536
    const CMP_DWORD overflowHeight = 0x10001u;  // 65537 — wraps in 32-bit multiply

    // Buffers are arbitrary small sizes; the overflow check fires before they
    // are examined.
    std::vector<uint8_t> inBuf(64, 0);
    std::vector<uint8_t> outBuf(64, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   overflowWidth,
                                   overflowHeight,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_MaxWidthMaxHeight_Overflow_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("UINT32_MAX x UINT32_MAX: worst-case overflow, must be caught.");

    std::vector<uint8_t> inBuf(64, 0);
    std::vector<uint8_t> outBuf(64, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float32,
                                   std::numeric_limits<CMP_DWORD>::max(),
                                   std::numeric_limits<CMP_DWORD>::max(),
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 5 — Unrecognised channel format
//
// Passing a non-float channel format (e.g. CF_8bit) must not cause undefined
// behaviour.  The format guard added by the fix returns
// CMP_ERR_INVALID_SOURCE_TEXTURE before the pixel loop.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_UnknownChannelFormat_Rejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();
    INFO("CF_8bit is not a float format; the format guard must reject it.");

    std::vector<uint8_t> inBuf(64, 0);
    std::vector<uint8_t> outBuf(64, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_8bit,
                                   4u,
                                   4u,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 6 — Happy-path: valid inputs must still succeed
//
// These tests confirm that the bounds-checking fix does not reject legitimate
// inputs.  Each uses exact-size buffers computed from the claimed dimensions
// so neither the input nor the output guard fires.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_CF_Float16_ValidBuffers_Succeeds", "[FloatToByte][Security]")
{
    const CMP_DWORD width  = 2u;
    const CMP_DWORD height = 2u;
    const CMP_DWORD pixels = width * height;  // 4

    // 4 pixels × 8 bytes/pixel = 32 bytes of zero-initialised half-float data.
    // The 16-bit zero pattern is the IEEE 754 representation of +0.0 in half precision.
    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT16, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_OK);
    // Output must be exactly pixels * 4 bytes of RGBA8888 data.
    CHECK(outBuf.size() == pixels * 4u);
}

TEST_CASE("FloatToByteSecurity_CF_Float32_ValidBuffers_Succeeds", "[FloatToByte][Security]")
{
    const CMP_DWORD width  = 2u;
    const CMP_DWORD height = 2u;
    const CMP_DWORD pixels = width * height;

    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT32, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float32,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_OK);
}

TEST_CASE("FloatToByteSecurity_CF_Float9995E_ValidBuffers_Succeeds", "[FloatToByte][Security]")
{
    const CMP_DWORD width  = 2u;
    const CMP_DWORD height = 2u;
    const CMP_DWORD pixels = width * height;

    // A R9G9B9E5 pixel with all mantissa and exponent fields zero represents
    // the tuple (0.0, 0.0, 0.0) — safe, well-defined input.
    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT9995E, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float9995E,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_OK);
}

TEST_CASE("FloatToByteSecurity_ZeroDimension_Succeeds", "[FloatToByte][Security]")
{
    // width == 0 → numPixels == 0 → no pixel loop, no buffer access beyond
    // the initial non-null check.  A non-null 1-byte placeholder satisfies the
    // assert(outBuffer) / assert(inBuffer) pre-conditions.
    uint8_t     inPlaceholder  = 0;
    uint8_t     outPlaceholder = 0;
    FloatParams params;

    CMP_ERROR result = FloatToByte(&outPlaceholder, 0u, reinterpret_cast<CMP_FLOAT*>(&inPlaceholder), 0u, CF_Float16, 0u, 4u, &params);

    CHECK(result == CMP_OK);
}

// ===========================================================================
// Group 7 — Output exactly matches declared pixel count (boundary condition)
//
// Verifies that an exact-fit buffer (numPixels == bufferSize / bytesPerPixel)
// is accepted rather than falsely rejected by an off-by-one in the guard.
// ===========================================================================
TEST_CASE("FloatToByteSecurity_CF_Float16_ExactFitOutput_Accepted", "[FloatToByte][Security]")
{
    // Exact fit: outBufferSize / 4 == numPixels (the boundary must not reject).
    const CMP_DWORD width  = 4u;
    const CMP_DWORD height = 4u;
    const CMP_DWORD pixels = width * height;  // 16

    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT16, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);  // exactly 64 bytes

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_OK);
}

TEST_CASE("FloatToByteSecurity_CF_Float16_OneByteTooSmallOutput_Rejected", "[FloatToByte][Security]")
{
    // One byte less than exact fit: the output guard must reject.
    const CMP_DWORD width  = 4u;
    const CMP_DWORD height = 4u;
    const CMP_DWORD pixels = width * height;  // 16 pixels → needs 64 output bytes

    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT16, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT - 1u, 0);  // 63 bytes — one short

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_DEST_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float16_OneByteTooSmallInput_Rejected", "[FloatToByte][Security]")
{
    // One byte less than exact fit for input: the input guard must reject.
    const CMP_DWORD width  = 4u;
    const CMP_DWORD height = 4u;
    const CMP_DWORD pixels = width * height;  // 16 pixels → needs 128 input bytes (CF_Float16)

    std::vector<uint8_t> inBuf(pixels * BYTES_PER_PIXEL_FLOAT16 - 1u, 0);  // 127 bytes — one short
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 8 — Investigation: 1- and 2-channel float sources (R_16F / RG_16F)
//
// GetChannelFormat() maps CMP_FORMAT_R_16F and CMP_FORMAT_RG_16F to the same
// CF_Float16 tag as 4-channel RGBA_16F (applications/_plugins/common/atiformats.cpp).
// AllocateMipLevelData() sizes m_dwLinearSize/dwDataSize using the real channel
// count (TDT_R => 1x, TDT_RG => 2x — cmp_framework/common/cmp_mips.cpp), so a
// genuine single/dual-channel float16 mip has far fewer than 8 bytes/pixel.
//
// FloatToByte's bytesPerPixelIn for CF_Float16 is hardcoded to 8 (4 channels),
// so these buffers are (correctly, per the current single-channel-format API)
// reported as CMP_ERR_INVALID_SOURCE_TEXTURE: the fixed-size check cannot tell
// CF_Float16-as-RGBA from CF_Float16-as-R apart. The per-pixel loop itself also
// unconditionally reads 4 interleaved half-floats for CF_Float16 regardless of
// real channel count, so this was already unsafe (OOB read) before the bounds
// check existed -- the check turns a silent OOB read into a safe rejection
// rather than introducing a new failure mode.
//
// Investigated call sites: mipstoqimage.cpp (x2) explicitly gate on
// tmpMipSet->m_TextureDataType == TDT_ARGB/TDT_XRGB before calling FloatToByte,
// and route single-channel (TDT_R, CF_16bit/integer) data through a completely
// separate non-float code path. acimageview.cpp::UpdatePixmapImage() has no
// such format gate of its own, but is only reachable from the EXR/HDR tone-map
// slider handlers (onExrExposure/Defog/KneeLow/KneeHigh/GammaChanged), which are
// only presented for 4-channel float images. So no shipped GUI/CLI call site
// currently feeds a 1- or 2-channel CF_Float16/CF_Float32 buffer into FloatToByte.
// This is a pre-existing limitation of the single-channel-format API, not a
// regression introduced by the security fix; a real fix would need FloatToByte
// to take an explicit channel count (e.g. derived from TextureDataType).
// ===========================================================================
TEST_CASE("FloatToByteSecurity_CF_Float16_SingleChannelR16F_CurrentlyRejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();

    INFO("Simulates a genuine 1-channel R_16F mip (TDT_R): dwDataSize = width*height*2 bytes.");
    INFO("bytesPerPixelIn assumes 4 channels (8 bytes/pixel), so this buffer looks undersized.");

    const CMP_DWORD width  = 4u;
    const CMP_DWORD height = 4u;
    const CMP_DWORD pixels = width * height;

    // Real allocation size for a 1-channel (TDT_R) CF_Float16 mip: 2 bytes/pixel.
    std::vector<uint8_t> inBuf(pixels * 2u, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    // Documents current (safe) behaviour -- see the investigation note above.
    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

TEST_CASE("FloatToByteSecurity_CF_Float16_DualChannelRG16F_CurrentlyRejected", "[FloatToByte][Security]")
{
    FLOATTOBYTE_SECURITY_INFO();

    INFO("Simulates a genuine 2-channel RG_16F mip (TDT_RG): dwDataSize = width*height*4 bytes.");
    INFO("bytesPerPixelIn assumes 4 channels (8 bytes/pixel), so this buffer looks undersized.");

    const CMP_DWORD width  = 4u;
    const CMP_DWORD height = 4u;
    const CMP_DWORD pixels = width * height;

    // Real allocation size for a 2-channel (TDT_RG) CF_Float16 mip: 4 bytes/pixel.
    std::vector<uint8_t> inBuf(pixels * 4u, 0);
    std::vector<uint8_t> outBuf(pixels * BYTES_PER_PIXEL_OUT, 0);

    FloatParams params;
    CMP_ERROR   result = FloatToByte(reinterpret_cast<CMP_BYTE*>(outBuf.data()),
                                   static_cast<CMP_DWORD>(outBuf.size()),
                                   reinterpret_cast<CMP_FLOAT*>(inBuf.data()),
                                   static_cast<CMP_DWORD>(inBuf.size()),
                                   CF_Float16,
                                   width,
                                   height,
                                   &params);

    // Documents current (safe) behaviour -- see the investigation note above.
    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}
