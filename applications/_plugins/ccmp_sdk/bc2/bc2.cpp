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
/// \file BC2.cpp
//
//=====================================================================

#include "bc2.h"

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_BC1)
SET_PLUGIN_TYPE("ENCODER")
SET_PLUGIN_NAME("BC2")
#else
void* make_Plugin_BC2()
{
    return new Plugin_BC2;
}
#endif

CMP_BC15Options g_BC2Encode;

#define GPU_OCL_BC2_COMPUTEFILE "./plugins/Compute/BC2_Encode_kernel.cpp"
#define GPU_DXC_BC2_COMPUTEFILE "./plugins/Compute/BC2_Encode_kernel.hlsl"

extern void CompressBlockBC2_Internal(const CMP_Vec4uc                  srcBlockTemp[16],
                                      CMP_GLOBAL CGU_UINT32             compressedBlock[4],
                                      CMP_GLOBAL const CMP_BC15Options* BC15options);

Plugin_BC2::Plugin_BC2()
{
    m_KernelOptions = NULL;
}

Plugin_BC2::~Plugin_BC2()
{
}

int Plugin_BC2::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_BC2::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

void* Plugin_BC2::TC_Create()
{
    return (void*)new BC2_EncodeClass();
}

void Plugin_BC2::TC_Destroy(void* codec)
{
    if (codec != nullptr)
    {
        BC2_EncodeClass* pcodec;
        pcodec = reinterpret_cast<BC2_EncodeClass*>(codec);
        delete pcodec;
        codec = nullptr;
    }
}

char* Plugin_BC2::TC_ComputeSourceFile(CGU_UINT32 Compute_type)
{
    switch (Compute_type)
    {
    case CMP_Compute_type::CMP_GPU_OCL:
        return (GPU_OCL_BC2_COMPUTEFILE);
    case CMP_Compute_type::CMP_GPU_DXC:
        return (GPU_DXC_BC2_COMPUTEFILE);
    }

    return ("");
}

void Plugin_BC2::TC_Start() {};
void Plugin_BC2::TC_End() {};

int Plugin_BC2::TC_Init(void* kernel_options)
{
    if (!kernel_options)
        return (-1);
    m_KernelOptions = reinterpret_cast<KernelOptions*>(kernel_options);

    memset(&g_BC2Encode, 0, sizeof(CMP_BC15Options));
    SetDefaultBC15Options(&g_BC2Encode);
    g_BC2Encode.m_src_width  = m_KernelOptions->width;
    g_BC2Encode.m_src_height = m_KernelOptions->height;
    g_BC2Encode.m_fquality   = m_KernelOptions->fquality;

    m_KernelOptions->data = &g_BC2Encode;
    m_KernelOptions->size = sizeof(g_BC2Encode);

    return (0);
}

int BC2_EncodeClass::DecompressBlock(void* cmpin, void* srcout)
{
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC2_EncodeClass::DecompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* cmpin, void* srcout)
{
    if ((xBlock != 0) && (yBlock != 0))
        return -1;
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC2_EncodeClass::CompressBlock(void* srcin, void* cmpout, void* blockoptions)
{
    CMP_BC15Options* BC2Encode = reinterpret_cast<CMP_BC15Options*>(blockoptions);
    if (BC2Encode == NULL)
        return -1;
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;
    return 0;
}

int BC2_EncodeClass::CompressTexture(void* srcin, void* cmpout, void* processOptions)
{
    // ToDo: Implement texture level decompression
    if (processOptions == NULL)
        return -1;
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;

    // MipSet* pSourceTexture =  reinterpret_cast<MipSet *>(cmpin);
    // MipSet* pDestTexture   =  reinterpret_cast<MipSet *>(srcout);
    return 0;
}

int BC2_EncodeClass::DecompressTexture(void* cmpin, void* srcout, void* processOptions)
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

int BC2_EncodeClass::CompressBlock(CGU_UINT32 xBlock, CGU_UINT32 yBlock, void* srcin, void* cmpout)
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

    int width_in_blocks = (m_srcWidth + 3) >> 2;

    CGU_UINT32 destI    = (xBlock * BC2CompBlockSize) + (yBlock * width_in_blocks * BC2CompBlockSize);
    size_t     srcindex = 4 * ((size_t)yBlock * m_srcWidth + xBlock);
    int        blkindex = 0;

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
        CMP_DWORD dwWidth = CMP_MIN(static_cast<unsigned int>(BlockX), m_srcWidth - xBlock * BlockX);
        CMP_DWORD i, j, srcIndex;

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
                PadLine(i, BlockX, 4, (CMP_BYTE*)&srcData[j * BlockX]);
        }
        if (j < BlockY)
            PadBlock(j, BlockX, BlockY, 4, (CMP_BYTE*)srcData);
    }

    CompressBlockBC2_Internal(srcData, (CGU_UINT32*)&compressedBlocks[destI], &g_BC2Encode);
    return (0);
}
