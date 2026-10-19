//=====================================================================
// Copyright 2016-2024 (c), Advanced Micro Devices, Inc. All rights reserved
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
//=====================================================================

#ifdef _DEBUG
//#include <vld.h>   Enable to check for code leaks
#endif

#ifdef _WIN32
#include <windows.h>
#include <libloaderapi.h>
#include "dll_security_check.h"
#endif

#if (OPTION_CMP_QT == 1)
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#endif

#include "cmdline.h"
#include "cmp_fileio.h"
#include "cmp_plugininterface.h"
#include "plugininterface.h"
#include "pluginmanager.h"
#include "textureio.h"
#include "time.h"
#include "version.h"

// Standard App Static Plugin Interfaces for minimal support
#if (OPTION_BUILD_ASTC == 1)
#pragma comment(lib, "Image_ASTC.lib")
#endif

#if (OPTION_BUILD_EXR == 1)
#pragma comment(lib, "Image_EXR.lib")
#endif

#if (OPTION_BUILD_KTX2 == 1)
#pragma comment(lib, "Image_KTX.lib")
#ifdef _WIN32
#pragma comment(lib, "Image_KTX2.lib")
#endif
#endif

#pragma comment(lib, "Image_TGA.lib")
#pragma comment(lib, "Image_Analysis.lib")

#ifdef USE_LOSSLESS_COMPRESSION
#pragma comment(lib, "Image_BRLG.lib")
#endif

#ifdef USE_LOSSLESS_COMPRESSION_BINARY
#pragma comment(lib, "Image_BINARY.lib")
#endif

#if (OPTION_BUILD_ASTC == 1)
extern void* make_Plugin_ASTC();
#endif

extern void* make_Plugin_EXR();
extern void* make_Plugin_TGA();
extern void* make_Plugin_KTX();
extern void* make_Plugin_CAnalysis();

#ifdef USE_LOSSLESS_COMPRESSION
extern void* make_Image_Plugin_BRLG();
#endif

#ifdef USE_LOSSLESS_COMPRESSION_BINARY
extern void* make_Image_Plugin_BINARY();
#endif

#ifdef _WIN32
extern void* make_Plugin_KTX2();
#endif

// Setup Static Host Pluging Libs
extern void CMP_RegisterHostPlugins();

extern bool CompressionCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2);
extern void LocalPrintF(char* buff);

extern PluginManager g_pluginManager;
bool                 g_bAbortCompression = false;
CMIPS*               g_CMIPS;  // Global MIPS functions shared between app and all IMAGE plugins

void AboutCompressonator()
{
    char year[5] = "2026";

#ifdef USE_AUTODATE
    time_t now     = time(0);
    auto   tm      = localtime(&now);
    int    tm_year = 0;
    if (tm != nullptr)
        sprintf_s(year, "%d", tm->tm_year + 1900);
#endif

    printf("------------------------------------------------\n");
    // current build release
    if (VERSION_MINOR_MAJOR)
        printf("compressonatorcli V%d.%d.%d Copyright AMD %s\n", VERSION_MAJOR_MAJOR, VERSION_MAJOR_MINOR, VERSION_MINOR_MAJOR, year);
    else
    {
        // Keep track of Customer patches from last release to current
        // This is what is shown when you build the exe outside of the automated Build System (such as Jenkins)
        printf("compressonatorcli V%d.%d.0 Copyright AMD %s\n", VERSION_MAJOR_MAJOR, VERSION_MAJOR_MINOR, year);
    }
    printf("------------------------------------------------\n");
    printf("\n");
}

void PrintUsage()
{
    AboutCompressonator();

    printf("Usage: compressonatorcli [options] SourceFile DestFile\n\n");
    printf("MipMap options:\n\n");
    printf("-nomipmap                 Turns off Mipmap generation\n");
    printf("-mipsize    <size>        The size in pixels used to determine\n");
    printf("                          how many mip levels to generate\n");
    printf("-miplevels  <Level>       Sets Mips Level for output, range is 1 to 20\n");
    printf("                          (mipSize overides this option): default is 1\n");
    printf("-FilterGamma <value>      A gamma correction level to apply after mipmap generation, in the range 1.0 to 2.6\n");
#ifdef _WIN32
    printf("-GenGPUMipMaps            When encoding with GPU this flag will enable mipmap level generation\n");
    printf("                          using GPU HW. Default level is 1 unless miplevels is set\n");
    printf("-UseSRGBFrames            When encoding with GPU, GL_FRAMEBUFFER_SRGB will be enabled else it will use GL_FRAMEBUFFER\n");
#endif
    printf("\nCompression options:\n\n");

#ifdef CMP_ENABLE_LEGACY_OPTIONS
    printf("-fs <format>    Optionally specifies the source file format to use\n");
#endif

    printf("-fd <format>          Specifies the destination texture format to use\n");
    printf("-decomp <filename>    If the destination file is compressed optionally\n");
    printf("                      decompress it\n");
    printf("                      to the specified file. Note the destination  must\n");
    printf("                      be compatible \n");
    printf("                      with the sources format,decompress formats are typically\n");
    printf("                      set to ARGB_8888 or ARGB_32F\n");

#ifdef _WIN32
    printf("-EncodeWith           Compression with CPU, HPC, GPU, OCL or DXC\n");
    printf("                      GPU will use GL Compress Extensions, OCL or DXC will use CMP_Core SDK Shaders\n");
    printf("-DecodeWith           GPU based decompression using OpenGL, DirectX or Vulkan\n");
#else
    printf("-EncodeWith          Compression with CPU, HPC\n");
#endif
#ifdef USE_MESH_DRACO_EXTENSION
    printf("-draco               Enable draco compression. (only support glTF files)\n");
#ifdef USE_MESH_DRACO_SETTING
    printf("-dracolvl            Draco compression level (0-10), default=5 , -draco has to be enabled.\n");
    printf("-qpos                Draco quantization bits for position attribute (0-30), default=14. -draco has to be enabled.\n");
    printf("-qtexc               Draco quantization bits for texture coordinates attribute (0-30), default=10. -draco has to be enabled.\n");
    printf("-qnorm               Draco quantization bits for normal attribute (0-30), default=10. -draco has to be enabled.\n");
    printf("-qgen                Draco quantization bits for generic attribute (0-30), default=8. -draco has to be enabled.\n");
#endif
#endif
    printf("-UseMangledFileNames Enable file mangling for destination files by appending the source extension and codec type to the file name.\n");
    printf("-doswizzle           Swizzle the source images Red and Blue channels\n");
    printf("-PackageBRLG         Packages all files in a directory and its subdirectories into a single BRLG file output\n");
    printf("\n");
    printf("The following is a list of channel formats\n");
    printf("ARGB_8888      format with 8-bit fixed channels\n");
    printf("ARGB_16F       format with 16-bit floating-point channels\n");
    printf("ARGB_32F       format with 32-bit floating-point channels\n");
    printf("RGBA_1010102   format with 10-bit fixed channels for color\n");
#ifdef CMP_ENABLE_TRANSCODECHANNEL_SUPPORT
    printf("ARGB_16        format with 16-bit fixed channels\n");
    printf("ARGB_2101010   format with 10-bit fixed channels for color\n");
    printf("               and a 2-bit fixed channel for alpha\n");
    printf("R_8            Single component format with 8-bit fixed channels\n");
    printf("R_16           Single component format with 16-bit fixed channels\n");
    printf("R_16F          Two component format with 32-bit floating-point channels\n");
    printf("R_32F          Single component with 32-bit floating-point channels\n");
    printf("RG_8           Two component format with 8-bit fixed channels\n");
    printf("RG_16          Two component format with 16-bit fixed channels\n");
    printf("RG_16F         Two component format with 16-bit floating-point channels\n");
    printf("RG_32F         Two component format with 32-bit floating-point channels\n");
    printf("RGB_888        RGB format with 8-bit fixed channels\n");
#endif
    printf("\n");
    printf("The following is a list of compression formats\n");
#if (OPTION_BUILD_ASTC == 1)
    printf("ASTC           Adaptive Scalable Texture Compression\n");
#endif
    printf("ATC_RGB                 Compressed RGB format\n");
    printf("ATC_RGBA_Explicit       ARGB format with explicit alpha\n");
    printf("ATC_RGBA_Interpolated   ARGB format with interpolated alpha\n");
    printf("ATI1N          Single component compression format using the same\n");
    printf("               technique as DXT5 alpha. Four bits per pixel\n");
    printf("ATI2N          Two component compression format using the same\n");
    printf("               technique as DXT5 alpha. Designed for compression object\n");
    printf("               space normal maps. Eight bits per pixel\n");
    printf("ATI2N_XY       Two component compression format using the same technique\n");
    printf("               as DXT5 alpha. The same as ATI2N but with the channels swizzled.\n");
    printf("               Eight bits per pixel\n");
    printf("ATI2N_DXT5     An ATI2N like format using DXT5. Intended for use on GPUs that\n");
    printf("               do not natively support ATI2N. Eight bits per pixel\n");
    printf("BC1            Four component opaque (or 1-bit alpha) compressed texture\n");
    printf("               format. Four bit per pixel\n");
    printf("BC2            Four component compressed texture format with explicit\n");
    printf("               alpha.  Eight bits per pixel\n");
    printf("BC3            Four component compressed texture format with interpolated\n");
    printf("               alpha.  Eight bits per pixel\n");
    printf("BC4            Single component compressed texture format\n");
    printf("BC4_S          Signed Single component compressed texture format\n");
    printf("BC5            Two component compressed texture format\n");
    printf("BC5_S          Signed Two component compressed texture format\n");
    printf("BC6H           High-Dynamic Range  compression format\n");
    printf("BC7            High-quality compression of RGB and RGBA data\n\n");
    printf("DXT1           An opaque (or 1-bit alpha) DXTC compressed texture format.\n");
    printf("               Four bits per pixel\n");
    printf("DXT3           DXTC compressed texture format with explicit alpha.\n");
    printf("               Eight bits per pixel\n");
    printf("DXT5           DXTC compressed texture format with interpolated alpha.\n");
    printf("               Eight bits per pixel\n");
    printf("DXT5_xGBR      DXT5 with the red component swizzled into the alpha channel.\n");
    printf("               Eight bits per pixel\n");
    printf("DXT5_RxBG      Swizzled DXT5 format with the green component swizzled\n");
    printf("               into the alpha channel. Eight bits per pixel\n");
    printf("DXT5_RBxG      Swizzled DXT5 format with the green component swizzled\n");
    printf("               into the alpha channel & the blue component swizzled into\n");
    printf("               the green channel. Eight bits per pixel\n");
    printf("DXT5_xRBG      Swizzled DXT5 format with the green component swizzled\n");
    printf("               into the alpha channel & the red component swizzled into the\n");
    printf("               green channel. Eight bits per pixel\n");
    printf("DXT5_RGxB      Swizzled DXT5 format with the blue component swizzled\n");
    printf("               into the alpha channel. Eight bits per pixel\n");
    printf("DXT5_xGxR      Two-component swizzled DXT5 format with the red component\n");
    printf("               swizzled into the alpha channel & the green component in the\n");
    printf("               green channel. Eight bits per pixel\n");
    printf("ETC_RGB        Ericsson Texture Compression - Compressed RGB format.\n");
    printf("ETC2_RGB       Ericsson Texture Compression - Compressed RGB format.\n");
    printf("ETC2_RGBA      Ericsson Texture Compression - Compressed RGB with 8 bit Alpha.\n");
    printf("ETC2_RGBA1     Ericsson Texture Compression - Compressed RGB with 1 bit Alpha.\n");
#ifdef USE_GTC
    printf("GTC            Compressed RGB 8:8:8 format \n");
    printf("               This is a preview version for evaluation: subject to changes\n");
#endif
#ifdef USE_APC
    printf("APC            Compressed RGB 8:8:8 format \n");
    printf("               This is a preview version for evaluation: subject to changes\n");
#endif
#ifdef USE_LOSSLESS_COMPRESSION
    printf("BRLG           Lossless Compression using Brotli-G\n");
#endif
    printf("\n");
    printf("<codec options>: Reference  documentation for range of values\n\n");
    printf("-UseChannelWeighting <value> Use channel weightings\n");
    printf("-WeightR <value>             The weighting of the Red or X Channel\n");
    printf("-WeightG <value>             The weighting of the Green or Y Channel\n");
    printf("-WeightB <value>             The weighting of the Blue or Z Channel\n");
    printf("-AlphaThreshold <value>      The alpha threshold to use when compressing\n");
    printf("                             to DXT1 & BC1\n");
    printf("                             Texels with an alpha value less than the threshold\n");
    printf("                             are treated as transparent\n");
    printf("                             value is in the range of 1 to 255, 0 sets off\n");
#if (OPTION_BUILD_ASTC == 1)
    printf("-BlockRate <value>           ASTC 2D only - sets block size or bit rate\n");
    printf("                             value can be a bit per pixel rate from 0.0 to 9.9\n");
    printf("                             or can be a combination of x and y axes with paired\n");
    printf("                             values of 4,5,6,8,10 or 12 from 4x4 to 12x12\n");
#endif
    printf("-DXT1UseAlpha <value>        Encode single-bit alpha data.\n");
    printf("                             This option is deprecated use AlphaThreshold\n");
    printf("-RefineSteps <value>         Adds extra steps in encoding for BC1\n");
    printf("                             To improve quality over performance.\n");
    printf("                             Step values are 1 and 2\n");
    printf("-CompressionSpeed <value>    The trade-off between compression speed & quality\n");
    printf("                             This setting is not used in BC6H and BC7\n");
    printf("-NumThreads <value>          Number of threads to initialize for BC6H,BC7\n");
    printf("                             encoding (Max up to 128). Default set to 8\n");
    printf("-Quality <value>             Sets quality of encoding for BC7\n");
    printf("-Performance <value>         Sets performance of encoding for BC7\n");
    printf("-ColourRestrict <value>      This setting is a quality tuning setting for BC7\n");
    printf("                             which may be necessary for convenience in some\n");
    printf("                             applications\n");
    printf("-AlphaRestrict <value>       This setting is a quality tuning setting for BC7\n");
    printf("                             which may be necessary for some textures\n");
    printf("-ModeMask <value>            Mode to set BC7 to encode blocks using any of 8\n");
    printf("                             different block modes in order to obtain the\n");
    printf("                             highest quality\n");
#ifdef USE_LOSSLESS_COMPRESSION
    printf("-PageSize <value>            Page size, in bytes, to use for Brotli-G compression\n");
    printf("-NoPreconditionBRLG          Disable preconditioning of BCn textures before Brotli-G compression\n");
    printf("-DoSwizzleBRLG               Enable block swizzling during Brotli-G preconditioning for BCn textures, might further reduce compressed file size\n");
    printf(
        "-DoDeltaEncodeBRLG           Enable delta encoding of colours during Brotli-G preconditioning for BCn textures, might further reduce compressed file "
        "size\n");
#endif
#ifdef USE_3DMESH_OPTIMIZE
    printf("-optVCacheSize <value>        Enable vertices optimization with hardware cache size in the value specified. \n");
    printf(
        "                              (Value should be in range 1- no limit as it allows users to simulate hardware cache size to find the most "
        "optimum size).\n");
    printf("                              By default, mesh vertices optimization is enabled with cache size = 16. \n");
    printf("-optVCacheFIFOSize <value>    Enable vertices optimization with hardware cache size(FIFO replacement policy) in the value specified. \n");
    printf(
        "                              (Value should be in range 1- no limit as it allows users to simulate hardware cache size to find the most "
        "optimum size).\n");
    printf("                              By default, mesh vertices optimization with FIFO cache is disabled. \n");
    printf("-optOverdrawACMRThres <value> Enable overdraw optimization with ACMR (average cache miss ratio) threshold value specified. \n");
    printf("                              (Value range 1.00 - 3.00) - default is enabled with ACMR value = 1.05 (i.e. 5 percent worse).\n");
    printf("-optVFetch  <value>           Enable vertices fetch optimization. (Valus is either 0 to disable or 1 to enable.) \n");
    printf("-simplifyMeshLOD <value>      Enable mesh simplification with the LOD (level of details) in the value specified. \n");
    printf(
        "                              (Value should be in range 1- no limit as it allows users to simplify the mesh until the level they desired "
        "for experiment purpose. Higher level means less triangles drawn, less details.)\n");
#endif
    printf("-Analysis <image1> <image2>  Generate analysis metric like SSIM, PSNR values \n");
    printf("                             between 2 images with same size. Analysis_Result.xml file will be generated.\n");
    printf("\n\n");
    printf("-diff_image <image1> <image2> Generate difference between 2 images with same size \n");
    printf("                             A .bmp file will be generated. Please use compressonator GUI to increase the contrast to view the diff pixels.\n");
    printf("-log                         Logs process information to a process_results.txt file containing\n");
    printf("                             file info, performance data, SSIM, PSNR and MSE. \n");
    printf("-logcsv                      Logs process information to a process_results.csv file containing\n");
    printf("                             file info, performance data, SSIM, PSNR and MSE. \n");
    printf("-logfile <filename>          Logs process information to a user defined text file\n");
    printf("-logcsvfile <filename>       Logs process information to a user defined csv  file\n");
    printf("\n\n");
    printf("-imageprops <image>           Print image properties of image files specifies. \n");
    printf("\n\n");
    printf("Output options:\n\n");
    printf("-silent                      Disable print messages\n");
    printf("-performance                 Shows various performance stats\n");
    printf("-noprogress                  Disables showing of compression progress messages\n");
    printf("\n\n");
    printf("Example compression:\n\n");
#if (OPTION_BUILD_ASTC == 1)
    printf("compressonatorcli.exe -fd ASTC image.bmp result.astc \n");
    printf("compressonatorcli.exe -fd ASTC -BlockRate 0.8 image.bmp result.astc\n");
    printf("compressonatorcli.exe -fd ASTC -BlockRate 12x12 image.bmp result.astc\n");
#endif
    printf("compressonatorcli -fd BC7  image.bmp result.dds \n");
    printf("compressonatorcli -fd BC7  image.bmp result.bmp\n");
    printf("compressonatorcli -fd BC7  -NumTheads 16 image.bmp result.dds\n");
    printf("compressonatorcli -fd BC7  -ff PNG -fx KTX ./source_dir/ ./dist_dir/\n");
    printf("compressonatorcli -fd BC6H image.exr result.exr\n\n");
#ifdef _WIN32
    printf("Example compression using GPU Hardware or shader code with frameworks like OpenCL or DirectX:\n\n");
    printf("compressonatorcli -fd BC1 -EncodeWith GPU image.bmp result.dds \n");
    printf("compressonatorcli -fd BC1 -EncodeWith DXC image.bmp result.dds \n");
    printf("compressonatorcli -fd BC1 -EncodeWith OCL image.bmp result.dds \n");
#endif
    printf("Example decompression from compressed image using CPU:\n\n");
    printf("compressonatorcli result.dds image.bmp\n\n");
#ifdef _WIN32
    printf("Example decompression from compressed image using GPU:\n\n");
    printf("compressonatorcli -UseGPUDecompress result.dds image.bmp\n\n");
#endif
    printf("Example compression with decompressed result (Useful for qualitative analysis):\n\n");
    printf("compressonatorcli -fd BC7 -decomp result_decompression.bmp images.bmp result.dds\n\n");
#ifdef USE_MESH_DRACO_EXTENSION
    printf("Example draco compression usage (support glTF and OBJ file only):\n\n");
    printf("compressonatorcli -draco source.gltf dest.gltf\n");
    printf("compressonatorcli -draco source.obj dest.drc\n");
    printf("Note: only .obj file produces compressed .drc file. glTF does not produce .drc compressed file.\n");
    printf("Note: You can specify .obj as compressed file format as well, but a new .drc file will be created for this case.\n\n");
#ifdef USE_MESH_DRACO_SETTING
    printf("Specifies quantization bits settings:\n");
    printf("compressonatorcli -draco -dracolvl 7 -qpos 12 -qtexc 8 -qnorm 8 source.gltf dest.gltf\n\n");
#endif
    printf("Example draco decompression usage (support glTF and drc file only):\n\n");
    printf("compressonatorcli source.gltf dest.gltf\n");
    printf("compressonatorcli source.drc dest.obj\n");
#endif
#ifdef USE_3DMESH_OPTIMIZE
    printf("\n\n");
    printf("Example mesh optimization usage(support glTF and OBJ file only):\n");
    printf(
        "Using default settings : Optimize vertices with cache size = 16; Optimize overdraw with ACMR Threshold = 1.05; Optimize vertices fetch. "
        "\n\n");
    printf("compressonatorcli -meshopt source.gltf dest.gltf\n");
    printf("compressonatorcli -meshopt source.obj dest.obj\n\n");
    printf("Specifies settings :\n\n");
    printf("compressonatorcli -meshopt -optVCacheSize  32 -optOverdrawACMRThres  1.03 -optVFetch 0 source.gltf dest.gltf\n\n");
#ifdef USE_LOSSLESS_COMPRESSION
    printf("Example compression using Brotli-G:\n\n");
    printf("compressonatorcli -fd BRLG source.dds dest.brlg\n");
    printf("compressonatorcli -fd BRLG -DoSwizzleBRLG source.dds dest.brlg\n");
    printf("compressonatorcli -fd BRLG -NoPreconditionBRLG source.dds dest.brlg\n\n");

    printf("Example Brotli-G packaging:\n\n");
    printf("compressonatorcli -fd BRLG source_folder/ dest.brlg\n");
    printf("compressonatorcli packaged.brlg dest_folder/\n\n");
#endif
#endif
#ifdef _WIN32
    printf("For additional help go to documents folder and type index.html \n");
#else
    printf("For additional help go to documents html folder and type index.html \n");
#endif
}

bool ProgressCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    return CompressionCallback(fProgress, pUser1, pUser2);
}

//---------------------------------------------
// Compute codecs
//---------------------------------------------
#ifdef USE_GTC

#include "cmp_plugininterface.h"
PluginInterface_Encoder* g_plugin_EncoderGTC = NULL;
CMP_Encoder*             g_Codec_GTC         = NULL;

extern void (*GTC_DecompressBlock)(void* out, void* in);
extern void (*GTC_CompressBlock)(void* srcblock, void* dest, void* blockoptions);

void g_GTC_DecompressBlock(void* in, void* out)
{
    if (g_Codec_GTC)
        g_Codec_GTC->DecompressBlock(in, out);
}

void g_GTC_CompressBlock(void* in, void* out, void* blockoptions)
{
    if (g_Codec_GTC)
        g_Codec_GTC->CompressBlock(in, out, blockoptions);
}
#endif

//---------------------------------------------
// Lossless Compression
//---------------------------------------------
#ifdef USE_LOSSLESS_COMPRESSION

#include "cmp_plugininterface.h"
PluginInterface_Encoder* g_plugin_EncoderBRLG = NULL;
CMP_Encoder*             g_Codec_BRLG         = NULL;

extern void (*BRLG_DecompressBlock)(void* out, void* in);
extern void (*BRLG_CompressBlock)(void* srcblock, void* dest, void* blockoptions);

void g_BRLG_DecompressBlock(void* in, void* out)
{
    if (g_Codec_BRLG)
        g_Codec_BRLG->DecompressBlock(in, out);
}

void g_BRLG_CompressBlock(void* in, void* out, void* blockoptions)
{
    if (g_Codec_BRLG)
        g_Codec_BRLG->CompressBlock(in, out, blockoptions);
}
#endif

//----------------- BASIS: Run Time Encoder ------------------
#ifdef USE_BASIS
PluginInterface_Encoder* g_plugin_EncoderBASIS = NULL;
CMP_Encoder*             g_Codec_BASIS         = NULL;
extern int (*BASIS_CompressTexture)(void* in, void* out, void* blockoptions);
extern int (*BASIS_DecompressTexture)(void* in, void* out, void* blockoptions);

int g_BASIS_CompressTexture(void* in, void* out, void* blockoptions)
{
    if (g_Codec_BASIS)
    {
        return g_Codec_BASIS->CompressTexture(in, out, blockoptions);
    }
    return 0;
}

int g_BASIS_DecompressTexture(void* in, void* out, void* blockoptions)
{
    if (g_Codec_BASIS)
    {
        return g_Codec_BASIS->DecompressTexture(in, out, blockoptions);
    }
    return 0;
}

#endif

//----------------- APC: Run Time Encoder ------------------
#ifdef USE_APC
PluginInterface_Encoder* g_plugin_EncoderAPC = NULL;
CMP_Encoder*             g_Codec_APC         = NULL;
extern void (*APC_DecompressBlock)(void* out, void* in);
extern void (*APC_CompressBlock)(void* srcblock, void* dest, void* blockoptions);

void g_APC_DecompressBlock(void* in, void* out)
{
    if (g_Codec_APC)
        g_Codec_APC->DecompressBlock(in, out);
}

void g_APC_CompressBlock(void* in, void* out, void* blockoptions)
{
    if (g_Codec_APC)
        g_Codec_APC->CompressBlock(in, out, blockoptions);
}

#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32) && defined(DLL_SECURITY_CHECK_ALL_DLLS)
    // Apply DLL hijacking mitigation
    // This must be the first call in main()
    const bool dllLoadingHardened = DllSecurity::SecureDllLoading();

    // Validate DLL integrity after secure loading
    // Detects DLL proxy attacks and suspicious file modifications
    // This is turned on when Binary Security is enabled in the build configuration
    // Logging is suppressed so a successful scan never pollutes stdout for scripted use;
    // only failures are reported, on stderr.
#if defined(NDEBUG)
    // Dev/debug builds use locally-built, unsigned DLLs that would always fail this check,
    // so only release builds (where every shipped DLL is AMD-signed) run the scan at all.
    // The search-order hardening is defense in depth for runtime LoadLibrary calls, not an
    // integrity check, so a host that cannot apply it is warned rather than blocked.
    if (!dllLoadingHardened)
        fprintf(stderr, "Warning: unable to apply the restricted DLL search order.\n");

    // The DLL trust scan above only covers the application directory, so plugin discovery must
    // not fall back to a PATH-found directory either.
    g_pluginManager.restrictPluginSearchToApplicationDirectory();

    DllSecurity::SecurityScanResult result = DllSecurity::CheckDllSecurity(false);
    if (!result.isSecure)
    {
        fprintf(stderr,
                "DLL integrity check failed: %d invalid signature(s), %d suspicious file(s).\n",
                result.invalidSignatures,
                (int)result.suspiciousFiles.size());
        for (const std::wstring& suspicious : result.suspiciousFiles)
            fprintf(stderr, "  - %ls\n", suspicious.c_str());
        for (const std::wstring& warning : result.warnings)
            fprintf(stderr, "  ! %ls\n", warning.c_str());
        fprintf(stderr, "Exiting application.\n");
        return -1;
    }
#else
    (void)dllLoadingHardened;
#endif
#endif

    // Check if print status line has been assigned
    // if not get it a default to printf
    if (PrintStatusLine == NULL)
        PrintStatusLine = &LocalPrintF;

#if (OPTION_CMP_QT == 1)
    QCoreApplication app(argc, argv);
#endif

    g_CMIPS            = new CMIPS;
    g_CMIPS->PrintLine = PrintStatusLine;

#if (OPTION_BUILD_ASTC == 1)
    g_pluginManager.registerStaticPlugin("IMAGE", "ASTC", (void*)make_Plugin_ASTC);
#endif

#if (OPTION_BUILD_EXR == 1)
    g_pluginManager.registerStaticPlugin("IMAGE", "EXR", (void*)make_Plugin_EXR);
#endif

    g_pluginManager.registerStaticPlugin("IMAGE", "TGA", (void*)make_Plugin_TGA);  // Use for load only, Qt will be used for Save
    g_pluginManager.registerStaticPlugin("IMAGE", "ANALYSIS", (void*)make_Plugin_CAnalysis);

#if (OPTION_BUILD_KTX2 == 1)
    g_pluginManager.registerStaticPlugin("IMAGE", "KTX", (void*)make_Plugin_KTX);
#ifdef _WIN32
    g_pluginManager.registerStaticPlugin("IMAGE", "KTX2", (void*)make_Plugin_KTX2);
#endif
#endif

#ifdef USE_LOSSLESS_COMPRESSION
    g_pluginManager.registerStaticPlugin("IMAGE", "BRLG", (void*)make_Image_Plugin_BRLG);
#endif
#ifdef USE_LOSSLESS_COMPRESSION_BINARY
    g_pluginManager.registerStaticPlugin("IMAGE", "BINARY", (void*)make_Image_Plugin_BINARY);
#endif

    g_pluginManager.getPluginList("\\Plugins");

    CMP_RegisterHostPlugins();

#if (OPTION_CMP_QT == 1)
    QString dirPath = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(dirPath + "./plugins/imageformats");
#endif

    //----------------------------------
    // Process user command line parameters
    //----------------------------------
    if (argc > 1)
    {
        bool ParseOk = ParseParams(argc, argv);
        if (!ParseOk)
        {
            delete g_CMIPS;
            return -1;
        }

        if (g_CmdPrams.SourceFile.length() == 0)
        {
            printf("Source file was not supplied!\n");
            delete g_CMIPS;
            return -2;
        }

#ifdef USE_GTC
        //---------------------------------------
        // attempt to load GTC Codec
        //---------------------------------------
        g_plugin_EncoderGTC = reinterpret_cast<PluginInterface_Encoder*>(g_pluginManager.GetPlugin("ENCODER", "GTC"));
        // Found GTC Codec
        if (g_plugin_EncoderGTC)
        {
            //-------------------------------
            // create the compression  Codec
            //-------------------------------
            g_Codec_GTC = (CMP_Encoder*)g_plugin_EncoderGTC->TC_Create();

            //------------------------------------------------------------
            // Assign compressonator lib GTC codec to Compute GTC Codec
            //------------------------------------------------------------
            if (g_Codec_GTC)
            {
                GTC_CompressBlock   = g_GTC_CompressBlock;
                GTC_DecompressBlock = g_GTC_DecompressBlock;
            }
        }
#endif

#ifdef USE_LOSSLESS_COMPRESSION
        //---------------------------------------
        // attempt to load BRLG Codec
        //---------------------------------------
        g_plugin_EncoderBRLG = reinterpret_cast<PluginInterface_Encoder*>(g_pluginManager.GetPlugin("ENCODER", "BRLG"));
        // Found BRLG Codec
        if (g_plugin_EncoderBRLG)
        {
            //-------------------------------
            // create the compression Codec
            //-------------------------------
            g_Codec_BRLG = (CMP_Encoder*)g_plugin_EncoderBRLG->TC_Create();

            //------------------------------------------------------------
            // Assign compressonator lib codec to BRLG Codec
            //------------------------------------------------------------
            if (g_Codec_BRLG)
            {
                BRLG_CompressBlock   = g_BRLG_CompressBlock;
                BRLG_DecompressBlock = g_BRLG_DecompressBlock;
            }
        }
#endif

#ifdef USE_BASIS
        //---------------------------------------
        // attempt to load compute BASIS Codec
        //---------------------------------------
        g_plugin_EncoderBASIS = reinterpret_cast<PluginInterface_Encoder*>(g_pluginManager.GetPlugin("ENCODER", "BASIS"));
        // Found BASIS Codec
        if (g_plugin_EncoderBASIS)
        {
            //-------------------------------
            // create the compression  Codec
            //-------------------------------
            g_Codec_BASIS = (CMP_Encoder*)g_plugin_EncoderBASIS->TC_Create();

            // ToDo: Assignment to new encoder interfaces
            if (g_Codec_BASIS)
            {
                BASIS_CompressTexture   = g_BASIS_CompressTexture;
                BASIS_DecompressTexture = g_BASIS_DecompressTexture;
            }
        }
#endif

#ifdef USE_APC
        g_plugin_EncoderAPC = reinterpret_cast<PluginInterface_Encoder*>(g_pluginManager.GetPlugin("ENCODER", "APC"));
        if (g_plugin_EncoderAPC)
        {
            //-------------------------------
            // create the compression  Codec
            //-------------------------------
            g_Codec_APC = (CMP_Encoder*)g_plugin_EncoderAPC->TC_Create();

            //------------------------------------------------------------
            // Assign compressonator lib APC codec to Compute APC Codec
            //------------------------------------------------------------
            if (g_Codec_APC)
            {
                APC_CompressBlock   = g_APC_CompressBlock;
                APC_DecompressBlock = g_APC_DecompressBlock;
            }
        }
#endif

        if ((g_CmdPrams.CompressOptions.nEncodeWith != CMP_Compute_type::CMP_GPU_HW) &&
            (g_CmdPrams.CompressOptions.genGPUMipMaps || g_CmdPrams.CompressOptions.useSRGBFrames))
        {
            printf("Setup Error: genGPUMipMaps or useSRGBFrames requires EncodeWith GPU\n");
            return -1;
        }

        int ret = ProcessCMDLine(&CompressionCallback, NULL);

        delete g_CMIPS;

#ifdef USE_GTC
        //------------------------------------------
        // Cleanup the compute GTC compression Codec
        //------------------------------------------
        if (g_plugin_EncoderGTC)
        {
            if (g_Codec_GTC)
                g_plugin_EncoderGTC->TC_Destroy(g_Codec_GTC);
            delete g_plugin_EncoderGTC;
        }
#endif
#ifdef USE_LOSSLESS_COMPRESSION
        //------------------------------------------
        // Cleanup the compute compression Codec
        //------------------------------------------
        if (g_plugin_EncoderBRLG)
        {
            if (g_Codec_BRLG)
                g_plugin_EncoderBRLG->TC_Destroy(g_Codec_BRLG);
            delete g_plugin_EncoderBRLG;
        }
#endif
#ifdef USE_BASIS
        //------------------------------------------
        // Cleanup the compute GTC compression Codec
        //------------------------------------------
        if (g_plugin_EncoderBASIS)
        {
            if (g_Codec_BASIS)
                g_plugin_EncoderBASIS->TC_Destroy(g_Codec_BASIS);
            delete g_plugin_EncoderBASIS;
        }
#endif
#ifdef USE_APC
        //------------------------------------------
        // Cleanup the compute GTC compression Codec
        //------------------------------------------
        if (g_plugin_EncoderAPC)
        {
            if (g_Codec_APC)
                g_plugin_EncoderAPC->TC_Destroy(g_Codec_APC);
            delete g_plugin_EncoderAPC;
        }
#endif

        return ret;
    }
    else
    {
        PrintUsage();
        delete g_CMIPS;
        return 0;
    }
}
