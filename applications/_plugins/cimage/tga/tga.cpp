//=====================================================================
// Copyright 2023-2024 (c), Advanced Micro Devices, Inc. All rights reserved.
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
#include <stdlib.h>
#include "tc_pluginapi.h"
#include "tc_plugininternal.h"
#include "compressonator.h"
#include "tga.h"

CMIPS*             TGA_CMips;
TGA_FileSaveParams g_FileSaveParams;

// TGA_CMips outlives every call, so the fallback instance must not be a stack local.
static CMP_CMIPS g_TGA_CMipsFallback;

static bool ReadRemainingFileSize(FILE* pFile, CMP_DWORD& size)
{
    long lCurrPos = ftell(pFile);
    fseek(pFile, 0, SEEK_END);
    long lEndPos = ftell(pFile);
    fseek(pFile, lCurrPos, SEEK_SET);
    if (lCurrPos < 0 || lEndPos < 0 || lEndPos <= lCurrPos)
        return false;

    unsigned long long remaining = static_cast<unsigned long long>(lEndPos - lCurrPos);
    if (remaining > UINT32_MAX)
        return false;

    size = static_cast<CMP_DWORD>(remaining);
    return true;
}

#ifdef BUILD_AS_PLUGIN_DLL
DECLARE_PLUGIN(Plugin_TGA)
SET_PLUGIN_TYPE("IMAGE")
SET_PLUGIN_NAME("TGA")
#else
void* make_Plugin_TGA()
{
    return new Plugin_TGA;
}
#endif

#ifndef _WIN32
typedef CMP_DWORD* LPDWORD;
#endif

#pragma comment(lib, "advapi32.lib")  // for RegCloseKey and other Reg calls ...

Plugin_TGA::Plugin_TGA()
{
}

Plugin_TGA::~Plugin_TGA()
{
}

int Plugin_TGA::TC_PluginSetSharedIO(void* Shared)
{
    if (Shared)
    {
        TGA_CMips = static_cast<CMIPS*>(Shared);
        return 0;
    }
    return 1;
}

int Plugin_TGA::TC_PluginGetVersion(TC_PluginVersion* pPluginVersion)
{
#ifdef _WIN32
    pPluginVersion->guid = g_GUID;
#endif
    pPluginVersion->dwAPIVersionMajor    = TC_API_VERSION_MAJOR;
    pPluginVersion->dwAPIVersionMinor    = TC_API_VERSION_MINOR;
    pPluginVersion->dwPluginVersionMajor = TC_PLUGIN_VERSION_MAJOR;
    pPluginVersion->dwPluginVersionMinor = TC_PLUGIN_VERSION_MINOR;
    return 0;
}

int Plugin_TGA::TC_PluginFileLoadTexture(const char* pszFilename, CMP_Texture* srcTexture)
{
    return 0;
}

int Plugin_TGA::TC_PluginFileSaveTexture(const char* pszFilename, CMP_Texture* srcTexture)
{
    return 0;
}

#define STB_IMAGE_STATIC
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "stb_image.h"

int Plugin_TGA::TC_PluginFileLoadTexture(const char* pszFilename, MipSet* pMipSet)
{
    if (!TGA_CMips)
    {
        TGA_CMips = &g_TGA_CMipsFallback;
    }

    // ATI code
    FILE* pFile = NULL;
    pFile       = fopen(pszFilename, ("rb"));
    if (pFile == NULL)
    {
        TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) opening file = %s "), EL_Error, IDS_ERROR_FILE_OPEN, pszFilename);
        return -1;
    }

    // Read the header
    TGAHeader header;
    if (fread(&header, sizeof(TGAHeader), 1, pFile) != 1)
    {
        TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) invalid TGA header. Filename = %s "), EL_Error, IDS_ERROR_NOT_TGA, pszFilename);
        fclose(pFile);
        return -1;
    }

    // Skip the ID field
    if (header.cIDFieldLength)
        fseek(pFile, header.cIDFieldLength, SEEK_CUR);

    if (!TGA_CMips->AllocateMipSet(pMipSet, CF_8bit, TDT_ARGB, TT_2D, header.nWidth, header.nHeight, 1))
    {  // depthsupport, what should nDepth be set as here?
        fclose(pFile);
        return PE_Unknown;
    }

    if (header.cColorMapType == 0)
    {
        TC_PluginError loadResult = PE_Unknown;
        bool           dispatched = true;

        if (header.cImageType == ImageType_ARGB8888 && header.cColorDepth == 32)
            loadResult = LoadTGA_ARGB8888(pFile, pMipSet, header);
        else if (header.cImageType == ImageType_ARGB8888_RLE && header.cColorDepth == 32)
            loadResult = LoadTGA_ARGB8888_RLE(pFile, pMipSet, header);
        else if (header.cImageType == ImageType_ARGB8888 && header.cColorDepth == 24)
            loadResult = LoadTGA_RGB888(pFile, pMipSet, header);
        else if (header.cImageType == ImageType_ARGB8888_RLE && header.cColorDepth == 24)
            loadResult = LoadTGA_RGB888_RLE(pFile, pMipSet, header);
        else if (header.cImageType == ImageType_G8_RLE && header.cColorDepth == 8)
            loadResult = LoadTGA_G8_RLE(pFile, pMipSet, header);  // Raw greyscale -> Converted to RGBA channels where R=G=B and alpha is 255
        else
            dispatched = false;

        if (dispatched)
        {
            // The loaders allocate the mip level before decoding, so a malformed or truncated
            // stream would otherwise leak the decoded image buffer on every attempt. FreeMipSet
            // is idempotent, so the caller reclaiming it again is harmless.
            if (loadResult != PE_OK)
                TGA_CMips->FreeMipSet(pMipSet);
            return loadResult;
        }

        // use stbi
        {
            fclose(pFile);
            char*   sti_pData;
            int32_t width, height, channels;
            sti_pData = (char*)stbi_load(pszFilename, &width, &height, &channels, STBI_rgb_alpha);
            if (sti_pData)
            {
                // Reject dimensions where width * height * sizeof(CMP_COLOR) would overflow and undersize the
                // buffer, or exceed INT32_MAX and risk signed overflow in the later pixel-offset arithmetic.
                if (width <= 0 || height <= 0 ||
                    static_cast<CMP_DWORD>(height) > (static_cast<CMP_DWORD>(INT32_MAX) / sizeof(CMP_COLOR)) / static_cast<CMP_DWORD>(width))
                {
                    stbi_image_free(sti_pData);
                    TGA_CMips->FreeMipSet(pMipSet);
                    return PE_Unknown;
                }
                // The mip level was already allocated from the header dimensions above.
                // AllocateMipLevelData overwrites m_pbData without freeing it, so release the
                // header-sized buffer first.
                MipLevel* pMipLevel = TGA_CMips->GetMipLevel(pMipSet, 0);
                if (pMipLevel)
                    TGA_CMips->FreeMipLevelData(pMipLevel);

                if (!pMipLevel || !TGA_CMips->AllocateMipLevelData(pMipLevel, width, height, CF_8bit, TDT_ARGB))
                {
                    stbi_image_free(sti_pData);
                    TGA_CMips->FreeMipSet(pMipSet);
                    return PE_Unknown;
                }

                // The header dimensions can disagree with what stb actually decoded. Both the mip level
                // and sti_pData are sized from the decoded values, so the MipSet must describe those too,
                // otherwise the copy below is sized against the wrong buffer.
                pMipSet->m_nWidth  = width;
                pMipSet->m_nHeight = height;

                CMP_DWORD dwSize           = static_cast<CMP_DWORD>(width) * static_cast<CMP_DWORD>(height) * static_cast<CMP_DWORD>(sizeof(CMP_COLOR));
                pMipSet->m_ChannelFormat   = CF_8bit;
                pMipSet->m_TextureDataType = TDT_ARGB;
                pMipSet->m_dwFourCC        = 0;
                pMipSet->m_dwFourCC2       = 0;
                pMipSet->m_format          = CMP_FORMAT_ARGB_8888;
                pMipSet->m_nMipLevels      = 1;
                CMP_BYTE* cmp_pData        = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData);
                memcpy(cmp_pData, sti_pData, dwSize);
                stbi_image_free(sti_pData);
                return PE_OK;
            }
            TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) unsupported type Filename = %s "), EL_Error, IDS_ERROR_UNSUPPORTED_TYPE, pszFilename);
            TGA_CMips->FreeMipSet(pMipSet);
            return -1;
        }
    }

    TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) unsupported colour map type. Filename = %s "), EL_Error, IDS_ERROR_UNSUPPORTED_TYPE, pszFilename);
    fclose(pFile);
    TGA_CMips->FreeMipSet(pMipSet);

    return -1;
}

int Plugin_TGA::TC_PluginFileSaveTexture(const char* pszFilename, MipSet* pMipSet)
{
    if (!TGA_CMips)
    {
        TGA_CMips = &g_TGA_CMipsFallback;
    }
    assert(pszFilename);
    assert(pMipSet);

    FILE* pFile = NULL;
    pFile       = fopen(pszFilename, ("wb"));
    if (pFile == NULL)
    {
        TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) saving file = %s "), EL_Error, IDS_ERROR_FILE_OPEN, pszFilename);
        return -1;
    }

    TGAHeader header;
    memset(&header, 0, sizeof(header));
    switch (pMipSet->m_dwFourCC)
    {
    case CMP_MAKEFOURCC('G', '8', ' ', ' '):
        header.cImageType  = ImageType_G8_RLE;
        header.cColorDepth = 8;
        break;
    case 0:  // Not a FourCC texture
    default:
        if (pMipSet->m_TextureDataType == TDT_ARGB)
        {
            header.cColorDepth  = 32;
            header.cFormatFlags = 0x8;
        }
        else
            header.cColorDepth = 24;
        header.cImageType = ImageType_ARGB8888;
        break;
    }

    // napatel need to add this somehow!
    //if(pTGAFileSaveParams->bRLECompressed)
    //    header.cImageType |= 0x8;

    header.nWidth  = static_cast<short>(pMipSet->m_nWidth);
    header.nHeight = static_cast<short>(pMipSet->m_nHeight);

    fwrite(&header, sizeof(header), 1, pFile);

    if (header.cImageType == ImageType_G8)
        return SaveTGA_G8(pFile, pMipSet);
    else if (header.cImageType == ImageType_G8_RLE)
        return SaveTGA_G8_RLE(pFile, pMipSet);
    else if (header.cImageType == ImageType_ARGB8888 && pMipSet->m_TextureDataType == TDT_ARGB)
        return SaveTGA_ARGB8888(pFile, pMipSet);
    else if (header.cImageType == ImageType_ARGB8888_RLE && pMipSet->m_TextureDataType == TDT_ARGB)
        return SaveTGA_ARGB8888_RLE(pFile, pMipSet);
    else if (header.cImageType == ImageType_ARGB8888 && pMipSet->m_TextureDataType == TDT_XRGB)
        return SaveTGA_RGB888(pFile, pMipSet);
    else if (header.cImageType == ImageType_ARGB8888_RLE && pMipSet->m_TextureDataType == TDT_XRGB)
        return SaveTGA_RGB888_RLE(pFile, pMipSet);

    TGA_CMips->PrintError(("Error(%d): TGA Plugin ID(%d) unsupported type Filename = %s "), EL_Error, IDS_ERROR_UNSUPPORTED_TYPE, pszFilename);
    fclose(pFile);

    return -1;
}

//---------------- TGA Code -----------------------------------

TC_PluginError LoadTGA_ARGB8888(FILE* pFile, MipSet* pMipSet, TGAHeader& Header)
{
    if (!TGA_CMips->AllocateMipLevelData(TGA_CMips->GetMipLevel(pMipSet, 0), Header.nWidth, Header.nHeight, CF_8bit, TDT_ARGB))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;
    pMipSet->m_nMipLevels      = 1;

    // Reject dimensions where width * height * sizeof(CMP_COLOR) would overflow and undersize the buffer,
    // or exceed INT32_MAX and risk signed overflow in the later pixel-offset arithmetic.
    if (pMipSet->m_nWidth <= 0 || pMipSet->m_nHeight <= 0 ||
        static_cast<CMP_DWORD>(pMipSet->m_nHeight) > (static_cast<CMP_DWORD>(INT32_MAX) / sizeof(CMP_COLOR)) / static_cast<CMP_DWORD>(pMipSet->m_nWidth))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD      dwSize = static_cast<CMP_DWORD>(pMipSet->m_nWidth) * static_cast<CMP_DWORD>(pMipSet->m_nHeight) * static_cast<CMP_DWORD>(sizeof(CMP_COLOR));
    unsigned char* pTempData = static_cast<unsigned char*>(malloc(dwSize));
    if (!pTempData)
    {
        fclose(pFile);
        return PE_Unknown;
    }

    // A truncated file would otherwise leave part of pTempData uninitialized
    // and that heap content would be decoded straight into the returned image.
    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        fclose(pFile);
        return PE_Unknown;
    }
    fclose(pFile);

    CMP_BYTE* pTempPtr = pTempData;

    int nStart, nEnd, nIncrement;
    // Bottom up ?
    if (Header.cFormatFlags & 0x20)
    {
        nStart     = 0;
        nEnd       = pMipSet->m_nHeight;
        nIncrement = 1;
    }
    else
    {
        nStart     = pMipSet->m_nHeight - 1;
        nEnd       = -1;
        nIncrement = -1;
    }

    CMP_BYTE nBlue;
    CMP_BYTE nGreen;
    CMP_BYTE nRed;
    CMP_BYTE nAlpha;

    for (int j = nStart; j != nEnd; j += nIncrement)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * pMipSet->m_nWidth * sizeof(CMP_COLOR)));
        for (int i = 0; i < pMipSet->m_nWidth; i++)
        {
            // Note MIPSet is  RGBA
            // TGA is saved as BGRA
            nBlue  = *pTempPtr++;
            nGreen = *pTempPtr++;
            nRed   = *pTempPtr++;
            nAlpha = *pTempPtr++;

            // printf("{%d,%d,%d,%d}\n", nRed, nGreen, nBlue, nAlpha);

            *pData++ = nRed;
            *pData++ = nGreen;
            *pData++ = nBlue;
            *pData++ = nAlpha;
        }
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoadTGA_ARGB8888_RLE(FILE* pFile, MipSet* pMipSet, TGAHeader& Header)
{
    if (!TGA_CMips->AllocateMipLevelData(TGA_CMips->GetMipLevel(pMipSet, 0), Header.nWidth, Header.nHeight, CF_8bit, TDT_ARGB))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;
    pMipSet->m_nMipLevels      = 1;

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD dwTempSize = 0;
    if (!ReadRemainingFileSize(pFile, dwTempSize))
    {
        fclose(pFile);
        return PE_Unknown;
    }
    unsigned char* pTempData = static_cast<unsigned char*>(malloc(dwTempSize));
    if (!pTempData)
    {
        fclose(pFile);
        return PE_Unknown;
    }
    if (fread(pTempData, dwTempSize, 1, pFile) != 1)
    {
        free(pTempData);
        fclose(pFile);
        return PE_Unknown;
    }
    fclose(pFile);

    CMP_DWORD       dwPitch  = pMipSet->m_nWidth * sizeof(CMP_COLOR);
    CMP_BYTE*       pTempPtr = pTempData;
    const CMP_BYTE* pTempEnd = pTempData + dwTempSize;

    int nStart, nEnd, nIncrement;
    // Bottom up ?
    if (Header.cFormatFlags & 0x20)
    {
        nStart     = 0;
        nEnd       = pMipSet->m_nHeight;
        nIncrement = 1;
    }
    else
    {
        nStart     = pMipSet->m_nHeight - 1;
        nEnd       = -1;
        nIncrement = -1;
    }

    // A TGA RLE packet may legally span a scanline boundary, so its type, remaining count, and
    // (for a run-length packet) repeated pixel value must persist across rows instead of being
    // re-derived from a freshly read header at the start of every row.
    unsigned char nRepeat = 0;
    unsigned char nLength = 0;
    CMP_BYTE      nBlue = 0, nGreen = 0, nRed = 0, nAlpha = 0;

    for (int j = nStart; j != nEnd; j += nIncrement)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitch));

        int nColumn = 0;
        while (nColumn < pMipSet->m_nWidth)
        {
            if (nRepeat == 0)
            {
                if (pTempPtr >= pTempEnd)
                {
                    free(pTempData);
                    return PE_Unknown;
                }
                nLength = *pTempPtr++;
                nRepeat = (nLength & 0x7f) + 1;

                if (nLength & 0x80)
                {
                    // Run-length packet: the single repeated pixel value is read once per packet,
                    // not once per row it happens to cross.
                    if (pTempEnd - pTempPtr < 4)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    nBlue  = *pTempPtr++;
                    nGreen = *pTempPtr++;
                    nRed   = *pTempPtr++;
                    nAlpha = *pTempPtr++;
                }
            }

            if (nLength & 0x80)
            {
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    *pData++ = nRed;
                    *pData++ = nGreen;
                    *pData++ = nBlue;
                    *pData++ = nAlpha;
                    nColumn++;
                    nRepeat--;
                }
            }
            else
            {
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    if (pTempEnd - pTempPtr < 4)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    nBlue    = *pTempPtr++;
                    nGreen   = *pTempPtr++;
                    nRed     = *pTempPtr++;
                    nAlpha   = *pTempPtr++;
                    *pData++ = nRed;
                    *pData++ = nGreen;
                    *pData++ = nBlue;
                    *pData++ = nAlpha;
                    nColumn++;
                    nRepeat--;
                }
            }
        }
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoadTGA_RGB888(FILE* pFile, MipSet* pMipSet, TGAHeader& Header)
{
    if (!TGA_CMips->AllocateMipLevelData(TGA_CMips->GetMipLevel(pMipSet, 0), Header.nWidth, Header.nHeight, CF_8bit, TDT_ARGB))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_nMipLevels      = 1;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;

    // Reject dimensions where width * height * 3 would overflow and undersize the buffer, or exceed
    // INT32_MAX and risk signed overflow in the later pixel-offset arithmetic.
    if (pMipSet->m_nWidth <= 0 || pMipSet->m_nHeight <= 0 ||
        static_cast<CMP_DWORD>(pMipSet->m_nHeight) > (static_cast<CMP_DWORD>(INT32_MAX) / 3u) / static_cast<CMP_DWORD>(pMipSet->m_nWidth))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD      dwSize    = static_cast<CMP_DWORD>(pMipSet->m_nWidth) * static_cast<CMP_DWORD>(pMipSet->m_nHeight) * 3u;
    unsigned char* pTempData = static_cast<unsigned char*>(malloc(dwSize));
    if (!pTempData)
    {
        fclose(pFile);
        return PE_Unknown;
    }

    // A truncated file would otherwise leave part of pTempData uninitialized
    // and that heap content would be decoded straight into the returned image.
    if (fread(pTempData, dwSize, 1, pFile) != 1)
    {
        free(pTempData);
        fclose(pFile);
        return PE_Unknown;
    }
    fclose(pFile);

    CMP_BYTE* pTempPtr = pTempData;

    int nStart, nEnd, nIncrement;
    // Bottom up ?
    if (Header.cFormatFlags & 0x20)
    {
        nStart     = 0;
        nEnd       = pMipSet->m_nHeight;
        nIncrement = 1;
    }
    else
    {
        nStart     = pMipSet->m_nHeight - 1;
        nEnd       = -1;
        nIncrement = -1;
    }

    CMP_BYTE nBlue;
    CMP_BYTE nGreen;
    CMP_BYTE nRed;

    for (int j = nStart; j != nEnd; j += nIncrement)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * pMipSet->m_nWidth * sizeof(CMP_COLOR)));
        for (int i = 0; i < pMipSet->m_nWidth; i++)
        {
            nBlue  = *pTempPtr++;
            nGreen = *pTempPtr++;
            nRed   = *pTempPtr++;

            *pData++ = nRed;
            *pData++ = nGreen;
            *pData++ = nBlue;
            *pData++ = 0xff;
        }
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoadTGA_RGB888_RLE(FILE* pFile, MipSet* pMipSet, TGAHeader& Header)
{
    if (!TGA_CMips->AllocateMipLevelData(TGA_CMips->GetMipLevel(pMipSet, 0), Header.nWidth, Header.nHeight, CF_8bit, TDT_ARGB))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_nMipLevels      = 1;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD dwTempSize = 0;
    if (!ReadRemainingFileSize(pFile, dwTempSize))
    {
        fclose(pFile);
        return PE_Unknown;
    }
    unsigned char* pTempData = static_cast<unsigned char*>(malloc(dwTempSize));
    if (!pTempData)
    {
        fclose(pFile);
        return PE_Unknown;
    }
    if (fread(pTempData, dwTempSize, 1, pFile) != 1)
    {
        free(pTempData);
        fclose(pFile);
        return PE_Unknown;
    }
    fclose(pFile);

    CMP_BYTE*       pTempPtr   = pTempData;
    const CMP_BYTE* pTempEnd   = pTempData + dwTempSize;
    CMP_DWORD       dwPitchOut = pMipSet->m_nWidth * sizeof(CMP_COLOR);

    int nStart, nEnd, nIncrement;
    // Bottom up ?
    if (Header.cFormatFlags & 0x20)
    {
        nStart     = 0;
        nEnd       = pMipSet->m_nHeight;
        nIncrement = 1;
    }
    else
    {
        nStart     = pMipSet->m_nHeight - 1;
        nEnd       = -1;
        nIncrement = -1;
    }

    unsigned char nRepeat = 0;
    unsigned char nLength = 0;
    CMP_BYTE      nBlue = 0, nGreen = 0, nRed = 0;
    for (int j = nStart; j != nEnd; j += nIncrement)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitchOut));

        int nColumn = 0;
        while (nColumn < pMipSet->m_nWidth)
        {
            if (nRepeat == 0)
            {
                if (pTempPtr >= pTempEnd)
                {
                    free(pTempData);
                    return PE_Unknown;
                }
                nLength = *pTempPtr++;
                nRepeat = (nLength & 0x7f) + 1;

                if (nLength & 0x80)
                {
                    // Run-length packet: the single repeated pixel value is read once per packet,
                    // not once per row it happens to cross.
                    if (pTempEnd - pTempPtr < 3)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    nBlue  = *pTempPtr++;
                    nGreen = *pTempPtr++;
                    nRed   = *pTempPtr++;
                }
            }

            if (nLength & 0x80)
            {
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    *pData++ = nRed;
                    *pData++ = nGreen;
                    *pData++ = nBlue;
                    *pData++ = 0xff;
                    nColumn++;
                    nRepeat--;
                }
            }
            else
            {
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    if (pTempEnd - pTempPtr < 3)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    CMP_BYTE rawBlue  = *pTempPtr++;
                    CMP_BYTE rawGreen = *pTempPtr++;
                    CMP_BYTE rawRed   = *pTempPtr++;
                    *pData++          = rawRed;
                    *pData++          = rawGreen;
                    *pData++          = rawBlue;
                    *pData++          = 0xff;
                    nColumn++;
                    nRepeat--;
                }
            }
        }
    }

    free(pTempData);

    return PE_OK;
}

TC_PluginError LoadTGA_G8_RLE(FILE* pFile, MipSet* pMipSet, TGAHeader& Header)
{
    if (!TGA_CMips->AllocateMipLevelData(TGA_CMips->GetMipLevel(pMipSet, 0), Header.nWidth, Header.nHeight, CF_8bit, TDT_ARGB))
    {
        fclose(pFile);
        return PE_Unknown;
    }

    pMipSet->m_ChannelFormat   = CF_8bit;
    pMipSet->m_TextureDataType = TDT_ARGB;
    pMipSet->m_dwFourCC        = 0;
    pMipSet->m_dwFourCC2       = 0;
    pMipSet->m_nMipLevels      = 1;
    pMipSet->m_format          = CMP_FORMAT_ARGB_8888;

    // Allocate a temporary buffer and read the bitmap data into it
    CMP_DWORD dwTempSize = 0;
    if (!ReadRemainingFileSize(pFile, dwTempSize))
    {
        fclose(pFile);
        return PE_Unknown;
    }
    unsigned char* pTempData = static_cast<unsigned char*>(malloc(dwTempSize));
    if (!pTempData)
    {
        fclose(pFile);
        return PE_Unknown;
    }
    if (fread(pTempData, dwTempSize, 1, pFile) != 1)
    {
        free(pTempData);
        fclose(pFile);
        return PE_Unknown;
    }
    fclose(pFile);

    CMP_BYTE*       pTempPtr = pTempData;
    const CMP_BYTE* pTempEnd = pTempData + dwTempSize;

    int nStart, nEnd, nIncrement;
    // Bottom up ?
    if (Header.cFormatFlags & 0x20)
    {
        nStart     = 0;
        nEnd       = pMipSet->m_nHeight;
        nIncrement = 1;
    }
    else
    {
        nStart     = pMipSet->m_nHeight - 1;
        nEnd       = -1;
        nIncrement = -1;
    }

    // A TGA RLE packet may legally span a scanline boundary, so its type, remaining count, and
    // (for a run-length packet) repeated grey value must persist across rows instead of being
    // re-derived from a freshly read header at the start of every row.
    unsigned char nRepeat = 0;
    unsigned char nLength = 0;
    CMP_BYTE      gray    = 0;

    for (int j = nStart; j != nEnd; j += nIncrement)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * pMipSet->m_nWidth * sizeof(CMP_COLOR)));

        int nColumn = 0;
        while (nColumn < pMipSet->m_nWidth)
        {
            if (nRepeat == 0)
            {
                if (pTempPtr >= pTempEnd)
                {
                    free(pTempData);
                    return PE_Unknown;
                }
                nLength = *pTempPtr++;
                nRepeat = (nLength & 0x7f) + 1;

                if (nLength & 0x80)
                {
                    // Run-length packet: the single repeated grey value is read once per packet,
                    // not once per row it happens to cross.
                    if (pTempPtr >= pTempEnd)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    gray = *pTempPtr++;
                }
            }

            if (nLength & 0x80)
            {
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    *pData++ = gray;
                    *pData++ = gray;
                    *pData++ = gray;
                    *pData++ = 0xff;
                    nColumn++;
                    nRepeat--;
                }
            }
            else
            {
                // Raw packet: each sample carries its own grey byte. Before v4.6 this branch
                // replicated the packet's first grey byte across every sample, so greyscale RLE
                // TGAs containing raw packets decoded to incorrect pixel values. Decoded output
                // for such files therefore changes with this fix (see
                // cmp_unittests/tga_rle_security_tests.cpp, Group 5).
                while (nRepeat && nColumn < pMipSet->m_nWidth)
                {
                    if (pTempPtr >= pTempEnd)
                    {
                        free(pTempData);
                        return PE_Unknown;
                    }
                    CMP_BYTE rawGray = *pTempPtr++;
                    *pData++         = rawGray;
                    *pData++         = rawGray;
                    *pData++         = rawGray;
                    *pData++         = 0xff;
                    nColumn++;
                    nRepeat--;
                }
            }
        }
    }

    free(pTempData);

    return PE_OK;
}

CMP_BYTE CalcRunLength(CMP_BYTE* pThis, CMP_BYTE* pEnd, int nSize, int nOffset)
{
    CMP_BYTE* pNext      = pThis + nOffset;
    CMP_BYTE  cRunLength = 1;

    while (pNext < pEnd && memcmp(pThis, pNext, nSize) == 0 && cRunLength < 0x80)
    {
        cRunLength++;
        pNext += nOffset;
    }

    return cRunLength;
}

CMP_BYTE CalcRawLength(CMP_BYTE* pThis, CMP_BYTE* pEnd, int nSize, int nOffset)
{
    CMP_BYTE* pNext      = pThis + nOffset;
    CMP_BYTE  cRawLength = 1;

    while (pNext < pEnd && memcmp(pThis, pNext, nSize) != 0 && cRawLength < 0x80)
    {
        cRawLength++;
        pThis += nOffset;
        pNext += nOffset;
    }

    return cRawLength;
}

// *** NOTE ****
// The Save data code needs to be rewored as
// TGA is saved as BGRA and our MipSet is using RGBA
//
// Current implementation is saving in wronge format as RGBA
//
// Need to Fix this

void SaveLineRLE(FILE* pFile, CMP_BYTE* pThis, CMP_BYTE* pEnd, int nSize, int nOffset)
{
    while (pThis < pEnd)
    {
        CMP_BYTE* pNext = pThis + nOffset;
        // Are the next two pixel the same ?
        if (pNext < pEnd && memcmp(pThis, pNext, nSize) == 0)
        {
            CMP_BYTE cRunLength       = CalcRunLength(pThis, pEnd, nSize, nOffset);
            CMP_BYTE cRepetitionCount = (cRunLength - 1) | 0x80;
            fwrite(&cRepetitionCount, sizeof(cRepetitionCount), 1, pFile);
            fwrite(pThis, nSize, 1, pFile);
            pThis += (cRunLength * nOffset);
        }
        else
        {
            CMP_BYTE cRawLength       = CalcRawLength(pThis, pEnd, nSize, nOffset);
            CMP_BYTE cRepetitionCount = (cRawLength - 1);
            fwrite(&cRepetitionCount, sizeof(cRepetitionCount), 1, pFile);
            if (nSize == nOffset)
            {
                fwrite(pThis, nSize * cRawLength, 1, pFile);
                pThis += (cRawLength * nOffset);
            }
            else
            {
                for (CMP_BYTE i = 0; i < cRawLength; i++)
                {
                    fwrite(pThis, nSize, 1, pFile);
                    pThis += nOffset;
                }
            }
        }
    }
}

TC_PluginError SaveRLE(FILE* pFile, const MipSet* pMipSet, int nSize, int nOffset)
{
    CMP_DWORD dwPitch = pMipSet->m_nWidth * nOffset;
    for (int j = pMipSet->m_nHeight - 1; j >= 0; j--)
    {
        CMP_BYTE* pThis = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitch));
        CMP_BYTE* pEnd  = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + ((j + 1) * dwPitch));

        SaveLineRLE(pFile, pThis, pEnd, nSize, nOffset);
    }

    fclose(pFile);

    return PE_OK;
}

//--------------------------------------------------------
// Source mipset should now always be RGBA
// Code using the pMipSet->m_swizzle should be reviewed
//--------------------------------------------------------

TC_PluginError SaveTGA_ARGB8888(FILE* pFile, const MipSet* pMipSet)
{
    CMP_DWORD dwPitch = pMipSet->m_nWidth * 4;

    //if (pMipSet->m_swizzle)
    {
        CMP_BYTE RGBA[4];
        for (int j = pMipSet->m_nHeight - 1; j >= 0; j--)
        {
            CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitch));
            for (int i = 0; i < pMipSet->m_nWidth; i++)
            {
                RGBA[2] = (CMP_BYTE)*pData++;
                RGBA[1] = (CMP_BYTE)*pData++;
                RGBA[0] = (CMP_BYTE)*pData++;
                RGBA[3] = (CMP_BYTE)*pData++;
                fwrite(RGBA, 4, 1, pFile);
            }
        }
    }
    //else
    //{
    //    for (int j = pMipSet->m_nHeight - 1; j >= 0; j--)
    //    {
    //        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitch));
    //        fwrite(pData, dwPitch, 1, pFile);
    //    }
    //}

    fclose(pFile);

    return PE_OK;
}

TC_PluginError SaveTGA_ARGB8888_RLE(FILE* pFile, const MipSet* pMipSet)
{
    return SaveRLE(pFile, pMipSet, sizeof(CMP_COLOR), sizeof(CMP_COLOR));
}

TC_PluginError SaveTGA_RGB888(FILE* pFile, const MipSet* pMipSet)
{
    CMP_DWORD dwPitch = pMipSet->m_nWidth * sizeof(CMP_COLOR);
    for (int j = pMipSet->m_nHeight - 1; j >= 0; j--)
    //    for(int j=0; j < pMipSet->m_nHeight-1; j++)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * dwPitch));
        for (int i = 0; i < pMipSet->m_nWidth; i++)
        {
            fwrite(pData, 3, 1, pFile);
            pData += sizeof(CMP_COLOR);
        }
    }

    fclose(pFile);

    return PE_OK;
}

TC_PluginError SaveTGA_RGB888_RLE(FILE* pFile, const MipSet* pMipSet)
{
    return SaveRLE(pFile, pMipSet, 3, sizeof(CMP_COLOR));
}

TC_PluginError SaveTGA_G8(FILE* pFile, const MipSet* pMipSet)
{
    for (int j = pMipSet->m_nHeight - 1; j >= 0; j--)
    {
        CMP_BYTE* pData = (CMP_BYTE*)(TGA_CMips->GetMipLevel(pMipSet, 0)->m_pbData + (j * pMipSet->m_nWidth));
        fwrite(pData, pMipSet->m_nWidth, 1, pFile);
    }

    fclose(pFile);

    return PE_OK;
}

TC_PluginError SaveTGA_G8_RLE(FILE* pFile, const MipSet* pMipSet)
{
    return SaveRLE(pFile, pMipSet, sizeof(CMP_BYTE), sizeof(CMP_BYTE));
}

// ------------ Registry!
#define DEFAULT_JPG_QUALITY 75
const char szKeyTGAPlugin[] = ("Software\\ATI Research, Inc.\\TextureAPI\\Plugins\\TGA");

void LoadRegistryKeyDefaults(TGA_FileSaveParams* pParams)
{
    memset(pParams, 0, sizeof(TGA_FileSaveParams));
    pParams->dwSize = sizeof(TGA_FileSaveParams);

    pParams->bRLECompressed = true;
}

void LoadRegistryKeys(TGA_FileSaveParams* pParams)
{
#ifdef _WIN32
    LoadRegistryKeyDefaults(pParams);

    HKEY      hKey;
    CMP_DWORD dwSize;
    RegOpenKeyEx(HKEY_CURRENT_USER, szKeyTGAPlugin, 0, KEY_ALL_ACCESS, &hKey);

    CMP_DWORD dwTemp;
    if (RegQueryValueEx(hKey, ("RLECompressed"), NULL, NULL, (CMP_BYTE*)&dwTemp, (LPDWORD)&dwSize) != ERROR_SUCCESS)
        pParams->bRLECompressed = dwTemp ? true : false;

    RegCloseKey(hKey);
#endif
}

void SaveRegistryKeys(const TGA_FileSaveParams* pParams)
{
#ifdef _WIN32
    HKEY hKey;
    RegCreateKeyEx(HKEY_CURRENT_USER, szKeyTGAPlugin, 0, (""), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);

    CMP_DWORD dwTemp;
    dwTemp = pParams->bRLECompressed ? 1 : 0;
    RegSetValueEx(hKey, ("RLECompressed"), NULL, REG_DWORD, (CMP_BYTE*)&dwTemp, sizeof(CMP_DWORD));

    RegCloseKey(hKey);
#endif
}
