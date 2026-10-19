//===============================================================================
// Copyright (c) 2007-2024  Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
//===============================================================================
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
//
//  File Name:   Codec_ASTC.cpp
//  Description: implementation of the CCodec_ASTC class
//
//////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4100)  // Ignore warnings of unreferenced formal parameters
#pragma warning(disable : 4101)  // Ignore warnings of unreferenced local variable
#pragma warning(disable : 4996)  // This function or variable may be unsafe

#include "common.h"
#include "compressonator.h"

#include "astc/codec_astc.h"
#include "astc/astc_library.h"

#include "astc/arm/astc_codec_internals.h"
#include "debug.h"

#include <atomic>
#include <chrono>
#include <system_error>
#include <cstring>

#ifdef ASTC_COMPDEBUGGER
#include "compclient.h"
extern CompViewerClient g_CompClient;
#endif

//======================================================================================
#define USE_MULTITHREADING 1

// Gets the total numver of active processor cores on the running host system
extern CMP_INT CMP_GetNumberOfProcessors();

struct ASTCEncodeThreadParam
{
    ASTCBlockEncoder* encoder;

    // Encoder params
    astc_codec_image*             input_image;
    uint8_t*                      bp;
    int                           xdim;
    int                           ydim;
    int                           zdim;
    int                           x;
    int                           y;
    int                           z;
    astc_decode_mode              decode_mode;
    const error_weighting_params* ewp;

    // Atomics, not volatile: these are the synchronisation between the producer and the worker,
    // and volatile provides no inter-thread ordering or visibility guarantees.
    std::atomic<bool> run;
    std::atomic<bool> exit;
};

static ASTCEncodeThreadParam* g_EncodeParameterStorage = NULL;

//////////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////////////

CCodec_ASTC::CCodec_ASTC()
    : CCodec_DXTC(CT_ASTC)
{
    m_LibraryInitialized   = false;
    m_AbortRequested       = false;
    m_NumThreads           = 0;
    m_NumEncodingThreads   = 0;  // new auto setting to use max processors * 2 threads
    m_EncodingThreadHandle = NULL;
    m_xdim                 = 4;
    m_ydim                 = 4;
    m_zdim                 = 1;
    m_decoder              = NULL;
    m_Quality              = 0.05;
}

CCodec_ASTC::~CCodec_ASTC()
{
    if (m_LibraryInitialized)
    {
        // The worker threads are started for every encoding thread slot, including the
        // single threaded case, so they must always be signalled and joined here. Leaving
        // them running keeps the thread polling g_EncodeParameterStorage and calling through
        // its encoder after both have been freed below.
        // Tell all the live threads that they can exit when they have finished any current work
        for (int i = 0; i < m_LiveThreads; i++)
        {
            // If a thread is in the running state then we need to wait for it to finish
            // any queued work from the producer before we can tell it to exit.
            //
            // If we don't wait then there is a race condition here where we have
            // told the thread to run but it hasn't yet been scheduled - if we set
            // the exit flag before it runs then its block will not be processed.
#pragma warning(push)
#pragma warning(disable : 4127)  //warning C4127: conditional expression is constant
            while (1)
            {
                if (g_EncodeParameterStorage == NULL)
                    break;
                if (!g_EncodeParameterStorage[i].run.load(std::memory_order_acquire))
                {
                    break;
                }
                // Yield instead of spinning: the worker may be waiting on a core this loop holds.
                std::this_thread::yield();
            }
#pragma warning(pop)
            // Signal to the thread that it can exit
            if (g_EncodeParameterStorage)
                g_EncodeParameterStorage[i].exit.store(true, std::memory_order_release);
        }

        // Now wait for all threads to have exited
        if (m_EncodingThreadHandle)
        {
            for (int i = 0; i < m_LiveThreads; i++)
            {
                std::thread& curThread = m_EncodingThreadHandle[i];

                if (curThread.joinable())
                    curThread.join();

                curThread = std::thread();
            }

            delete[] m_EncodingThreadHandle;
        }

        m_LiveThreads          = 0;
        m_EncodingThreadHandle = NULL;

        if (g_EncodeParameterStorage)
        {
            delete[] g_EncodeParameterStorage;
            g_EncodeParameterStorage = NULL;
        }

        for (int i = 0; i < m_NumEncodingThreads; i++)
        {
            if (m_encoder[i])
            {
                delete m_encoder[i];
                m_encoder[i] = NULL;
            }
        }

        if (m_decoder)
        {
            delete m_decoder;
            m_decoder = NULL;
        }

        m_LibraryInitialized = false;
    }
}

void CCodec_ASTC::find_closest_blockdim_2d(float target_bitrate, int* x, int* y, int consider_illegal)
{
    int blockdims[6] = {4, 5, 6, 8, 10, 12};

    float best_error     = 1000;
    float aspect_of_best = 1;
    int   i, j;

    // Y dimension
    for (i = 0; i < 6; i++)
    {
        // X dimension
        for (j = i; j < 6; j++)
        {
            //              NxN       MxN         8x5               10x5              10x6
            int is_legal = (j == i) || (j == i + 1) || (j == 3 && j == 1) || (j == 4 && j == 1) || (j == 4 && j == 2);

            if (consider_illegal || is_legal)
            {
                float bitrate       = 128.0f / (blockdims[i] * blockdims[j]);
                float bitrate_error = fabs(bitrate - target_bitrate);
                float aspect        = (float)blockdims[j] / blockdims[i];
                if (bitrate_error < best_error || (bitrate_error == best_error && aspect < aspect_of_best))
                {
                    *x             = blockdims[j];
                    *y             = blockdims[i];
                    best_error     = bitrate_error;
                    aspect_of_best = aspect;
                }
            }
        }
    }
}

void CCodec_ASTC::find_closest_blockxy_2d(int* x, int* y, int consider_illegal)
{
    int blockdims[6] = {4, 5, 6, 8, 10, 12};

    bool exists_x = std::find(std::begin(blockdims), std::end(blockdims), (*x)) != std::end(blockdims);
    bool exists_y = std::find(std::begin(blockdims), std::end(blockdims), (*y)) != std::end(blockdims);

    if (exists_x && exists_y)
    {
        if ((*x) < (*y))
        {
            int temp = *x;
            *x       = *y;
            *y       = temp;
        }
        float bitrateF = float(128.0f / ((*x) * (*y)));
        find_closest_blockdim_2d(bitrateF, x, y, 0);
    }
    else
    {
        float bitrateF = float(128.0f / ((*x) * (*y)));
        find_closest_blockdim_2d(bitrateF, x, y, 0);
    }
}

void CCodec_ASTC::find_closest_blockdim_3d(float target_bitrate, int* x, int* y, int* z, int consider_illegal)
{
    int blockdims[4] = {3, 4, 5, 6};

    float best_error     = 1000;
    float aspect_of_best = 1;
    int   i, j, k;

    for (i = 0; i < 4; i++)      // Z
        for (j = i; j < 4; j++)  // Y
            for (k = j; k < 4; k++)
            {  // X
                //              NxNxN              MxNxN                  MxMxN
                int is_legal = ((k == j) && (j == i)) || ((k == j + 1) && (j == i)) || ((k == j) && (j == i + 1));

                if (consider_illegal || is_legal)
                {
                    float bitrate       = 128.0f / (blockdims[i] * blockdims[j] * blockdims[k]);
                    float bitrate_error = fabs(bitrate - target_bitrate);
                    float aspect        = (float)blockdims[k] / blockdims[j] + (float)blockdims[j] / blockdims[i] + (float)blockdims[k] / blockdims[i];

                    if (bitrate_error < best_error || (bitrate_error == best_error && aspect < aspect_of_best))
                    {
                        *x             = blockdims[k];
                        *y             = blockdims[j];
                        *z             = blockdims[i];
                        best_error     = bitrate_error;
                        aspect_of_best = aspect;
                    }
                }
            }
}

bool CCodec_ASTC::SetParameter(const CMP_CHAR* pszParamName, CMP_CHAR* sValue)
{
    if (sValue == NULL)
        return false;

    if (strcmp(pszParamName, CodecParameters::NumThreads) == 0)
    {
        m_NumThreads = (CMP_BYTE)std::stoi(sValue) & 0xFF;
    }
    if (strcmp(pszParamName, "BlockRate") == 0)
    {
        // BlockRate can be a bit value or dimension

        if (strchr(sValue, '.') != NULL)
        {
            m_target_bitrate = static_cast<float>(atof(sValue));
            find_closest_blockdim_2d(m_target_bitrate, &m_xdim, &m_ydim, DEBUG_ALLOW_ILLEGAL_BLOCK_SIZES);
        }
        else
        {
            int dimensions = sscanf(sValue, "%dx%dx", &m_xdim, &m_ydim);
            if (dimensions < 2)
                return false;
            find_closest_blockxy_2d(&m_xdim, &m_ydim, DEBUG_ALLOW_ILLEGAL_BLOCK_SIZES);

            // Valid block sizes are for 2D support only  (3D is todo later)
            // are in cominations of {4,5,6,8,10,12}
            if ((m_xdim < 4) || (m_xdim > 12))
                return false;
            if ((m_ydim < 4) || (m_ydim > 12))
                return false;
            if ((m_xdim == 7) || (m_xdim == 9) || (m_xdim == 11))
                return false;
            if ((m_ydim == 7) || (m_ydim == 9) || (m_ydim == 11))
                return false;
        }
    }
    if (strcmp(pszParamName, "Quality") == 0)
    {
        m_Quality = std::stof(sValue);
        if ((m_Quality < 0) || (m_Quality > 1.0))
        {
            return false;
        }
    }
    else
        return CCodec_DXTC::SetParameter(pszParamName, sValue);
    return true;
}

bool CCodec_ASTC::SetParameter(const CMP_CHAR* pszParamName, CMP_DWORD dwValue)
{
    if (strcmp(pszParamName, CodecParameters::NumThreads) == 0)
    {
        m_NumThreads = (CMP_BYTE)dwValue;
    }
    else
        return CCodec_DXTC::SetParameter(pszParamName, dwValue);
    return true;
}

bool CCodec_ASTC::SetParameter(const CMP_CHAR* pszParamName, CODECFLOAT fValue)
{
    if (strcmp(pszParamName, "Quality") == 0)
        m_Quality = fValue;
    else
        return CCodec_DXTC::SetParameter(pszParamName, fValue);
    return true;
}

//
// Thread procedure for encoding a block
//
// The thread stays alive, and expects blocks to be pushed to it by a producer
// process that signals it when new work is available. When the producer is finished
// it should set the exit flag in the parameters to allow the tread to quit
//

#include "astc_host.h"
ASTC_Encoder::ASTC_Encode g_ASTCEncode;

unsigned int ASTCThreadProcEncode(void* param)
{
    ASTCEncodeThreadParam* tp = (ASTCEncodeThreadParam*)param;

    while (!tp->exit.load(std::memory_order_acquire))
    {
        // Acquire pairs with the producer's release store of run, so the block parameters written
        // by the producer are guaranteed visible here before the block is encoded.
        if (tp->run.load(std::memory_order_acquire))
        {
            g_ASTCEncode.m_xdim = tp->xdim;
            g_ASTCEncode.m_ydim = tp->ydim;
            g_ASTCEncode.m_zdim = tp->zdim;

            tp->encoder->CompressBlock_kernel((ASTC_Encoder::astc_codec_image*)tp->input_image, tp->bp, tp->x, tp->y, tp->z, &g_ASTCEncode);

            tp->run.store(false, std::memory_order_release);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    return 0;
}

CodecError CCodec_ASTC::InitializeASTCLibrary()
{
    if (!m_LibraryInitialized)
    {
        g_ASTCEncode.m_decode_mode            = ASTC_Encoder::DECODE_HDR;
        g_ASTCEncode.m_rgb_force_use_of_hdr   = 0;
        g_ASTCEncode.m_alpha_force_use_of_hdr = 0;
        g_ASTCEncode.m_perform_srgb_transform = 0;
        g_ASTCEncode.m_Quality                = (float)m_Quality;
        g_ASTCEncode.m_target_bitrate         = m_target_bitrate;
        g_ASTCEncode.m_xdim                   = m_xdim;
        g_ASTCEncode.m_ydim                   = m_ydim;
        g_ASTCEncode.m_zdim                   = m_zdim;
        ASTC_Encoder::init_ASTC(&g_ASTCEncode);

        //====================== Threads
        for (CMP_DWORD i = 0; i < MAX_ASTC_THREADS; i++)
        {
            m_encoder[i] = NULL;
        }

        // Create threaded encoder instances
        m_LiveThreads        = 0;
        m_LastThread         = 0;
        m_NumEncodingThreads = MIN(m_NumThreads, (decltype(m_NumThreads))MAX_ASTC_THREADS);
        if (m_NumEncodingThreads == 0)
        {
            m_NumEncodingThreads = CMP_GetNumberOfProcessors();
            if (m_NumEncodingThreads <= 2)
                m_NumEncodingThreads = 8;  // fallback to a default!
            if (m_NumEncodingThreads > 128)
                m_NumEncodingThreads = 128;
        }
        m_Use_MultiThreading = (m_NumEncodingThreads != 1);

        g_EncodeParameterStorage = new ASTCEncodeThreadParam[m_NumEncodingThreads];
        if (!g_EncodeParameterStorage)
        {
            return CE_Unknown;
        }

        m_EncodingThreadHandle = new std::thread[m_NumEncodingThreads];
        if (!m_EncodingThreadHandle)
        {
            delete[] g_EncodeParameterStorage;
            g_EncodeParameterStorage = NULL;

            return CE_Unknown;
        }

        CMP_INT i;

        for (i = 0; i < m_NumEncodingThreads; i++)
        {
            // Create single encoder instance
            m_encoder[i] = new ASTCBlockEncoder();

            // Cleanup if problem!
            if (!m_encoder[i])
            {
                delete[] g_EncodeParameterStorage;
                g_EncodeParameterStorage = NULL;

                delete[] m_EncodingThreadHandle;
                m_EncodingThreadHandle = NULL;

                for (CMP_INT j = 0; j < i; j++)
                {
                    delete m_encoder[j];
                    m_encoder[j] = NULL;
                }

                return CE_Unknown;
            }

#ifdef USE_DBGTRACE
            //DbgTrace(("Encoder[%d]:ModeMask %X, Quality %f", i, m_ModeMask, m_Quality));
#endif
        }

        // m_LibraryInitialized stays false on every path below, so the destructor will not tear
        // the pool down: any worker already started still polls g_EncodeParameterStorage and
        // calls through m_encoder, and destroying an unjoined std::thread calls std::terminate().
        auto shutdownEncoderPool = [this]() {
            for (int t = 0; t < m_LiveThreads; t++)
                g_EncodeParameterStorage[t].exit.store(true, std::memory_order_release);

            for (int t = 0; t < m_LiveThreads; t++)
            {
                std::thread& curThread = m_EncodingThreadHandle[t];
                if (curThread.joinable())
                    curThread.join();
                curThread = std::thread();
            }
            m_LiveThreads = 0;

            delete[] m_EncodingThreadHandle;
            m_EncodingThreadHandle = NULL;

            delete[] g_EncodeParameterStorage;
            g_EncodeParameterStorage = NULL;

            for (CMP_INT j = 0; j < m_NumEncodingThreads; j++)
            {
                delete m_encoder[j];
                m_encoder[j] = NULL;
            }
        };

        // Create the encoding threads
        for (i = 0; i < m_NumEncodingThreads; i++)
        {
            // Initialize thread parameters.
            g_EncodeParameterStorage[i].encoder = m_encoder[i];
            // Inform the thread that at the moment it doesn't have any work to do
            // but that it should wait for some and not exit
            g_EncodeParameterStorage[i].run.store(false, std::memory_order_relaxed);
            g_EncodeParameterStorage[i].exit.store(false, std::memory_order_relaxed);

            try
            {
                m_EncodingThreadHandle[i] = std::thread(ASTCThreadProcEncode, (void*)&g_EncodeParameterStorage[i]);
            }
            catch (const std::system_error&)
            {
                // Out of thread handles: without this the exception escapes with the workers
                // started so far left spinning for the life of the process.
                shutdownEncoderPool();
                return CE_Unknown;
            }
            m_LiveThreads++;
        }

        // Create single decoder instance
        m_decoder = new ASTCBlockDecoder();

        if (!m_decoder)
        {
            shutdownEncoderPool();
            return CE_Unknown;
        }

        m_LibraryInitialized = true;
    }
    return CE_OK;
}

CodecError CCodec_ASTC::EncodeASTCBlock(astc_codec_image* input_image, uint8_t* bp, int xdim, int ydim, int zdim, int x, int y, int z)
{
    if (m_Use_MultiThreading)
    {
        CMP_WORD threadIndex;

        // Loop and look for an available thread
        CMP_BOOL found = FALSE;
        threadIndex    = m_LastThread;
        while (found == FALSE)
        {
            if (g_EncodeParameterStorage == NULL)
                return CE_Unknown;

            if (!g_EncodeParameterStorage[threadIndex].run.load(std::memory_order_acquire))
            {
                found = TRUE;
                break;
            }

            // Increment and wrap the thread index
            threadIndex++;
            if (threadIndex == m_LiveThreads)
            {
                threadIndex = 0;
            }
        }

        m_LastThread = threadIndex;

        g_EncodeParameterStorage[threadIndex].input_image = input_image;
        g_EncodeParameterStorage[threadIndex].bp          = bp;
        g_EncodeParameterStorage[threadIndex].xdim        = xdim;
        g_EncodeParameterStorage[threadIndex].ydim        = ydim;
        g_EncodeParameterStorage[threadIndex].zdim        = zdim;
        g_EncodeParameterStorage[threadIndex].x           = x;
        g_EncodeParameterStorage[threadIndex].y           = y;
        g_EncodeParameterStorage[threadIndex].z           = z;
        // Tell the thread to start working
        g_EncodeParameterStorage[threadIndex].run.store(true, std::memory_order_release);
    }
    else
    {
        g_ASTCEncode.m_xdim = xdim;
        g_ASTCEncode.m_ydim = ydim;
        g_ASTCEncode.m_zdim = zdim;

        m_encoder[0]->CompressBlock_kernel((ASTC_Encoder::astc_codec_image*)input_image, bp, x, y, z, &g_ASTCEncode);
    }
    return CE_OK;
}

CodecError CCodec_ASTC::FinishASTCEncoding(void)
{
    if (!m_LibraryInitialized)
    {
        return CE_Unknown;
    }

    if (!g_EncodeParameterStorage)
    {
        return CE_Unknown;
    }

    if (m_Use_MultiThreading)
    {
        // Wait for all the live threads to finish any current work
        for (CMP_DWORD i = 0; i < m_LiveThreads; i++)
        {
            // If a thread is in the running state then we need to wait for it to finish
            // its work from the producer
            while (g_EncodeParameterStorage[i].run.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    return CE_OK;
}

struct encode_astc_image_info
{
    int                           xdim;
    int                           ydim;
    int                           zdim;
    const error_weighting_params* ewp;
    uint8_t*                      buffer;
    int                           thread_id;
    int                           threadcount;
    astc_decode_mode              decode_mode;
    swizzlepattern                swz_encode;
    volatile int*                 counters;
    volatile int*                 threads_completed;
    const astc_codec_image*       input_image;
    Codec_Feedback_Proc           pFeedbackProc;
    CMP_DWORD_PTR                 pUser1;
    CMP_DWORD_PTR                 pUser2;
};

#define USE_ARM_CODE

CodecError CCodec_ASTC::Compress(CCodecBuffer& bufferIn, CCodecBuffer& bufferOut, Codec_Feedback_Proc pFeedbackProc, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    m_AbortRequested = false;

    int xsize = bufferIn.GetWidth();
    int ysize = bufferIn.GetHeight();
    int zsize = 1;  //todo: add depth to support 3d textures
    m_xdim    = bufferOut.GetBlockWidth();
    m_ydim    = bufferOut.GetBlockHeight();
    m_zdim    = 1;

    CodecError err = InitializeASTCLibrary();
    if (err != CE_OK)
        return err;

#ifdef ASTC_COMPDEBUGGER
    CompViewerClient g_CompClient;
    if (g_CompClient.connect())
    {
#ifdef USE_DBGTRACE
        DbgTrace(("-------> Remote Server Connected\n"));
#endif
    }
#endif

#ifdef USE_DBGTRACE
    DbgTrace(("IN : BufferType %d ChannelCount %d ChannelDepth %d", bufferIn.GetBufferType(), bufferIn.GetChannelCount(), bufferIn.GetChannelDepth()));
    DbgTrace(("   : Height %d Width %d Pitch %d isFloat %d", bufferIn.GetHeight(), bufferIn.GetWidth(), bufferIn.GetWidth(), bufferIn.IsFloat()));

    DbgTrace(("OUT: BufferType %d ChannelCount %d ChannelDepth %d", bufferOut.GetBufferType(), bufferOut.GetChannelCount(), bufferOut.GetChannelDepth()));
    DbgTrace(("   : Height %d Width %d Pitch %d isFloat %d", bufferOut.GetHeight(), bufferOut.GetWidth(), bufferOut.GetWidth(), bufferOut.IsFloat()));
#endif

    int bitness = 0;  //todo: replace astc_codec_image with bufferIn and rewrite fetch_imageblock()
    switch (bufferIn.GetBufferType())
    {
    case CBT_BGRA8888:
    case CBT_ARGB8888:
    case CBT_RGBA8888:
    case CBT_RGB888:
    case CBT_RG8:
    case CBT_R8:
        bitness = 8;
        break;
    case CBT_RGBA8888S:
    case CBT_RGB888S:
    case CBT_RG8S:
    case CBT_R8S:
        bitness = 8;
        break;
    case CBT_RGBA2101010:
        break;
    case CBT_RGBA16:
    case CBT_RG16:
    case CBT_R16:
        break;
    case CBT_RGBA32:
    case CBT_RG32:
    case CBT_R32:
        break;
    case CBT_RGBA16F:
    case CBT_RG16F:
    case CBT_R16F:
        break;
    case CBT_RGBA32F:
    case CBT_RG32F:
    case CBT_R32F:
        break;
    default:
        break;
    }

    if (bitness != 8)
        assert("Unsupported type of input buffer");

    astc_codec_image_cpu* input_image = allocate_image_cpu(bitness, xsize, ysize, zsize, 0);

    if (!input_image)
        assert("Unable to allocate image buffer");

    // Loop through the original input image and setup compression threads for each
    // block to encode  we will load the buffer to pass to ASTC code as 8 bit 4x4 blocks
    // the fill in source image. ASTC code will then use the adaptive sizes for process on the input
    BYTE* pData = bufferIn.GetData();
    int   ii    = 0;
    for (int y = 0; y < ysize; y++)
    {
        for (int x = 0; x < xsize; x++)
        {
            input_image->imagedata8[0][y][4 * x] = pData[ii];  // Red
            ii++;
            input_image->imagedata8[0][y][4 * x + 1] = pData[ii];  // Green
            ii++;
            input_image->imagedata8[0][y][4 * x + 2] = pData[ii];  // Blue
            ii++;
            input_image->imagedata8[0][y][4 * x + 3] = pData[ii];  // Alpha
            ii++;
        }
    }

    m_NumEncodingThreads = MIN(m_NumThreads, (decltype(m_NumThreads))MAX_ASTC_THREADS);
    if (m_NumEncodingThreads == 0)
    {
        m_NumEncodingThreads = CMP_GetNumberOfProcessors();
        if (m_NumEncodingThreads <= 2)
            m_NumEncodingThreads = 8;  // fallback to a default!
        if (m_NumEncodingThreads > 128)
            m_NumEncodingThreads = 128;
    }

    // Common ARM and AMD Code
    CodecError result       = CE_OK;
    int        xdim         = m_xdim;
    int        ydim         = m_ydim;
    int        zdim         = m_zdim;
    uint8_t*   bufferOutput = bufferOut.GetData();

    // Common ARM and Compressonator Code
    int   x, y, z, i;
    int   xblocks         = (xsize + xdim - 1) / xdim;
    int   yblocks         = (ysize + ydim - 1) / ydim;
    int   zblocks         = (zsize + zdim - 1) / zdim;
    float TotalBlocks     = (float)(yblocks * xblocks);
    int   processingBlock = 0;

    for (z = 0; z < zblocks; z++)
    {
        for (y = 0; y < yblocks; y++)
        {
            for (x = 0; x < xblocks; x++)
            {
                int      offset = ((z * yblocks + y) * xblocks + x) * 16;
                uint8_t* bp     = bufferOutput + offset;
                EncodeASTCBlock((astc_codec_image*)input_image, bp, xdim, ydim, zdim, x * xdim, y * ydim, z * zdim);
                processingBlock++;
            }

            if (pFeedbackProc)
            {
                float fProgress = 100.f * ((float)(processingBlock) / TotalBlocks);
                if (pFeedbackProc(fProgress, pUser1, pUser2))
                {
                    result = CE_Aborted;
                    break;
                }
            }
        }
    }

    CodecError EncodeResult = FinishASTCEncoding();

    if (result != CE_Aborted)
        result = EncodeResult;

    destroy_image_cpu(input_image);

#ifdef ASTC_COMPDEBUGGER
    g_CompClient.disconnect();
#endif

    return result;
}

// notes:
// Slow CPU based decompression : Should look into also using HW based decompression with this interface
//
CodecError CCodec_ASTC::Decompress(CCodecBuffer&       bufferIn,
                                   CCodecBuffer&       bufferOut,
                                   Codec_Feedback_Proc pFeedbackProc,
                                   CMP_DWORD_PTR       pUser1,
                                   CMP_DWORD_PTR       pUser2)
{
    m_xdim = bufferIn.GetBlockWidth();
    m_ydim = bufferIn.GetBlockHeight();
    m_zdim = 1;

    CodecError err = InitializeASTCLibrary();
    if (err != CE_OK)
        return err;

    // Our Compressed data Blocks are always 128 bit long (4x4 blocks)
    const CMP_DWORD imageWidth  = bufferIn.GetWidth();
    const CMP_DWORD imageHeight = bufferIn.GetHeight();
    const CMP_DWORD imageDepth  = 1;
    const BYTE      bitness     = 8;

    const CMP_DWORD CompBlockX   = bufferIn.GetBlockWidth();
    const CMP_DWORD CompBlockY   = bufferIn.GetBlockHeight();
    CMP_BYTE        Block_Width  = bufferIn.GetBlockWidth();
    CMP_BYTE        Block_Height = bufferIn.GetBlockHeight();

    const CMP_DWORD dwBlocksX       = ((bufferIn.GetWidth() + (CompBlockX - 1)) / CompBlockX);
    const CMP_DWORD dwBlocksY       = ((bufferIn.GetHeight() + (CompBlockY - 1)) / CompBlockY);
    const CMP_DWORD dwBlocksZ       = 1;
    const CMP_DWORD dwBufferInDepth = 1;

    // Override the current input buffer Pitch size  (Since it will be set according to the Compressed Block Sizes
    // and not to the Compressed Codec data which is for ASTC 16 Bytes per block x Number of blocks per row
    bufferIn.SetPitch(16 * dwBlocksX);

    // Output data size Pitch
    CMP_DWORD dwPitch = bufferOut.GetPitch();

    // Output Buffer
    BYTE* pDataOut = bufferOut.GetData();

    const CMP_DWORD dwBlocksXY = dwBlocksX * dwBlocksY;

    for (CMP_DWORD cmpRowY = 0; cmpRowY < dwBlocksY; cmpRowY++)
    {  // Compressed images row = height
        for (CMP_DWORD cmpColX = 0; cmpColX < dwBlocksX; cmpColX++)
        {  // Compressed images Col = width
            union FBLOCKS
            {
                float decodedBlock[144][4];  // max 12x12 block size
                float destBlock[576];        // max 12x12x4
            } DecData;

            union BBLOCKS
            {
                CMP_DWORD compressedBlock[4];
                BYTE      out[16];
                BYTE      in[16];
            } CompData;

            bufferIn.ReadBlock(cmpColX * 4, cmpRowY * 4, CompData.compressedBlock, 4);

            // Encode to the appropriate location in the compressed image
            m_decoder->DecompressBlock(Block_Width, Block_Height, bitness, DecData.decodedBlock, CompData.in);

            // Now that we have a decoded block lets copy that data over to the target image buffer
            CMP_DWORD outCol    = cmpColX * Block_Width;
            CMP_DWORD outRow    = cmpRowY * Block_Height;
            CMP_DWORD outImgRow = outRow;
            CMP_DWORD outImgCol = outCol;

            for (int row = 0; row < Block_Height; row++)
            {
                CMP_DWORD nextRowCol = (outRow + row) * dwPitch + (outCol * 4);
                CMP_BYTE* pData      = (CMP_BYTE*)(pDataOut + nextRowCol);
                if ((outImgRow + row) < imageHeight)
                {
                    outImgCol = outCol;
                    for (int col = 0; col < Block_Width; col++)
                    {
                        CMP_DWORD w = outImgCol + col;
                        if (w < imageWidth)
                        {
                            int index = row * Block_Width + col;
                            *pData++  = (CMP_BYTE)DecData.decodedBlock[index][BC_COMP_RED];
                            *pData++  = (CMP_BYTE)DecData.decodedBlock[index][BC_COMP_GREEN];
                            *pData++  = (CMP_BYTE)DecData.decodedBlock[index][BC_COMP_BLUE];
                            *pData++  = (CMP_BYTE)DecData.decodedBlock[index][BC_COMP_ALPHA];
                        }
                        else
                            break;
                    }
                }
            }
        }

        if (pFeedbackProc)
        {
            float fProgress = 100.f * (cmpRowY * dwBlocksX) / dwBlocksXY;
            if (pFeedbackProc(fProgress, pUser1, pUser2))
            {
                return CE_Aborted;
            }
        }
    }

    return CE_OK;
}
