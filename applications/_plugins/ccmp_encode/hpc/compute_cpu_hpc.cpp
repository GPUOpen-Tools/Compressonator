//=====================================================================
// Copyright (c) 2020-2024    Advanced Micro Devices, Inc. All rights reserved.
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
/// \file Compute_CPU_HPC.cpp
//
//=====================================================================

#include "plugininterface.h"
#include "cmp_plugininterface.h"
#include "atiformats.h"
#include "ccpu_hpc.h"

#include <chrono>
#include <system_error>
#include <thread>

using namespace std::chrono;

#ifndef _WIN32
#define _stdcall
#endif

//#include "debug.h"

#ifdef _LOCAL_DEBUG
char DbgTracer::buff[MAX_DBGBUFF_SIZE];
char DbgTracer::PrintBuff[MAX_DBGPPRINTBUFF_SIZE];
#endif

// Bytes per channel of the source buffers the encoders index into. 0 for formats the HPC path
// does not feed to an encoder.
static size_t CMP_HPCSourceChannelSize(CMP_FORMAT format)
{
    switch (GetChannelFormat(format))
    {
    case CF_8bit:
    case CF_2101010:
    case CF_1010102:
        return 1;
    case CF_Float16:
    case CF_16bit:
        return 2;
    case CF_Float32:
    case CF_32bit:
        return 4;
    default:
        return 0;
    }
}

// Channels per pixel. Only the narrow layouts need listing; every other format the encoders
// accept is 4 channel.
static size_t CMP_HPCSourceChannelCount(CMP_FORMAT format)
{
    switch (format)
    {
    case CMP_FORMAT_R_8:
    case CMP_FORMAT_R_8_S:
    case CMP_FORMAT_R_16:
    case CMP_FORMAT_R_16F:
    case CMP_FORMAT_R_32F:
        return 1;
    case CMP_FORMAT_RG_8:
    case CMP_FORMAT_RG_8_S:
    case CMP_FORMAT_RG_16:
    case CMP_FORMAT_RG_16F:
    case CMP_FORMAT_RG_32F:
        return 2;
    case CMP_FORMAT_RGB_888:
    case CMP_FORMAT_RGB_888_S:
    case CMP_FORMAT_BGR_888:
    case CMP_FORMAT_RGB_32F:
    case CMP_FORMAT_BGR_32F:
        return 3;
    default:
        return 4;
    }
}

unsigned int _stdcall ProcEncode(void* param)
{
    using namespace std::chrono;
    ThreadParam* tp = (ThreadParam*)param;

    //printf("Thead Active [%4x]\n",std::this_thread::get_id());
    std::this_thread::sleep_for(std::chrono::milliseconds(0));

    using namespace std::chrono;

    while (tp->exit == false)
    {
        if (tp->run == true)
        {
            if (tp->cmp_encoder->CompressBlock(tp->x, tp->y, tp->in, tp->out) != 0 && tp->blockFailed)
                tp->blockFailed->store(true);
            tp->run = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    // printf("Thead Closed [%x] run[%d]\n",std::this_thread::get_id(),tp->run?1:0);
    return 0;
}

float CCPU_HPC::GetProcessElapsedTimeMS()
{
    return m_computeShaderElapsedMS;
}

float CCPU_HPC::GetMTxPerSec()
{
    return m_CmpMTxPerSec;
}

int CCPU_HPC::GetBlockSize()
{
    return m_num_blocks;
}

int CCPU_HPC::GetMaxUCores()
{
    return m_maxUCores;
}

const char* CCPU_HPC::GetDeviceName()
{
    return m_deviceName.c_str();
}

const char* CCPU_HPC::GetVersion()
{
    return m_version.c_str();
}

CodecError CCPU_HPC::CreateEncoderThreadPool()
{
    if (!m_ThreadCodecInitialized)
    {
        const auto cleanupPartialPool = [this](int encoderCount) {
            JoinStartedEncoderThreads();
            for (int index = 0; index < encoderCount; ++index)
            {
                m_plugin_compute->TC_Destroy(m_encoder[index]);
                m_encoder[index]                            = NULL;
                m_EncodeParameterStorage[index].cmp_encoder = NULL;
            }
        };

        //printf("m_ThreadCodecInitialized %d\n",m_NumEncodingThreads);
        // Create the encoding threads in the suspended state
        for (int i = 0; i < m_NumEncodingThreads; i++)
        {
            m_encoder[i] = (CMP_Encoder*)m_plugin_compute->TC_Create();

            // Cleanup if problem!
            if (!m_encoder[i])
            {
                // Workers started by earlier iterations are still polling m_EncodeParameterStorage
                // and using their encoder, so stop them before either is destroyed.
                cleanupPartialPool(i);
                return CE_Unknown;
            }

            m_encoder[i]->m_quality   = m_SourceInfo.m_fquality;
            m_encoder[i]->m_srcHeight = m_SourceInfo.m_src_height;
            m_encoder[i]->m_srcWidth  = m_SourceInfo.m_src_width;
            // pass actual source buffer size to the encoder so
            // CompressBlock() can validate that pixel indices stay in-bounds.
            // Saturate rather than truncate: a wrapped count would reject a valid source.
            m_encoder[i]->m_srcBufferSizeInPixels =
                (m_source_buffer_size_in_pixels > UINT32_MAX) ? UINT32_MAX : static_cast<unsigned int>(m_source_buffer_size_in_pixels);
            m_encoder[i]->m_xdim = m_SourceInfo.m_width_in_blocks;
            m_encoder[i]->m_ydim = m_SourceInfo.m_height_in_blocks;
            m_encoder[i]->m_zdim = 0;

            m_EncodeParameterStorage[i].cmp_encoder = m_encoder[i];
            m_EncodeParameterStorage[i].run         = false;
            m_EncodeParameterStorage[i].exit        = false;
            m_EncodeParameterStorage[i].in          = NULL;
            m_EncodeParameterStorage[i].out         = NULL;
            m_EncodeParameterStorage[i].x           = 0;
            m_EncodeParameterStorage[i].y           = 0;
            m_EncodeParameterStorage[i].blockFailed = &m_blockEncodeFailed;

            try
            {
                m_EncodingThreadHandle[i] = std::thread(ProcEncode, (void*)&m_EncodeParameterStorage[i]);
            }
            catch (const std::system_error&)
            {
                cleanupPartialPool(i + 1);
                return CE_Unknown;
            }
            m_StartedThreadCount = static_cast<CMP_WORD>(i + 1);
        }

        m_ThreadCodecInitialized = true;
    }

    return CE_OK;
}

void CCPU_HPC::JoinStartedEncoderThreads()
{
    for (CMP_WORD dwThread = 0; dwThread < m_StartedThreadCount; dwThread++)
    {
        m_EncodeParameterStorage[dwThread].exit = true;
        std::thread& curThread                  = m_EncodingThreadHandle[dwThread];
        if (curThread.joinable())
            curThread.join();
    }
    m_StartedThreadCount = 0;
}

void CCPU_HPC::DeleteEncoderThreadPool()
{
    // Make sure threads are not in running any state
    // Wait for all the live threads to finish any current work
    JoinStartedEncoderThreads();

    if (!m_EncodeParameterStorage)
    {
        m_ThreadCodecInitialized = false;
        return;
    }

    // Clean up data storage
    for (unsigned int i = 0; i < m_NumEncodingThreads; i++)
    {
        if (m_encoder[i] && m_plugin_compute)
            m_plugin_compute->TC_Destroy(m_encoder[i]);
        m_encoder[i] = NULL;

        m_EncodeParameterStorage[i].cmp_encoder = NULL;

        m_EncodeParameterStorage[i].run         = false;
        m_EncodeParameterStorage[i].exit        = false;
        m_EncodeParameterStorage[i].in          = NULL;
        m_EncodeParameterStorage[i].out         = NULL;
        m_EncodeParameterStorage[i].x           = 0;
        m_EncodeParameterStorage[i].y           = 0;
        m_EncodeParameterStorage[i].blockFailed = NULL;
    }

    m_ThreadCodecInitialized = false;
}

CodecError CCPU_HPC::EncodeThreadBlock(int x, int y, void* in, void* out)
{
    if (m_Use_MultiThreading)
    {
        CMP_WORD threadIndex;

        // Loop and look for an available thread
        CMP_BOOL found = false;
        threadIndex    = 0;
        // printf("threadIndex = %d\n",threadIndex);
        while (found == false)
        {
            if (m_EncodeParameterStorage[threadIndex].run == false)
            {
                found = true;
                break;
            }

            // Increment and wrap the thread index
            threadIndex++;
            if (threadIndex == m_NumEncodingThreads)
            {
                threadIndex = 0;
            }
        }

        m_EncodeParameterStorage[threadIndex].x = x;
        m_EncodeParameterStorage[threadIndex].y = y;

        // point to the input data
        m_EncodeParameterStorage[threadIndex].in = in;

        // Set the output pointer for the thread to the provided location
        m_EncodeParameterStorage[threadIndex].out = out;

        // Tell the thread to start working
        m_EncodeParameterStorage[threadIndex].run = true;
    }
    else
    {
        //printf("HPC Use single threaded\n");
        // Copy the input data into the thread storage
        // point to the input data
        m_EncodeParameterStorage[0].in = in;

        // Set the output pointer for the thread to the provided location
        m_EncodeParameterStorage[0].out = out;

        m_EncodeParameterStorage[0].x = x;
        m_EncodeParameterStorage[0].y = y;

        if (m_encoder[0]->CompressBlock(x, y, in, out) != 0)
        {
            // The block encoder rejected the block (bounds/overflow guard); the output block was
            // not written, so the failure must not be reported as a successful encode.
            m_blockEncodeFailed.store(true);
            return CE_Unknown;
        }
    }
    return CE_OK;
}

void CCPU_HPC::FinishThreadEncoding()
{
    using namespace std::chrono;

    // Wait for all the live threads to finish any current work
    for (CMP_DWORD i = 0; i < m_NumEncodingThreads; i++)
    {
        // If a thread is in the running state then we need to wait for it to finish
        // its work from the producer
        while (m_EncodeParameterStorage[i].run == true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        m_EncodeParameterStorage[i].exit = true;
    }
}

void CCPU_HPC::Init()
{
    m_cputimer = cpu_timer();

    m_plugin_compute         = NULL;
    m_ThreadCodecInitialized = false;
    m_StartedThreadCount     = 0;
    m_blockEncodeFailed.store(false);
    m_current_format         = CMP_FORMAT_Unknown;
    m_computeShaderElapsedMS = 0.0f;
    m_num_blocks             = 0;
    m_CmpMTxPerSec           = 0.0f;

    m_source_buffer_size           = 0;
    m_source_buffer_size_in_pixels = 0;
    //printf("HPC Threads input %d\n",m_kernel_options->threads);
    if (m_kernel_options->threads != 1)
    {
        m_NumEncodingThreads = (CMP_WORD)m_kernel_options->threads;
        if (m_NumEncodingThreads == 0)
        {
            m_NumEncodingThreads = (CMP_WORD)CMP_NumberOfProcessors();
            if (m_NumEncodingThreads <= 2)
                m_NumEncodingThreads = 8;  // fallback to a default!
            if (m_NumEncodingThreads > 128)
                m_NumEncodingThreads = 128;
        }
        m_Use_MultiThreading = true;  //always enable multithread by default for this release!
    }
    else
    {
        m_NumEncodingThreads = 1;
        m_Use_MultiThreading = false;
    }

    //printf("HPC Threads set %d\n",m_kernel_options->threads);

    m_EncodeParameterStorage = new ThreadParam[m_NumEncodingThreads];
    m_EncodingThreadHandle   = new std::thread[m_NumEncodingThreads];
    if (!m_EncodingThreadHandle)
    {
        delete[] m_EncodeParameterStorage;
        m_EncodeParameterStorage = NULL;
    }

    for (int i = 0; i < m_NumEncodingThreads; i++)
    {
        m_encoder[i] = NULL;
    }
}

CCPU_HPC::CCPU_HPC(void* ukernelOptions)
{
    m_kernel_options = (KernelOptions*)ukernelOptions;
    Init();
}

void CCPU_HPC::SetComputeOptions(ComputeOptions* CLOptions)
{
    // Hold ref to the compute codec plugin for use in Compute Framework
    m_plugin_compute = reinterpret_cast<PluginInterface_Encoder*>(CLOptions->plugin_compute);
}

CCPU_HPC::~CCPU_HPC()
{
    // Compress() normally pairs Create/DeleteEncoderThreadPool, but if it returned early the
    // workers are still polling the storage that is freed below.
    DeleteEncoderThreadPool();

    if (m_EncodeParameterStorage)
    {
        delete[] m_EncodeParameterStorage;
        m_EncodeParameterStorage = NULL;
    }
    if (m_EncodingThreadHandle)
    {
        delete[] m_EncodingThreadHandle;
        m_EncodingThreadHandle = NULL;
    }
}

#include "cmp_hpc.h"

//extern void CompressImage(const texture_surface* input, BYTE* output);
extern void InitWin32Threads(int numThreads);
extern void DestroyThreads();
extern void Initialize();

CMP_Encoder* g_plugin_compute;

void CompressTexture(const texture_surface* input, unsigned char* output)
{
    g_plugin_compute->CompressTexture((void*)input, (void*)output, nullptr);
}

CMP_ERROR CCPU_HPC::Compress(KernelOptions* Options, MipSet& SrcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    if (m_plugin_compute == NULL)
        return (CMP_ERR_UNABLE_TO_INIT_COMPUTELIB);

#if (defined(USE_CONVECTION_KERNELS) || defined(USE_GTC) || defined(USE_APC) || defined(USE_LOSSLESS_COMPRESSION))
    if ((destTexture.m_format == CMP_FORMAT_GTC)
#ifdef USE_APC
        || (destTexture.m_format == CMP_FORMAT_APC)
#endif
        || (destTexture.m_format == CMP_FORMAT_BROTLIG)
#ifdef USE_CONVECTION_KERNELS
        || (destTexture.m_format == CMP_FORMAT_BC1) || (destTexture.m_format == CMP_FORMAT_BC5)
#endif
    )
    {
        if (Options->fquality == 0.11f)
        {
            CMP_Encoder* encoder = (CMP_Encoder*)m_plugin_compute->TC_Create();
            if (encoder)
            {
                int res = encoder->CompressTexture(&SrcTexture, &destTexture, nullptr);
                if (res != 0)
                {
                    // printf("failed");
                }
                m_plugin_compute->TC_Destroy(encoder);
                return (CMP_OK);
            }
            return (CMP_ERR_GENERIC);
        }
    }
#endif

#ifdef USE_ASPM_CODE
    // Prototype Code: Enabling use of SPMD (enabled only on Windows Platforms)
    if (destTexture.m_format == CMP_FORMAT_BC7)
    {
        // When ASPM code path is enabled
        if (Options->fquality == 0.99f)
        {
            g_plugin_compute = (CMP_Encoder*)m_plugin_compute->TC_Create();
            Initialize();
            InitWin32Threads(Options->threads);
            texture_surface edged_img;
            edged_img.height = SrcTexture.dwHeight;
            edged_img.width  = SrcTexture.dwWidth;
            edged_img.stride = SrcTexture.dwWidth * 4;
            edged_img.ptr    = SrcTexture.pData;
            m_plugin_compute->TC_Start();
            CompressSTMT(&edged_img, destTexture.pData, Options->threads);
            m_plugin_compute->TC_End();
            DestroyThreads();
            m_plugin_compute->TC_Destroy(g_plugin_compute);
            return (CMP_OK);
        }
    }
#endif

    // Get Encoder source file
    m_source_file = Options->srcfile;

    // Update user format options
    memcpy(m_kernel_options, Options, sizeof(Options->encodeoptions));

    // Update kernel option settings
    m_kernel_options->data    = Options->data;
    m_kernel_options->size    = Options->size;
    m_kernel_options->format  = Options->format;
    m_kernel_options->dataSVM = Options->dataSVM;

    // Get Texture info
    m_source_buffer_size = SrcTexture.dwDataSize;
    // The encoders index the source as 4 channels per pixel; BC6H uses CMP_HALF channels,
    // so the byte size cannot be divided by a fixed 4 to get a pixel count. m_format tracks the
    // buffer TC_Compress() swapped in, whereas m_ChannelFormat still describes the original.
    const size_t srcChannelSize = CMP_HPCSourceChannelSize(SrcTexture.m_format);
    if (srcChannelSize == 0)
        return CMP_ERR_UNSUPPORTED_SOURCE_FORMAT;  // fail closed: without this the encoder bounds guard would be disabled

    // A narrower source would make the 4-channel pixel count below an underestimate, silently
    // failing every block in the encoder bounds guard. Reject it here with a meaningful error
    // instead: CreateCompatibleBuffer is expected to have widened the buffer already.
    if (CMP_HPCSourceChannelCount(SrcTexture.m_format) != 4)
        return CMP_ERR_UNSUPPORTED_SOURCE_FORMAT;

    m_source_buffer_size_in_pixels = m_source_buffer_size / (4 * srcChannelSize);
    if (m_source_buffer_size != 0 && m_source_buffer_size_in_pixels == 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    p_destination      = destTexture.pData;
    m_destination_size = destTexture.dwDataSize;

    // The encode loop writes one compressed block per padded block coordinate at offsets
    // derived from the claimed dimensions, so a TC_Compress() caller that supplies an undersized
    // destination MipSet would have the workers write past it. CMP_CalculateBufferSize() rounds the
    // dimensions up to whole blocks exactly as the loop does, and is what CMP_ProcessTexture()
    // already uses to size the allocation.
    CMP_Texture requiredDest  = {};
    requiredDest.dwSize       = sizeof(requiredDest);
    requiredDest.dwWidth      = SrcTexture.dwWidth;
    requiredDest.dwHeight     = SrcTexture.dwHeight;
    requiredDest.dwPitch      = 0;
    requiredDest.format       = destTexture.m_format;
    requiredDest.nBlockWidth  = destTexture.m_nBlockWidth;
    requiredDest.nBlockHeight = destTexture.m_nBlockHeight;

    const CMP_DWORD requiredDestSize = CMP_CalculateBufferSize(&requiredDest);
    if (p_destination == NULL || requiredDestSize == 0 || m_destination_size < requiredDestSize)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    CMP_UINT m_padded_width_in_blocks;
    CMP_UINT m_padded_height_in_blocks;

    m_current_format = destTexture.m_format;

    if (destTexture.m_nBlockWidth > 0)
    {
        m_width_in_blocks        = (CMP_UINT)SrcTexture.dwWidth / destTexture.m_nBlockWidth;
        m_padded_width_in_blocks = (((CMP_UINT)SrcTexture.dwWidth + 3) >> 2);
    }
    else
    {
        return CMP_ERR_GENERIC;
    }

    if (destTexture.m_nBlockHeight > 0)
    {
        m_height_in_blocks        = (CMP_UINT)SrcTexture.dwHeight / destTexture.m_nBlockHeight;
        m_padded_height_in_blocks = (((CMP_UINT)SrcTexture.dwHeight + 3) >> 2);
    }
    else
    {
        return CMP_ERR_GENERIC;
    }

    m_psource                       = (CMP_Vec4uc*)SrcTexture.pData;
    m_SourceInfo.m_src_height       = SrcTexture.dwHeight;
    m_SourceInfo.m_src_width        = SrcTexture.dwWidth;
    m_SourceInfo.m_width_in_blocks  = m_width_in_blocks;
    m_SourceInfo.m_height_in_blocks = m_height_in_blocks;
    m_SourceInfo.m_fquality         = Options->fquality;
    m_num_blocks                    = m_width_in_blocks * m_height_in_blocks;

    {
        CMP_DOUBLE pFeedbackTimeMS = 0;  // Tracks time spent outside of encoder loop

        // initialize the Encoder based on num thread set in Init
        m_blockEncodeFailed.store(false);
        if (CreateEncoderThreadPool() != CE_OK)
        {
            // The pool tore down every encoder/worker it had created, so there is nothing to
            // encode with: m_encoder[0] is NULL and no worker is polling for work. Continuing
            // would dereference a NULL encoder (single threaded) or spin forever looking for a
            // free worker slot (multithreaded), so fail closed before the encode loop starts.
            // TC_Start() has not run yet, so no matching TC_End() is owed here.
            return CMP_ERR_UNABLE_TO_CREATE_ENCODER;
        }

        // printf("Encoder %x Source %x  Destination %x\n",m_plugin_compute, m_psource,p_destination);
        CMP_FLOAT xyblocks = (CMP_FLOAT)m_padded_height_in_blocks * m_padded_width_in_blocks;

        if (xyblocks <= 0.01f)
            xyblocks = 1.0;

        unsigned int y = 0;
        unsigned int x;
        float        progress;
        float        progress_old  = FLT_MAX;
        CMP_INT      lineAtPercent = (CMP_INT)(m_padded_height_in_blocks * 0.01f);
        if (lineAtPercent <= 0)
            lineAtPercent = 1;

        if (Options->getPerfStats)
            m_cputimer.Start(0);

        m_plugin_compute->TC_Start();

        while (y < m_padded_height_in_blocks)
        {
            for (x = 0; x < m_padded_width_in_blocks; x++)
            {
                if (EncodeThreadBlock(x, y, (void*)m_psource, (void*)p_destination) != CE_OK)
                    break;
            }
            if (m_blockEncodeFailed.load())
                break;
            y++;
            if (pFeedback)
            {
                if (Options->getPerfStats)
                    m_cputimer.Start(1);
                if (((y % lineAtPercent) == 0) && (xyblocks > 0))
                {
                    progress = (x * y) / xyblocks;
                    if (progress_old != progress)
                    {
                        progress_old = progress;
                        if (pFeedback(progress * 100.0f, pUser1, pUser2))
                        {
                            break;
                        }
                    }
                }

                if (Options->getPerfStats)
                {
                    m_cputimer.Stop(1);
                    pFeedbackTimeMS += m_cputimer.GetTimeMS(1);
                }
            }
        }

        // Wait for all threads to Finish
        FinishThreadEncoding();

        // Delete the Encoder Thread Pool
        DeleteEncoderThreadPool();

        if (Options->getPerfStats)
            m_cputimer.Stop(0);

        // Only collect perf data on topmost miplevel
        if (Options->getPerfStats && (destTexture.m_nIterations < 1))
        {
            m_computeShaderElapsedMS = ((CGU_FLOAT)(m_cputimer.GetTimeMS(0) - pFeedbackTimeMS)) / m_num_blocks;
            if (m_computeShaderElapsedMS > 0)
            {
                CMP_FLOAT blocksPerSecond = (1000.0f / m_computeShaderElapsedMS);
                m_CmpMTxPerSec            = (BLOCK_SIZE_4X4 * blocksPerSecond) / 1000000.0f;
            }
            else
                m_CmpMTxPerSec = 0;
        }

        m_plugin_compute->TC_End();
    }

    if (m_blockEncodeFailed.load())
    {
        // At least one block was rejected by the block encoder (e.g. a bounds/overflow guard),
        // so the destination contains unwritten blocks: report the failure to the caller.
        return CMP_ERR_GENERIC;
    }

    return (CMP_OK);
}
