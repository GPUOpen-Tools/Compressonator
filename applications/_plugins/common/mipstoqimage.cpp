//=====================================================================
// Copyright 2019-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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

#include "mipstoqimage.h"
#include "format_conversion.h"

#ifndef OPTION_CMP_QT
int QImage2MIPS(QImage*, CMIPS*, MipSet*)
{
    return 0;  // no-op
}
QImage* MIPS2QImage(CMIPS*, MipSet*, int, int, CMP_CompressOptions, CMP_Feedback_Proc)
{
    return nullptr;  // no-op
}
#else
#include "textureio.h"

#include <QImage>

#ifdef _DEBUG
#pragma comment(lib, "Qt5Cored.lib")
#pragma comment(lib, "Qt5Guid.lib")
#else
#pragma comment(lib, "Qt5Core.lib")
#pragma comment(lib, "Qt5Gui.lib")
#endif

static float cmp_clampf(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

/* conversion from the ILM Half
* format into the normal 32 bit pixel format. Refer to http://www.openexr.com/using.html
* on each steps regarding how to display your image
*/
unsigned int floatToQrgba(float r, float g, float b, float a)
{
    // step 3) Values, which are now 1.0, are called "middle gray".
    //     If defog and exposure are both set to 0.0, then
    //     middle gray corresponds to a raw pixel value of 0.18.
    //     In step 6, middle gray values will be mapped to an
    //     intensity 3.5 f-stops below the display's maximum
    //     intensity.

    // step 4) Apply a knee function.  The knee function has two
    //     parameters, kneeLow and kneeHigh.  Pixel values
    //     below 2^kneeLow are not changed by the knee
    //     function.  Pixel values above kneeLow are lowered
    //     according to a logarithmic curve, such that the
    //     value 2^kneeHigh is mapped to 2^3.5 (in step 6,
    //     this value will be mapped to the display's
    //     maximum intensity).
    //     kneeLow = 0.0 (2^0.0 => 1); kneeHigh = 5.0 (2^5 =>32)
    if (r > 1.0)
        r = 1.0 + log((r - 1.0) * 0.184874 + 1) / 0.184874;
    if (g > 1.0)
        g = 1.0 + log((g - 1.0) * 0.184874 + 1) / 0.184874;
    if (b > 1.0)
        b = 1.0 + log((b - 1.0) * 0.184874 + 1) / 0.184874;
    if (a > 1.0)
        a = 1.0 + log((a - 1.0) * 0.184874 + 1) / 0.184874;
    //
    // Step 5) Gamma-correct the pixel values, assuming that the
    //     screen's gamma is 0.4545 (or 1/2.2).
    r = pow(r, 0.4545f);
    g = pow(g, 0.4545f);
    b = pow(b, 0.4545f);
    a = pow(a, 0.4545f);

    // Step  6) Scale the values such that pixels middle gray
    //     pixels are mapped to 84.66 (or 3.5 f-stops below
    //     the display's maximum intensity).
    //
    // Step 7) Clamp the values to [0, 255].
    return qRgba((unsigned char)(cmp_clampf(r * 84.66f, 0.f, 255.f)),
                 (unsigned char)(cmp_clampf(g * 84.66f, 0.f, 255.f)),
                 (unsigned char)(cmp_clampf(b * 84.66f, 0.f, 255.f)),
                 (unsigned char)(cmp_clampf(a * 84.66f, 0.f, 255.f)));
}

QImage::Format MipFormat2QFormat(MipSet* mipset)
{
    QImage::Format format = QImage::Format_Invalid;

    if (CMP_IsCompressedFormat(mipset->m_format))
        return format;

    switch (mipset->m_ChannelFormat)
    {
    case CF_8bit: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_Float16: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_Float32: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_Float9995E: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_Compressed: {
        break;
    }
    case CF_16bit: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_2101010:
    case CF_1010102: {
        format = QImage::Format_ARGB32;
        break;
    }
    case CF_32bit: {
        format = QImage::Format_ARGB32;
        break;
    }
    default: {
        break;
    }
    }

    return format;
}

int QImage2MIPS(QImage* image, CMIPS* m_CMips, MipSet* pMipSet, CMP_Feedback_Proc pFeedbackProc)
{
    if (image == nullptr)
    {
        return -1;
    }

    // QImage info for debugging
    // QImageFormatInfo(image);
    QImage::Format format = image->format();

    // Check supported format
    if (!((format == QImage::Format_ARGB32) || (format == QImage::Format_ARGB32_Premultiplied) || (format == QImage::Format_RGB32) ||
          (format == QImage::Format_Mono) || (format == QImage::Format_Grayscale8) || (format == QImage::Format_Indexed8)))
    {
        return -1;
    }

    // Set the channel formats and mip levels
    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_TextureType     = TT_2D;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;
    pMipSet->m_nDepth          = 1;  // depthsupport

    // Allocate default MipSet header
    m_CMips->AllocateMipSet(pMipSet,
                            pMipSet->m_ChannelFormat,
                            pMipSet->m_TextureDataType,
                            pMipSet->m_TextureType,
                            image->width(),
                            image->height(),
                            pMipSet->m_nDepth);  // depthsupport, what should nDepth be set as here?

    // Determine buffer size and set Mip Set Levels we want to use for now
    MipLevel* mipLevel    = m_CMips->GetMipLevel(pMipSet, 0);
    pMipSet->m_nMipLevels = 1;
    m_CMips->AllocateMipLevelData(mipLevel, pMipSet->m_nWidth, pMipSet->m_nHeight, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType);

    // We have allocated a data buffer to fill get its referance
    CMP_BYTE* pData = (CMP_BYTE*)(mipLevel->m_pbData);
    QRgb      qRGB;
    int       i = 0;

    if (pMipSet->m_swizzle)
    {
        for (int y = 0; y < image->height(); y++)
        {
            for (int x = 0; x < image->width(); x++)
            {
                qRGB     = image->pixel(x, y);
                pData[i] = qBlue(qRGB);
                i++;
                pData[i] = qGreen(qRGB);
                i++;
                pData[i] = qRed(qRGB);
                i++;
                pData[i] = qAlpha(qRGB);
                i++;
            }

            if (pFeedbackProc)
            {
                float fProgress = 100.f * (y * image->width()) / (image->width() * image->height());
                if (pFeedbackProc(fProgress, NULL, NULL))
                    return -1;
            }
        }
        pMipSet->m_swizzle = false;  //already swizzled; reset
    }
    else
    {
        for (int y = 0; y < image->height(); y++)
        {
            for (int x = 0; x < image->width(); x++)
            {
                qRGB     = image->pixel(x, y);
                pData[i] = qRed(qRGB);
                i++;
                pData[i] = qGreen(qRGB);
                i++;
                pData[i] = qBlue(qRGB);
                i++;
                pData[i] = qAlpha(qRGB);
                i++;
            }

            if (pFeedbackProc)
            {
                float fProgress = 100.f * (y * image->width()) / (image->width() * image->height());
                if (pFeedbackProc(fProgress, NULL, NULL))
                    return -1;
            }
        }
    }

    return 0;
}

// SNORM int ranges from -128 to 127
CMP_BYTE CMP_SBYTE_to_UBYTE(CMP_SBYTE value)
{
    if (value < -128)
        return 0;

    if (value > 127)
        return (255);

    return (value + 128);
}

//load data byte in mipset into Qimage ARGB32 format
// TODO: A lot of this could probabaly be reworked and removed in favor of using the functions in format_conversion.h
QImage* MIPS2QImage(CMIPS* m_CMips, MipSet* tmpMipSet, int mipmapLevel, int depthLevel, CMP_CompressOptions option, CMP_Feedback_Proc pFeedbackProc)
{
    if (tmpMipSet == NULL)
    {
        QImage* image = new QImage(":/compressonatorgui/images/compressedimageerror.png");
        return image;
    }

    MipLevel* mipLevel = m_CMips->GetMipLevel(tmpMipSet, mipmapLevel, depthLevel);
    if (!mipLevel)
    {
        return nullptr;
    }

    FloatParams params(&option);
    QImage*     image = NULL;

    if ((tmpMipSet->m_TextureDataType == TDT_ARGB) || (tmpMipSet->m_TextureDataType == TDT_XRGB))
    {
        if ((tmpMipSet->m_ChannelFormat == CF_Float32) || (tmpMipSet->m_ChannelFormat == CF_Float16))
        {
            if (tmpMipSet->m_ChannelFormat == CF_Float32)
            {
                float* pData = mipLevel->m_pfData;
                if (pData == NULL)
                    return nullptr;
            }
            else if (tmpMipSet->m_ChannelFormat == CF_Float16)
            {
                CMP_HALFSHORT* pData = mipLevel->m_phfsData;
                if (pData == NULL)
                    return nullptr;
            }
            else
            {
                CMP_WORD* pData = mipLevel->m_pwData;
                if (pData == NULL)
                    return nullptr;
            }

            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, QImage::Format_RGBA8888);
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            // QImage pixels are uninitialized until written, so a failed conversion must not be returned.
            if (FloatToByte(image->bits(),
                            static_cast<CMP_DWORD>(image->byteCount()),
                            mipLevel->m_pfData,
                            mipLevel->m_dwLinearSize,
                            tmpMipSet->m_ChannelFormat,
                            static_cast<CMP_DWORD>(mipLevel->m_nWidth),
                            static_cast<CMP_DWORD>(mipLevel->m_nHeight),
                            &params) != CMP_OK)
            {
                delete image;
                return nullptr;
            }
        }
        else if (tmpMipSet->m_ChannelFormat == CF_Float9995E)
        {
            float* pData = mipLevel->m_pfData;
            if (pData == NULL)
                return nullptr;

            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, QImage::Format_RGBA8888);
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            // QImage pixels are uninitialized until written, so a failed conversion must not be returned.
            if (FloatToByte(image->bits(),
                            static_cast<CMP_DWORD>(image->byteCount()),
                            mipLevel->m_pfData,
                            mipLevel->m_dwLinearSize,
                            tmpMipSet->m_ChannelFormat,
                            static_cast<CMP_DWORD>(mipLevel->m_nWidth),
                            static_cast<CMP_DWORD>(mipLevel->m_nHeight),
                            &params) != CMP_OK)
            {
                delete image;
                return nullptr;
            }
        }
        else if (tmpMipSet->m_ChannelFormat == CF_16bit)
        {
            // We have allocated a data buffer to fill get its referance
            CMP_WORD* pData = mipLevel->m_pwData;
            if (pData == NULL)
                return nullptr;

            // We dont support the conversion
            if (MipFormat2QFormat(tmpMipSet) == QImage::Format_Invalid)
            {
                PrintInfo("Conversion Format is not supported!");
                return nullptr;
            }

            // Allocates a uninitialized buffer of specified size and format
            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, MipFormat2QFormat(tmpMipSet));
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            //QImageFormatInfo(image);

            bool isRGBA       = (tmpMipSet->m_TextureDataType == TDT_ARGB) ? true : false;
            bool isFixedAlpha = false;

            // BC4 only Red channel is valid. All other channels are set equal to that channels value
            // Compressonator decoder also set Alpha to the red channel value!
            // The Alpha should be set to 255 if viewing in GUI!
            if (tmpMipSet->m_isDeCompressed == CMP_FORMAT_ATI1N)
            {
                isFixedAlpha = true;
            }

            // Initialize the buffer
            CMP_BYTE R, G, B, A;
            int      i = 0;
            for (int y = 0; y < mipLevel->m_nHeight; y++)
            {
                for (int x = 0; x < mipLevel->m_nWidth; x++)
                {
                    R = pData[i] / 257;
                    i++;
                    G = pData[i] / 257;
                    i++;
                    B = pData[i] / 257;
                    i++;
                    if (isRGBA)
                    {
                        if (isFixedAlpha)
                            A = 255;
                        else
                            A = pData[i] / 257;
                        i++;
                    }
                    else
                        A = 255;
                    image->setPixel(x, y, qRgba(R, G, B, A));
                }
                if (pFeedbackProc)
                {
                    float fProgress = 100.f * (y * mipLevel->m_nWidth) / (mipLevel->m_nWidth * mipLevel->m_nHeight);
                    if (pFeedbackProc(fProgress, NULL, NULL))
                        return NULL;
                }
            }
        }
        else if (tmpMipSet->m_ChannelFormat == CF_1010102)
        {
            // We have allocated a data buffer to fill get its referance
            CMP_DWORD* pData = mipLevel->m_pdwData;
            if (pData == NULL)
                return nullptr;

            // Allocates a uninitialized buffer of specified size and format
            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, QImage::Format_RGBA8888);
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            // To fix potential issues with some RGBA1010102 images with bad alpha channels, we disable
            // the alpha of RGBA1010102 images in the GUI
            bool hasAlpha = false;

            ConvertRGBA1010102ToRGBA8888((CMP_DWORD*)image->bits(), pData, mipLevel->m_nWidth * mipLevel->m_nHeight, hasAlpha);
        }
        else
        {
            // CF_8bit

            // We have allocated a data buffer to fill get its referance

            if (mipLevel->m_pbData == NULL)
                return nullptr;

            // We dont support the conversion
            if (MipFormat2QFormat(tmpMipSet) == QImage::Format_Invalid)
            {
                PrintInfo("Mipset to Qt format is not supported!\n");
                return nullptr;
            }

            // Allocates a uninitialized buffer of specified size and format
            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, MipFormat2QFormat(tmpMipSet));
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            //QImageFormatInfo(image);

            bool isRGBA       = (tmpMipSet->m_TextureDataType == TDT_ARGB) ? true : false;
            bool isFixedAlpha = false;

            CMP_BYTE channels;

            // BC4 only Red channel is valid. All other channels are set equal to that channels value
            // Compressonator decoder also set Alpha to the red channel value!
            // The Alpha should be set to 255 if viewing in GUI!
            if ((tmpMipSet->m_isDeCompressed == CMP_FORMAT_ATI1N) || (tmpMipSet->m_isDeCompressed == CMP_FORMAT_BC4) ||
                (tmpMipSet->m_isDeCompressed == CMP_FORMAT_BC4_S))
            {
                isFixedAlpha = true;
                channels     = 0b0001;  // red only
            }
            else if ((tmpMipSet->m_isDeCompressed == CMP_FORMAT_ATI2N) || (tmpMipSet->m_isDeCompressed == CMP_FORMAT_BC5) ||
                     (tmpMipSet->m_isDeCompressed == CMP_FORMAT_BC5_S))
            {
                isFixedAlpha = true;
                channels     = 0b0011;  // red + green only
            }
            else
                channels = 0b1111;

            if ((tmpMipSet->m_TextureDataType == TDT_XRGB) || (tmpMipSet->m_TextureDataType == TDT_RGB))
            {
                isRGBA       = true;
                isFixedAlpha = true;
                channels     = 0b0111;
            }

            // Initialize the buffer
            if (tmpMipSet->m_format == CMP_FORMAT_RGBA_8888_S)
            {
                CMP_SBYTE* pData = mipLevel->m_psbData;
                CMP_SBYTE  R, G, B, A;
                CMP_FLOAT  nR, nG, nB, nA;
                CMP_BYTE   bR, bG, bB, bA;
                int        i = 0;
                for (int y = 0; y < mipLevel->m_nHeight; y++)
                {
                    for (int x = 0; x < mipLevel->m_nWidth; x++)
                    {
                        R = pData[i];
                        i++;
                        G = pData[i];
                        i++;
                        B = pData[i];
                        i++;

                        if (isRGBA)
                        {
                            if (isFixedAlpha)
                                A = 127;
                            else
                                A = pData[i];
                            i++;
                        }
                        else
                            A = 127;

                        // Normalize signed int
                        nR = R / 127.0f;
                        nG = G / 127.0f;
                        nB = B / 127.0f;
                        nA = A / 127.0f;

                        // Covert from SNORM -> UINT
                        bR = ((nR * 0.5) + 0.5) * 255.0f;
                        bG = ((nG * 0.5) + 0.5) * 255.0f;
                        bB = ((nB * 0.5) + 0.5) * 255.0f;
                        bA = 255;

                        //CMP_SBYTE_to_UBYTE(A);
                        image->setPixel(x, y, qRgba(bR, bG, bB, 255));
                    }
                    if (pFeedbackProc)
                    {
                        float fProgress = 100.f * (y * mipLevel->m_nWidth) / (mipLevel->m_nWidth * mipLevel->m_nHeight);
                        if (pFeedbackProc(fProgress, NULL, NULL))
                            return NULL;
                    }
                }
            }
            else
            {
                CMP_BYTE* pData = mipLevel->m_pbData;
                CMP_BYTE  R, G, B, A;
                int       i = 0;
                for (int y = 0; y < mipLevel->m_nHeight; y++)
                {
                    for (int x = 0; x < mipLevel->m_nWidth; x++)
                    {
                        R = pData[i];
                        i++;
                        G = pData[i];
                        i++;
                        B = pData[i];
                        i++;
                        if (isRGBA)
                        {
                            if (isFixedAlpha)
                                A = 255;
                            else
                                A = pData[i];
                            i++;
                        }
                        else
                            A = 255;
                        image->setPixel(x, y, qRgba(R, G, B, A));
                    }
                    if (pFeedbackProc)
                    {
                        float fProgress = 100.f * (y * mipLevel->m_nWidth) / (mipLevel->m_nWidth * mipLevel->m_nHeight);
                        if (pFeedbackProc(fProgress, NULL, NULL))
                            return NULL;
                    }
                }
            }
        }
    }
    else if (tmpMipSet->m_TextureDataType == TDT_R)
    {
        if (tmpMipSet->m_ChannelFormat == CF_16bit)
        {
            // We have allocated a data buffer to fill get its referance
            CMP_WORD* pData = mipLevel->m_pwData;
            if (pData == NULL)
                return nullptr;

            // Allocates a uninitialized buffer of specified size and format
            image = new QImage(mipLevel->m_nWidth, mipLevel->m_nHeight, MipFormat2QFormat(tmpMipSet));
            if (image == NULL)
            {
                image = new QImage(":/compressonatorgui/images/outofmemoryerror.png");
                return nullptr;
            }

            //QImageFormatInfo(image);

            bool isRGBA       = (tmpMipSet->m_TextureDataType == TDT_ARGB) ? true : false;
            bool isFixedAlpha = false;

            // BC4 only Red channel is valid. All other channels are set equal to that channels value
            // Compressonator decoder also set Alpha to the red channel value!
            // The Alpha should be set to 255 if viewing in GUI!
            if (tmpMipSet->m_isDeCompressed == CMP_FORMAT_ATI1N)
            {
                isFixedAlpha = true;
            }

            // Initialize the buffer
            CMP_BYTE R, G, B, A;
            int      i = 0;
            for (int y = 0; y < mipLevel->m_nHeight; y++)
            {
                for (int x = 0; x < mipLevel->m_nWidth; x++)
                {
                    R = pData[i] / 257;
                    i++;
                    G = pData[i] / 257;
                    i++;
                    B = pData[i] / 257;
                    i++;
                    if (isRGBA)
                    {
                        if (isFixedAlpha)
                            A = 255;
                        else
                            A = pData[i] / 257;
                        i++;
                    }
                    else
                        A = 255;
                    image->setPixel(x, y, qRgba(R, G, B, A));
                }
                if (pFeedbackProc)
                {
                    float fProgress = 100.f * (y * mipLevel->m_nWidth) / (mipLevel->m_nWidth * mipLevel->m_nHeight);
                    if (pFeedbackProc(fProgress, NULL, NULL))
                        return NULL;
                }
            }
        }
    }
    return image;
}
#endif
