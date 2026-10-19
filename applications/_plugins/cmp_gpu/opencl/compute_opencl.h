//=====================================================================
// Copyright (c) 2020-2024    Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef H_COMPUTE_OPENCL
#define H_COMPUTE_OPENCL

#define __CL_ENABLE_EXCEPTIONS

#include <cl/opencl.h>

#include "common_def.h"  // Updated at run time by COpenCL

#include "compute_base.h"
#include "common_kerneldef.h"
#include "cpu_timing.h"
#include "textureio.h"
#include "crc32.h"
#include "cmp_math_common.h"

#define MAX_PLATFORMS 4

using namespace CMP_Compute_Base;

#define NUM_THREADS 1  // Number of threads per work group.(can have upto 64 Threads)
// #define USE_CRC32                        // Use source file checking first to update .cmp files at run time. (slows compute loading time)
// Disable this definition to use .cmp files first and then try src if not found

struct OpenCLBinary_Header
{
    unsigned int version;
    unsigned int crc32;
};

class COpenCL : public ComputeBase
{
public:
    COpenCL(void* kerneloptions);
    COpenCL(ComputeOptions CLOptions);
    ~COpenCL();
    CMP_ERROR   Compress(KernelOptions* Options, MipSet& SrcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback);
    void        SetComputeOptions(ComputeOptions* CLOptions);
    float       GetProcessElapsedTimeMS();
    float       GetMTxPerSec();
    int         GetBlockSize();
    const char* GetDeviceName();
    const char* GetVersion();
    int         GetMaxUCores();

private:
    bool       m_initDeviceOk;
    bool       m_programRun;
    CMP_FORMAT m_codecFormat;

    // Performance Info
    bool  m_getPerfStats;
    int   m_num_blocks;
    float m_CmpMTxPerSec;
    float m_computeShaderElapsedMS;

    // Device Info
    std::string m_deviceName;
    std::string m_version;
    std::string m_driverVersion;
    cl_uint     m_maxUCores;

    // Internal
    std::string      m_source_file;
    KernelOptions*   m_kernel_options;
    cl_uint          m_num_platforms;
    cl_int           m_result;
    cl_platform_id   m_platform_ids[MAX_PLATFORMS];
    cl_platform_id   m_platform_id;
    cl_device_id     m_device_id;
    cl_context       m_context;
    cl_program       m_program_encoder;
    cl_kernel        m_kernel;
    cl_mem           m_device_source_buffer;
    cl_mem           m_device_destination_buffer;
    cl_mem           m_Source_Info_buffer;
    cl_mem           m_Encoder_buffer;
    cl_command_queue m_command_queue;
    size_t           m_destination_size;
    size_t           m_source_buffer_size;
    cl_uint          m_width_in_blocks;
    cl_uint          m_height_in_blocks;

    // from NV code
    // cl_int          m_blocksPerLaunch;
    cl_int m_blocks;

    // Options that users can override or add
    bool m_force_rebuild;

    Source_Info m_SourceInfo;
    bool        m_svmSupport;
    void*       m_svmData;

    // OpenCL file (Binary or Source)
    char m_compile_options[256];

    union
    {
        char*          buffer;
        unsigned char* ubuffer;
    } p_program;

    size_t m_program_size;
    bool   m_isBinary;
    long   m_sourcefile_CRC32;
    float  ocl_time_device = 0;

    long file_size(FILE* p_file);
    bool load_file();

    // Cache file for the compiled kernel binary, keyed on the device and driver it was built for.
    std::string BinaryCacheFilePath() const;

    // Need to fill these
    CMP_BYTE*   p_destination;
    CMP_Vec4uc* m_psource;
    void        Init();
    bool        GetPlatformID();
    bool        SearchForGPU();
    bool        GetDeviceInfo();
    bool        CreateContext();
    bool        Create_Program_File();
    bool        CreateProgramEncoder();
    bool        CreateIOBuffers();
    bool        RunKernel();
    bool        GetResults();

    void CleanUpProgramEncoder();
    void CleanUpKernelAndIOBuffers();
};

#endif
