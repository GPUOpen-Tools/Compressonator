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

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <limits>
#include <vector>

#include "compressonator.h"
#include "bc1/bc1.h"

// ---------------------------------------------------------------------------
// Security metadata macro — logged via Catch2 INFO, visible with -s or on
// failure in the JUnit/HTML report Message column.
// ---------------------------------------------------------------------------
#define BC1COMPRESS_SECURITY_INFO()                                                       \
    INFO("Test     : Out-of-Bounds Read / Integer Overflow / Improper Input Validation"); \
    INFO("Status   : FIXED");                                                             \
    INFO("Component: bc1.cpp / compute_base.cpp")

// ---------------------------------------------------------------------------
// MipSet test helper
//
// Creates a CMP_MipSet with a single TT_2D mip level whose pixel buffer is
// exactly bufferBytes in size and whose m_dwLinearSize is set to that value.
// This lets us simulate a mip level where m_dwLinearSize < m_nWidth * m_nHeight * 4
// (the attack scenario) by passing a smaller bufferBytes than the full image would
// require.
//
// Caller must release with FreeMalformedMipSet().
// ---------------------------------------------------------------------------
static void CreateMalformedMipSet(CMP_MipSet* pMipSet, int claimedWidth, int claimedHeight, CMP_DWORD bufferBytes, CMP_BYTE fillValue = 0)
{
    memset(pMipSet, 0, sizeof(CMP_MipSet));

    pMipSet->m_nWidth        = claimedWidth;
    pMipSet->m_nHeight       = claimedHeight;
    pMipSet->dwWidth         = static_cast<CMP_DWORD>(claimedWidth);
    pMipSet->dwHeight        = static_cast<CMP_DWORD>(claimedHeight);
    pMipSet->m_nDepth        = 1;
    pMipSet->m_nMipLevels    = 1;
    pMipSet->m_nMaxMipLevels = 1;

    pMipSet->m_format          = CMP_FORMAT_RGBA_8888;
    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_TextureType     = TT_2D;

    pMipSet->m_nBlockWidth  = 4;
    pMipSet->m_nBlockHeight = 4;
    pMipSet->m_nBlockDepth  = 1;

    // Allocate the mip-level table (array of CMP_MipLevel* pointers).
    pMipSet->m_pMipLevelTable = reinterpret_cast<CMP_MipLevelTable*>(calloc(1, sizeof(CMP_MipLevel*)));
    REQUIRE(pMipSet->m_pMipLevelTable != nullptr);

    // Allocate the single mip level descriptor.
    CMP_MipLevel* pLevel = reinterpret_cast<CMP_MipLevel*>(calloc(1, sizeof(CMP_MipLevel)));
    REQUIRE(pLevel != nullptr);

    pLevel->m_nWidth       = claimedWidth;
    pLevel->m_nHeight      = claimedHeight;
    pLevel->m_dwLinearSize = bufferBytes;  // intentionally undersized for attack tests

    // Allocate the actual pixel buffer (only bufferBytes large).
    if (bufferBytes > 0)
    {
        pLevel->m_pbData = reinterpret_cast<CMP_BYTE*>(calloc(1, bufferBytes));
        REQUIRE(pLevel->m_pbData != nullptr);
        memset(pLevel->m_pbData, fillValue, bufferBytes);
    }

    // Wire the descriptor into the table.
    // m_pMipLevelTable is CMP_MipLevel** in the implementation; the first element
    // is a CMP_MipLevel* pointing to our descriptor.
    reinterpret_cast<CMP_MipLevel**>(pMipSet->m_pMipLevelTable)[0] = pLevel;

    // Set the flat convenience pointers used by some paths.
    pMipSet->pData      = pLevel->m_pbData;
    pMipSet->dwDataSize = bufferBytes;
}

static void FreeMalformedMipSet(CMP_MipSet* pMipSet)
{
    if (!pMipSet || !pMipSet->m_pMipLevelTable)
        return;

    CMP_MipLevel* pLevel = reinterpret_cast<CMP_MipLevel**>(pMipSet->m_pMipLevelTable)[0];
    if (pLevel)
    {
        if (pLevel->m_pbData)
            free(pLevel->m_pbData);
        free(pLevel);
    }
    free(pMipSet->m_pMipLevelTable);
    pMipSet->m_pMipLevelTable = nullptr;
}

// ---------------------------------------------------------------------------
// Helper: invoke CMP_ProcessTexture with BC1 target and return error code.
// The destination MipSet is allocated and freed internally.
// ---------------------------------------------------------------------------
static CMP_ERROR TryCompressBC1(CMP_MipSet* pSrcMipSet)
{
    CMP_MipSet dstMipSet = {};

    KernelOptions opts = {};
    opts.format        = CMP_FORMAT_BC1;
    opts.fquality      = 0.05f;
    opts.threads       = 1;

    CMP_ERROR err = CMP_ProcessTexture(pSrcMipSet, &dstMipSet, opts, nullptr);

    // Free destination if anything was allocated before the early return.
    if (dstMipSet.m_pMipLevelTable)
        CMP_FreeMipSet(&dstMipSet);

    return err;
}

// ===========================================================================
// Mandatory metadata test — always passes, anchors PSIRT record in every run.
// ===========================================================================
TEST_CASE("BC1CompressSecurity_Metadata", "[BC1Compress][Security]")
{
    SUCCEED(
        "Out-of-Bounds Read / Integer Overflow | "
        "Status: FIXED");
}

// ===========================================================================
// Group 1 — Inflated dimensions: claimed 64x64 but only 64 bytes actual data.
// CMP_ProcessTexture must return CMP_ERR_INVALID_SOURCE_TEXTURE, not crash.
// ===========================================================================
TEST_CASE("BC1CompressSecurity_InflatedDimensions_Rejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    // 64×64 ARGB would need 64*64*4 = 16 384 bytes.
    // We supply only 64 bytes — one 4×4 tile — simulating the attack payload.
    CMP_MipSet src = {};
    CreateMalformedMipSet(&src,
                          /*claimedWidth */ 64,
                          /*claimedHeight*/ 64,
                          /*bufferBytes  */ 64u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    CHECK(result != CMP_OK);
    // The fix inserts an early return with this specific error code.
    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 2 — Integer overflow: 65 536×65 536 would wrap uint32_t.
// CMP_ProcessTexture must detect the overflow and return an error.
// ===========================================================================
TEST_CASE("BC1CompressSecurity_IntegerOverflow_Rejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    // Dimensions whose product overflows uint32_t.
    // Actual buffer is a trivial allocation; the overflow guard fires before
    // any buffer-size comparison.
    CMP_MipSet src = {};
    CreateMalformedMipSet(&src,
                          /*claimedWidth */ 65536,
                          /*claimedHeight*/ 65536,
                          /*bufferBytes  */ 64u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    CHECK(result != CMP_OK);
    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 3 — Zero dimension: width or height of zero must be rejected.
// ===========================================================================
TEST_CASE("BC1CompressSecurity_ZeroWidth_Rejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    CMP_MipSet src = {};
    CreateMalformedMipSet(&src,
                          /*claimedWidth */ 0,
                          /*claimedHeight*/ 64,
                          /*bufferBytes  */ 64u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    // Zero dimension is rejected — exact error code varies by framework path
    // (CMP_ERR_INVALID_SOURCE_TEXTURE or CMP_ERR_MEM_ALLOC_FOR_MIPSET).
    CHECK(result != CMP_OK);
}

TEST_CASE("BC1CompressSecurity_ZeroHeight_Rejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    CMP_MipSet src = {};
    CreateMalformedMipSet(&src,
                          /*claimedWidth */ 64,
                          /*claimedHeight*/ 0,
                          /*bufferBytes  */ 64u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    // Zero dimension is rejected — exact error code varies by framework path.
    CHECK(result != CMP_OK);
}

// ===========================================================================
// Group 4 — Valid source: properly-sized buffer must NOT be rejected by the
// new guard.  (The call may still fail for unrelated reasons such as the HPC
// compute plugin not being available in the unit-test environment, but it must
// not return CMP_ERR_INVALID_SOURCE_TEXTURE.)
// ===========================================================================
TEST_CASE("BC1CompressSecurity_ValidSource_NotRejectedByNewGuard", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    // 4×4 ARGB — minimum BC1 block size, buffer exactly right.
    const int       W     = 4;
    const int       H     = 4;
    const CMP_DWORD bytes = static_cast<CMP_DWORD>(W) * H * 4u;

    CMP_MipSet src = {};
    CreateMalformedMipSet(&src, W, H, bytes, /*fill=*/128u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    // The validation guard must NOT fire for a properly-sized source.
    CHECK(result != CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 5 — Boundary: exact-fit buffer (m_dwLinearSize == w * h * 4) is not
// rejected; one-byte-short is.
// ===========================================================================
TEST_CASE("BC1CompressSecurity_BoundaryExact_NotRejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    const int       W     = 4;
    const int       H     = 4;
    const CMP_DWORD exact = static_cast<CMP_DWORD>(W) * H * 4u;  // 64 bytes

    CMP_MipSet src = {};
    CreateMalformedMipSet(&src, W, H, exact, /*fill=*/0u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    CHECK(result != CMP_ERR_INVALID_SOURCE_TEXTURE);
}

TEST_CASE("BC1CompressSecurity_BoundaryOneByteShort_Rejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    const int       W     = 4;
    const int       H     = 4;
    const CMP_DWORD exact = static_cast<CMP_DWORD>(W) * H * 4u;  // 64 bytes

    CMP_MipSet src = {};
    // Pass one byte fewer than the minimum required.
    CreateMalformedMipSet(&src, W, H, exact - 1u, /*fill=*/0u);

    CMP_ERROR result = TryCompressBC1(&src);

    FreeMalformedMipSet(&src);

    CHECK(result != CMP_OK);
    CHECK(result == CMP_ERR_INVALID_SOURCE_TEXTURE);
}

// ===========================================================================
// Group 6 — Regression test for the m_srcStride fix in
// BC1_EncodeClass::CompressBlock() (both the complete-block fast path and the
// partial/edge-block path). A caller-supplied stride describes a source image
// that is a sub-region of a larger, padded canvas (e.g. srcStride/4 pixels per
// row instead of the tightly-packed m_srcWidth). Gathering with the wrong
// per-row advance would read the padding/"trap" columns instead of the real
// image columns, changing the pixels fed to the encoder and therefore the
// compressed output.
//
// This is verified without needing a decoder: encoding the same logical pixel
// content through two different buffer layouts (padded-with-stride vs.
// tightly-packed-with-default-stride) must produce byte-identical compressed
// blocks, since CompressBlock is a deterministic function of the gathered
// pixels alone. Width 6 with 4x4 blocks also exercises both a complete block
// (xBlock 0) and a partial/edge block (xBlock 1, only 2 real columns).
// ===========================================================================
namespace
{
// Distinct, position-dependent pixel value so any wrong gather offset changes the result.
CMP_Vec4uc BC1StrideTestPixel(int x, int y)
{
    return CMP_Vec4uc(static_cast<unsigned char>(10 + x * 30), static_cast<unsigned char>(5 + y * 50), static_cast<unsigned char>(3 + ((x ^ y) * 17)), 255);
}

// Value used to fill the padding columns of the larger canvas; must never be read by a
// correctly-strided gather, and must differ from every value BC1StrideTestPixel() can produce.
CMP_Vec4uc BC1StrideTestSentinel()
{
    return CMP_Vec4uc(0xEE, 0xEE, 0xEE, 0xEE);
}
}  // namespace

TEST_CASE("BC1CompressSecurity_CustomStride_MatchesTightlyPacked", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    const unsigned int logicalWidth  = 6;   // 2 blocks: xBlock 0 complete, xBlock 1 partial/edge
    const unsigned int logicalHeight = 4;   // exactly one row of blocks
    const unsigned int canvasWidth   = 10;  // padded row pitch, wider than logicalWidth

    std::vector<CMP_Vec4uc> paddedCanvas(canvasWidth * logicalHeight);
    for (unsigned int y = 0; y < logicalHeight; y++)
    {
        for (unsigned int x = 0; x < canvasWidth; x++)
        {
            paddedCanvas[y * canvasWidth + x] = (x < logicalWidth) ? BC1StrideTestPixel(static_cast<int>(x), static_cast<int>(y)) : BC1StrideTestSentinel();
        }
    }

    std::vector<CMP_Vec4uc> tightCanvas(logicalWidth * logicalHeight);
    for (unsigned int y = 0; y < logicalHeight; y++)
    {
        for (unsigned int x = 0; x < logicalWidth; x++)
        {
            tightCanvas[y * logicalWidth + x] = BC1StrideTestPixel(static_cast<int>(x), static_cast<int>(y));
        }
    }

    KernelOptions kernelOpts = {};
    kernelOpts.width         = logicalWidth;
    kernelOpts.height        = logicalHeight;
    kernelOpts.fquality      = 1.0f;

    Plugin_BC1 plugin;
    plugin.TC_Init(&kernelOpts);
    void* codec = plugin.TC_Create();
    REQUIRE(codec != nullptr);
    CMP_Encoder* encoder = reinterpret_cast<BC1_EncodeClass*>(codec);

    CMP_BYTE outPadded[16] = {};
    CMP_BYTE outTight[16]  = {};

    // Padded canvas: caller-supplied stride reflects the wider canvas.
    encoder->m_srcWidth              = logicalWidth;
    encoder->m_srcHeight             = logicalHeight;
    encoder->m_srcStride             = canvasWidth * 4;
    encoder->m_dstStride             = 0;
    encoder->m_srcBufferSizeInPixels = canvasWidth * logicalHeight;

    CHECK(encoder->CompressBlock(0, 0, paddedCanvas.data(), outPadded) == 0);
    CHECK(encoder->CompressBlock(1, 0, paddedCanvas.data(), outPadded) == 0);

    // Tightly-packed reference: default stride derived from m_srcWidth.
    encoder->m_srcStride             = 0;
    encoder->m_srcBufferSizeInPixels = logicalWidth * logicalHeight;

    CHECK(encoder->CompressBlock(0, 0, tightCanvas.data(), outTight) == 0);
    CHECK(encoder->CompressBlock(1, 0, tightCanvas.data(), outTight) == 0);

    plugin.TC_Destroy(codec);

    // Both the complete block (bytes 0-7) and the partial/edge block (bytes 8-15) must gather
    // identical pixels regardless of the source buffer's stride, so the compressed output must
    // match byte-for-byte.
    CHECK(memcmp(outPadded, outTight, sizeof(outPadded)) == 0);
}

TEST_CASE("BC1CompressSecurity_NarrowImage_NotRejected", "[BC1Compress][Security]")
{
    BC1COMPRESS_SECURITY_INFO();

    // Every full mipmap chain ends in levels narrower than one 4x4 block, so the stride guards
    // must not fail closed there: for an image narrower than BlockX the default row stride is
    // m_srcWidth itself, which is legitimately less than BlockX. Only the partial-block path
    // runs for these sizes, and it reads just the dwWidth columns that exist.
    for (unsigned int size : {1u, 2u, 3u})
    {
        std::vector<CMP_Vec4uc> canvas(size * size);
        for (unsigned int y = 0; y < size; y++)
        {
            for (unsigned int x = 0; x < size; x++)
            {
                canvas[y * size + x] = BC1StrideTestPixel(static_cast<int>(x), static_cast<int>(y));
            }
        }

        KernelOptions kernelOpts = {};
        kernelOpts.width         = size;
        kernelOpts.height        = size;
        kernelOpts.fquality      = 1.0f;

        Plugin_BC1 plugin;
        plugin.TC_Init(&kernelOpts);
        void* codec = plugin.TC_Create();
        REQUIRE(codec != nullptr);
        CMP_Encoder* encoder = reinterpret_cast<BC1_EncodeClass*>(codec);

        encoder->m_srcWidth              = size;
        encoder->m_srcHeight             = size;
        encoder->m_srcStride             = 0;
        encoder->m_dstStride             = 0;
        encoder->m_srcBufferSizeInPixels = size * size;

        CMP_BYTE out[8] = {};
        INFO("image size " << size << "x" << size);
        CHECK(encoder->CompressBlock(0, 0, canvas.data(), out) == 0);

        plugin.TC_Destroy(codec);
    }
}
