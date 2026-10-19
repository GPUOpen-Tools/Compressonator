//=====================================================================
// Copyright 2022-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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
/// \file format_conversion.h
//
//=====================================================================

#ifndef FORMAT_CONVERSION_H_
#define FORMAT_CONVERSION_H_

#include "compressonator.h"

// TODO: Replace the AMD_CODEC_EXPOSURE_DEFAULT, etc. values with these
#define EXPOSURE_VALUE_DEFAULT 0.0f  ///< This is the default value set for exposure value of hdr/exr input image
#define DEFOG_VALUE_DEFAULT 0.0f     ///< This is the default value set for defog value of hdr/exr input image
#define KNEELOW_VALUE_DEFAULT 0.0f   ///< This is the default value set for kneelow value of hdr/exr input image
#define KNEEHIGH_VALUE_DEFAULT 5.0f  ///< This is the default value set for kneehigh value of hdr/exr input image
#define GAMMA_VALUE_DEFAULT 2.2f     ///< This is the default value set for gamma value of hdr/exr input image

// TODO: these RGBA2101010 values might be wrong?
#define RGBA2101010_OFFSET_A 30
#define RGBA2101010_OFFSET_R 20
#define RGBA2101010_OFFSET_G 10
#define RGBA2101010_OFFSET_B 0

#define RGBA1010102_OFFSET_R 0
#define RGBA1010102_OFFSET_G 10
#define RGBA1010102_OFFSET_B 20
#define RGBA1010102_OFFSET_A 30

#define TWO_BIT_MASK 0x0003
#define TEN_BIT_MASK 0x03ff

// Extracts channels from an RGBA1010102 DWORD
#define RGBA1010102_GET_R(x) (((x) >> RGBA1010102_OFFSET_R) & TEN_BIT_MASK)
#define RGBA1010102_GET_G(x) (((x) >> RGBA1010102_OFFSET_G) & TEN_BIT_MASK)
#define RGBA1010102_GET_B(x) (((x) >> RGBA1010102_OFFSET_B) & TEN_BIT_MASK)
#define RGBA1010102_GET_A(x) (((x) >> RGBA1010102_OFFSET_A) & TWO_BIT_MASK)

#define CONVERT_WORD_TO_10BIT(b) ((b >> 6) & TEN_BIT_MASK)
#define CONVERT_WORD_TO_2BIT(b) ((b >> 14) & TWO_BIT_MASK)

// parameters used for converting floating point values
struct FloatParams
{
    CMP_FLOAT defog;
    CMP_FLOAT exposure;
    CMP_FLOAT kneeLow;
    CMP_FLOAT kneeHigh;
    CMP_FLOAT gamma;

    FloatParams()
        : defog(DEFOG_VALUE_DEFAULT)
        , exposure(EXPOSURE_VALUE_DEFAULT)
        , kneeLow(KNEELOW_VALUE_DEFAULT)
        , kneeHigh(KNEEHIGH_VALUE_DEFAULT)
        , gamma(GAMMA_VALUE_DEFAULT)
    {
    }
    explicit FloatParams(const CMP_AnalysisData* analysisData);
    explicit FloatParams(const CMP_CompressOptions* compressOptions);
};

// Contains the result from the compatible buffer creation functions
// There are two possible states this could be in:
//     1. isBufferNew == false, which means that the data contained within is identical to the source buffer
//     2. isBufferNew == true, some conversion was done to create a new buffer that is compatible with the target format
// in either case, the destructor will automatically clean up any extra data created by this struct
struct ConvertedBuffer
{
    bool isBufferNew;

    void*     data;
    CMP_DWORD dataSize;

    CMP_FORMAT format;

    ConvertedBuffer();
    ConvertedBuffer(ConvertedBuffer&& other);
    ~ConvertedBuffer();

    ConvertedBuffer& operator=(ConvertedBuffer&& other);
};

// Creates and returns a buffer that is compatible with the target format, using the given the source data and format
ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT targetFormat, const MipSet* srcMipSet, const FloatParams* params = 0);
ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT targetFormat, const CMP_Texture* srcTexture, const FloatParams* params = 0);
ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT         targetFormat,
                                       CMP_FORMAT         srcFormat,
                                       void*              srcData,
                                       CMP_DWORD          srcDataSize,
                                       CMP_DWORD          srcWidth,
                                       CMP_DWORD          srcHeight,
                                       const FloatParams* params = 0);

// Converts 16-bit, 32-bit, or R9G9B9E5 floating point data into RGBA8888 format.
// outBufferSize and inBufferSize are the sizes in bytes of the respective buffers;
// the function returns an error if the declared width*height exceeds either buffer.
//
// outBufferSize must be the caller's actual destination allocation size. The MipSet
// and CMP_Texture overloads cannot derive it (a MipSet/CMP_Texture describes the
// source, not the destination), so it is passed explicitly rather than assumed to be
// width * height * 4.
CMP_ERROR FloatToByte(CMP_BYTE* outBuffer, CMP_DWORD outBufferSize, CMP_FLOAT* inBuffer, const MipSet* srcMipSet, const FloatParams* params);
CMP_ERROR FloatToByte(CMP_BYTE* outBuffer, CMP_DWORD outBufferSize, CMP_FLOAT* inBuffer, const CMP_Texture* srcTexture, const FloatParams* params);
CMP_ERROR FloatToByte(CMP_BYTE*          outBuffer,
                      CMP_DWORD          outBufferSize,
                      CMP_FLOAT*         inBuffer,
                      CMP_DWORD          inBufferSize,
                      CMP_ChannelFormat  channelFormat,
                      CMP_DWORD          width,
                      CMP_DWORD          height,
                      const FloatParams* params);

// Convert 8-bit integer data into 16-bit float bits
CMP_ERROR ByteToHalfShort(CMP_HALFSHORT* outBuffer, CMP_BYTE* inBuffer, CMP_DWORD numElements);

// Convert 16-bit floats stored as bit data to 32-bit floats
CMP_ERROR HalfShortToFloat(CMP_FLOAT* outBuffer, CMP_HALFSHORT* inBuffer, CMP_DWORD numElements);
CMP_FLOAT HalfShortToFloat(CMP_HALFSHORT halfShort);

// Convert 32-bit floats to 16-bit floats stored as bit data
CMP_ERROR FloatToHalfShort(CMP_HALFSHORT* outBuffer, CMP_FLOAT* inBuffer, CMP_DWORD numElements);

// Convert 16-bit integer data into 8-bit integer data
CMP_ERROR WordToByte(CMP_BYTE* outBuffer, CMP_WORD* inBuffer, CMP_DWORD numElements);

// These two functions convert between signed and unsigned 8-bit data
CMP_ERROR SByteToByte(CMP_BYTE* outBuffer, CMP_SBYTE* inBuffer, CMP_DWORD numElements);
CMP_ERROR ByteToSByte(CMP_SBYTE* outBuffer, CMP_BYTE* inBuffer, CMP_DWORD numElements);

// Convert an RGBA 1010102 format image into RGBA 8888, the result is a buffer which contains pixels in the order RR GG BB AA
void ConvertRGBA1010102ToRGBA8888(void* outBuffer, const void* inBuffer, unsigned long numPixels, bool hasAlpha = true);

#endif