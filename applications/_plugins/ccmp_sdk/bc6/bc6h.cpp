//=====================================================================
// Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All rights reserved.
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
/// \file BC6H.cpp
//
//=====================================================================

#include "bc6h.h"

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_BC6H)
SET_PLUGIN_TYPE("ENCODER")
SET_PLUGIN_NAME("BC6H")
#else
void* make_Plugin_BC6H()
{
    return new Plugin_BC6H;
}
void* make_Plugin_BC6H_SF()
{
    return new Plugin_BC6H;
}
#endif

using namespace BC6H_FILE;

#define GPU_OCL_BC6H_COMPUTEFILE "./plugins/Compute/BC6_Encode_kernel.cpp"
#define GPU_DXC_BC6H_COMPUTEFILE "./plugins/Compute/BC6_Encode_kernel.hlsl"
// ToDo #define VULKAN_BC6H_COMPUTEFILE     "./plugins/Compute/BC6.spv"

extern void CompressBlockBC6_Internal(CMP_GLOBAL unsigned char*     outdata,
                                      CGU_UINT32                    destIdx,
                                      BC6H_Encode_local*            BC6HEncode_local,
                                      CMP_GLOBAL const BC6H_Encode* BC6HEncode);

Plugin_BC6H::Plugin_BC6H()
{
    m_KernelOptions = NULL;
    InitCodecDone   = false;
}

Plugin_BC6H::~Plugin_BC6H()
{
}

void Plugin_BC6H::TC_Start() {};
void Plugin_BC6H::TC_End() {};

int Plugin_BC6H::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_BC6H::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

char* Plugin_BC6H::TC_ComputeSourceFile(unsigned int Compute_type)
{
    switch (Compute_type)
    {
    case CMP_Compute_type::CMP_GPU_OCL:
        return (GPU_OCL_BC6H_COMPUTEFILE);
    case CMP_Compute_type::CMP_GPU_DXC:
        return (GPU_DXC_BC6H_COMPUTEFILE);
        //    case CMP_Compute_type::CMP_GPU_VLK:
        //        return(VULKAN_BC6H_COMPUTEFILE);
    }
    return ("");
}

void* Plugin_BC6H::TC_Create()
{
    void* codec = new BC6H_EncodeClass();
    InitCodecDefaults();
    return codec;
}

void Plugin_BC6H::TC_Destroy(void* codec)
{
    if (codec != nullptr)
    {
        BC6H_EncodeClass* pcodec;
        pcodec = reinterpret_cast<BC6H_EncodeClass*>(codec);
        delete pcodec;
        codec = nullptr;
    }
}

void Plugin_BC6H::InitCodecDefaults()
{
    if (!InitCodecDone)
    {
        InitCodecDone = true;
        memset(&g_BC6HEncode, 0, sizeof(BC6H_Encode));
        init_members(&g_BC6HEncode);
    }
}

int Plugin_BC6H::TC_Init(void* kernel_options)
{
    if (!kernel_options)
        return (-1);
    m_KernelOptions = reinterpret_cast<KernelOptions*>(kernel_options);
    if (!m_KernelOptions)
        return (-1);
    m_KernelOptions->data = &g_BC6HEncode;
    m_KernelOptions->size = sizeof(g_BC6HEncode);
    InitCodecDefaults();
    g_BC6HEncode.m_quality = m_KernelOptions->fquality;
    if (m_KernelOptions->format == CMP_FORMAT_BC6H_SF)
        g_BC6HEncode.m_isSigned = TRUE;
    return (0);
}

int BC6H_EncodeClass::DecompressBlock(void* cmpin, void* srcout)
{
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC6H_EncodeClass::DecompressBlock(unsigned int xBlock, unsigned int yBlock, void* cmpin, void* srcout)
{
    if ((xBlock != 0) && (yBlock != 0))
        return -1;
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC6H_EncodeClass::CompressTexture(void* srcin, void* cmpout, void* processOptions)
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
    return (0);
}

int BC6H_EncodeClass::DecompressTexture(void* cmpin, void* srcout, void* processOptions)
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

int BC6H_EncodeClass::CompressBlock(void* in, void* out, void* blockoptions)
{
    CMP_UNUSED(blockoptions);

    CMP_HALF* p_source_pixels = (CMP_HALF*)in;

    BC6H_Encode_local BC6HEncode_local;

    memset(&BC6HEncode_local, 0, sizeof(BC6H_Encode_local));

    // if the srcWidth and srcHeight is not set try using the alternate user setting
    // that was set by user for block level codec access!
    if (g_BC6HEncode.m_src_width == 0 && g_BC6HEncode.m_src_height == 0)
    {
        if (CMP_Encoder::m_srcWidth == 0 && CMP_Encoder::m_srcHeight == 0)
        {
            return -1;
        }

        g_BC6HEncode.m_src_width  = CMP_Encoder::m_srcWidth;
        g_BC6HEncode.m_src_height = CMP_Encoder::m_srcHeight;
        g_BC6HEncode.m_quality    = CMP_Encoder::m_quality;
    }

    // Note in data is expected to be float type
    memcpy(BC6HEncode_local.din, in, 256);
    int src = 0;
    for (int i = 0; i < 16; i++)
    {
        BC6HEncode_local.din[i][0] = (p_source_pixels[src++]).bits();
        BC6HEncode_local.din[i][1] = (p_source_pixels[src++]).bits();
        BC6HEncode_local.din[i][2] = (p_source_pixels[src++]).bits();
        BC6HEncode_local.din[i][3] = 0.0;  //force alpha channel to 0.0 as bc6 does not process alpha channel
    }

    CompressBlockBC6_Internal((unsigned char*)out, 0, &BC6HEncode_local, &g_BC6HEncode);
    return 0;
}

int BC6H_EncodeClass::CompressBlock(unsigned int xBlock, unsigned int yBlock, void* srcin, void* cmpout)
{
    CMP_HALF* p_source_pixels = (CMP_HALF*)srcin;
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
    // out-of-range block and indexes p_source_pixels out of bounds; destI would also run past cmpout.
    if (xBlock >= ((m_srcWidth - 1) / BlockX) + 1 || yBlock >= ((m_srcHeight - 1) / BlockY) + 1)
        return (-1);

    BC6H_Encode_local BC6HEncode_local;

    memset(&BC6HEncode_local, 0, sizeof(BC6H_Encode_local));
    g_BC6HEncode.m_src_width  = m_srcWidth;
    g_BC6HEncode.m_src_height = m_srcHeight;
    g_BC6HEncode.m_quality    = m_quality;

    CGU_UINT32 stride          = m_srcWidth * BYTEPP;
    CGU_UINT32 srcOffset       = (xBlock * BlockX * BYTEPP) + (yBlock * stride * BlockY);
    int        width_in_blocks = (m_srcWidth + 3) >> 2;

    unsigned int destI  = (xBlock * COMPRESSED_BLOCK_SIZE) + (yBlock * width_in_blocks * COMPRESSED_BLOCK_SIZE);
    unsigned int srcidx = 0;

    //Check if it is a complete 4X4 block
    if (((xBlock + 1) * BlockX <= m_srcWidth) && ((yBlock + 1) * BlockY <= m_srcHeight))
    {
        // Indices below are in CMP_HALF units (BYTEPP channels per pixel); the gather walks
        // BlockX rows of stride halves, so the last read must stay inside the accepted buffer.
        const size_t lastIndex = (size_t)srcOffset + 3 * (size_t)stride + (BlockY * BYTEPP - 1);
        if (m_srcBufferSizeInPixels > 0u && lastIndex >= (size_t)m_srcBufferSizeInPixels * BYTEPP)
            return (-1);  // stride/dimension combination reads past the source buffer

        for (int i = 0; i < BlockX; i++)
        {
            srcidx = i * stride;
            for (int j = 0; j < BlockY; j++)
            {
                CMP_HALF srcpix                         = p_source_pixels[srcOffset + srcidx++];
                BC6HEncode_local.din[i * BlockX + j][0] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[i * BlockX + j][0] = -BC6HEncode_local.din[i * BlockX + j][0];
                    else
                        BC6HEncode_local.din[i * BlockX + j][0] = 0.0;
                }

                srcpix                                  = p_source_pixels[srcOffset + srcidx++];
                BC6HEncode_local.din[i * BlockX + j][1] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[i * BlockX + j][1] = -BC6HEncode_local.din[i * BlockX + j][1];
                    else
                        BC6HEncode_local.din[i * BlockX + j][1] = 0.0;
                }

                srcpix                                  = p_source_pixels[srcOffset + srcidx++];
                BC6HEncode_local.din[i * BlockX + j][2] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[i * BlockX + j][2] = -BC6HEncode_local.din[i * BlockX + j][2];
                    else
                        BC6HEncode_local.din[i * BlockX + j][2] = 0.0;
                }

                BC6HEncode_local.din[i * BlockX + j][3] = 0.0;  //force alpha channel to 0.0 as bc6 does not process alpha channel
                srcidx++;                                       //skip alpha channel
            }
        }
    }
    else
    {
        CMP_DWORD dwWidth = CMP_MIN(static_cast<unsigned int>(BlockX), m_srcWidth - xBlock * BlockX);  //x block width
        CMP_DWORD i, j, srcIndex;

        //Go through line by line
        for (j = 0; j < BlockY && (BlockY * yBlock + j) < m_srcHeight; j++)
        {
            //Copy the real data
            srcIndex = ((yBlock * BlockY + j) * (m_srcWidth * BYTEPP)) + (((xBlock * BlockX)) * BYTEPP);
            for (i = 0; i < dwWidth; i++)
            {
                CMP_HALF srcpix                         = p_source_pixels[srcIndex + BYTEPP * i];
                BC6HEncode_local.din[j * BlockX + i][0] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[j * BlockX + i][0] = -BC6HEncode_local.din[j * BlockX + i][0];
                    else
                        BC6HEncode_local.din[j * BlockX + i][0] = 0.0;
                }

                srcpix                                  = p_source_pixels[srcIndex + BYTEPP * i + 1];
                BC6HEncode_local.din[j * BlockX + i][1] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[j * BlockX + i][1] = -BC6HEncode_local.din[j * BlockX + i][1];
                    else
                        BC6HEncode_local.din[j * BlockX + i][1] = 0.0;
                }

                srcpix                                  = p_source_pixels[srcIndex + BYTEPP * i + 2];
                BC6HEncode_local.din[j * BlockX + i][2] = (srcpix).bits();
                if ((srcpix).isNan() || srcpix < 0.00001f)
                {
                    if (g_BC6HEncode.m_isSigned)
                        BC6HEncode_local.din[j * BlockX + i][2] = -BC6HEncode_local.din[j * BlockX + i][2];
                    else
                        BC6HEncode_local.din[j * BlockX + i][2] = 0.0;
                }

                BC6HEncode_local.din[j * BlockX + i][3] = 0.0;  //force alpha channel to 0.0 as bc6 does not process alpha channel
            }
            if (i < BlockX)
                PadLine(i, BlockX, 4, (CMP_FLOAT*)&BC6HEncode_local.din[j * BlockX]);
        }
        if (j < BlockY)
            PadBlock(j, BlockX, BlockY, 4, (CMP_FLOAT*)BC6HEncode_local.din);
    }

    CompressBlockBC6_Internal((unsigned char*)cmpout, destI, &BC6HEncode_local, &g_BC6HEncode);
    return (0);
}
//============================= BC6H Codec Host Code ====================================

int expandbits_(int bits, int v)
{
    return (v << (8 - bits) | v >> (2 * bits - 8));
}

void init_members(BC6H_Encode* BC6HEncode)
{
    BC6HEncode->m_quality             = 0.05F;
    BC6HEncode->m_partitionSearchSize = ((std::max))((1.0F / 16.0F), ((BC6HEncode->m_quality * 2.0F) / qFAST_THRESHOLD));
}
