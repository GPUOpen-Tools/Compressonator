//===============================================================================
// Copyright (c) 2014-2024  Advanced Micro Devices, Inc. All rights reserved.
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
//  File Name:   Codec_BC7.cpp
//  Description: implementation of the CCodec_BC7 class
//
//////////////////////////////////////////////////////////////////////////////

#pragma warning(disable : 4100)  // Ignore warnings of unreferenced formal parameters
#include "common.h"
#include "codec_bc7.h"
#include "bc7_library.h"
#include <chrono>
#include <system_error>

#ifdef BC7_COMPDEBUGGER
#include "compclient.h"
#endif

#ifdef BC7_COMPDEBUGGER
#ifndef _DEBUG
#undef BC7_COMPDEBUGGER
#endif
#endif

//======================================================================================
#ifdef USE_FILEIO
#include <stdio.h>
FILE* bc7_File       = NULL;
int   bc7_blockcount = 0;
int   bc7_total_MSE  = 0;
#endif

// Gets the total numver of active processor cores on the running host system
extern CMP_INT CMP_GetNumberOfProcessors();

//
// Thread procedure for encoding a block
//
// The thread stays alive, and expects blocks to be pushed to it by a producer
// process that signals it when new work is available. When the producer is finished
// it should set the exit flag in the parameters to allow the tread to quit
//

unsigned int BC7ThreadProcEncode(void* param)
{
    BC7EncodeThreadParam* tp = (BC7EncodeThreadParam*)param;

    while (!tp->exit.load(std::memory_order_acquire))
    {
        // Acquire pairs with the producer's release store of run, so tp->in and tp->out are
        // guaranteed visible here before the block is encoded.
        if (tp->run.load(std::memory_order_acquire))
        {
            tp->encoder->CompressBlock(tp->in, tp->out);
            tp->run.store(false, std::memory_order_release);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////////////

CCodec_BC7::CCodec_BC7()
    : CCodec_DXTC(CT_BC7)
{
    m_LibraryInitialized = false;

    m_Use_MultiThreading = true;
    m_ModeMask           = 0xCF;  // If you reset this default: seach for comments with dwmodeMask and change the values also
    m_Quality            = AMD_CODEC_QUALITY_DEFAULT;
    m_Performance        = 1.00;
    m_ColourRestrict     = FALSE;
    m_AlphaRestrict      = FALSE;
    m_ImageNeedsAlpha    = TRUE;

    m_NumThreads           = 0;
    m_NumEncodingThreads   = m_NumThreads;
    m_EncodingThreadHandle = NULL;
    m_LiveThreads          = 0;
    m_LastThread           = 0;
}

bool CCodec_BC7::SetParameter(const CMP_CHAR* pszParamName, CMP_CHAR* sValue)
{
    if (sValue == NULL)
        return false;

    if (strcmp(pszParamName, "ModeMask") == 0)
    {
        m_ModeMask = (CMP_BYTE)std::stoi(sValue) & 0xFF;
        if (m_ModeMask <= 0)
            m_ModeMask = 0xFF;
    }
    else if (strcmp(pszParamName, "ColourRestrict") == 0)
        m_ColourRestrict = std::stoi(sValue) > 0 ? TRUE : FALSE;
    else if (strcmp(pszParamName, "AlphaRestrict") == 0)
        m_AlphaRestrict = std::stoi(sValue) > 0 ? TRUE : FALSE;
    else if (strcmp(pszParamName, "ImageNeedsAlpha") == 0)
        m_ImageNeedsAlpha = std::stoi(sValue) > 0 ? TRUE : FALSE;
    else if (strcmp(pszParamName, CodecParameters::NumThreads) == 0)
    {
        m_NumThreads         = (CMP_BYTE)std::stoi(sValue) & 0xFF;
        m_Use_MultiThreading = m_NumThreads != 1;
        //printf("BC7 CPU set threads = %d\n",m_NumThreads);
    }
    if (strcmp(pszParamName, "Quality") == 0)
    {
        m_Quality = std::stof(sValue);
        if ((m_Quality < 0) || (m_Quality > 1.0))
        {
            return false;
        }
    }
    else if (strcmp(pszParamName, "Performance") == 0)
    {
        m_Performance = std::stof(sValue);
        if ((m_Performance < 0) || (m_Performance > 1.0))
        {
            return false;
        }
    }
    else
        return CCodec_DXTC::SetParameter(pszParamName, sValue);
    return true;
}

bool CCodec_BC7::SetParameter(const CMP_CHAR* pszParamName, CMP_DWORD dwValue)
{
    if (strcmp(pszParamName, "ModeMask") == 0)
        m_ModeMask = (CMP_BYTE)dwValue & 0xFF;
    else if (strcmp(pszParamName, "ColourRestrict") == 0)
        m_ColourRestrict = (dwValue & 1) ? TRUE : FALSE;
    else if (strcmp(pszParamName, "AlphaRestrict") == 0)
        m_AlphaRestrict = (dwValue & 1) ? TRUE : FALSE;
    else if (strcmp(pszParamName, "ImageNeedsAlpha") == 0)
        m_ImageNeedsAlpha = (dwValue & 1) ? TRUE : FALSE;
    else if (strcmp(pszParamName, CodecParameters::NumThreads) == 0)
    {
        m_NumThreads         = (CMP_BYTE)dwValue;
        m_Use_MultiThreading = (m_NumThreads != 1) ? TRUE : FALSE;
    }
    else
        return CCodec_DXTC::SetParameter(pszParamName, dwValue);
    return true;
}

bool CCodec_BC7::SetParameter(const CMP_CHAR* pszParamName, CODECFLOAT fValue)
{
    if (strcmp(pszParamName, "Quality") == 0)
        m_Quality = fValue;
    else if (strcmp(pszParamName, "Performance") == 0)
        m_Performance = fValue;
    else
        return CCodec_DXTC::SetParameter(pszParamName, fValue);
    return true;
}

CCodec_BC7::~CCodec_BC7()
{
    if (m_LibraryInitialized)
    {
        // The worker threads are started for every encoding thread slot, including the
        // single threaded case, so they must always be signalled and joined here. Leaving
        // them running keeps the thread polling m_EncodeParameterStorage and calling through
        // m_encoder after both have been freed below.
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
                if (m_EncodeParameterStorage == NULL)
                    break;
                if (!m_EncodeParameterStorage[i].run.load(std::memory_order_acquire))
                {
                    break;
                }
                // Yield instead of spinning: the worker may be waiting on a core this loop holds.
                std::this_thread::yield();
            }
#pragma warning(pop)
            // Signal to the thread that it can exit
            if (m_EncodeParameterStorage)
                m_EncodeParameterStorage[i].exit.store(true, std::memory_order_release);
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

        if (m_EncodeParameterStorage)
            delete[] m_EncodeParameterStorage;
        m_EncodeParameterStorage = NULL;

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

        Quant_DeInit();

        m_LibraryInitialized = false;
    }
}

CodecError CCodec_BC7::InitializeBC7Library()
{
    if (!m_LibraryInitialized)
    {
        // One time initialisation for quantizer and shaker
        Quant_Init();

        for (CMP_DWORD i = 0; i < MAX_BC7_THREADS; i++)
        {
            m_encoder[i] = NULL;
        }

        // Create threaded encoder instances
        m_LiveThreads = 0;
        m_LastThread  = 0;
        //printf("BC7 CPU Num user threads = %d\n",m_NumEncodingThreads);
        m_NumEncodingThreads = cmp_minT(m_NumThreads, MAX_BC7_THREADS);
        if (m_NumEncodingThreads == 0)
        {
            m_NumEncodingThreads = CMP_GetNumberOfProcessors();
            if (m_NumEncodingThreads <= 2)
                m_NumEncodingThreads = 8;  // fallback to a default!
            if (m_NumEncodingThreads > 128)
                m_NumEncodingThreads = 128;
        }
        m_Use_MultiThreading = (m_NumEncodingThreads != 1);

        m_EncodeParameterStorage = new BC7EncodeThreadParam[m_NumEncodingThreads];
        if (!m_EncodeParameterStorage)
        {
            return CE_Unknown;
        }

        m_EncodingThreadHandle = new std::thread[m_NumEncodingThreads];
        if (!m_EncodingThreadHandle)
        {
            delete[] m_EncodeParameterStorage;
            m_EncodeParameterStorage = NULL;

            return CE_Unknown;
        }

        CMP_INT i;
        //printf("BC7 CPU Num threads used = %d\n",m_NumEncodingThreads);
        for (i = 0; i < m_NumEncodingThreads; i++)
        {
            // Create single encoder instance
            m_encoder[i] = new BC7BlockEncoder(m_ModeMask, m_ImageNeedsAlpha, m_Quality, m_ColourRestrict, m_AlphaRestrict, m_Performance);

            // Cleanup if problem!
            if (!m_encoder[i])
            {
                delete[] m_EncodeParameterStorage;
                m_EncodeParameterStorage = NULL;

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
            DbgTrace(("Encoder[%d]:ModeMask %X, Quality %f", i, m_ModeMask, m_Quality));
#endif
        }

        // m_LibraryInitialized stays false on every path below, so the destructor will not tear
        // the pool down: any worker already started still polls m_EncodeParameterStorage and
        // calls through m_encoder, and destroying an unjoined std::thread calls std::terminate().
        auto shutdownEncoderPool = [this]() {
            for (int t = 0; t < m_LiveThreads; t++)
                m_EncodeParameterStorage[t].exit.store(true, std::memory_order_release);

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

            delete[] m_EncodeParameterStorage;
            m_EncodeParameterStorage = NULL;

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
            m_EncodeParameterStorage[i].encoder = m_encoder[i];
            // Inform the thread that at the moment it doesn't have any work to do
            // but that it should wait for some and not exit
            m_EncodeParameterStorage[i].run.store(false, std::memory_order_relaxed);
            m_EncodeParameterStorage[i].exit.store(false, std::memory_order_relaxed);

            try
            {
                m_EncodingThreadHandle[i] = std::thread(BC7ThreadProcEncode, (void*)&m_EncodeParameterStorage[i]);
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
        m_decoder = new BC7BlockDecoder();
        if (!m_decoder)
        {
            shutdownEncoderPool();
            return CE_Unknown;
        }

        m_LibraryInitialized = true;
    }
    return CE_OK;
}

CodecError CCodec_BC7::EncodeBC7Block(double in[BC7_BLOCK_PIXELS][MAX_DIMENSION_BIG], CMP_BYTE* out)
{
#ifdef USE_SINGLETHREADING
    m_Use_MultiThreading = false;
#endif

    if (m_Use_MultiThreading)
    {
        CMP_WORD threadIndex;

        if ((!m_LibraryInitialized) || (!in) || (!out))
        {
            return CE_Unknown;
        }

        // Loop and look for an available thread
        CMP_BOOL found = FALSE;
        threadIndex    = m_LastThread;
        while (found == FALSE)
        {
            if (m_EncodeParameterStorage == NULL)
                return CE_Unknown;

            if (!m_EncodeParameterStorage[threadIndex].run.load(std::memory_order_acquire))
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

        // Copy the input data into the thread storage
        std::memcpy(m_EncodeParameterStorage[threadIndex].in, in, MAX_SUBSET_SIZE * MAX_DIMENSION_BIG * sizeof(double));

        // Set the output pointer for the thread to the provided location
        m_EncodeParameterStorage[threadIndex].out = out;

        // Tell the thread to start working
        m_EncodeParameterStorage[threadIndex].run.store(true, std::memory_order_release);
    }
    else
    {
        //printf("BC7 CPU Single Threaded\n");
        // Copy the input data into the thread storage
        std::memcpy(m_EncodeParameterStorage[0].in, in, MAX_SUBSET_SIZE * MAX_DIMENSION_BIG * sizeof(double));
        // Set the output pointer for the thread to write
        m_EncodeParameterStorage[0].out = out;
        m_encoder[0]->CompressBlock(m_EncodeParameterStorage[0].in, m_EncodeParameterStorage[0].out);
    }
    return CE_OK;
}

CodecError CCodec_BC7::FinishBC7Encoding(void)
{
    if (!m_LibraryInitialized)
    {
        return CE_Unknown;
    }

    if (!m_EncodeParameterStorage)
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
            while (m_EncodeParameterStorage[i].run.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    return CE_OK;
}

#ifdef USE_THREADED_CALLBACKS
//#include <atomic>
//std::atomic<bool> cmp_bc7_end_process(false);
//std::atomic<bool> cmp_bc7_progress_update(false);
bool cmp_bc7_progress_update(false);

CMP_PROGRESS_THREAD CCodec_BC7::m_progress           = {0.0f, false};
Codec_Feedback_Proc CCodec_BC7::m_user_pFeedbackProc = nullptr;
CMP_DWORD_PTR       CCodec_BC7::m_pUser1             = NULL;
CMP_DWORD_PTR       CCodec_BC7::m_pUser2             = NULL;

// Used by a none-blocking thread while compresion is in progress!
void CCodec_BC7::Run()
{
    // printf("Block:");
    // char ch = getchar();
    while (!m_progress.abort)
    {
        if (cmp_bc7_progress_update)
        {
            cmp_bc7_progress_update = false;
            if (m_user_pFeedbackProc)
            {
                m_progress.abort = m_user_pFeedbackProc(m_progress.progress, CCodec_BC7::m_pUser1, CCodec_BC7::m_pUser2);
            }
        }
    }
}
#endif

CodecError CCodec_BC7::Compress(CCodecBuffer& bufferIn, CCodecBuffer& bufferOut, Codec_Feedback_Proc pFeedbackProc, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    assert(bufferIn.GetWidth() == bufferOut.GetWidth());
    assert(bufferIn.GetHeight() == bufferOut.GetHeight());

    if (bufferIn.GetWidth() != bufferOut.GetWidth() || bufferIn.GetHeight() != bufferOut.GetHeight())
        return CE_Unknown;

    CodecError err = InitializeBC7Library();
    if (err != CE_OK)
        return err;

#ifdef USE_THREADED_CALLBACKS
    // Create a progress thread that will track
    // the current progress of encoding 100% = done

    m_user_pFeedbackProc    = pFeedbackProc;
    m_pUser1                = pUser1;
    m_pUser2                = pUser2;
    cmp_bc7_progress_update = false;
    m_progress.abort        = false;
    m_progress.progress     = 0.0f;

    // Spawn a progress thread
    std::thread* cmp_progress = new std::thread(CCodec_BC7::Run);  // cmp_bc7_progress);
#else
    float progress;
    float old_progress = FLT_MAX;
#endif

#ifdef BC7_COMPDEBUGGER
    CompViewerClient CompClient;
    bool             hold_UseMultitheading = m_Use_MultiThreading;
    if (CompClient.connect())
    {
        m_Use_MultiThreading = false;
#ifdef USE_DBGTRACE
        DbgTrace(("-------> Remote Server Connected"));
#endif
    }
#endif

    const CMP_DWORD dwBlocksX     = ((bufferIn.GetWidth() + 3) >> 2);
    const CMP_DWORD dwBlocksY     = ((bufferIn.GetHeight() + 3) >> 2);
    const CMP_FLOAT fBlocksXY     = (CMP_FLOAT)(dwBlocksX * dwBlocksY);
    int             lineAtPercent = (int)(dwBlocksY * 0.01F);
    if (lineAtPercent <= 0)
        lineAtPercent = 1;

#ifdef USE_DBGTRACE
    DbgTrace(("***********-----------START-------------***********"));
    DbgTrace(("IN : BufferType %d ChannelCount %d ChannelDepth %d", bufferIn.GetBufferType(), bufferIn.GetChannelCount(), bufferIn.GetChannelDepth()));
    DbgTrace(("   : Height %d Width %d Pitch %d isFloat %d", bufferIn.GetHeight(), bufferIn.GetWidth(), bufferIn.GetWidth(), bufferIn.IsFloat()));

    DbgTrace(("OUT: BufferType %d ChannelCount %d ChannelDepth %d", bufferOut.GetBufferType(), bufferOut.GetChannelCount(), bufferOut.GetChannelDepth()));
    DbgTrace(("   : Height %d Width %d Pitch %d isFloat %d", bufferOut.GetHeight(), bufferOut.GetWidth(), bufferOut.GetWidth(), bufferOut.IsFloat()));
#endif

    char row, col, srcIndex;

    CMP_BYTE* pOutBuffer;
    pOutBuffer = bufferOut.GetData();

    CMP_BYTE* pInBuffer;
    pInBuffer = bufferIn.GetData();

#ifdef USE_FILEIO
    bc7_File       = fopen("bc7_report.txt", "w");
    bc7_blockcount = 0;
    bc7_total_MSE  = 0;
#endif

    CMP_DWORD block = 0;
    for (CMP_DWORD j = 0; j < dwBlocksY; j++)
    {
        for (CMP_DWORD i = 0; i < dwBlocksX; i++)
        {
#ifdef USE_FILEIO
            if (bc7_File)
                fprintf(bc7_File, "\nBlock:%4d x=%3d y=%3d\n", bc7_blockcount++, i, j);
#endif

#ifdef USE_DBGTRACE
            DbgTrace(("--------------  Block: x=%3d y=%3d ---------------", i, j));
#endif

            double   blockToEncode[BLOCK_SIZE_4X4][CHANNEL_SIZE_ARGB];
            CMP_BYTE srcBlock[BLOCK_SIZE_4X4X4];

            memset(srcBlock, 0, sizeof(srcBlock));
            bufferIn.ReadBlockRGBA(i * 4, j * 4, 4, 4, srcBlock);

#ifdef BC7_COMPDEBUGGER
            if (CompClient.Connected())
            {
                CompClient.SendData(1, sizeof(srcBlock), srcBlock);
            }
#endif

            // Create the block for encoding
            srcIndex = 0;
            for (row = 0; row < BLOCK_SIZE_4; row++)
            {
                for (col = 0; col < BLOCK_SIZE_4; col++)
                {
                    blockToEncode[row * BLOCK_SIZE_4 + col][BC_COMP_RED]   = (double)srcBlock[srcIndex];
                    blockToEncode[row * BLOCK_SIZE_4 + col][BC_COMP_GREEN] = (double)srcBlock[srcIndex + 1];
                    blockToEncode[row * BLOCK_SIZE_4 + col][BC_COMP_BLUE]  = (double)srcBlock[srcIndex + 2];
                    blockToEncode[row * BLOCK_SIZE_4 + col][BC_COMP_ALPHA] = (double)srcBlock[srcIndex + 3];
                    srcIndex += 4;
                }
            }

            // printf("[i %3d, j%3d]\n",i,j);
            EncodeBC7Block(blockToEncode, pOutBuffer + block);

#ifdef BC7_COMPDEBUGGER  // Checks decompression it should match or be close to source
            if (CompClient.Connected())
            {
                CMP_BYTE destBlock[BLOCK_SIZE_4X4X4];

                union FBLOCKS
                {
                    double decodedBlock[16][4];
                    double destBlock[BLOCK_SIZE_4X4X4];
                } DecData;

                // Encode to the appropriate location in the compressed image
                m_decoder->DecompressBlock(DecData.decodedBlock, pOutBuffer + block);

                // Create the block for decoding
                int srcIndex = 0;
                for (int row = 0; row < BLOCK_SIZE_4; row++)
                {
                    for (int col = 0; col < BLOCK_SIZE_4; col++)
                    {
                        destBlock[srcIndex]     = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_RED];
                        destBlock[srcIndex + 1] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_GREEN];
                        destBlock[srcIndex + 2] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_BLUE];
                        destBlock[srcIndex + 3] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_ALPHA];
                        srcIndex += 4;
                    }
                }
                CompClient.SendData(2, sizeof(destBlock), destBlock);
            }
#endif
#ifdef USE_FILEIO
            // Analysis
            double   blockToSave[16][4];
            CMP_BYTE cmp[16];
            memcpy(cmp, pOutBuffer + block, 16);

            m_decoder->DecompressBlock(blockToSave, cmp);

            double bMSE = 0, gMSE = 0, rMSE = 0;
            int    MSE = 0, PSNR = 0;

            for (int y = 0; y < 16; y++)
            {
                bMSE += pow(blockToSave[y][2] - blockToEncode[y][2], 2.0);
                gMSE += pow(blockToSave[y][1] - blockToEncode[y][1], 2.0);
                rMSE += pow(blockToSave[y][0] - blockToEncode[y][0], 2.0);
            }

            bMSE *= (1.0 / 16);
            gMSE *= (1.0 / 16);
            rMSE *= (1.0 / 16);
            MSE = int((bMSE + gMSE + rMSE) / 3);

            if (MSE == 0)
            {
                if (bc7_File)
                    fprintf(bc7_File, "MSE 000 PSNR ---\n", MSE);
            }
            else
            {
                bc7_total_MSE += MSE;
                PSNR = (int)(20 * log10(pow(2.0, 8.0) - 1) - 10 * log10(MSE));
                if (bc7_File)
                    fprintf(bc7_File, "MSE %3d PSNR %3d\n", MSE, PSNR);
            }
#endif
            block += 16;
        }

#ifdef USE_THREADED_CALLBACKS
        if (m_user_pFeedbackProc)
        {
            m_progress.progress = 100.f * (j * dwBlocksX) / dwBlocksXY;
            // Inform progress thread to update user feedback
            cmp_bc7_progress_update = true;

            // Check if user abort the process
            if (m_progress.abort)
            {
                cmp_progress->detach();
                delete cmp_progress;
#ifdef BC7_COMPDEBUGGER
                CompClient.disconnect();
#endif
                FinishBC7Encoding();
                return CE_Aborted;
            }
        }
#else
        if (pFeedbackProc)
        {
            if ((j % lineAtPercent) == 0)
            {
                progress = (j * dwBlocksX) / fBlocksXY;
                if (progress != old_progress)
                {
                    old_progress = progress;
                    if (pFeedbackProc(progress * 100.0f, pUser1, pUser2))
                    {
#ifdef BC7_COMPDEBUGGER
                        CompClient.disconnect();
#endif
                        FinishBC7Encoding();
                        return CE_Aborted;
                    }
                }
            }
        }
#endif
    }

#ifdef USE_FILEIO
    if (bc7_File)
    {
        if (bc7_blockcount == 0)
            bc7_blockcount = 1;
        fprintf(bc7_File, "Total MSE %6d AVG_MSE %6d\n\n", bc7_total_MSE, bc7_total_MSE / bc7_blockcount);
        fclose(bc7_File);
        bc7_File = NULL;
    }
#endif
#ifdef BC7_COMPDEBUGGER
    CompClient.disconnect();
    m_Use_MultiThreading = hold_UseMultitheading;
#endif

#ifdef USE_THREADED_CALLBACKS
    // Close progress thread Loop
    m_progress.abort = true;
    cmp_progress->detach();
    delete cmp_progress;
#endif
    // Close up remaining compression blocks
    CodecError cError = FinishBC7Encoding();

#ifdef USE_DBGTRACE
    DbgTrace(("###########-----------DONE -------------###########"));
#endif

    return cError;
}

CodecError CCodec_BC7::Decompress(CCodecBuffer&       bufferIn,
                                  CCodecBuffer&       bufferOut,
                                  Codec_Feedback_Proc pFeedbackProc,
                                  CMP_DWORD_PTR       pUser1,
                                  CMP_DWORD_PTR       pUser2)
{
    assert(bufferIn.GetWidth() == bufferOut.GetWidth());
    assert(bufferIn.GetHeight() == bufferOut.GetHeight());

    CodecError err = InitializeBC7Library();
    if (err != CE_OK)
        return err;

    if (bufferIn.GetWidth() != bufferOut.GetWidth() || bufferIn.GetHeight() != bufferOut.GetHeight())
        return CE_Unknown;

    const CMP_DWORD dwBlocksX = ((bufferIn.GetWidth() + 3) >> 2);
    const CMP_DWORD dwBlocksY = ((bufferIn.GetHeight() + 3) >> 2);
    const CMP_FLOAT fBlocksXY = (CMP_FLOAT)(dwBlocksX * dwBlocksY);

    for (CMP_DWORD j = 0; j < dwBlocksY; j++)
    {
        for (CMP_DWORD i = 0; i < dwBlocksX; i++)
        {
            union FBLOCKS
            {
                double decodedBlock[16][4];
                double destBlock[BLOCK_SIZE_4X4X4];
            } DecData;

            union BBLOCKS
            {
                CMP_DWORD compressedBlock[4];
                CMP_BYTE  out[16];
                CMP_BYTE  in[16];
            } CompData;

            CMP_BYTE destBlock[BLOCK_SIZE_4X4X4];

            bufferIn.ReadBlock(i * 4, j * 4, CompData.compressedBlock, 4);

            // Encode to the appropriate location in the compressed image
            m_decoder->DecompressBlock(DecData.decodedBlock, CompData.in);

            // Create the block for decoding
            int srcIndex = 0;
            for (int row = 0; row < BLOCK_SIZE_4; row++)
            {
                for (int col = 0; col < BLOCK_SIZE_4; col++)
                {
                    destBlock[srcIndex]     = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_RED];
                    destBlock[srcIndex + 1] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_GREEN];
                    destBlock[srcIndex + 2] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_BLUE];
                    destBlock[srcIndex + 3] = (CMP_BYTE)DecData.decodedBlock[row * BLOCK_SIZE_4 + col][BC_COMP_ALPHA];
                    srcIndex += 4;
                }
            }

            bufferOut.WriteBlockRGBA(i * 4, j * 4, 4, 4, destBlock);
        }

        if (pFeedbackProc)
        {
            float fProgress = 100.f * (j * dwBlocksX) / fBlocksXY;
            if (pFeedbackProc(fProgress, pUser1, pUser2))
            {
                return CE_Aborted;
            }
        }
    }
    return CE_OK;
}

// Not implemented
CodecError CCodec_BC7::Compress_Fast(CCodecBuffer&       bufferIn,
                                     CCodecBuffer&       bufferOut,
                                     Codec_Feedback_Proc pFeedbackProc,
                                     CMP_DWORD_PTR       pUser1,
                                     CMP_DWORD_PTR       pUser2)
{
    return CE_OK;
}

// Not implemented
CodecError CCodec_BC7::Compress_SuperFast(CCodecBuffer&       bufferIn,
                                          CCodecBuffer&       bufferOut,
                                          Codec_Feedback_Proc pFeedbackProc,
                                          CMP_DWORD_PTR       pUser1,
                                          CMP_DWORD_PTR       pUser2)
{
    return CE_OK;
}
