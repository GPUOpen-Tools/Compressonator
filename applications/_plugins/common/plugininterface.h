//=====================================================================
// Copyright 2016-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
//=====================================================================
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
/// \file PluginInterface.h
/// \version 3.2
/// \brief Declares the interface to the Compressonator SDK
//
//=====================================================================

#ifndef _PLUGININTERFACE_H
#define _PLUGININTERFACE_H

#include "compressonator.h"
#include "pluginbase.h"
#include "pluginmanager.h"

#ifndef _WIN32
typedef CMP_DWORD WNDPROC;
#endif

// make cmp plugins optional when using code with plugin apps based off c++17 features.
// #if (_MSVC_LANG > 201700L)
// #if __has_include("cmp_plugininterface.h")
// #include "cmp_plugininterface.h"
// #endif
// #else
// #include "cmp_plugininterface.h"
// #endif

class PluginInterface_WindowViews : PluginBase
{
public:
    PluginInterface_WindowViews()
    {
    }
    virtual ~PluginInterface_WindowViews()
    {
    }
    virtual int   TC_PluginGetVersion(TC_PluginVersion* pPluginVersion) = 0;
    virtual void  ShowWindow(bool Show)                                 = 0;
    virtual void* CreateView(void* window)                              = 0;
    virtual void  DeleteView()                                          = 0;
};

class CMP_Encoder
{
public:
    CMP_Encoder()
    {
        m_srcWidth              = 0;
        m_srcHeight             = 0;
        m_srcBufferSizeInPixels = 0;
        m_srcStride             = 0;
        m_dstStride             = 0;
        m_xdim                  = 4;
        m_ydim                  = 4;
        m_zdim                  = 1;
        m_quality               = 0.05f;
    };
    ~CMP_Encoder() {};

    // Block Level Processing
    virtual int CompressBlock(unsigned int xBlock, unsigned int yBlock, void* in, void* out)   = 0;
    virtual int CompressBlock(void* in, void* out, void* blockoptions)                         = 0;
    virtual int DecompressBlock(unsigned int xBlock, unsigned int yBlock, void* in, void* out) = 0;
    virtual int DecompressBlock(void* in, void* out)                                           = 0;

    // Full Texture Processing
    virtual int CompressTexture(void* in, void* out, void* processOptions)   = 0;
    virtual int DecompressTexture(void* in, void* out, void* processOptions) = 0;

    // Original image size
    unsigned int m_srcWidth;
    unsigned int m_srcHeight;

    // Actual source buffer size in 4-channel pixels; the channel size depends on the source
    // format (1 byte for 8-bit, 2 for BC6H half-float, 4 for 32-bit).
    // Zero means "not set" (block-encoder API callers are trusted).
    unsigned int m_srcBufferSizeInPixels;

    // Image strides used by the (xBlock, yBlock) block-level CompressBlock() overload.
    // Both are "advance per row of blocks", not per row of pixels, and both are optional:
    // zero means "use the tightly packed default derived from m_srcWidth".
    //
    //   m_srcStride  Number of source pixel ELEMENTS (not bytes) to advance for each
    //                increment of yBlock. A 4-row-high block means the tightly packed
    //                default is m_srcWidth * 4, which is the value the shipped
    //                framework examples pass explicitly.
    //   m_dstStride  Number of destination BYTES to advance for each increment of yBlock.
    //                The tightly packed default is ((m_srcWidth + 3) / 4) * <block bytes>.
    //
    // Only the BC1 encoder currently honours m_srcStride; the other block encoders assume
    // the tightly packed layout.
    unsigned int m_srcStride;
    unsigned int m_dstStride;

    // Source block dimensions to compress
    unsigned int m_xdim;
    unsigned int m_ydim;
    unsigned int m_zdim;
    // Compression quality to apply during compression
    float m_quality;
};

class PluginInterface_Vision : PluginBase
{
public:
    PluginInterface_Vision()
    {
    }
    virtual ~PluginInterface_Vision()
    {
    }
    virtual int TC_PluginGetVersion(TC_PluginVersion* pPluginVersion) = 0;

    virtual void VisionProcess(char* srcFile, char* testFile, void* options, void* results) = 0;
};

class PluginInterface_Engine : PluginBase
{
public:
    PluginInterface_Engine()
    {
    }
    virtual ~PluginInterface_Engine()
    {
    }
    virtual int  TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)                             = 0;
    virtual int  InitEngine(void* remoteEngine, void* remoteImGui, void* remoteHWND, int nCmdShow) = 0;
    virtual void CloseEngine()                                                                     = 0;
    virtual int  ProcessFrame(void* pMipSet, bool getFrame, int framenumber)                       = 0;
    virtual int  GetRenderTarget(void** GroundTruth, void** Upscaled)                              = 0;
};

class PluginInterface_ImageMetrics : PluginBase
{
public:
    PluginInterface_ImageMetrics()
    {
    }
    virtual ~PluginInterface_ImageMetrics()
    {
    }
    virtual int   TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)                                                                    = 0;
    virtual int   Init(void* remoteDevice, void* remoteCommandList, void* remoteDescriptoList)                                             = 0;
    virtual void  Close()                                                                                                                  = 0;
    virtual float GetPSNR(unsigned int index)                                                                                              = 0;
    virtual float GetSSIM(unsigned int index)                                                                                              = 0;
    virtual int   ComputePSNR(unsigned int index, void* GroundTruthResource, void* UpscaledResource, void* descriptor0, void* descriptor1) = 0;
    virtual int   ComputeSSIM(unsigned int index, void* GroundTruthResource, void* UpscaledResource, void* descriptor0, void* descriptor1) = 0;
    virtual void* GetPSNRMetric(unsigned int index)                                                                                        = 0;
    virtual void* GetSSIMMetric(unsigned int index)                                                                                        = 0;
};

class PluginInterface_UpscaleContext : PluginBase
{
public:
    PluginInterface_UpscaleContext()
    {
    }
    virtual ~PluginInterface_UpscaleContext()
    {
    }
    virtual int  TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)                                     = 0;
    virtual int  Init(void* remoteEngine, void* remoteSetting)                                             = 0;
    virtual void Close()                                                                                   = 0;
    virtual int  CreateContext(unsigned int version, unsigned int uType, void* initParams, void** Context) = 0;
};

class PluginInterface_ImageProcessRT : PluginBase
{
public:
    PluginInterface_ImageProcessRT()
    {
    }
    virtual ~PluginInterface_ImageProcessRT()
    {
    }
    virtual int   TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)                                                      = 0;
    virtual int   Init(void* remoteEngine, void* settings)                                                                   = 0;
    virtual void  Close()                                                                                                    = 0;
    virtual void* GetRenderTarget(unsigned int index)                                                                        = 0;
    virtual int   Process(unsigned int index, void* Rendertarget1, void* Rendertarget2, void* ImageMetrics2, void* settings) = 0;
};

class PluginInterface_SubjectiveImageMetric : PluginBase
{
public:
    PluginInterface_SubjectiveImageMetric()
    {
    }
    virtual ~PluginInterface_SubjectiveImageMetric()
    {
    }
    virtual float Process(int type, const char* imagefile, const char* modelPath) = 0;
};

#endif
