//===============================================================================
// Copyright (c) 2007-2024  Advanced Micro Devices, Inc. All rights reserved.
// Copyright (c) 2004-2006 ATI Technologies Inc.
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
//  File Name:   Compress.cpp
//  Description: A library to compress/decompress textures
//
//////////////////////////////////////////////////////////////////////////////

#include "compress.h"

#include <assert.h>
#include <algorithm>
#include <thread>

#ifdef _WIN32
#include "windows.h"
#include "sysinfoapi.h"
#endif

#include "atiformats.h"
#include "codec.h"
#include "codec_common.h"
#include "common.h"
#include "compressonator.h"
#include "texture_utils.h"

#define MAX_THREADS 64

CMP_INT CMP_GetNumberOfProcessors()
{
#ifndef _WIN32
    //    return sysconf(_SC_NPROCESSORS_ONLN);
    return std::thread::hardware_concurrency();
#else
    // Figure out how many cores there are on this machine
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (sysinfo.dwNumberOfProcessors);
#endif
}

static bool NeedSwizzle(CMP_FORMAT destformat)
{
    // determin of the swizzle flag needs to be turned on!
    switch (destformat)
    {
    case CMP_FORMAT_BC4:
    case CMP_FORMAT_BC4_S:
    case CMP_FORMAT_ATI1N:  // same as BC4
    case CMP_FORMAT_BC5:
    case CMP_FORMAT_BC5_S:
    case CMP_FORMAT_ATI2N:       // Green & Red Channels
    case CMP_FORMAT_ATI2N_XY:    // same as ATI2N  with XY = Red & Green channels
    case CMP_FORMAT_ATI2N_DXT5:  // same as BC5
    case CMP_FORMAT_BC1:
    case CMP_FORMAT_DXT1:  // same as BC1
    case CMP_FORMAT_BC2:
    case CMP_FORMAT_DXT3:  // same as BC2
    case CMP_FORMAT_BC3:
    case CMP_FORMAT_DXT5:  // same as BC3
    case CMP_FORMAT_ATC_RGB:
    case CMP_FORMAT_ATC_RGBA_Explicit:
    case CMP_FORMAT_ATC_RGBA_Interpolated:
        return true;
        break;
    default:
        break;
    }

    return false;
}

CMP_ERROR GetError(CodecError err)
{
    switch (err)
    {
    case CE_OK:
        return CMP_OK;
    case CE_Aborted:
        return CMP_ABORTED;
    case CE_Unknown:
        return CMP_ERR_GENERIC;
    default:
        return CMP_ERR_GENERIC;
    }
}

#ifndef USE_OLD_SWIZZLE

// For now this function will only handle a single case
// where the source data remains the same size and only RGBA channels
// are swizzled according output compressed formats,
// if source is compressed then no change is performed

static void CMP_PrepareCMPSourceForIMG_Destination(CMP_Texture* destTexture, CMP_FORMAT srcFormat)
{
    CMP_DWORD  dwWidth      = destTexture->dwWidth;
    CMP_DWORD  dwHeight     = destTexture->dwHeight;
    CMP_FORMAT newDstFormat = destTexture->format;
    CMP_BYTE*  pData;

    pData = destTexture->pData;

    switch (srcFormat)
    {
    // decompressed Data  is in the form BGRA
    case CMP_FORMAT_ATI1N:
    case CMP_FORMAT_ATI2N:
    case CMP_FORMAT_ATI2N_XY:
    case CMP_FORMAT_ATI2N_DXT5:
    case CMP_FORMAT_ATC_RGB:
    case CMP_FORMAT_ATC_RGBA_Explicit:
    case CMP_FORMAT_ATC_RGBA_Interpolated:
    case CMP_FORMAT_BC1:
    case CMP_FORMAT_BC2:
    case CMP_FORMAT_BC3:
    case CMP_FORMAT_BC4:
    case CMP_FORMAT_BC4_S:
    case CMP_FORMAT_BC5:
    case CMP_FORMAT_BC5_S:
    case CMP_FORMAT_DXT1:
    case CMP_FORMAT_DXT3:
    case CMP_FORMAT_DXT5:
    case CMP_FORMAT_DXT5_xGBR:
    case CMP_FORMAT_DXT5_RxBG:
    case CMP_FORMAT_DXT5_RBxG:
    case CMP_FORMAT_DXT5_xRBG:
    case CMP_FORMAT_DXT5_RGxB:
    case CMP_FORMAT_DXT5_xGxR: {
        switch (newDstFormat)
        {
        case CMP_FORMAT_BGRA_8888:
            break;
        case CMP_FORMAT_RGBA_8888: {
            CMP_Map_Bytes(pData, dwWidth, dwHeight, {2, 1, 0, 3}, 4);
            break;
        }
        default:
            break;
        }
    }
    // decompressed Data  is in the form RGBA_8888
#if (OPTION_BUILD_ASTC == 1)
    case CMP_FORMAT_ASTC:
#endif
    case CMP_FORMAT_BC6H:
    case CMP_FORMAT_BC7:
    case CMP_FORMAT_GT:
    case CMP_FORMAT_ETC_RGB:
    case CMP_FORMAT_ETC2_RGB:
    case CMP_FORMAT_ETC2_SRGB:
    case CMP_FORMAT_ETC2_RGBA:
    case CMP_FORMAT_ETC2_RGBA1:
    case CMP_FORMAT_ETC2_SRGBA:
    case CMP_FORMAT_ETC2_SRGBA1: {
        switch (newDstFormat)
        {
        case CMP_FORMAT_RGBA_8888:
            break;
        case CMP_FORMAT_BGRA_8888: {
            CMP_Map_Bytes(pData, dwWidth, dwHeight, {2, 1, 0, 3}, 4);
            break;
        }
        default:
            break;
        }
    }
    }
}

#endif

CMP_ERROR CodecCompressTexture(const CMP_Texture* srcTexture, CMP_Texture* destTexture, const CMP_CompressOptions* options, CMP_Feedback_Proc feedbackProc, CMP_DWORD_PTR user1, CMP_DWORD_PTR user2)
{
    CodecType destType = GetCodecType(destTexture->format);
    if (destType == CT_Unknown)
        return CMP_ERR_UNSUPPORTED_DEST_FORMAT;

    CCodec* codec = CreateCodec(destType);
    if (codec == NULL)
        return CMP_ERR_UNABLE_TO_INIT_CODEC;

    CMP_BOOL swizzleSrcBuffer = false;

    // Have we got valid options ?
    if (options && options->dwSize == sizeof(CMP_CompressOptions))
    {
        // Set weightings ?
        if (options->bUseChannelWeighting && (options->fWeightingRed > 0.0 || options->fWeightingGreen > 0.0 || options->fWeightingBlue > 0.0))
        {
            codec->SetParameter("UseChannelWeighting", (CMP_DWORD)1);
            codec->SetParameter("WeightR", options->fWeightingRed > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingRed : MINIMUM_WEIGHT_VALUE);
            codec->SetParameter("WeightG", options->fWeightingGreen > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingGreen : MINIMUM_WEIGHT_VALUE);
            codec->SetParameter("WeightB", options->fWeightingBlue > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingBlue : MINIMUM_WEIGHT_VALUE);
        }
        codec->SetParameter("UseAdaptiveWeighting", (CMP_DWORD)options->bUseAdaptiveWeighting);
        codec->SetParameter("DXT1UseAlpha", (CMP_DWORD)options->bDXT1UseAlpha);
        codec->SetParameter("AlphaThreshold", (CMP_DWORD)options->nAlphaThreshold);
        if (options->bUseRefinementSteps)
            codec->SetParameter("RefineSteps", (CMP_DWORD)options->nRefinementSteps);
        // New override to that set quality if compresion for DXTn & ATInN codecs
        if (options->fquality != AMD_CODEC_QUALITY_DEFAULT)
        {
            codec->SetParameter("Quality", (CODECFLOAT)options->fquality);
#ifndef _WIN64
            if (options->fquality < 0.3)
                codec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_SuperFast);
            else if (options->fquality < 0.6)
                codec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_Fast);
            else
#endif
                codec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_Normal);
        }
        else
            codec->SetParameter("CompressionSpeed", (CMP_DWORD)options->nCompressionSpeed);

        switch (destType)
        {
        case CT_BC7:
            codec->SetParameter("MultiThreading", (CMP_DWORD)!options->bDisableMultiThreading);

            if (!options->bDisableMultiThreading)
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)options->dwnumThreads);
            else
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)1);

            codec->SetParameter("ModeMask", (CMP_DWORD)options->dwmodeMask);
            codec->SetParameter("ColourRestrict", (CMP_DWORD)options->brestrictColour);
            codec->SetParameter("AlphaRestrict", (CMP_DWORD)options->brestrictAlpha);
            codec->SetParameter("Quality", (CODECFLOAT)options->fquality);
            break;
#ifdef USE_BASIS
        case CT_BASIS:
#endif
#if (OPTION_BUILD_ASTC == 1)
        case CT_ASTC:
            codec->SetParameter("Quality", (CODECFLOAT)options->fquality);
            if (!options->bDisableMultiThreading)
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)options->dwnumThreads);
            else
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)1);
            break;
#endif
#ifdef USE_APC
        case CT_APC:
#endif
#ifdef USE_GTC
        case CT_GTC:
#endif
#ifdef USE_LOSSLESS_COMPRESSION
        case CT_BRLG: {
            CMP_DWORD pageSize = options->dwPageSize;
            codec->SetParameter(CodecParameters::PageSize, pageSize ? pageSize : AMD_CODEC_PAGE_SIZE_DEFAULT);
        }
        break;
#endif
        case CT_BC6H:
        case CT_BC6H_SF:
            codec->SetParameter("Quality", (CODECFLOAT)options->fquality);
            if (!options->bDisableMultiThreading)
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)options->dwnumThreads);
            else
                codec->SetParameter(CodecParameters::NumThreads, (CMP_DWORD)1);
            break;
        }

        // This will eventually replace the above code for setting codec options
        if (options->NumCmds > 0)
        {
            int maxCmds = options->NumCmds;
            if (options->NumCmds > AMD_MAX_CMDS)
                maxCmds = AMD_MAX_CMDS;
            for (int i = 0; i < maxCmds; i++)
                codec->SetParameter(options->CmdSet[i].strCommand, (CMP_CHAR*)options->CmdSet[i].strParameter);
        }

        // GPUOpen issue # 59 fix
        CodecBufferType srcBufferType = GetCodecBufferType(srcTexture->format);
        if (NeedSwizzle(destTexture->format))
        {
            switch (srcBufferType)
            {
            case CBT_BGRA8888:
            case CBT_BGR888:
            case CBT_R8:
                swizzleSrcBuffer = false;
                break;
            default:
                swizzleSrcBuffer = true;
                break;
            }
        }
    }

    CodecBufferType srcBufferType = GetCodecBufferType(srcTexture->format);

    // There are some specific settings to configure when compressing to Brotli-G
    if (destType == CT_BRLG)
    {
        // We always want to have the source data act as if it were binary data when compressing to Brotli-G
        srcBufferType = GetCodecBufferType(CMP_FORMAT_BINARY);

        if (srcTexture->format != CMP_FORMAT_BINARY)
        {
            codec->SetParameter(CodecParameters::Precondition, (CMP_DWORD)options->doPreconditionBRLG);
            codec->SetParameter(CodecParameters::Swizzle, (CMP_DWORD)options->doSwizzleBRLG);
            codec->SetParameter(CodecParameters::DeltaEncode, (CMP_DWORD)options->doDeltaEncodeBRLG);

            codec->SetParameter(CodecParameters::TextureFormat, (CMP_DWORD)srcTexture->format);
            codec->SetParameter(CodecParameters::TextureWidth, srcTexture->dwWidth);
            codec->SetParameter(CodecParameters::TextureHeight, srcTexture->dwHeight);

            if (srcTexture->pMipSet)
                codec->SetParameter(CodecParameters::MipmapLevels, (CMP_DWORD)(((CMP_MipSet*)srcTexture->pMipSet)->m_nMipLevels));
        }
    }

    CCodecBuffer* srcBuffer = CreateCodecBuffer(srcBufferType,
                                                srcTexture->nBlockWidth,
                                                srcTexture->nBlockHeight,
                                                srcTexture->nBlockDepth,
                                                srcTexture->dwWidth,
                                                srcTexture->dwHeight,
                                                srcTexture->dwPitch,
                                                srcTexture->pData,
                                                srcTexture->dwDataSize);

    CCodecBuffer* destBuffer = codec->CreateBuffer(destTexture->nBlockWidth,
                                                   destTexture->nBlockHeight,
                                                   destTexture->nBlockDepth,
                                                   destTexture->dwWidth,
                                                   destTexture->dwHeight,
                                                   destTexture->dwPitch,
                                                   destTexture->pData,
                                                   destTexture->dwDataSize);

    assert(srcBuffer);
    assert(destBuffer);
    if (srcBuffer == NULL || destBuffer == NULL)
    {
        SAFE_DELETE(codec);
        SAFE_DELETE(srcBuffer);
        SAFE_DELETE(destBuffer);
        return CMP_ERR_GENERIC;
    }

    srcBuffer->SetFormat(srcTexture->format);
    destBuffer->SetFormat(destTexture->format);

    // GPUOpen issue # 59 and #67 fix
    srcBuffer->m_bSwizzle = swizzleSrcBuffer;

    DISABLE_FP_EXCEPTIONS;
    CodecError err = codec->Compress(*srcBuffer, *destBuffer, feedbackProc, user1, user2);
    RESTORE_FP_EXCEPTIONS;

    destTexture->dwDataSize = destBuffer->GetDataSize();

    SAFE_DELETE(codec);
    SAFE_DELETE(srcBuffer);
    SAFE_DELETE(destBuffer);

    return GetError(err);
}

CMP_ERROR CodecDecompressTexture(const CMP_Texture* srcTexture, CMP_Texture* destTexture, const CMP_CompressOptions* options, CMP_Feedback_Proc feedbackProc, CMP_DWORD_PTR user1, CMP_DWORD_PTR user2)
{
    CodecType srcType = GetCodecType(srcTexture->format);
    if (srcType == CT_Unknown)
        return CMP_ERR_UNSUPPORTED_SOURCE_FORMAT;

    CCodec* codec = CreateCodec(srcType);
    assert(codec);

    if (codec == NULL)
        return CMP_ERR_UNABLE_TO_INIT_CODEC;

    if (options && options->dwSize == sizeof(CMP_CompressOptions))
    {
        codec->SetParameter(CodecParameters::UseGPUDecompression, options->bUseGPUDecompress ? (CMP_DWORD)1 : (CMP_DWORD)0);
    }

    CCodecBuffer* srcBuffer = codec->CreateBuffer(srcTexture->nBlockWidth,
                                                  srcTexture->nBlockHeight,
                                                  srcTexture->nBlockDepth,
                                                  srcTexture->dwWidth,
                                                  srcTexture->dwHeight,
                                                  srcTexture->dwPitch,
                                                  srcTexture->pData,
                                                  srcTexture->dwDataSize);

    destTexture->nBlockWidth  = srcTexture->nBlockWidth;
    destTexture->nBlockHeight = srcTexture->nBlockHeight;
    destTexture->nBlockDepth  = srcTexture->nBlockDepth;

    CodecBufferType destBufferType = GetCodecBufferType(destTexture->format);

    if (srcTexture->format == CMP_FORMAT_BROTLIG)
        destBufferType = GetCodecBufferType(CMP_FORMAT_BINARY);

    CCodecBuffer* destBuffer = CreateCodecBuffer(destBufferType,
                                                 destTexture->nBlockWidth,
                                                 destTexture->nBlockHeight,
                                                 destTexture->nBlockDepth,
                                                 destTexture->dwWidth,
                                                 destTexture->dwHeight,
                                                 destTexture->dwPitch,
                                                 destTexture->pData,
                                                 destTexture->dwDataSize);

    if (srcBuffer == NULL || destBuffer == NULL)
    {
        SAFE_DELETE(codec);
        SAFE_DELETE(srcBuffer);
        SAFE_DELETE(destBuffer);
        return CMP_ERR_GENERIC;
    }

    DISABLE_FP_EXCEPTIONS;

    srcBuffer->SetBlockHeight(srcTexture->nBlockHeight);
    srcBuffer->SetBlockWidth(srcTexture->nBlockWidth);
    srcBuffer->SetBlockDepth(srcTexture->nBlockDepth);
    srcBuffer->SetFormat(srcTexture->format);
    srcBuffer->SetTranscodeFormat(srcTexture->transcodeFormat);

    destBuffer->SetBlockHeight(destTexture->nBlockHeight);
    destBuffer->SetBlockWidth(destTexture->nBlockWidth);
    destBuffer->SetBlockDepth(destTexture->nBlockDepth);
    destBuffer->SetFormat(destTexture->format);

    CodecError err1 = codec->Decompress(*srcBuffer, *destBuffer, feedbackProc, user1, user2);

    RESTORE_FP_EXCEPTIONS;

    destTexture->dwDataSize = destBuffer->GetDataSize();

#ifndef USE_OLD_SWIZZLE
    CMP_PrepareCMPSourceForIMG_Destination(destTexture, srcTexture->format);
#endif

    SAFE_DELETE(codec);
    SAFE_DELETE(srcBuffer);
    SAFE_DELETE(destBuffer);

    return GetError(err1);
}

#ifdef THREADED_COMPRESS

class CATICompressThreadData
{
public:
    CATICompressThreadData();
    ~CATICompressThreadData();

    CCodec*           m_pCodec;
    CCodecBuffer*     m_pSrcBuffer;
    CCodecBuffer*     m_pDestBuffer;
    CMP_Feedback_Proc m_pFeedbackProc;
    CMP_DWORD_PTR     m_pUser1;
    CMP_DWORD_PTR     m_pUser2;
    CodecError        m_errorCode;
};

CATICompressThreadData::CATICompressThreadData()
    : m_pCodec(NULL)
    , m_pSrcBuffer(NULL)
    , m_pDestBuffer(NULL)
    , m_pFeedbackProc(NULL)
    , m_pUser1(NULL)
    , m_pUser2(NULL)
    , m_errorCode(CE_OK)
{
}

CATICompressThreadData::~CATICompressThreadData()
{
    SAFE_DELETE(m_pCodec);
    SAFE_DELETE(m_pSrcBuffer);
    SAFE_DELETE(m_pDestBuffer);
}

void ThreadedCompressProc(void* lpParameter)
{
    CATICompressThreadData* pThreadData = (CATICompressThreadData*)lpParameter;
    DISABLE_FP_EXCEPTIONS;
    CodecError err = pThreadData->m_pCodec->Compress(*pThreadData->m_pSrcBuffer, *pThreadData->m_pDestBuffer, pThreadData->m_pFeedbackProc, pThreadData->m_pUser1, pThreadData->m_pUser2);
    RESTORE_FP_EXCEPTIONS;
    pThreadData->m_errorCode = err;
}

CMP_ERROR CodecCompressTextureThreaded(const CMP_Texture*         srcTexture,
                                       CMP_Texture*               destTexture,
                                       const CMP_CompressOptions* options,
                                       CMP_Feedback_Proc          feedbackProc,
                                       CMP_DWORD_PTR              user1,
                                       CMP_DWORD_PTR              user2)
{
    CodecType destType = GetCodecType(destTexture->format);
    if (destType == CT_Unknown)
        return CMP_ERR_UNSUPPORTED_DEST_FORMAT;

    // Note function should not be called for the following Codecs....
    if (destType == CT_BC7)
        return CMP_ABORTED;
#ifdef USE_APC
    if (destType == CT_APC)
        return CMP_ABORTED;
#endif
#ifdef USE_GTC
    if (destType == CT_GTC)
        return CMP_ABORTED;
#endif
#ifdef USE_LOSSLESS_COMPRESSION
    if (destType == CT_BRLG)
        return CMP_ABORTED;
#endif
#ifdef USE_BASIS
    if (destType == CT_BASIS)
        return CMP_ABORTED;
#endif
#if (OPTION_BUILD_ASTC == 1)
    if (destType == CT_ASTC)
        return CMP_ABORTED;
#endif

    CMP_DWORD dwMaxThreadCount = cmp_minT(CMP_GetNumberOfProcessors(), MAX_THREADS);
    CMP_DWORD dwLinesRemaining = destTexture->dwHeight;
    CMP_BYTE* pSourceData      = srcTexture->pData;
    CMP_BYTE* pDestData        = destTexture->pData;
    CMP_BOOL  swizzleSrcBuffer = false;

#ifdef _DEBUG
    if ((destTexture->format == CMP_FORMAT_ETC2_RGBA) || (destTexture->format == CMP_FORMAT_ETC2_RGBA1))
        dwMaxThreadCount = 1;
#endif

    CATICompressThreadData aThreadData[MAX_THREADS];
    std::thread            ahThread[MAX_THREADS];

    CMP_DWORD dwThreadCount = 0;
    for (CMP_DWORD dwThread = 0; dwThread < dwMaxThreadCount; dwThread++)
    {
        CATICompressThreadData& threadData = aThreadData[dwThread];

        // Compressing
        threadData.m_pCodec = CreateCodec(destType);
        assert(threadData.m_pCodec);
        if (threadData.m_pCodec == NULL)
            return CMP_ERR_UNABLE_TO_INIT_CODEC;

        // Have we got valid options ?
        if (options && options->dwSize == sizeof(CMP_CompressOptions))
        {
            // Set weightings ?
            if (options->bUseChannelWeighting && (options->fWeightingRed > 0.0 || options->fWeightingGreen > 0.0 || options->fWeightingBlue > 0.0))
            {
                threadData.m_pCodec->SetParameter("UseChannelWeighting", (CMP_DWORD)1);
                threadData.m_pCodec->SetParameter("WeightR",
                                                  options->fWeightingRed > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingRed : MINIMUM_WEIGHT_VALUE);
                threadData.m_pCodec->SetParameter(
                    "WeightG", options->fWeightingGreen > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingGreen : MINIMUM_WEIGHT_VALUE);
                threadData.m_pCodec->SetParameter("WeightB",
                                                  options->fWeightingBlue > MINIMUM_WEIGHT_VALUE ? (CODECFLOAT)options->fWeightingBlue : MINIMUM_WEIGHT_VALUE);
            }
            threadData.m_pCodec->SetParameter("UseAdaptiveWeighting", (CMP_DWORD)options->bUseAdaptiveWeighting);
            threadData.m_pCodec->SetParameter("DXT1UseAlpha", (CMP_DWORD)options->bDXT1UseAlpha);
            threadData.m_pCodec->SetParameter("AlphaThreshold", (CMP_DWORD)options->nAlphaThreshold);
            threadData.m_pCodec->SetParameter("RefineSteps", (CMP_DWORD)options->nRefinementSteps);
            threadData.m_pCodec->SetParameter("Quality", (CODECFLOAT)options->fquality);

            // New override to that set quality if compresion for DXTn & ATInN codecs
            if (options->fquality != AMD_CODEC_QUALITY_DEFAULT)
            {
                if (options->fquality < 0.3)
                    threadData.m_pCodec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_SuperFast);
                else if (options->fquality < 0.6)
                    threadData.m_pCodec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_Fast);
                else
                    threadData.m_pCodec->SetParameter("CompressionSpeed", (CMP_DWORD)CMP_Speed_Normal);
            }
            else
                threadData.m_pCodec->SetParameter("CompressionSpeed", (CMP_DWORD)options->nCompressionSpeed);

            switch (destType)
            {
            case CT_BC6H:
                // Reserved
                break;
            }

            // This will eventually replace the above code for setting codec options
            // It is currently implemented with BC6H and can be expanded to other codec
            if (options->NumCmds > 0)
            {
                int maxCmds = options->NumCmds;
                if (options->NumCmds > AMD_MAX_CMDS)
                    maxCmds = AMD_MAX_CMDS;
                for (int i = 0; i < maxCmds; i++)
                    threadData.m_pCodec->SetParameter(options->CmdSet[i].strCommand, (CMP_CHAR*)options->CmdSet[i].strParameter);
            }
        }

        CodecBufferType srcBufferType = GetCodecBufferType(srcTexture->format);

        // GPUOpen issue # 59 fix
        if (NeedSwizzle(destTexture->format))
        {
            switch (srcBufferType)
            {
            case CBT_BGRA8888:
            case CBT_BGR888:
            case CBT_R8:
                swizzleSrcBuffer = false;
                break;
            default:
                swizzleSrcBuffer = true;
                break;
            }
        }

        CMP_DWORD dwThreadsRemaining = dwMaxThreadCount - dwThread;
        CMP_DWORD dwHeight           = 0;
        if (dwThreadsRemaining > 1)
        {
            CMP_DWORD dwBlockHeight = threadData.m_pCodec->GetBlockHeight();
            dwHeight                = dwLinesRemaining / dwThreadsRemaining;
            dwHeight                = cmp_minT(((dwHeight + dwBlockHeight - 1) / dwBlockHeight) * dwBlockHeight, dwLinesRemaining);  // Round by block height
            dwLinesRemaining -= dwHeight;
        }
        else
            dwHeight = dwLinesRemaining;

        if (dwHeight > 0)
        {
            threadData.m_pSrcBuffer = CreateCodecBuffer(srcBufferType,
                                                        srcTexture->nBlockWidth,
                                                        srcTexture->nBlockHeight,
                                                        srcTexture->nBlockDepth,
                                                        srcTexture->dwWidth,
                                                        dwHeight,
                                                        srcTexture->dwPitch,
                                                        pSourceData,
                                                        srcTexture->dwDataSize);
            threadData.m_pSrcBuffer->SetFormat(srcTexture->format);

            threadData.m_pDestBuffer = threadData.m_pCodec->CreateBuffer(destTexture->nBlockWidth,
                                                                         destTexture->nBlockHeight,
                                                                         destTexture->nBlockDepth,
                                                                         destTexture->dwWidth,
                                                                         dwHeight,
                                                                         destTexture->dwPitch,
                                                                         pDestData,
                                                                         destTexture->dwDataSize);
            threadData.m_pDestBuffer->SetFormat(destTexture->format);

            pSourceData +=
                CalcBufferSize(srcTexture->format, srcTexture->dwWidth, dwHeight, srcTexture->dwPitch, srcTexture->nBlockWidth, srcTexture->nBlockHeight);
            pDestData += CalcBufferSize(destType, destTexture->dwWidth, dwHeight, destTexture->nBlockWidth, destTexture->nBlockHeight);

            assert(threadData.m_pSrcBuffer);
            assert(threadData.m_pDestBuffer);
            if (threadData.m_pSrcBuffer == NULL || threadData.m_pDestBuffer == NULL)
                return CMP_ERR_GENERIC;

            threadData.m_pSrcBuffer->m_bSwizzle = swizzleSrcBuffer;
            threadData.m_pFeedbackProc          = feedbackProc;
            threadData.m_pUser1                 = user1;
            threadData.m_pUser2                 = user2;

            ahThread[dwThreadCount++] = std::thread(ThreadedCompressProc, &threadData);
        }
    }

    for (CMP_DWORD dwThread = 0; dwThread < dwThreadCount; dwThread++)
    {
        std::thread& curThread = ahThread[dwThread];

        curThread.join();
    }

    CodecError err = CE_OK;
    for (CMP_DWORD dwThread = 0; dwThread < dwThreadCount; dwThread++)
    {
        CATICompressThreadData& threadData = aThreadData[dwThread];

        if (err == CE_OK)
            err = threadData.m_errorCode;

        ahThread[dwThread] = std::thread();
    }

    return GetError(err);
}
#endif  // THREADED_COMPRESS
