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
/// \file BC7.cpp
//
//=====================================================================

#include <stdio.h>
#include <stdint.h>
#include "bc7.h"

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_BC7)
SET_PLUGIN_TYPE("ENCODER")
SET_PLUGIN_NAME("BC7")
#else
void* make_Plugin_BC7()
{
    return new Plugin_BC7;
}
#endif

#define GPU_OCL_BC7_COMPUTEFILE "./plugins/Compute/BC7_Encode_kernel.cpp"
#define GPU_DXC_BC7_COMPUTEFILE "./plugins/Compute/BC7_Encode_kernel.hlsl"

BC7_Encode g_BC7Encode;

#ifdef _WIN32
static LARGE_INTEGER frequency = {0};
#endif

extern void CompressBlockBC7_Internal(CGU_UINT8                         image_src[SOURCE_BLOCK_SIZE][4],
                                      CMP_GLOBAL CGU_UINT8              cmp_out[COMPRESSED_BLOCK_SIZE],
                                      CMP_UNIFORM CMP_GLOBAL BC7_Encode BC7Encode[]);

double timeStampsecBC7()
{
#ifdef _WIN32
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart / double(frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1000000000.0;
#endif
}

CGU_INT   sequence          = 0;
CGU_INT   highestFunctionID = -1;
TIMERDATA timerdata[50]     = {0};

extern "C" CGU_INT timerStart(CGU_INT id)
{
    if (id > highestFunctionID)
    {
        if (id > 49)
            id = 49;
        highestFunctionID = id;
    }
    if (timerdata[id].sequence == -1)
    {
        sequence++;
        timerdata[id].sequence = sequence;
    }

    timerdata[id].start = timeStampsecBC7();

    return (0);
}

extern "C" CGU_INT timerEnd(CGU_INT id)
{
    timerdata[id].end += (timeStampsecBC7() - timerdata[id].start);
    timerdata[id].calls++;
    return (0);
}

extern "C" CGU_INT timerSummary()
{
    printf("\n---- function calls [%d] ----\n", sequence);
    for (CGU_INT seq = 1; seq <= sequence; seq++)
    {
        // print sorted sequence (slow method)
        for (CGU_INT func = 0; func <= highestFunctionID; func++)
        {
            if (timerdata[func].sequence == seq)
            {
                printf("%2d func %2d calls [%9d] total [%3.2f] avg [%3.2f] usec\n",
                       timerdata[func].sequence,
                       func,
                       timerdata[func].calls,
                       (timerdata[func].end * 1000000.0f),
                       timerdata[func].calls ? (timerdata[func].end / timerdata[func].calls) * 1000000.0f : 0.0f);
            }
        }
    }

    return (0);
}

// This limit is used for DualIndex Block and if fQuality is above this limit then Quantization shaking will always be performed
// on all indexs
CGU_FLOAT g_HIGHQUALITY_THRESHOLD = 0.7f;

Plugin_BC7::Plugin_BC7()
{
    m_KernelOptions = NULL;
#ifdef _WIN32
    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);
#endif
}

Plugin_BC7::~Plugin_BC7()
{
}

void Plugin_BC7::TC_Start()
{
    // reset
    for (CGU_INT j = 0; j < 50; j++)
    {
        timerdata[j].calls    = 0;
        timerdata[j].start    = 0;
        timerdata[j].end      = 0;
        timerdata[j].sequence = -1;
    }
    sequence = 0;
};

void Plugin_BC7::TC_End()
{
    if (sequence > 0)
        timerSummary();
}

int Plugin_BC7::TC_PluginSetSharedIO(void* Shared)
{
    CMips = reinterpret_cast<CMIPS*>(Shared);
    return 0;
}

int Plugin_BC7::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
    pPluginVersion->guid                 = g_GUID;
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

char* Plugin_BC7::TC_ComputeSourceFile(unsigned int Compute_type)
{
    switch (Compute_type)
    {
    case CMP_Compute_type::CMP_GPU_OCL:
        return (GPU_OCL_BC7_COMPUTEFILE);
    case CMP_Compute_type::CMP_GPU_DXC:
        return (GPU_DXC_BC7_COMPUTEFILE);
    }
    return ("");
}

extern void init_BC7ramps();

void* Plugin_BC7::TC_Create()
{
    BC7_EncodeClass* encoder = new BC7_EncodeClass();
    return (void*)encoder;
}

void Plugin_BC7::TC_Destroy(void* codec)
{
    if (codec != nullptr)
    {
        BC7_EncodeClass* pcodec;
        pcodec = reinterpret_cast<BC7_EncodeClass*>(codec);
        delete pcodec;
        codec = nullptr;
    }
}

int Plugin_BC7::TC_Init(void* kernel_options)
{
    if (!kernel_options)
        return (-1);
    m_KernelOptions = reinterpret_cast<KernelOptions*>(kernel_options);

#ifdef USE_ASPM_CODE
    // Init SSEn, AVXn,.. ramps
    aspm::init_ramps();
#endif

    // Init CPU ramps
    init_BC7ramps();

    m_KernelOptions->data = &g_BC7Encode;
    m_KernelOptions->size = sizeof(BC7_Encode);

    if (m_KernelOptions->fquality > 1.0)
        g_BC7Encode.quality = 1.0f;
    else if (m_KernelOptions->fquality < 0.051)
        g_BC7Encode.quality = 0.05f;
    else
        g_BC7Encode.quality = m_KernelOptions->fquality;

    //BC7 Encoding modes                      76543210
    g_BC7Encode.validModeMask = 0b11111111;

    // User can set options to enable various block modes based on alpha values
    g_BC7Encode.imageNeedsAlpha = FALSE;
    g_BC7Encode.colourRestrict  = FALSE;
    g_BC7Encode.alphaRestrict   = FALSE;
    g_BC7Encode.channels        = 4;

    CGU_FLOAT u_minThreshold = 5.0f;
    CGU_FLOAT u_maxThreshold = 65.0f;  // Lowing this improves SSIM

    g_BC7Encode.errorThreshold = u_maxThreshold * (1.0f - g_BC7Encode.quality);
    if (g_BC7Encode.quality > BC7_qFAST_THRESHOLD)  // Using this to match performance and quality of CPU code
        g_BC7Encode.errorThreshold = u_minThreshold;

    return (0);
}

BC7_EncodeClass::BC7_EncodeClass() {};

int BC7_EncodeClass::DecompressBlock(void* cmpin, void* srcout)
{
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC7_EncodeClass::DecompressBlock(unsigned int xBlock, unsigned int yBlock, void* cmpin, void* srcout)
{
    if ((xBlock != 0) && (yBlock != 0))
        return -1;
    if (srcout == NULL)
        return -1;
    if (cmpin == NULL)
        return -1;
    return 0;
}

int BC7_EncodeClass::CompressBlock(void* in, void* out, void* blockoptions)
{
    CompressBlockBC7_Internal((CGU_UINT8(*)[4])in, (unsigned char*)out, (BC7_Encode*)blockoptions);
    return 0;
}

int BC7_EncodeClass::CompressTexture(void* srcin, void* cmpout, void* processOptions)
{
#ifdef USE_ASPM_CODE
    // Prototype code: ASPM Code execution
    if (g_BC7Encode.quality == 0.99f)
        aspm::CompressBlockBC7_encode((aspm::texture_surface*)srcin, (unsigned char*)cmpout, (aspm::BC7_Encode*)&g_BC7Encode);
#else
    // ToDo: Implement texture level compression
    if (processOptions == NULL)
        return -1;
    if (srcin == NULL)
        return -1;
    if (cmpout == NULL)
        return -1;
    // MipSet* pSourceTexture  =  reinterpret_cast<MipSet *>(srcin);
    // MipSet* pDestTexture    =  reinterpret_cast<MipSet *>(cmpout);
#endif
    return 0;
}

int BC7_EncodeClass::DecompressTexture(void* cmpin, void* srcout, void* processOptions)
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

int BC7_EncodeClass::CompressBlock(unsigned int xBlock, unsigned int yBlock, void* in, void* out)
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

    CompressBlockBC7_Internal((CGU_UINT8(*)[4])srcData, (unsigned char*)out + destI, &g_BC7Encode);
    return (0);
}
