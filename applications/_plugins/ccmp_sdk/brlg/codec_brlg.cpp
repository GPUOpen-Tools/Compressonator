//=====================================================================
// Copyright (c) 2016-2024    Advanced Micro Devices, Inc. All rights reserved.
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
/// \file codec_brlg.cpp
//
//=====================================================================
#include "codec_brlg.h"

//#define BUILD_AS_PLUGIN_DLL

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Codec_Plugin_BRLG)
SET_PLUGIN_TYPE("ENCODER")
SET_PLUGIN_NAME("BRLG")
#else
void* make_Codec_Plugin_BRLG()
{
    return new Codec_Plugin_BRLG;
}
#endif

CMP_BRLGEncode g_BRLGEncodeOptions;

#define GPU_BRLG_COMPUTEFILE "./plugins/Compute/BRLG_Encode_kernel.cpp"

void CompressBlockBRLG_Internal(CMP_Vec4uc srcBlockTemp[MAX_SUBSET_SIZE], CMP_GLOBAL CGU_UINT8* block_out, CMP_GLOBAL CMP_BRLGEncode* BRLGEncode);

void BRLG_DecompressBlock(CMP_GLOBAL CGU_UINT8 out[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG], CGU_UINT8 in[COMPRESSED_BLOCK_SIZE]);

extern CGU_UINT8 g_maxModes;

Codec_Plugin_BRLG::Codec_Plugin_BRLG()
{
    m_KernelOptions = NULL;
}

Codec_Plugin_BRLG::~Codec_Plugin_BRLG()
{
}

int Codec_Plugin_BRLG::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Codec_Plugin_BRLG::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

void* Codec_Plugin_BRLG::TC_Create()
{
    return (void*)new BRLG_EncodeClass();
}

void Codec_Plugin_BRLG::TC_Destroy(void* codec)
{
    delete codec;
    codec = nullptr;
}

char* Codec_Plugin_BRLG::TC_ComputeSourceFile(CGU_UINT32 Compute_type)
{
    switch (Compute_type)
    {
    case CMP_Compute_type::CMP_GPU_OCL:
        return (GPU_BRLG_COMPUTEFILE);
    }

    cmp_autodetected_cpufeatures(CMP_MATH_USE_CPU);
    return ("");
}

int Codec_Plugin_BRLG::TC_Init(void* kernel_options)
{
    m_KernelOptions = reinterpret_cast<KernelOptions*>(kernel_options);

    if (!m_KernelOptions)
        return (-1);

    memset(&g_BRLGEncodeOptions, 0, sizeof(CMP_BRLGEncode));

    g_BRLGEncodeOptions.m_srcSize = m_KernelOptions->width * m_KernelOptions->height;

    m_KernelOptions->data         = &g_BRLGEncodeOptions;
    m_KernelOptions->size         = sizeof(g_BRLGEncodeOptions);
    g_BRLGEncodeOptions.m_quality = m_KernelOptions->fquality;

    return (0);
}

void Codec_Plugin_BRLG::TC_Start() {};
void Codec_Plugin_BRLG::TC_End() {};

int BRLG_EncodeClass::DecompressBlock(void* cmpin, void* srcout)
{
    BRLG_DecompressBlock(CMP_GLOBAL(CGU_UINT8(*)[MAX_DIMENSION_BIG]) srcout, (CGU_UINT8*)cmpin);
    return 0;
}

int BRLG_EncodeClass::DecompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* srcin, void* cmpout)
{
    // ToDo: Implement block level decoder
    if ((xBlock != 0) && (yBlock != 0))
        return -1;
    BRLG_DecompressBlock(CMP_GLOBAL(CGU_UINT8(*)[MAX_DIMENSION_BIG]) cmpout, (CGU_UINT8*)srcin);
    return 0;
}

int BRLG_EncodeClass::CompressTexture(void* srcin, void* cmpout, void* processOptions)
{
    // ToDo: Implement texture level compression
    if (processOptions == NULL)
        return -1;
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;
    // MipSet* pSourceTexture  =  reinterpret_cast<MipSet *>(srcin);
    // MipSet* pDestTexture    =  reinterpret_cast<MipSet *>(cmpout);
    return 0;
}

int BRLG_EncodeClass::DecompressTexture(void* cmpin, void* srcout, void* processOptions)
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

int BRLG_EncodeClass::CompressBlock(void* in, void* out, void* blockoptions)
{
    CMP_BRLGEncode* brlg_Encode;
    if (blockoptions != NULL)
    {
        brlg_Encode = reinterpret_cast<CMP_BRLGEncode*>(blockoptions);
    }
    else
    {
        CMP_BRLGEncode brlgDefault;
        brlg_Encode = &brlgDefault;
        SetDefaultBRLGOptions(brlg_Encode);
    }

    if (m_quality > 1.0f)
        m_quality = 1.0f;
    CompressBlockBRLG_Internal((CMP_Vec4uc*)in, (CGU_UINT8*)out, brlg_Encode);
    return 0;
}

int BRLG_EncodeClass::CompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* in, void* out)
{
    CMP_Vec4uc* ImageSource = (CMP_Vec4uc*)in;
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
    // out-of-range block and indexes ImageSource out of bounds; destI would also run past out.
    if (xBlock >= ((m_srcWidth - 1) / BlockX) + 1 || yBlock >= ((m_srcHeight - 1) / BlockY) + 1)
        return (-1);

    int width_in_blocks = (m_srcWidth + 3) >> 2;

    unsigned int destI    = (xBlock * COMPRESSED_BLOCK_SIZE) + (yBlock * width_in_blocks * COMPRESSED_BLOCK_SIZE);
    size_t       srcindex = 4 * ((size_t)yBlock * m_srcWidth + xBlock);
    int          blkindex = 0;

    CMP_Vec4uc srcData[16];

    //Check if it is a complete 4X4 block
    if (((xBlock + 1) * BlockX <= m_srcWidth) && ((yBlock + 1) * BlockY <= m_srcHeight))
    {
        // The gather reads four pixel rows starting at srcindex, advancing m_srcWidth per row, so
        // the last read must stay inside the accepted pixel count.
        const size_t lastIndex = srcindex + 3 * (size_t)m_srcWidth + 3;
        if (m_srcBufferSizeInPixels > 0u && lastIndex >= (size_t)m_srcBufferSizeInPixels)
            return (-1);  // stride/dimension combination reads past the source buffer

        for (int j = 0; j < 4; j++)
        {
            for (int i = 0; i < 4; i++)
            {
                memcpy(&srcData[blkindex++], &ImageSource[srcindex++], sizeof(CMP_Vec4uc));
            }
            srcindex += (m_srcWidth - 4);
        }
    }
    else
    {
        CGU_DWORD dwWidth = CMP_MIN(static_cast<unsigned int>(BlockX), m_srcWidth - xBlock * BlockX);
        CGU_DWORD i, j, srcIndex;

        //Go through line by line
        for (j = 0; j < BlockY && (BlockY * yBlock + j) < m_srcHeight; j++)
        {
            //Copy the real data
            srcIndex = ((yBlock * BlockY + j) * m_srcWidth + (xBlock * BlockX));
            for (i = 0; i < dwWidth; i++)
            {
                memcpy(&srcData[j * BlockX + i], &ImageSource[srcIndex + i], sizeof(CMP_Vec4uc));
            }
            if (i < BlockX)
                PadLine(i, BlockX, 4, (CGU_UINT8*)&srcData[j * BlockX]);
        }
        if (j < BlockY)
            PadBlock(j, BlockX, BlockY, 4, (CGU_UINT8*)srcData);
    }

    CompressBlockBRLG_Internal(srcData, (unsigned char*)out + destI, &g_BRLGEncodeOptions);

    return (0);
}