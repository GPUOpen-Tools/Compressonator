//=====================================================================
// Copyright (c) 2021-2024    Advanced Micro Devices, Inc. All rights reserved.
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
/// \file BC1.cpp
//
//=====================================================================

#include "bc1.h"

void SetUserBC15EncoderOptions(CMP_BC15Options* bc15options, KernelOptions* kerneloptions);

#ifdef USE_TIMERS
#include "cpu_timing.h"
#endif

//#define BUILD_AS_PLUGIN_DLL

#ifdef USE_CONVECTION_KERNELS
#pragma comment(lib, "convectionkernels.lib")
#include "convectionkernels.h"
using namespace cvtt;
#endif

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_BC1)
SET_PLUGIN_TYPE("ENCODER")
SET_PLUGIN_NAME("BC1")
#else
void* make_Plugin_BC1()
{
    return new Plugin_BC1;
}
#endif

CMP_BC15Options g_BC1Encode;

#define GPU_OCL_BC1_COMPUTEFILE "./plugins/Compute/BC1_Encode_kernel.cpp"
#define GPU_DXC_BC1_COMPUTEFILE "./plugins/Compute/BC1_Encode_kernel.hlsl"

void CompressBlockBC1_Internal(const CMP_Vec4uc srcBlockTemp[16], CMP_GLOBAL CGU_UINT32 compressedBlock[2], CMP_GLOBAL CMP_BC15Options* BC15options);

Plugin_BC1::Plugin_BC1()
{
    m_KernelOptions = NULL;
}

Plugin_BC1::~Plugin_BC1()
{
}

int Plugin_BC1::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_BC1::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

void* Plugin_BC1::TC_Create()
{
    return (void*)new BC1_EncodeClass();
}

void Plugin_BC1::TC_Destroy(void* codec)
{
    if (codec != nullptr)
    {
        BC1_EncodeClass* pcodec;
        pcodec = reinterpret_cast<BC1_EncodeClass*>(codec);
        delete pcodec;
        codec = nullptr;
    }
}

char* Plugin_BC1::TC_ComputeSourceFile(CGU_UINT32 Compute_type)
{
    switch (Compute_type)
    {
    case CMP_Compute_type::CMP_GPU_OCL:
        return (GPU_OCL_BC1_COMPUTEFILE);
    case CMP_Compute_type::CMP_GPU_DXC:
        return (GPU_DXC_BC1_COMPUTEFILE);
    }
    return ("");
}

void Plugin_BC1::TC_Start() {};
void Plugin_BC1::TC_End() {};

int Plugin_BC1::TC_Init(void* kernel_options)
{
    if (!kernel_options)
        return (-1);
    m_KernelOptions = reinterpret_cast<KernelOptions*>(kernel_options);

    memset(&g_BC1Encode, 0, sizeof(CMP_BC15Options));

    SetDefaultBC15Options(&g_BC1Encode);

    SetUserBC15EncoderOptions(&g_BC1Encode, m_KernelOptions);

    g_BC1Encode.m_src_width  = m_KernelOptions->width;
    g_BC1Encode.m_src_height = m_KernelOptions->height;
    g_BC1Encode.m_fquality   = m_KernelOptions->fquality;

    m_KernelOptions->data = &g_BC1Encode;
    m_KernelOptions->size = sizeof(g_BC1Encode);

    return (0);
}

int BC1_EncodeClass::DecompressBlock(void* cmpin, void* srcout)
{
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC1_EncodeClass::DecompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* cmpin, void* srcout)
{
    if ((xBlock != 0) && (yBlock != 0))
        return -1;
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC1_EncodeClass::CompressBlock(void* srcin, void* cmpout, void* blockoptions)
{
    CMP_BC15Options* BC1Encode = reinterpret_cast<CMP_BC15Options*>(blockoptions);
    if (BC1Encode == NULL)
        return -1;
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;
    return 0;
}

int BC1_EncodeClass::CompressTexture(void* srcin, void* cmpout, void* processOptions)
{
    (processOptions);
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;

    MipSet* pSourceTexture = reinterpret_cast<MipSet*>(srcin);

    CMP_DWORD src_height = pSourceTexture->dwHeight;
    CMP_DWORD src_width  = pSourceTexture->dwWidth;

    CMP_DWORD width_in_blocks  = src_width / 4;
    CMP_DWORD height_in_blocks = src_height / 4;
    int       stride           = src_width * BYTEPP;
    int       blockOffset      = 0;
    int       srcidx;

#ifdef USE_CONVECTION_KERNELS
    PixelBlockU8 pBlocks[8] = {0};
    CMP_BYTE     results[64];  // 8 x 8
#endif

    for (CMP_DWORD by = 0; by < height_in_blocks; by++)
    {
        for (CMP_DWORD bx = 0; bx < width_in_blocks; bx++)
        {
            int srcOffset = (bx * BlockX * BYTEPP) + (by * stride * BYTEPP);

            // Copy src block into Texel
            blockOffset = 0;
            for (CMP_DWORD i = 0; i < BlockX; i++)
            {
                srcidx = i * stride;
                for (CMP_DWORD j = 0; j < BlockY; j++)
                {
                    unsigned char R, G, B, A;
                    R = (pSourceTexture->pData[srcOffset + srcidx++]);
                    G = (pSourceTexture->pData[srcOffset + srcidx++]);
                    B = (pSourceTexture->pData[srcOffset + srcidx++]);
                    A = (pSourceTexture->pData[srcOffset + srcidx++]);

                    //printf("[%2d,%2d] [%2d][%2d] %2d,%2d,%2d,%2d\n",bx,by,numBlocks,blockOffset,R,G,B,A);
#ifdef USE_CONVECTION_KERNELS
                    pBlocks[numBlocks].m_pixels[blockOffset][0] = R;
                    pBlocks[numBlocks].m_pixels[blockOffset][1] = G;
                    pBlocks[numBlocks].m_pixels[blockOffset][2] = B;
                    pBlocks[numBlocks].m_pixels[blockOffset][3] = A;
#endif
                    blockOffset++;
                }
            }

#ifdef USE_CONVECTION_KERNELS
            numBlocks++;
            numBlocksProcessed++;

            // we are at 8 blocks
            if ((numBlocks == 8) || (numBlocksProcessed >= maxBlocks))
            {
                cvtt::Options options;
                options.flags = Flags::Default;
                Kernels::EncodeBC1(results, pBlocks, options);

                // save results
                for (int i = 0; i < numBlocks; i++)
                {
                    for (int j = 0; j < 8; j++)
                    {
                        pdest[j] = results[i * 8 + j];
                    }
                    pdest += 8;  // next comp data block to store results
                }
                numBlocks = 0;
            }
#endif
        }
    }

    return 0;
}

int BC1_EncodeClass::DecompressTexture(void* cmpin, void* srcout, void* processOptions)
{
    // ToDo: Implement texture level decompression
    if (processOptions == NULL)
        return -1;
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;

    // MipSet* pSourceTexture =  reinterpret_cast<MipSet *>(cmpin);
    // MipSet* pDestTexture   =  reinterpret_cast<MipSet *>(srcout);
    return 0;
}

int BC1_EncodeClass::CompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* srcin, void* cmpout)
{
    CMP_Vec4uc* ImageSource      = (CMP_Vec4uc*)srcin;
    CGU_UINT8*  compressedBlocks = (CGU_UINT8*)cmpout;

    // if the srcWidth and srcHeight is not set try using the alternate user setting
    // that was set by user for block level codec access!
    if ((m_srcHeight == 0) || (m_srcWidth == 0))
    {
        return (-1);
    }

    // Guard against integer overflow in m_srcWidth * m_srcHeight.
    if (m_srcHeight > UINT32_MAX / m_srcWidth)
        return (-1);  // prevent integer overflow

    // Guard: reject if the actual source buffer (set by the HPC thread pool
    // via m_srcBufferSizeInPixels) is smaller than the full pixel count implied by
    // the claimed dimensions.  A value of 0 means "not set" (block-encoder API path
    // that trusts the caller), so skip the check in that case.
    CGU_UINT32 totalPixels = (CGU_UINT32)m_srcWidth * (CGU_UINT32)m_srcHeight;
    if (m_srcBufferSizeInPixels > 0u && m_srcBufferSizeInPixels < totalPixels)
        return (-1);  // source buffer too small for claimed dimensions

    // Reject block coordinates outside the declared image before any offset is derived from them.
    // The partial-block path below computes m_srcWidth - xBlock * BlockX, which underflows for an
    // out-of-range block and indexes ImageSource out of bounds; destI would also run past cmpout.
    if (xBlock >= ((m_srcWidth - 1) / BlockX) + 1 || yBlock >= ((m_srcHeight - 1) / BlockY) + 1)
        return (-1);

    // The default strides below are derived from m_srcWidth. Reject a width whose derived strides
    // are not representable in 32 bits rather than silently wrapping into a small stride that
    // would gather the wrong rows: accumulate in 64-bit and fail closed.
    const CGU_UINT64 defaultSrcStride64 = (CGU_UINT64)m_srcWidth * 4;
    const CGU_UINT64 widthInBlocks64    = ((CGU_UINT64)m_srcWidth + 3) >> 2;
    const CGU_UINT64 defaultDstStride64 = widthInBlocks64 * BC1CompBlockSize;
    if (defaultSrcStride64 > (CGU_UINT64)UINT32_MAX || defaultDstStride64 > (CGU_UINT64)UINT32_MAX)
        return (-1);  // image width too large for the derived source/destination strides

    // Units (see CMP_Encoder in plugininterface.h): srcStride advances srcindex, which indexes
    // ImageSource as CMP_Vec4uc ELEMENTS, so it counts pixels - m_srcWidth * 4 is one row of
    // 4-pixel-high blocks. dstStride advances destI, which indexes compressedBlocks as BYTES.
    CGU_UINT32 srcStride = (CGU_UINT32)defaultSrcStride64;
    CGU_UINT32 dstStride = (CGU_UINT32)defaultDstStride64;

    // user override!
    if (m_srcStride > 0)
        srcStride = m_srcStride;
    if (m_dstStride > 0)
        dstStride = m_dstStride;

    // Accumulate the destination byte offset in 64-bit: with a caller-supplied dstStride,
    // yBlock * dstStride can exceed UINT32_MAX and wrap back into the buffer. size_t is only
    // 32 bits in the Win32 build, so the accumulation must not be done in size_t either.
    const CGU_UINT64 destI64 = ((CGU_UINT64)xBlock * BC1CompBlockSize) + ((CGU_UINT64)yBlock * dstStride);
    if (destI64 > (CGU_UINT64)UINT32_MAX - BC1CompBlockSize)
        return (-1);  // stride/coordinate combination overflows the destination offset

    CGU_UINT32 destI = (CGU_UINT32)destI64;

    // Pixel offsets are accumulated in 64-bit for the same reason: with a caller-supplied stride,
    // yBlock * srcStride can exceed UINT32_MAX for a source buffer that is otherwise valid and
    // accepted above, and would wrap in a 32-bit size_t.
    const CGU_UINT64 srcindex64 = ((CGU_UINT64)yBlock * srcStride) + ((CGU_UINT64)4 * xBlock);
    if (srcindex64 > (CGU_UINT64)SIZE_MAX)
        return (-1);  // stride/coordinate combination overflows the source index

    size_t srcindex = (size_t)srcindex64;
    int    blkindex = 0;

    CMP_Vec4uc srcData[16];

    // srcStride is the element advance for 4 rows (one yBlock, per plugininterface.h): the
    // per-row element advance used below to walk rows gathered from ImageSource must be
    // srcStride / 4, not m_srcWidth, in both the complete- and partial-block paths below.
    // Using m_srcWidth here silently discards a caller-supplied stride that does not match
    // the tightly-packed default, reading the wrong (or out-of-buffer) pixel rows.
    if ((srcStride % 4) != 0)
        return (-1);  // stride is not a whole number of pixel rows
    const CGU_UINT32 rowStride = srcStride / 4;

    //Check if it is a complete 4X4 block
    if (((xBlock + 1) * BlockX <= m_srcWidth) && ((yBlock + 1) * BlockY <= m_srcHeight))
    {
        if (rowStride < BlockX)
            return (-1);  // a complete block reads BlockX columns, so a row must be that wide

        // The gather reads four pixel rows starting at srcindex, advancing rowStride per row, so
        // a caller-supplied m_srcStride must not push the last read past the accepted pixel count.
        // Accumulated in 64-bit so it cannot wrap in a 32-bit size_t.
        const CGU_UINT64 lastIndexOffset = 3 * (CGU_UINT64)rowStride + 3;
        if (srcindex64 > UINT64_MAX - lastIndexOffset)
            return (-1);  // source index addition overflows before the last-read bounds check
        const CGU_UINT64 lastIndex = srcindex64 + lastIndexOffset;
        if (m_srcBufferSizeInPixels > 0u && lastIndex >= (CGU_UINT64)m_srcBufferSizeInPixels)
            return (-1);  // stride/dimension combination reads past the source buffer
        if (lastIndex > (CGU_UINT64)SIZE_MAX)
            return (-1);  // last read index is not representable as a source index

        for (int j = 0; j < 4; j++)
        {
            for (int i = 0; i < 4; i++)
            {
                memcpy(&srcData[blkindex++], &ImageSource[srcindex++], sizeof(CMP_Vec4uc));
            }
            srcindex += (rowStride - 4);
        }
    }
    else
    {
        CMP_DWORD dwWidth  = CMP_MIN(static_cast<unsigned int>(BlockX), m_srcWidth - xBlock * BlockX);
        CMP_DWORD rowsUsed = CMP_MIN(static_cast<unsigned int>(BlockY), m_srcHeight - yBlock * BlockY);
        CMP_DWORD i, j;
        size_t    srcIndex;

        // A partial block reads only the dwWidth columns that exist, so unlike the complete-block
        // path the row may legitimately be narrower than BlockX -- an image narrower than one
        // block (the tail of every mipmap chain) has a default rowStride of m_srcWidth. Require
        // only that the row actually spans the columns this block reads.
        if (rowStride < xBlock * BlockX + dwWidth)
            return (-1);  // row does not span the columns this block reads

        // Same reasoning as the complete-block path above: bound the last pixel this partial
        // block will read against the caller-reported buffer capacity before touching memory.
        if (rowsUsed > 0 && dwWidth > 0)
        {
            const CGU_UINT64 lastIndex = (CGU_UINT64)(yBlock * BlockY + rowsUsed - 1) * rowStride + (CGU_UINT64)(xBlock * BlockX) + (dwWidth - 1);
            if (m_srcBufferSizeInPixels > 0u && lastIndex >= (CGU_UINT64)m_srcBufferSizeInPixels)
                return (-1);  // stride/dimension combination reads past the source buffer
        }

        //Go through line by line
        for (j = 0; j < BlockY && (BlockY * yBlock + j) < m_srcHeight; j++)
        {
            //Copy the real data
            // Accumulate in 64-bit before narrowing to size_t: with a caller-supplied stride,
            // yBlock * rowStride can exceed UINT32_MAX even though the total pixel count check
            // above passed, so the multiplication must not be done in 32-bit CMP_DWORD (matches
            // the accumulation pattern used for srcindex64 in the complete-block path above).
            const CGU_UINT64 srcIndex64 = (CGU_UINT64)(yBlock * BlockY + j) * rowStride + (CGU_UINT64)(xBlock * BlockX);
            if (srcIndex64 > (CGU_UINT64)SIZE_MAX)
                return (-1);  // stride/coordinate combination overflows the source index
            srcIndex = (size_t)srcIndex64;
            for (i = 0; i < dwWidth; i++)
            {
                memcpy(&srcData[j * BlockX + i], &ImageSource[srcIndex + i], sizeof(CMP_Vec4uc));
            }
            if (i < BlockX)
                PadLine(i, BlockX, 4, (CMP_BYTE*)&srcData[j * BlockX]);
        }
        if (j < BlockY)
            PadBlock(j, BlockX, BlockY, 4, (CMP_BYTE*)srcData);
    }

#ifdef USE_TIMERS
    static int init = false;
    if (!init)
    {
        query_timer::Initialize();
        init = true;
    }
    {
        QUERY_PERFORMANCE("BC1");
#endif

        CompressBlockBC1_Internal(srcData, (CGU_UINT32*)&compressedBlocks[destI], &g_BC1Encode);

#ifdef USE_TIMERS
    }  // Query

    static int sum   = 0;
    static int count = 0;
    if (query_timer::m_elapsed_count < 30)
    {
        sum += (int)query_timer::m_elapsed_count;
        count++;
        printf("BC1 %f\n", (CGU_FLOAT)sum / (CGU_FLOAT)count);
    }
#endif
    return (0);
}
