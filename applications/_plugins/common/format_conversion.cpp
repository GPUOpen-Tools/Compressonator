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
/// \file format_conversion.cpp
//
//=====================================================================
#include <assert.h>

#include "compressonator.h"
#include "atiformats.h"
#include "format_conversion.h"

FloatParams::FloatParams(const CMP_AnalysisData* analysisData)
    : FloatParams()
{
    if (!analysisData)
        return;

    if (analysisData->fInputDefog != 0.0f || analysisData->fInputExposure != 0.0f || analysisData->fInputGamma != 0.0f ||
        analysisData->fInputKneeHigh != 0.0f || analysisData->fInputKneeLow != 0.0f)
    {
        defog    = analysisData->fInputDefog;
        exposure = analysisData->fInputExposure;
        gamma    = analysisData->fInputGamma;
        kneeHigh = analysisData->fInputKneeHigh;
        kneeLow  = analysisData->fInputKneeLow;
    }
}

FloatParams::FloatParams(const CMP_CompressOptions* compressOptions)
    : FloatParams()
{
    if (!compressOptions)
        return;

    if (compressOptions->fInputDefog != 0.0f || compressOptions->fInputExposure != 0.0f || compressOptions->fInputGamma != 0.0f ||
        compressOptions->fInputKneeHigh != 0.0f || compressOptions->fInputKneeLow != 0.0f)
    {
        defog    = compressOptions->fInputDefog;
        exposure = compressOptions->fInputExposure;
        gamma    = compressOptions->fInputGamma;
        kneeHigh = compressOptions->fInputKneeHigh;
        kneeLow  = compressOptions->fInputKneeLow;
    }
}

ConvertedBuffer::ConvertedBuffer()
    : isBufferNew(false)
    , data(0)
    , dataSize(0)
    , format(CMP_FORMAT_Unknown)
{
}
ConvertedBuffer::ConvertedBuffer(ConvertedBuffer&& other)
{
    isBufferNew = other.isBufferNew;
    data        = other.data;
    dataSize    = other.dataSize;
    format      = other.format;

    other.data = 0;
}

ConvertedBuffer::~ConvertedBuffer()
{
    if (isBufferNew && data)
        free(data);
    data = 0;
}

ConvertedBuffer& ConvertedBuffer::operator=(ConvertedBuffer&& other)
{
    std::swap(isBufferNew, other.isBufferNew);
    std::swap(data, other.data);
    std::swap(dataSize, other.dataSize);
    std::swap(format, other.format);

    return *this;
}

static CMP_FORMAT Get16FVariation(CMP_FORMAT srcFormat)
{
    switch (srcFormat)
    {
    case CMP_FORMAT_ABGR_32F:
        return CMP_FORMAT_ABGR_16F;
    case CMP_FORMAT_ARGB_32F:
        return CMP_FORMAT_ARGB_16F;
    case CMP_FORMAT_BGRA_32F:
        return CMP_FORMAT_BGRA_16F;
    case CMP_FORMAT_R_32F:
        return CMP_FORMAT_R_16F;
    case CMP_FORMAT_RG_32F:
        return CMP_FORMAT_RG_16F;
    case CMP_FORMAT_RGBA_32F:
        return CMP_FORMAT_RGBA_16F;
    default:
        return srcFormat;
    }
}

static CMP_FORMAT Get32FVariation(CMP_FORMAT srcFormat)
{
    switch (srcFormat)
    {
    case CMP_FORMAT_ABGR_16F:
        return CMP_FORMAT_ABGR_32F;
    case CMP_FORMAT_ARGB_16F:
        return CMP_FORMAT_ARGB_32F;
    case CMP_FORMAT_BGRA_16F:
        return CMP_FORMAT_BGRA_32F;
    case CMP_FORMAT_R_16F:
        return CMP_FORMAT_R_32F;
    case CMP_FORMAT_RG_16F:
        return CMP_FORMAT_RG_32F;
    case CMP_FORMAT_RGBA_16F:
        return CMP_FORMAT_RGBA_32F;
    default:
        return srcFormat;
    }
}

// True when a buffer of bufferSize bytes holds at least numElements elements of elementSize bytes.
// Written as a division so the element count can never overflow the comparison.
static bool BufferHasElements(CMP_DWORD bufferSize, CMP_DWORD numElements, CMP_DWORD elementSize)
{
    if (elementSize == 0)
        return false;

    return numElements <= bufferSize / elementSize;
}

// Channels per pixel for the float formats that Get16FVariation()/Get32FVariation() can relabel.
// Returns 0 for anything else, because converting those would leave the result mislabelled.
static int GetFloatFormatChannelCount(CMP_FORMAT format)
{
    switch (format)
    {
    case CMP_FORMAT_R_16F:
    case CMP_FORMAT_R_32F:
        return 1;
    case CMP_FORMAT_RG_16F:
    case CMP_FORMAT_RG_32F:
        return 2;
    case CMP_FORMAT_ABGR_16F:
    case CMP_FORMAT_ABGR_32F:
    case CMP_FORMAT_ARGB_16F:
    case CMP_FORMAT_ARGB_32F:
    case CMP_FORMAT_BGRA_16F:
    case CMP_FORMAT_BGRA_32F:
    case CMP_FORMAT_RGBA_16F:
    case CMP_FORMAT_RGBA_32F:
        return 4;
    default:
        return 0;
    }
}

ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT         targetFormat,
                                       CMP_FORMAT         srcFormat,
                                       void*              srcData,
                                       CMP_DWORD          srcDataSize,
                                       CMP_DWORD          srcWidth,
                                       CMP_DWORD          srcHeight,
                                       const FloatParams* params)
{
    // The branches below all emit an ARGB/RGBA result, so they are 4-channel by construction;
    // the size checks reject anything narrower rather than over-reading the source.
    static const int numChannels = 4;

    CMP_ChannelFormat targetChannelFormat = GetChannelFormat(targetFormat);
    CMP_ChannelFormat srcChannelFormat    = GetChannelFormat(srcFormat);

    // GetChannelFormat() has no case for RGBE and falls back to CF_8bit, which FloatToByte()
    // rejects. The data is really the packed 4 byte layout that CF_Float9995E describes.
    if (srcFormat == CMP_FORMAT_RGBE_32F)
        srcChannelFormat = CF_Float9995E;

    ConvertedBuffer result = {};
    result.data            = srcData;
    result.dataSize        = srcDataSize;
    result.format          = srcFormat;

    bool isSrcFloat      = CMP_IsFloatFormat(srcFormat);
    bool isSrcCompressed = CMP_IsCompressedFormat(srcFormat);

    bool isTargetFloat  = CMP_IsFloatFormat(targetFormat);
    bool isTargetSigned = targetFormat == CMP_FORMAT_BC4_S || targetFormat == CMP_FORMAT_BC5_S || targetFormat == CMP_FORMAT_BC6H_SF;

    // Guard against integer overflow in width * height before any allocation or conversion.
    // Zero dimensions are rejected outright: they would reach calloc(0, ...), whose return value
    // is implementation defined and indistinguishable from an allocation failure.
    if (srcWidth == 0 || srcHeight == 0 || srcHeight > UINT32_MAX / srcWidth)
        return {};

    CMP_DWORD numPixels = srcWidth * srcHeight;

    // Every allocation below is sized from numPixels * numChannels
    if (numPixels > UINT32_MAX / (CMP_DWORD)numChannels)
        return {};

    const CMP_DWORD numElements = numPixels * (CMP_DWORD)numChannels;

    // Only uncompressed Float16<->Float32 pairs are repacked here. Any other float mismatch
    // (notably a compressed float source such as BC6H, whose channel format is not CF_Float*)
    // is left untouched for the codec to handle.
    const bool isHalfToFloat = targetChannelFormat == CF_Float32 && srcChannelFormat == CF_Float16;
    const bool isFloatToHalf = targetChannelFormat == CF_Float16 && srcChannelFormat == CF_Float32;

    if (isSrcFloat && isTargetFloat && (isHalfToFloat || isFloatToHalf))
    {
        // These two branches keep the source channel layout, so they can use the real channel
        // count instead of the 4-channel assumption the rest of this function makes.
        const int srcFloatChannels = GetFloatFormatChannelCount(srcFormat);
        if (srcFloatChannels == 0 || numPixels > UINT32_MAX / (CMP_DWORD)srcFloatChannels)
            return {};

        const CMP_DWORD numFloatElements = numPixels * (CMP_DWORD)srcFloatChannels;

        if (isHalfToFloat)
        {
            // srcData must really hold numFloatElements half floats, and the resulting
            // byte count has to stay representable in the CMP_DWORD dataSize field.
            if (!BufferHasElements(srcDataSize, numFloatElements, (CMP_DWORD)sizeof(CMP_HALFSHORT)) ||
                numFloatElements > UINT32_MAX / (CMP_DWORD)sizeof(CMP_FLOAT))
                return {};

            CMP_FLOAT* tempData = (CMP_FLOAT*)calloc(numFloatElements, sizeof(CMP_FLOAT));
            if (!tempData)
                return {};

            HalfShortToFloat(tempData, (CMP_HALFSHORT*)srcData, numFloatElements);

            result.data        = tempData;
            result.dataSize    = sizeof(CMP_FLOAT) * numFloatElements;
            result.format      = Get32FVariation(srcFormat);
            result.isBufferNew = true;
        }
        else if (isFloatToHalf)
        {
            // srcData must really hold numFloatElements floats, and the resulting byte
            // count has to stay representable in the CMP_DWORD dataSize field.
            if (!BufferHasElements(srcDataSize, numFloatElements, (CMP_DWORD)sizeof(CMP_FLOAT)) ||
                numFloatElements > UINT32_MAX / (CMP_DWORD)sizeof(CMP_HALFSHORT))
                return {};

            CMP_HALFSHORT* tempData = (CMP_HALFSHORT*)calloc(numFloatElements, sizeof(CMP_HALFSHORT));
            if (!tempData)
                return {};

            FloatToHalfShort(tempData, (CMP_FLOAT*)srcData, numFloatElements);

            result.data        = tempData;
            result.dataSize    = sizeof(CMP_HALFSHORT) * numFloatElements;
            result.format      = Get16FVariation(srcFormat);
            result.isBufferNew = true;
        }
    }
    else if (isSrcFloat && !isTargetFloat && !isSrcCompressed)
    {
        // Compressed float sources (BC6H, BC6H_SF) are excluded: their payload is not the raw
        // float samples FloatToByte() expects, so they are left for the codec to decode.
        CMP_BYTE* byteData = (CMP_BYTE*)calloc(numElements, 1);
        if (!byteData)
            return {};

        FloatParams tempParams = FloatParams();
        if (params)
            tempParams = *params;

        CMP_ERROR err = FloatToByte(byteData, numElements, (CMP_FLOAT*)srcData, srcDataSize, srcChannelFormat, srcWidth, srcHeight, &tempParams);

        if (err != CMP_OK)
        {
            free(byteData);
            return {};
        }

        result.data        = byteData;
        result.dataSize    = numElements;
        result.format      = CMP_FORMAT_ARGB_8888;
        result.isBufferNew = true;
    }
    else if (!isSrcFloat && isTargetFloat)
    {
        // buffer used to hold intermediate conversion results in cases where multiple conversion steps are needed
        ConvertedBuffer intermediateBuffer;
        intermediateBuffer.data     = srcData;
        intermediateBuffer.dataSize = result.dataSize;
        intermediateBuffer.format   = srcFormat;

        if (srcFormat == CMP_FORMAT_RGBA_8888_S)
        {
            // srcData must really hold numElements signed bytes.
            if (!BufferHasElements(srcDataSize, numElements, 1))
                return {};

            CMP_BYTE* byteData = (CMP_BYTE*)calloc(numElements, 1);
            if (!byteData)
                return {};

            SByteToByte(byteData, (CMP_SBYTE*)srcData, numElements);

            intermediateBuffer.data        = byteData;
            intermediateBuffer.dataSize    = numElements;
            intermediateBuffer.format      = CMP_FORMAT_ARGB_8888;
            intermediateBuffer.isBufferNew = true;
        }
        else if (srcFormat == CMP_FORMAT_RGBA_1010102)
        {
            // srcData must really hold numPixels packed 32 bit pixels.
            if (!BufferHasElements(srcDataSize, numPixels, (CMP_DWORD)sizeof(CMP_DWORD)))
                return {};

            CMP_BYTE* tempData = (CMP_BYTE*)calloc(numElements, 1);
            if (!tempData)
                return {};

            // disable the alpha channel of RGBA1010102 images because some of our test images had badly set alpha values,
            // resulting in undesirable outcomes for users
            ConvertRGBA1010102ToRGBA8888(tempData, srcData, numPixels, false);

            intermediateBuffer.data        = tempData;
            intermediateBuffer.dataSize    = numElements;
            intermediateBuffer.format      = CMP_FORMAT_RGBA_8888;
            intermediateBuffer.isBufferNew = true;
        }

        // the intermediate buffer (which is srcData itself when no conversion ran above)
        // must really hold numElements bytes, and the result size has to fit a CMP_DWORD.
        if (!BufferHasElements(intermediateBuffer.dataSize, numElements, 1) || numElements > UINT32_MAX / (CMP_DWORD)sizeof(CMP_HALFSHORT))
            return {};

        CMP_HALFSHORT* halfShortData = (CMP_HALFSHORT*)calloc(numElements, sizeof(CMP_HALFSHORT));
        if (!halfShortData)
            return {};

        ByteToHalfShort(halfShortData, (CMP_BYTE*)intermediateBuffer.data, numElements);

        // intermediateBuffer owns any buffer it allocated and releases it in its destructor;
        // freeing it explicitly here would be a double free.

        result.data        = halfShortData;
        result.dataSize    = numElements * (CMP_DWORD)sizeof(CMP_HALFSHORT);
        result.format      = CMP_FORMAT_RGBA_16F;  // CMP_FORMAT_ARGB_16F;
        result.isBufferNew = true;
    }
    else if (!isSrcFloat && !isTargetFloat)
    {
        // Both channels are not float, check for matching source & target formats.
        CMP_BYTE srcBitSize    = GetChannelFormatBitSize(srcFormat);
        CMP_BYTE targetBitSize = GetChannelFormatBitSize(targetFormat);

        if (srcFormat == CMP_FORMAT_RGBA_1010102 && targetBitSize == 8)
        {
            // srcData must really hold numPixels packed 32 bit pixels.
            if (!BufferHasElements(srcDataSize, numPixels, (CMP_DWORD)sizeof(CMP_DWORD)))
                return {};

            CMP_BYTE* byteData = (CMP_BYTE*)calloc(numElements, 1);
            if (!byteData)
                return {};

            // disable the alpha channel of RGBA1010102 images because some of our test images had badly set alpha values,
            // resulting in undesirable outcomes for users
            ConvertRGBA1010102ToRGBA8888(byteData, srcData, numPixels, false);

            result.data        = byteData;
            result.dataSize    = numElements;
            result.format      = CMP_FORMAT_RGBA_8888;
            result.isBufferNew = true;
        }
        else if (srcBitSize != targetBitSize)
        {
            // srcData must really hold numElements 16 bit words.
            if (!BufferHasElements(srcDataSize, numElements, (CMP_DWORD)sizeof(CMP_WORD)))
                return {};

            CMP_BYTE* byteData = (CMP_BYTE*)calloc(numElements, 1);
            if (!byteData)
                return {};

            WordToByte(byteData, (CMP_WORD*)srcData, numElements);

            result.data        = byteData;
            result.dataSize    = numElements;
            result.format      = CMP_FORMAT_ARGB_8888;
            result.isBufferNew = true;
        }
        else if (srcBitSize == targetBitSize && srcFormat == CMP_FORMAT_RGBA_8888_S && !isTargetSigned)
        {
            // srcData must really hold numElements signed bytes.
            if (!BufferHasElements(srcDataSize, numElements, 1))
                return {};

            CMP_BYTE* byteData = (CMP_BYTE*)calloc(numElements, 1);
            if (!byteData)
                return {};

            SByteToByte(byteData, (CMP_SBYTE*)srcData, numElements);

            result.data        = byteData;
            result.dataSize    = numElements;
            result.format      = CMP_FORMAT_ARGB_8888;
            result.isBufferNew = true;
        }
    }

    return std::move(result);
}
ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT targetFormat, const MipSet* srcMipSet, const FloatParams* params)
{
    assert(srcMipSet);
    if (!srcMipSet)
        return {};

    return CreateCompatibleBuffer(targetFormat, srcMipSet->m_format, srcMipSet->pData, srcMipSet->dwDataSize, srcMipSet->dwWidth, srcMipSet->dwHeight, params);
}
ConvertedBuffer CreateCompatibleBuffer(CMP_FORMAT targetFormat, const CMP_Texture* srcTexture, const FloatParams* params)
{
    assert(srcTexture);
    if (!srcTexture)
        return {};

    return CreateCompatibleBuffer(
        targetFormat, srcTexture->format, srcTexture->pData, srcTexture->dwDataSize, srcTexture->dwWidth, srcTexture->dwHeight, params);
}

static inline float Clamp(float a, float l, float h)
{
    return (a < l) ? l : ((a > h) ? h : a);
}

static inline float Knee(double x, double f)
{
    return float(log(x * f + 1.f) / f);
}

static float FindKneeValue(float x, float y)
{
    float f0 = 0;
    float f1 = 1.f;

    while (Knee(x, f1) > y)
    {
        f0 = f1;
        f1 = f1 * 2.f;
    }

    for (int i = 0; i < 30; ++i)
    {
        const float f2 = (f0 + f1) / 2.f;
        const float y2 = Knee(x, f2);

        if (y2 < y)
        {
            f1 = f2;
        }
        else
        {
            f0 = f2;
        }
    }

    return (f0 + f1) / 2.f;
}

CMP_ERROR FloatToByte(CMP_BYTE*          outBuffer,
                      CMP_DWORD          outBufferSize,
                      CMP_FLOAT*         inBuffer,
                      CMP_DWORD          inBufferSize,
                      CMP_ChannelFormat  channelFormat,
                      CMP_DWORD          width,
                      CMP_DWORD          height,
                      const FloatParams* params)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(params);

    if (!inBuffer)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;
    // params is dereferenced unconditionally below; the assert above is compiled out in release.
    if (!params)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;

    // Guard against integer overflow in width * height
    if (width > 0 && height > UINT32_MAX / width)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;

    const CMP_DWORD numPixels = width * height;

    if (numPixels > 0)
    {
        // Determine the per-pixel byte size of the input buffer based on channel format
        CMP_DWORD bytesPerPixelIn = 0;
        if (channelFormat == CF_Float16)
            bytesPerPixelIn = 4u * static_cast<CMP_DWORD>(sizeof(CMP_HALF));  // 8 bytes/pixel (4x float16)
        else if (channelFormat == CF_Float32)
            bytesPerPixelIn = 4u * static_cast<CMP_DWORD>(sizeof(CMP_FLOAT));  // 16 bytes/pixel (4x float32)
        else if (channelFormat == CF_Float9995E)
            bytesPerPixelIn = static_cast<CMP_DWORD>(sizeof(CMP_DWORD));  // 4 bytes/pixel (packed R9G9B9E5)
        else
            return CMP_ERR_INVALID_SOURCE_TEXTURE;

        // Validate input buffer is large enough for the declared dimensions.
        // Written as division to avoid overflow: numPixels * bytesPerPixelIn > inBufferSize
        if (numPixels > inBufferSize / bytesPerPixelIn)
            return CMP_ERR_INVALID_SOURCE_TEXTURE;

        // Validate output buffer is large enough: always 4 bytes/pixel (RGBA8888 output).
        // Written as division to avoid overflow: numPixels * 4 > outBufferSize
        if (numPixels > outBufferSize / 4u)
            return CMP_ERR_INVALID_DEST_TEXTURE;
    }

    CMP_FLOAT* floatData     = inBuffer;
    CMP_HALF*  halfFloatData = (CMP_HALF*)inBuffer;
    CMP_DWORD* pixelData     = (CMP_DWORD*)inBuffer;

    const float kl          = powf(2.f, params->kneeLow);
    const float f           = FindKneeValue(powf(2.f, params->kneeHigh) - kl, powf(2.f, 3.5f) - kl);
    const float luminance3f = powf(2, -3.5);      // always assume max intensity is 1 and 3.5f darker for scale later
    const float invGamma    = 1 / params->gamma;  //for gamma correction
    const float scale       = (float)255.0 * powf(luminance3f, invGamma);

    // Unsigned so the byte offset can address the whole range permitted by the output-size guard
    // above (up to UINT32_MAX / 4 pixels); a signed int would overflow first.
    CMP_DWORD outIndex = 0;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            float r = 0, g = 0, b = 0, a = 0;

            if (channelFormat == CF_Float16)
            {
                r = (float)(*halfFloatData);
                ++halfFloatData;
                g = (float)(*halfFloatData);
                ++halfFloatData;
                b = (float)(*halfFloatData);
                ++halfFloatData;
                a = (float)(*halfFloatData);
                ++halfFloatData;
            }
            else if (channelFormat == CF_Float32)
            {
                r = (float)(*floatData);
                ++floatData;
                g = (float)(*floatData);
                ++floatData;
                b = (float)(*floatData);
                ++floatData;
                a = (float)(*floatData);
                ++floatData;
            }
            else if (channelFormat == CF_Float9995E)
            {
                union
                {
                    int32_t i;
                    float   f;
                } helper;

                uint32_t rm = (*pixelData) & 0x000001ff;
                uint32_t gm = ((*pixelData) & 0x0003fe00) >> 9;
                uint32_t bm = ((*pixelData) & 0x07fc0000) >> 18;
                uint32_t e  = ((*pixelData) & 0xf8000000) >> 27;

                helper.i = 0x33800000 + (e << 23);

                r = helper.f * (float)rm;
                g = helper.f * (float)gm;
                b = helper.f * (float)bm;
                a = 1.0f;

                ++pixelData;
            }

            //  1) Compensate for fogging by subtracting defog
            //     from the raw pixel values.
            // We assume a defog of 0
            if (params->defog > 0.0)
            {
                r = r - params->defog;
                g = g - params->defog;
                b = b - params->defog;
                a = a - params->defog;
            }

            //  2) Multiply the defogged pixel values by
            //     2^(exposure + 2.47393).
            const float exposeScale = powf(2, params->exposure + 2.47393f);
            r                       = r * exposeScale;
            g                       = g * exposeScale;
            b                       = b * exposeScale;
            a                       = a * exposeScale;

            //  3) Values that are now 1.0 are called "middle gray".
            //     If defog and exposure are both set to 0.0, then
            //     middle gray corresponds to a raw pixel value of 0.18.
            //     In step 6, middle gray values will be mapped to an
            //     intensity 3.5 f-stops below the display's maximum
            //     intensity.

            //  4) Apply a knee function.  The knee function has two
            //     parameters, kneeLow and kneeHigh.  Pixel values
            //     below 2^kneeLow are not changed by the knee
            //     function.  Pixel values above kneeLow are lowered
            //     according to a logarithmic curve, such that the
            //     value 2^kneeHigh is mapped to 2^3.5.  (In step 6,
            //     this value will be mapped to the the display's
            //     maximum intensity.)
            if (r > kl)
                r = kl + Knee(r - kl, f);
            if (g > kl)
                g = kl + Knee(g - kl, f);
            if (b > kl)
                b = kl + Knee(b - kl, f);
            if (a > kl)
                a = kl + Knee(a - kl, f);

            //  5) Gamma-correct the pixel values, according to the
            //     screen's gamma.  (We assume that the gamma curve
            //     is a simple power function.)
            r = powf(r, invGamma);
            g = powf(g, invGamma);
            b = powf(b, invGamma);
            a = powf(a, params->gamma);

            //  6) Scale the values such that middle gray pixels are
            //     mapped to a frame buffer value that is 3.5 f-stops
            //     below the display's maximum intensity.
            r *= scale;
            g *= scale;
            b *= scale;
            a *= scale;

            CMP_BYTE byteR = (CMP_BYTE)Clamp(r, 0.f, 255.f);
            CMP_BYTE byteG = (CMP_BYTE)Clamp(g, 0.f, 255.f);
            CMP_BYTE byteB = (CMP_BYTE)Clamp(b, 0.f, 255.f);
            CMP_BYTE byteA = (CMP_BYTE)Clamp(a, 0.f, 255.f);

            outBuffer[outIndex++] = byteR;
            outBuffer[outIndex++] = byteG;
            outBuffer[outIndex++] = byteB;
            outBuffer[outIndex++] = byteA;
        }
    }

    return CMP_OK;
}
CMP_ERROR FloatToByte(CMP_BYTE* outBuffer, CMP_DWORD outBufferSize, CMP_FLOAT* inBuffer, const MipSet* srcMipSet, const FloatParams* params)
{
    if (!srcMipSet)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;

    const CMP_DWORD width  = static_cast<CMP_DWORD>(srcMipSet->dwWidth);
    const CMP_DWORD height = static_cast<CMP_DWORD>(srcMipSet->dwHeight);

    // OutBufferSize is the caller's real allocation size. Deriving it from
    // width * height * 4 here would only restate the required size and would let an
    // undersized destination through, which is exactly what the core overload checks.
    return FloatToByte(outBuffer, outBufferSize, inBuffer, srcMipSet->dwDataSize, srcMipSet->m_ChannelFormat, width, height, params);
}
CMP_ERROR FloatToByte(CMP_BYTE* outBuffer, CMP_DWORD outBufferSize, CMP_FLOAT* inBuffer, const CMP_Texture* srcTexture, const FloatParams* params)
{
    if (!srcTexture)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;

    const CMP_ChannelFormat channelFormat = GetChannelFormat(srcTexture->format);

    // See the MipSet overload above -- outBufferSize must come from the caller.
    return FloatToByte(outBuffer, outBufferSize, inBuffer, srcTexture->dwDataSize, channelFormat, srcTexture->dwWidth, srcTexture->dwHeight, params);
}

CMP_ERROR HalfShortToFloat(CMP_FLOAT* outBuffer, CMP_HALFSHORT* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (CMP_DWORD i = 0; i < numElements; i++)
    {
        outBuffer[i] = HalfShortToFloat(inBuffer[i]);
    }

    return CMP_OK;
}

CMP_FLOAT HalfShortToFloat(CMP_HALFSHORT halfShort)
{
    CMP_HALF h;
    h.setBits(halfShort);
    return (float)h;
}

CMP_ERROR FloatToHalfShort(CMP_HALFSHORT* outBuffer, CMP_FLOAT* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (CMP_DWORD i = 0; i < numElements; i++)
    {
        outBuffer[i] = CMP_HALF(inBuffer[i]).bits();
    }

    return CMP_OK;
}

CMP_ERROR ByteToHalfShort(CMP_HALFSHORT* outBuffer, CMP_BYTE* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (CMP_DWORD i = 0; i < numElements; i++)
    {
        outBuffer[i] = CMP_HALF(float(inBuffer[i] / 255.0f)).bits();
    }

    return CMP_OK;
}

CMP_ERROR WordToByte(CMP_BYTE* outBuffer, CMP_WORD* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (CMP_DWORD i = 0; i < numElements; i++)
    {
        outBuffer[i] = (CMP_BYTE)(inBuffer[i] / 257);
    }

    return CMP_OK;
}

CMP_ERROR SByteToByte(CMP_BYTE* outBuffer, CMP_SBYTE* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (unsigned int i = 0; i < numElements; ++i)
    {
        outBuffer[i] = inBuffer[i] + 127;
    }

    return CMP_OK;
}

CMP_ERROR ByteToSByte(CMP_SBYTE* outBuffer, CMP_BYTE* inBuffer, CMP_DWORD numElements)
{
    assert(outBuffer);
    assert(inBuffer);
    assert(numElements > 0);

    if (!inBuffer || numElements <= 0)
        return CMP_ERR_INVALID_SOURCE_TEXTURE;
    if (!outBuffer)
        return CMP_ERR_INVALID_DEST_TEXTURE;

    for (unsigned int i = 0; i < numElements; ++i)
    {
        outBuffer[i] = inBuffer[i] - 127;
    }

    return CMP_OK;
}

void ConvertRGBA1010102ToRGBA8888(void* outBuffer, const void* inBuffer, unsigned long numPixels, bool hasAlpha)
{
    if (!inBuffer || !outBuffer || numPixels == 0)
        return;

    CMP_DWORD* inPixels  = (CMP_DWORD*)inBuffer;
    CMP_DWORD* outPixels = (CMP_DWORD*)outBuffer;

    static const CMP_BYTE alphaTable[] = {0x00, 0x0f, 0xf0, 0xff};

    for (uint32_t i = 0; i < numPixels; ++i)
    {
        CMP_BYTE r = (CMP_BYTE)((inPixels[i] >> 2) & TEN_BIT_MASK);
        CMP_BYTE g = (CMP_BYTE)((inPixels[i] >> 12) & TEN_BIT_MASK);
        CMP_BYTE b = (CMP_BYTE)((inPixels[i] >> 22) & TEN_BIT_MASK);

        CMP_BYTE a = 255;
        if (hasAlpha)
        {
            int alphaIndex = ((inPixels[i] >> 30) & TWO_BIT_MASK);
            a              = alphaTable[alphaIndex];
        }

        outPixels[i] = ((CMP_DWORD)a << 24) | ((CMP_DWORD)b << 16) | ((CMP_DWORD)g << 8) | (CMP_DWORD)r;
    }
}