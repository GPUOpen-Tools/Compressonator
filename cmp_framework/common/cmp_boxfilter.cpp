//=====================================================================
// Copyright 2020-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
// Copyright 2008 (c), ATI Technologies Inc. All rights reserved.
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

#include <stdio.h>
#include "cmp_mips.h"
#include "cmp_boxfilter.h"
#include "format_conversion.h"
#include "atiformats.h"

// the filter used for mipmap generation, holds pixel pointers for the four corners of the box
union BoxFilter
{
    BoxFilter()
        : tl(0)
        , tr(0)
        , bl(0)
        , br(0)
    {
    }

    struct
    {
        CMP_BYTE* tl;
        CMP_BYTE* tr;
        CMP_BYTE* bl;
        CMP_BYTE* br;
    };
    CMP_BYTE* pixels[4];
};

void CMP_SetMipLevelGammaLinearB(MipLevel* pCurMipLevel, CMP_BYTE* pdata, CMP_FLOAT Gamma, CMP_INT numchannels)
{
    for (int y = 0; y < pCurMipLevel->m_nHeight; y++)
    {
        for (int x = 0; x < pCurMipLevel->m_nWidth; x++)
        {
            // calc Gamma for the all color channels
            for (int i = 0; i < 3 && i < numchannels; i++)
            {
                CMP_FLOAT normpixel = 0.0f;
                if (*pdata > 0)
                {
                    normpixel = *pdata;
                    normpixel = normpixel / 255.0f;
                    normpixel = powf(normpixel, Gamma) * 255.0f;

                    // need to check for signed components
                    if (normpixel > 255)
                        normpixel = 255;
                    else if (normpixel < 0)
                        normpixel = 0;

                    *pdata = (CMP_BYTE)round(normpixel);
                }
                pdata++;
            }
            // if alpha skip it
            if (numchannels > 3)
                pdata++;
        }
    }
}

void CMP_SetMipLevelGammaLinearSB(MipLevel* pCurMipLevel, CMP_SBYTE* pdata, CMP_FLOAT Gamma, CMP_INT numchannels)
{
    for (int y = 0; y < pCurMipLevel->m_nHeight; y++)
    {
        for (int x = 0; x < pCurMipLevel->m_nWidth; x++)
        {
            // calc Gamma for the all color channels
            for (int i = 0; i < 3 && i < numchannels; i++)
            {
                CMP_FLOAT normpixel = 0.0f;
                if (*pdata > 0)
                {
                    normpixel = *pdata;
                    normpixel = normpixel / 127.0f;
                    normpixel = powf(normpixel, Gamma) * 127.0f;

                    // need to check for signed components
                    if (normpixel > 127)
                        normpixel = 127;
                    else if (normpixel < -127)
                        normpixel = -127;

                    *pdata = (CMP_SBYTE)round(normpixel);
                }
                pdata++;
            }
            // if alpha skip it
            if (numchannels > 3)
                pdata++;
        }
    }
}

template <typename T>
void CMP_SetMipLevelGammaf(MipLevel* pCurMipLevel, T* pdata, CMP_FLOAT Gamma, CMP_INT numchannels)
{
    for (int y = 0; y < pCurMipLevel->m_nHeight; y++)
    {
        for (int x = 0; x < pCurMipLevel->m_nWidth; x++)
        {
            // calc Gamma for the all color channels
            for (int i = 0; i < 3 && i < numchannels; i++)
                *pdata++ = pow(*pdata, Gamma);
            // if alpha skip it
            if (numchannels > 3)
                pdata++;
        }
    }
}

static CMP_FLOAT F16toF32(CMP_HALFSHORT f)
{
    CMP_HALF A;
    A.setBits(f);
    return ((CMP_FLOAT)A);
}

void CMP_SetMipLevelGammaHalfShort(MipLevel* pCurMipLevel, CMP_HALFSHORT* pdata, CMP_FLOAT Gamma, CMP_INT numchannels)
{
    for (int y = 0; y < pCurMipLevel->m_nHeight; y++)
    {
        for (int x = 0; x < pCurMipLevel->m_nWidth; x++)
        {
            // calc Gamma for the all color channels
            for (int i = 0; i < 3 && i < numchannels; i++)
            {
                CMP_FLOAT pixf = F16toF32(*pdata);  // convert short int to float
                pixf           = pow(pixf, Gamma);  // Do gamma using float
                CMP_HALF pixh  = pixf;              // back to half type
                *pdata         = pixh.bits();       // back to short int bits
                pdata++;                            // move on to next pixel
            }
            // if alpha skip it
            if (numchannels > 3)
                pdata++;
        }
    }
}

void CMP_SetMipSetGamma(MipSet* pMipSet, CMP_FLOAT Gamma)
{
    CMIPS     CMips;
    MipLevel* pCurMipLevel;
    CMP_INT   maxFaceOrSlice;
    if (pMipSet->m_TextureType & TT_CubeMap)
        maxFaceOrSlice = 6;
    else
        maxFaceOrSlice = 1;
    for (CMP_INT nCurMipLevel = 0; nCurMipLevel < pMipSet->m_nMipLevels; nCurMipLevel++)
    {
        for (CMP_INT nFaceOrSlice = 0; nFaceOrSlice < maxFaceOrSlice; nFaceOrSlice++)
        {
            pCurMipLevel = CMips.GetMipLevel(pMipSet, nCurMipLevel, nFaceOrSlice);
            if (pMipSet->m_ChannelFormat == CF_8bit)
            {
                if (pMipSet->m_format == CMP_FORMAT_RGBA_8888_S)
                    CMP_SetMipLevelGammaLinearSB(pCurMipLevel, pCurMipLevel->m_psbData, Gamma, 4);
                else
                    CMP_SetMipLevelGammaLinearB(pCurMipLevel, pCurMipLevel->m_pbData, Gamma, 4);
            }
            else if (pMipSet->m_ChannelFormat == CF_Float16)
                CMP_SetMipLevelGammaHalfShort(pCurMipLevel, pCurMipLevel->m_phfsData, Gamma, 4);
            else if (pMipSet->m_ChannelFormat == CF_Float32)
                CMP_SetMipLevelGammaf(pCurMipLevel, pCurMipLevel->m_pfData, Gamma, 4);
        }
    }
}

void GenerateMipmapLevel(MipLevel* currMipLevel, MipLevel** prevMipLevels, uint32_t numPrevLevels, CMP_FORMAT format)
{
    static const uint32_t numChannels = 4;

    assert(currMipLevel);
    assert(prevMipLevels);
    assert(numPrevLevels != 0);

    if (!currMipLevel || !prevMipLevels || !prevMipLevels[0] || numPrevLevels == 0)
        return;

    bool heightsDifferent = currMipLevel->m_nHeight != prevMipLevels[0]->m_nHeight;
    bool widthsDifferent  = currMipLevel->m_nWidth != prevMipLevels[0]->m_nWidth;
    assert(heightsDifferent || widthsDifferent);

    CMP_ChannelFormat channelFormat = GetChannelFormat(format);

    const uint32_t bytesPerChannel = GetChannelFormatBitSize(format) / 8;
    uint32_t       bytesPerPixel   = bytesPerChannel * numChannels;

    if (format == CMP_FORMAT_ARGB_2101010 || format == CMP_FORMAT_RGBA_1010102)
        bytesPerPixel = 4;

    CMP_BYTE* destPixel = currMipLevel->m_pbData;

    // A set of 4 pixels per previous level, used for averaging the result value
    std::vector<BoxFilter> levels(numPrevLevels);

    const uint32_t numPixelsPerBlock = sizeof(levels[0].pixels) / sizeof(levels[0].pixels[0]);
    const uint32_t totalNumPixels    = numPrevLevels * numPixelsPerBlock;

    for (uint32_t y = 0; y < currMipLevel->m_nHeight; ++y)
    {
        // update all of the previous layer pixel pointers
        for (uint32_t i = 0; i < numPrevLevels; ++i)
        {
            levels[i].tl = prevMipLevels[i]->m_pbData + 2 * y * prevMipLevels[i]->m_nWidth * bytesPerPixel;

            levels[i].bl = levels[i].tl;
            if (heightsDifferent)
                levels[i].bl = levels[i].tl + prevMipLevels[i]->m_nWidth * bytesPerPixel;

            levels[i].tr = levels[i].tl;
            levels[i].br = levels[i].bl;
            if (widthsDifferent)
            {
                levels[i].tr = levels[i].tl + bytesPerPixel;
                levels[i].br = levels[i].bl + bytesPerPixel;
            }
        }

        for (uint32_t x = 0; x < currMipLevel->m_nWidth; ++x)
        {
            if (format == CMP_FORMAT_RGBA_1010102)
            {
                CMP_DWORD r = 0;
                CMP_DWORD g = 0;
                CMP_DWORD b = 0;
                CMP_DWORD a = 0;

                for (uint32_t levelIndex = 0; levelIndex < numPrevLevels; ++levelIndex)
                {
                    for (uint32_t pixelIndex = 0; pixelIndex < numPixelsPerBlock; ++pixelIndex)
                    {
                        r += RGBA1010102_GET_R(*((CMP_DWORD*)levels[levelIndex].pixels[pixelIndex]));
                        g += RGBA1010102_GET_G(*((CMP_DWORD*)levels[levelIndex].pixels[pixelIndex]));
                        b += RGBA1010102_GET_B(*((CMP_DWORD*)levels[levelIndex].pixels[pixelIndex]));
                        a += RGBA1010102_GET_A(*((CMP_DWORD*)levels[levelIndex].pixels[pixelIndex]));
                    }
                }

                r /= totalNumPixels;
                g /= totalNumPixels;
                b /= totalNumPixels;
                a /= totalNumPixels;

                CMP_DWORD pixelValue = (r << RGBA1010102_OFFSET_R) | (g << RGBA1010102_OFFSET_G) | (b << RGBA1010102_OFFSET_B) | (a << RGBA1010102_OFFSET_A);

                *((CMP_DWORD*)destPixel) = pixelValue;
                destPixel += bytesPerPixel;
            }
            else if (format == CMP_FORMAT_RGBA_8888_S || format == CMP_FORMAT_ARGB_8888_S)
            {
                for (uint32_t i = 0; i < numChannels; ++i)
                {
                    CMP_SBYTE pixelValue = 0;
                    for (uint32_t j = 0; j < numPrevLevels; ++j)
                    {
                        pixelValue += (*((CMP_SBYTE*)levels[j].tl + i) + *((CMP_SBYTE*)levels[j].tr + i) + *((CMP_SBYTE*)levels[j].bl + i) +
                                       *((CMP_SBYTE*)levels[j].br + i)) /
                                      4;
                    }

                    *((CMP_SBYTE*)destPixel) = pixelValue;
                    destPixel += bytesPerChannel;
                }
            }
            else if (channelFormat == CF_8bit)
            {
                for (uint32_t i = 0; i < numChannels; ++i)
                {
                    CMP_BYTE pixelValue = 0;
                    for (uint32_t j = 0; j < numPrevLevels; ++j)
                    {
                        pixelValue += (*(levels[j].tl + i) + *(levels[j].tr + i) + *(levels[j].bl + i) + *(levels[j].br + i)) / 4;
                    }

                    *destPixel = pixelValue;
                    destPixel += bytesPerChannel;
                }
            }
            else if (channelFormat == CF_Float16)
            {
                for (uint32_t i = 0; i < numChannels; ++i)
                {
                    CMP_HALFSHORT pixelValue = 0;
                    for (uint32_t j = 0; j < numPrevLevels; ++j)
                    {
                        pixelValue += (*((CMP_HALFSHORT*)levels[j].tl + i) + *((CMP_HALFSHORT*)levels[j].tr + i) + *((CMP_HALFSHORT*)levels[j].bl + i) +
                                       *((CMP_HALFSHORT*)levels[j].br + i)) /
                                      (CMP_HALFSHORT)4;
                    }

                    *((CMP_HALFSHORT*)destPixel) = pixelValue;
                    destPixel += bytesPerChannel;
                }
            }
            else if (channelFormat == CF_Float32)
            {
                for (uint32_t i = 0; i < numChannels; ++i)
                {
                    CMP_FLOAT pixelValue = 0;
                    for (uint32_t j = 0; j < numPrevLevels; ++j)
                    {
                        pixelValue += (*((CMP_FLOAT*)levels[j].tl + i) + *((CMP_FLOAT*)levels[j].tr + i) + *((CMP_FLOAT*)levels[j].bl + i) +
                                       *((CMP_FLOAT*)levels[j].br + i)) /
                                      4.0f;
                    }

                    *((CMP_FLOAT*)destPixel) = pixelValue;
                    destPixel += bytesPerChannel;
                }
            }
            else
            {
                assert(!"Unsupported format");
            }

            // move all pixel pointers over to the next block in the row
            for (uint32_t i = 0; i < numPrevLevels; ++i)
            {
                levels[i].tl += bytesPerPixel * 2;
                levels[i].tr += bytesPerPixel * 2;
                levels[i].bl += bytesPerPixel * 2;
                levels[i].br += bytesPerPixel * 2;
            }
        }
    }
}

//nMinSize : The size in pixels used to determine how many mip levels to generate. Once all dimensions are less than or equal to nMinSize your mipper should generate no more mip levels.
CMP_INT CMP_API CMP_GenerateMIPLevelsEx(CMP_MipSet* pMipSet, CMP_CFilterParams* CFilterParam)
{
    CMP_CMIPS CMips;
    assert(pMipSet);
    assert(pMipSet->m_nMipLevels);

    CMP_INT        nWidth          = pMipSet->m_nWidth;
    CMP_INT        nHeight         = pMipSet->m_nHeight;
    CMP_HALFSHORT* null_half       = 0;
    CMP_FLOAT*     null_float      = 0;
    CMP_MipLevel*  null_tempMipTwo = nullptr;

    pMipSet->m_nMipLevels = 1;

    while (nWidth > CFilterParam->nMinSize || nHeight > CFilterParam->nMinSize)
    {
        nWidth                   = CMP_MAX(nWidth >> 1, 1);
        nHeight                  = CMP_MAX(nHeight >> 1, 1);
        CMP_INT nCurMipLevel     = pMipSet->m_nMipLevels;
        CMP_INT maxFacesOrSlices = CMP_MAX((pMipSet->m_TextureType == TT_VolumeTexture) ? (CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1) >> 1)
                                                                                        : CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1),
                                           1);

        for (CMP_INT nFaceOrSlice = 0; nFaceOrSlice < maxFacesOrSlices; nFaceOrSlice++)
        {
            CMP_MipLevel* pThisMipLevel = CMips.GetMipLevel(pMipSet, nCurMipLevel, nFaceOrSlice);
            if (!pThisMipLevel)
                continue;
            assert(CMips.GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice)->m_pbData);  //prev miplevel ok

            if (pThisMipLevel->m_pbData)
            {  // Space for mip level already allocated ?
                if (pThisMipLevel->m_nWidth != nWidth || pThisMipLevel->m_nHeight != nHeight)
                {
                    // Wrong size - release & reallocate
                    //CMips->FreeMipLevelData(pThisMipLevel);
                    if (CMips.AllocateMipLevelData(pThisMipLevel, nWidth, nHeight, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType) == NULL)
                    {
                        return CMP_ERR_GENERIC;
                    }
                }
            }
            else if (CMips.AllocateMipLevelData(pThisMipLevel, nWidth, nHeight, pMipSet->m_ChannelFormat, pMipSet->m_TextureDataType) == NULL)
            {
                return CMP_ERR_GENERIC;
            }

            assert(pThisMipLevel->m_pbData);

            if (pMipSet->m_TextureType == TT_VolumeTexture && CMP_MaxFacesOrSlices(pMipSet, nCurMipLevel - 1) > 1)
            {
                //prev miplevel had 2 or more slices, so avg together slices

                MipLevel* prevMipLevels[] = {CMips.GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice * 2),
                                             CMips.GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice * 2 + 1)};

                GenerateMipmapLevel(pThisMipLevel, prevMipLevels, 2, pMipSet->m_format);
            }
            else
            {
                CMP_MipLevel* prevMipLevel = CMips.GetMipLevel(pMipSet, nCurMipLevel - 1, nFaceOrSlice);
                GenerateMipmapLevel(pThisMipLevel, &prevMipLevel, 1, pMipSet->m_format);
            }
        }

        if (pMipSet->m_nMipLevels < MAX_MIPLEVEL_SUPPORTED)
            ++pMipSet->m_nMipLevels;
        else
            break;
        if (nWidth == 1 && nHeight == 1)
            break;
    }

    if (CFilterParam->fGammaCorrection != 1.0f && CFilterParam->fGammaCorrection != 0.0f)
        CMP_SetMipSetGamma(pMipSet, CFilterParam->fGammaCorrection);

    return CMP_OK;
}

CMP_INT CMP_API CMP_GenerateMIPLevels(CMP_MipSet* pMipSet, CMP_INT nMinSize)
{
    CMP_CFilterParams CFilterParam  = {};
    CFilterParam.dwMipFilterOptions = 0;
    CFilterParam.nFilterType        = 0;
    CFilterParam.nMinSize           = nMinSize;
    CFilterParam.fGammaCorrection   = 1.0;
    return CMP_GenerateMIPLevelsEx(pMipSet, &CFilterParam);
}
