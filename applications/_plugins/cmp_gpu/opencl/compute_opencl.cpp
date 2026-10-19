//=============================================================================
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
//==============================================================================

#include "copencl.h"
#include "cmp_fileio.h"

#include <cctype>
#include <cstring>
#include <new>
#include <string>

#ifdef USE_CPU_PERFORMANCE_COUNTERS
#include "cpu_timing.h"  // can use CPU timing but pref is to use GPU counters
#endif

#ifdef _M_X64
//#define ENABLE_SVM //- disable for now, causing build issue in 64bit release
#endif

#ifdef cl_amd_fp64
#pragma OPENCL EXTENSION cl_amd_fp64 : enable
#define fp64_supported 1
#elif defined(cl_khr_fp64)
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#define fp64_supported 1
#else
// "Double precision floating point not supported by OpenCL implementation."
#define fp64_supported 0
#endif

#ifdef ENABLE_SVM
extern bool SVMInitCodec(KernelOptions* options);
#endif

extern CMIPS* GPU_CLMips;

#define LOG_BUFFER_SIZE 102400
#define KERNEL_ARG_SOURCE 0
#define KERNEL_ARG_DESTINATION 1
#define KERNEL_ARG_SOURCEINFO 2
#define KERNEL_ARG_ENCODE 3

static bool is64Bit()
{
    return (sizeof(int*) == 8);
}

void PrintCL(const char* Format, ...)
{
    // define a pointer to save argument list
    va_list args;
    char    buff[1024];
    // process the arguments into our debug buffer
    va_start(args, Format);
    vsprintf_s(buff, Format, args);
    va_end(args);

    if (GPU_CLMips)
    {
        GPU_CLMips->Print(buff);
    }
    else
    {
        printf(buff);
    }
}

void PrintOCLError(cl_int error)
{
    switch (error)
    {
    case CL_DEVICE_NOT_FOUND:
        PrintCL("Error: CL_DEVICE_NOT_FOUND\n");
        break;
    case CL_DEVICE_NOT_AVAILABLE:
        PrintCL("Error: CL_DEVICE_NOT_AVAILABLE\n");
        break;
    case CL_COMPILER_NOT_AVAILABLE:
        PrintCL("Error: CL_COMPILER_NOT_AVAILABLE\n");
        break;
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        PrintCL("Error: CL_MEM_OBJECT_ALLOCATION_FAILURE\n");
        break;
    case CL_OUT_OF_RESOURCES:
        PrintCL("Error: CL_OUT_OF_RESOURCES\n");
        break;
    case CL_OUT_OF_HOST_MEMORY:
        PrintCL("Error: CL_OUT_OF_HOST_MEMORY\n");
        break;
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        PrintCL("Error: CL_PROFILING_INFO_NOT_AVAILABLE\n");
        break;
    case CL_MEM_COPY_OVERLAP:
        PrintCL("Error: CL_MEM_COPY_OVERLAP\n");
        break;
    case CL_IMAGE_FORMAT_MISMATCH:
        PrintCL("Error: CL_IMAGE_FORMAT_MISMATCH\n");
        break;
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        PrintCL("Error: CL_IMAGE_FORMAT_NOT_SUPPORTED\n");
        break;
    case CL_BUILD_PROGRAM_FAILURE:
        PrintCL("Error: CL_BUILD_PROGRAM_FAILURE\n");
        break;
    case CL_MAP_FAILURE:
        PrintCL("Error: CL_MAP_FAILURE\n");
        break;
    case CL_MISALIGNED_SUB_BUFFER_OFFSET:
        PrintCL("Error: CL_MISALIGNED_SUB_BUFFER_OFFSET\n");
        break;
    case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
        PrintCL("Error: CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST\n");
        break;
    case CL_COMPILE_PROGRAM_FAILURE:
        PrintCL("Error: CL_COMPILE_PROGRAM_FAILURE\n");
        break;
    case CL_LINKER_NOT_AVAILABLE:
        PrintCL("Error: CL_LINKER_NOT_AVAILABLE\n");
        break;
    case CL_LINK_PROGRAM_FAILURE:
        PrintCL("Error: CL_LINK_PROGRAM_FAILURE\n");
        break;
    case CL_DEVICE_PARTITION_FAILED:
        PrintCL("Error: CL_DEVICE_PARTITION_FAILED\n");
        break;
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
        PrintCL("Error: CL_KERNEL_ARG_INFO_NOT_AVAILABLE\n");
        break;
    case CL_INVALID_VALUE:
        PrintCL("Error: CL_INVALID_VALUE\n");
        break;
    case CL_INVALID_DEVICE_TYPE:
        PrintCL("Error: CL_INVALID_DEVICE_TYPE\n");
        break;
    case CL_INVALID_PLATFORM:
        PrintCL("Error: CL_INVALID_PLATFORM\n");
        break;
    case CL_INVALID_DEVICE:
        PrintCL("Error: CL_INVALID_DEVICE\n");
        break;
    case CL_INVALID_CONTEXT:
        PrintCL("Error: CL_INVALID_CONTEXT\n");
        break;
    case CL_INVALID_QUEUE_PROPERTIES:
        PrintCL("Error: CL_INVALID_QUEUE_PROPERTIES\n");
        break;
    case CL_INVALID_COMMAND_QUEUE:
        PrintCL("Error: CL_INVALID_COMMAND_QUEUE\n");
        break;
    case CL_INVALID_HOST_PTR:
        PrintCL("Error: CL_INVALID_HOST_PTR\n");
        break;
    case CL_INVALID_MEM_OBJECT:
        PrintCL("Error: CL_INVALID_MEM_OBJECT\n");
        break;
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        PrintCL("Error: CL_INVALID_IMAGE_FORMAT_DESCRIPTOR\n");
        break;
    case CL_INVALID_IMAGE_SIZE:
        PrintCL("Error: CL_INVALID_IMAGE_SIZE\n");
        break;
    case CL_INVALID_SAMPLER:
        PrintCL("Error: CL_INVALID_SAMPLER\n");
        break;
    case CL_INVALID_BINARY:
        PrintCL("Error: CL_INVALID_BINARY\n");
        break;
    case CL_INVALID_BUILD_OPTIONS:
        PrintCL("Error: CL_INVALID_BUILD_OPTIONS\n");
        break;
    case CL_INVALID_PROGRAM:
        PrintCL("Error: CL_INVALID_PROGRAM\n");
        break;
    case CL_INVALID_PROGRAM_EXECUTABLE:
        PrintCL("Error: CL_INVALID_PROGRAM_EXECUTABLE\n");
        break;
    case CL_INVALID_KERNEL_NAME:
        PrintCL("Error: CL_INVALID_KERNEL_NAME\n");
        break;
    case CL_INVALID_KERNEL_DEFINITION:
        PrintCL("Error: CL_INVALID_KERNEL_DEFINITION\n");
        break;
    case CL_INVALID_KERNEL:
        PrintCL("Error: CL_INVALID_KERNEL\n");
        break;
    case CL_INVALID_ARG_INDEX:
        PrintCL("Error: CL_INVALID_ARG_INDEX\n");
        break;
    case CL_INVALID_ARG_VALUE:
        PrintCL("Error: CL_INVALID_ARG_VALUE\n");
        break;
    case CL_INVALID_ARG_SIZE:
        PrintCL("Error: CL_INVALID_ARG_SIZE\n");
        break;
    case CL_INVALID_KERNEL_ARGS:
        PrintCL("Error: CL_INVALID_KERNEL_ARGS\n");
        break;
    case CL_INVALID_WORK_DIMENSION:
        PrintCL("Error: CL_INVALID_WORK_DIMENSION\n");
        break;
    case CL_INVALID_WORK_GROUP_SIZE:
        PrintCL("Error: CL_INVALID_WORK_GROUP_SIZE\n");
        break;
    case CL_INVALID_WORK_ITEM_SIZE:
        PrintCL("Error: CL_INVALID_WORK_ITEM_SIZE\n");
        break;
    case CL_INVALID_GLOBAL_OFFSET:
        PrintCL("Error: CL_INVALID_GLOBAL_OFFSET\n");
        break;
    case CL_INVALID_EVENT_WAIT_LIST:
        PrintCL("Error: CL_INVALID_EVENT_WAIT_LIST\n");
        break;
    case CL_INVALID_EVENT:
        PrintCL("Error: CL_INVALID_EVENT\n");
        break;
    case CL_INVALID_OPERATION:
        PrintCL("Error: CL_INVALID_OPERATION\n");
        break;
    case CL_INVALID_GL_OBJECT:
        PrintCL("Error: CL_INVALID_GL_OBJECT\n");
        break;
    case CL_INVALID_BUFFER_SIZE:
        PrintCL("Error: CL_INVALID_BUFFER_SIZE\n");
        break;
    case CL_INVALID_MIP_LEVEL:
        PrintCL("Error: CL_INVALID_MIP_LEVEL\n");
        break;
    case CL_INVALID_GLOBAL_WORK_SIZE:
        PrintCL("Error: CL_INVALID_GLOBAL_WORK_SIZE\n");
        break;
    case CL_INVALID_PROPERTY:
        PrintCL("Error: CL_INVALID_PROPERTY\n");
        break;
    case CL_INVALID_IMAGE_DESCRIPTOR:
        PrintCL("Error: CL_INVALID_IMAGE_DESCRIPTOR\n");
        break;
    case CL_INVALID_COMPILER_OPTIONS:
        PrintCL("Error: CL_INVALID_COMPILER_OPTIONS\n");
        break;
    case CL_INVALID_LINKER_OPTIONS:
        PrintCL("Error: CL_INVALID_LINKER_OPTIONS\n");
        break;
    case CL_INVALID_DEVICE_PARTITION_COUNT:
        PrintCL("Error: CL_INVALID_DEVICE_PARTITION_COUNT\n");
        break;
    case CL_INVALID_PIPE_SIZE:
        PrintCL("Error: CL_INVALID_PIPE_SIZE\n");
        break;
    case CL_INVALID_DEVICE_QUEUE:
        PrintCL("Error: CL_INVALID_DEVICE_QUEUE\n");
        break;
    default:
        PrintCL("Error: UKNOWN 0x%X\n", error);
        break;
    }
}

//====================================== Framework Common Interfaces : OpenCL Compute  ==========================================

void COpenCL::Init()
{
    query_timer::Initialize();

    m_initDeviceOk              = false;
    m_programRun                = false;
    m_codecFormat               = CMP_FORMAT_Unknown;
    m_num_blocks                = 0;
    m_CmpMTxPerSec              = 0;
    m_computeShaderElapsedMS    = 0.0f;
    m_platform_id               = NULL;
    m_device_id                 = NULL;
    m_num_platforms             = 0;
    m_command_queue             = NULL;
    m_kernel                    = NULL;
    m_device_destination_buffer = NULL;
    m_device_source_buffer      = NULL;
    m_Source_Info_buffer        = NULL;
    m_Encoder_buffer            = NULL;
    m_program_encoder           = NULL;
    m_context                   = NULL;
    m_svmSupport                = false;
    m_svmData                   = NULL;
    p_program.buffer            = NULL;
    ocl_time_device             = 0;
    m_deviceName                = "";
    m_version                   = "";
    m_driverVersion             = "";
    m_maxUCores                 = 12;

    //-------------------------
    // OpenCL compiler options
    //-------------------------
    long cmp_opt_size    = sizeof(m_compile_options);
    m_compile_options[0] = 0;

    // Make all warnings into errors, use -w to Inhitit all warning messages
    // strncat_s(m_compile_options, cmp_opt_size, "-Werror ", _TRUNCATE);

    // single and double precision denormalized numbers may be flushed to zero
    strncat_s(m_compile_options, cmp_opt_size, "-cl-denorms-are-zero ", _TRUNCATE);

    // Looks for additional include file in this sub-folder. The kernels ship with the
    // application, so prefer the install directory over the current working directory.
    const std::string kernelIncludeDir = CMP_ResolveInstallPath("./plugins/Compute");
    if (!kernelIncludeDir.empty() && kernelIncludeDir != "./plugins/Compute")
    {
        // Appended as a single unit: truncating part way through the path would drop the
        // closing quote and make every later kernel build fail with an opaque compiler error.
        const std::string includeOption = "-I \"" + kernelIncludeDir + "\" ";
        if ((strlen(m_compile_options) + includeOption.length()) < (size_t)cmp_opt_size)
            strncat_s(m_compile_options, cmp_opt_size, includeOption.c_str(), _TRUNCATE);
        else
            PrintCL("Kernel include path is too long for the compiler options, using the working directory instead\n");
    }
    strncat_s(m_compile_options, cmp_opt_size, "-I ./Plugins/Compute/ ", _TRUNCATE);

    // Use this to debug with CodeXL or other debuggers
    // This option disables all optimizations
    // strncat_s(m_compile_options, cmp_opt_size, "-g -cl-opt-disable", _TRUNCATE);

    // User override options set after this init call
    m_force_rebuild = false;
}

COpenCL::COpenCL(ComputeOptions CLOptions)
{
    Init();
    m_force_rebuild = CLOptions.force_rebuild;
}

COpenCL::~COpenCL()
{
    // Objects must be released in reverse creation order, with the context last: its child
    // objects hold implicit references that keep the context - and the driver's worker and
    // JIT compiler threads - alive. Cleanup is also unconditional here; gating it on
    // m_programRun leaked every object whenever an encode failed part way through, which
    // left those driver threads running until DLL unload.
    if (m_command_queue)
        clFinish(m_command_queue);

#ifdef ENABLE_SVM
    if (m_context && m_svmData)
    {
        clSVMFree(m_context, m_svmData);
        m_svmData = NULL;
    }
#endif

    CleanUpKernelAndIOBuffers();
    CleanUpProgramEncoder();

    // Hands back the resources held by the OpenCL JIT compiler at a controlled point,
    // instead of leaving them for the driver to reclaim while the DLL is being unloaded.
    if (m_platform_id)
        clUnloadPlatformCompiler(m_platform_id);

    if (m_context)
    {
        clReleaseContext(m_context);
        m_context = NULL;
    }

    m_programRun   = false;
    m_initDeviceOk = false;
}

void COpenCL::SetComputeOptions(ComputeOptions* CLOptions)
{
    m_force_rebuild = CLOptions->force_rebuild;
}

COpenCL::COpenCL(void* kerneloptions)
{
    m_kernel_options = (KernelOptions*)kerneloptions;
    Init();
}

// Handles are cleared as they are released so that both cleanup calls stay idempotent and
// safe to call on a partially constructed device.
void COpenCL::CleanUpProgramEncoder()
{
    // Encoder Program & Buffer
    if (p_program.buffer)
    {
        delete[] p_program.buffer;
        p_program.buffer = NULL;
    }
    if (m_program_encoder)
    {
        clReleaseProgram(m_program_encoder);
        m_program_encoder = NULL;
    }
}

void COpenCL::CleanUpKernelAndIOBuffers()
{
    // IO Buffers
    if (m_Encoder_buffer)
    {
        clReleaseMemObject(m_Encoder_buffer);
        m_Encoder_buffer = NULL;
    }
    if (m_Source_Info_buffer)
    {
        clReleaseMemObject(m_Source_Info_buffer);
        m_Source_Info_buffer = NULL;
    }
    if (m_device_destination_buffer)
    {
        clReleaseMemObject(m_device_destination_buffer);
        m_device_destination_buffer = NULL;
    }
    if (m_device_source_buffer)
    {
        clReleaseMemObject(m_device_source_buffer);
        m_device_source_buffer = NULL;
    }

    // Kernel and the queue that referenced the buffers above
    if (m_kernel)
    {
        clReleaseKernel(m_kernel);
        m_kernel = NULL;
    }
    if (m_command_queue)
    {
        clReleaseCommandQueue(m_command_queue);
        m_command_queue = NULL;
    }
}

bool COpenCL::GetPlatformID()
{
    //QUERY_PERFORMANCE("Get Platform ID ");

    m_result = clGetPlatformIDs(MAX_PLATFORMS, m_platform_ids, &m_num_platforms);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to get the GPU platforms!\n");
        PrintOCLError(m_result);
        return false;
    }
    return true;
}

bool COpenCL::SearchForGPU()
{
    for (uint32_t i = 0; i < m_num_platforms; i++)
    {
        // Get the device ids.
        m_result = clGetDeviceIDs(m_platform_ids[i], CL_DEVICE_TYPE_GPU, 1, &m_device_id, NULL);
        if (m_result == CL_SUCCESS)
        {
            m_platform_id = m_platform_ids[i];
            break;
        }

    }  // end for

    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to get a GPU device!\n");
        PrintOCLError(m_result);
        return false;
    }

    return true;
}

bool COpenCL::GetDeviceInfo()
{
    // Show the device info.
    char device_name[256] = {0};
    if (clGetDeviceInfo(m_device_id, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL) == CL_SUCCESS)
        m_deviceName = device_name;

    char openclVersion[1024];
    if (clGetDeviceInfo(m_device_id, CL_DEVICE_OPENCL_C_VERSION, sizeof(openclVersion), openclVersion, NULL) == CL_SUCCESS)
        m_version = openclVersion;

    char driverVersion[256] = {0};
    if (clGetDeviceInfo(m_device_id, CL_DRIVER_VERSION, sizeof(driverVersion), driverVersion, NULL) == CL_SUCCESS)
        m_driverVersion = driverVersion;

    if (clGetDeviceInfo(m_device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(m_maxUCores), &m_maxUCores, NULL) != CL_SUCCESS)
        m_maxUCores = 12;

    // long long GlobalMem;
    // clGetDeviceInfo(m_device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(long long), &GlobalMem, NULL);
    // PrintCL("Device Global Mem: %I64d Bytes\n", GlobalMem);
    //
    // long long LocalMem;
    // clGetDeviceInfo(m_device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(long long), &LocalMem, NULL);
    // PrintCL("Device Local  Mem: %I64d Bytes\n", LocalMem);
    //
    // size_t MaxWorkGroupSize;
    // clGetDeviceInfo(m_device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(int), &MaxWorkGroupSize, NULL);
    // PrintCL("Max work Groups  :%ld\n", MaxWorkGroupSize);
    //
    // cl_ulong MaxConstDataSize;
    // clGetDeviceInfo(m_device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &MaxConstDataSize, NULL);
    // PrintCL("Max Const Data   : %ld Bytes\n", MaxConstDataSize);

#ifdef ENABLE_SVM
    // SVM Support
    cl_device_svm_capabilities caps;
    clGetDeviceInfo(m_device_id, CL_DEVICE_SVM_CAPABILITIES, sizeof(cl_device_svm_capabilities), &caps, NULL);
    if (caps > 0)
    {
        m_svmSupport = true;
        PrintCL("SVM Course Grain Buffer Support: %s\n", (caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER) ? "Yes" : "No");
        PrintCL("SVM Fine Grain Buffer Support  : %s\n", (caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER) ? "Yes" : "No");
        PrintCL("SVM Fine Grain System Support  : %s\n", (caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM) ? "Yes" : "No");
        PrintCL("SVM Atomics                    : %s\n", (caps & CL_DEVICE_SVM_ATOMICS) ? "Yes" : "No");
    }
#endif

    return true;
}

bool COpenCL::CreateContext()
{
    // Create a context.
    m_context = clCreateContext(NULL, 1, &m_device_id, NULL, NULL, &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to create a context!\n");
        return false;
    }
    return true;
}

long COpenCL::file_size(FILE* p_file)
{
    // Get the size of the program.
    if (fseek(p_file, 0, SEEK_END) != 0)
        return 0;
    long program_size = ftell(p_file);
    fseek(p_file, 0, SEEK_SET);

    return program_size;
}

std::string COpenCL::BinaryCacheFilePath() const
{
    // A cached binary is only valid for the device and driver that produced it, so both are
    // part of the name: a GPU swap or driver upgrade then misses the cache instead of feeding
    // stale machine code to clCreateProgramWithBinary().
    std::string deviceKey = m_deviceName + "_" + m_driverVersion;
    for (std::size_t i = 0; i < deviceKey.length(); ++i)
    {
        const char character = deviceKey[i];
        if (!isalnum((unsigned char)character) && character != '.' && character != '-')
            deviceKey[i] = '_';
    }

    return CMP_GetShaderCacheFilePath(m_source_file, "." + deviceKey + ".cmp");
}

bool COpenCL::load_file()
{
#ifdef _DEBUG
    PrintCL("Loading [%s]\n", m_source_file.c_str());
#endif

    errno_t fopen_result;

    m_isBinary = false;

    // Compiled kernels are cached under the user's application data folder: the
    // install directory is read only for standard users.
    const std::string binaryCacheFile = BinaryCacheFilePath();

    if (!m_force_rebuild)
    {
        //===========================
        // Try loading the Binary file
        //===========================
        FILE* p_file_bin = NULL;
        bool  rebuild    = cmp_recompile_shader(m_source_file, binaryCacheFile);

        if (!rebuild)
        {
#ifdef _WIN32
            fopen_result = fopen_s(&p_file_bin, binaryCacheFile.c_str(), "rb");
            if (fopen_result != 0)
                rebuild = true;
#else
            p_file_bin = fopen(binaryCacheFile.c_str(), "rb");
            if (p_file_bin == NULL)
                rebuild = true;
#endif
        }

        // Found a .cmp file use it
        if (!rebuild)
        {
            // The cache lives in a user writable folder, so a damaged or tampered entry is
            // expected rather than exceptional: discard it and recompile from source instead
            // of failing the encode outright.
            OpenCLBinary_Header BinFile_Header;
            if (fread(&BinFile_Header, sizeof(OpenCLBinary_Header), 1, p_file_bin) != 1 || BinFile_Header.version != 1)
            {
                fclose(p_file_bin);
                p_file_bin = NULL;
                PrintCL("Cached kernel \"%s\" is unusable, rebuilding from source\n", binaryCacheFile.c_str());
            }
            else
            {
                const long cachedSize = file_size(p_file_bin) - (long)sizeof(OpenCLBinary_Header);

                if (cachedSize > 0)
                {
                    unsigned char* cachedProgram = new (std::nothrow) unsigned char[cachedSize];

                    if (cachedProgram != NULL)
                    {
                        // Reposition to bin data that is after header
                        fseek(p_file_bin, sizeof(OpenCLBinary_Header), SEEK_SET);

                        if (fread(cachedProgram, cachedSize, 1, p_file_bin) == 1)
                        {
                            // An unread buffer would leave the CRC at its seed, which GetCRC()
                            // reports as 0 -- the value every pre-CRC cache header carries.
                            CRC32 crc;
                            crc.ProcessBuffer(cachedProgram, (int)cachedSize, 1);

                            if (crc.GetCRC() == BinFile_Header.crc32)
                            {
                                fclose(p_file_bin);
                                p_program.ubuffer = cachedProgram;
                                m_program_size    = (size_t)cachedSize;
                                m_isBinary        = true;
                                return true;
                            }
                        }

                        delete[] cachedProgram;
                    }
                }

                fclose(p_file_bin);
                p_file_bin = NULL;
                PrintCL("Cached kernel \"%s\" failed its integrity check, rebuilding from source\n", binaryCacheFile.c_str());
            }
        }
    }  // !m_force_rebuild

    //===========================
    // Try loading the source file
    //===========================
    FILE* p_file_src = NULL;
    fopen_result     = fopen_s(&p_file_src, m_source_file.c_str(), "rb");
    if (fopen_result == 0)
    {
        m_program_size = file_size(p_file_src);

        if (m_program_size == 0)
        {
            fclose(p_file_src);
            PrintCL("Failed to read \"%s\" file size!\n", m_source_file.c_str());
            return false;
        }

        // Allocate memory for the program.
        p_program.buffer = new char[m_program_size];

        // Read the program in to memory.
        size_t read_size = fread(p_program.buffer, m_program_size, 1, p_file_src);
        if (read_size != 1)
        {
            fclose(p_file_src);
            PrintCL("Failed to read \"%s\" in to memory!\n", m_source_file.c_str());
            return false;
        }

        fclose(p_file_src);
        return true;
    }

    //===========================
    // Failed to load the file
    //===========================

    return false;
}

bool COpenCL::Create_Program_File()
{
    //------------------------------
    // Load the Source or Binary file
    //------------------------------
    load_file();

    cl_int result;

    if (!m_isBinary)
    {
        // Create the program.
        m_program_encoder = clCreateProgramWithSource(m_context, 1, const_cast<char const**>(&p_program.buffer), &m_program_size, &result);
        if (result != CL_SUCCESS)
        {
            PrintCL("Failed to create the program!\n");
            PrintOCLError(result);
            return false;
        }

        delete[] p_program.buffer;
        p_program.buffer = NULL;

        // Build the program.
        result = clBuildProgram(m_program_encoder, 1, &m_device_id, m_compile_options, NULL, NULL);
        if (result != CL_SUCCESS)
        {
            char message[LOG_BUFFER_SIZE];
            result = clGetProgramBuildInfo(m_program_encoder, m_device_id, CL_PROGRAM_BUILD_LOG, LOG_BUFFER_SIZE, message, NULL);
            if (result != CL_SUCCESS)
                message[0] = char(0);
            //PrintCL("Failed to build the program!\n%s",message);
            printf("Failed to build the program!\n%s", message);
            return false;
        }

        size_t compiled_size = 0;
        result               = clGetProgramInfo(m_program_encoder, CL_PROGRAM_BINARY_SIZES, sizeof(compiled_size), &compiled_size, NULL);

        uint8_t* p_binary = new uint8_t[compiled_size];
        result            = clGetProgramInfo(m_program_encoder, CL_PROGRAM_BINARIES, sizeof(p_binary), &p_binary, NULL);

        // Save the compiled code
        const std::string binaryCacheFile = BinaryCacheFilePath();
        FILE*             p_file          = NULL;
#ifdef _WIN32
        errno_t fopen_result = fopen_s(&p_file, binaryCacheFile.c_str(), "wb");
        if (fopen_result != 0)
            p_file = NULL;
#else
        p_file = fopen(binaryCacheFile.c_str(), "wb");
#endif
        if (p_file)
        {
            CRC32 crc;
            crc.ProcessBuffer(p_binary, (int)compiled_size, 1);

            OpenCLBinary_Header BinFile_Header;
            BinFile_Header.version = 1;
            BinFile_Header.crc32   = crc.GetCRC();
            fwrite(&BinFile_Header, sizeof(OpenCLBinary_Header), 1, p_file);
            fwrite(p_binary, compiled_size, 1, p_file);
            fclose(p_file);
        }
        else
            PrintCL("Unable to cache the compiled kernel to \"%s\"\n", binaryCacheFile.c_str());

    }  // not precompiled code
    else
    {
        // Create the program.
        m_program_encoder = clCreateProgramWithBinary(m_context, 1, &m_device_id, &m_program_size, (const unsigned char**)&p_program.ubuffer, NULL, &result);

        if (result != CL_SUCCESS)
        {
            PrintCL("Failed to load the binary program!\n");
            PrintOCLError(result);
            return false;
        }

        delete[] p_program.buffer;
        p_program.buffer = NULL;

        // Build the program.
        result = clBuildProgram(m_program_encoder, 1, &m_device_id, NULL, NULL, NULL);
        if (result != CL_SUCCESS)
        {
            PrintCL("Failed to build the program!\n");
            PrintOCLError(result);

            char message[128 * 1024];
            result = clGetProgramBuildInfo(m_program_encoder, m_device_id, CL_PROGRAM_BUILD_LOG, sizeof(message), message, NULL);
            if (result == CL_SUCCESS)
            {
                PrintCL(message);
            }

            return false;
        }
    }
    return true;
}

bool COpenCL::CreateProgramEncoder()
{
    //    QUERY_PERFORMANCE("Create Program  ");
    // Create the program.
    if (!Create_Program_File())
    {
        return false;
    }
    return true;
}

bool COpenCL::CreateIOBuffers()
{
#ifdef ENABLE_SVM
    // AllocateBuffers()
    // SVM
    if (m_svmSupport && is64Bit() && (m_kernel_options->size > 0))
    {
        // initialize any device/SVM memory here.
        m_svmData = clSVMAlloc(m_context, CL_MEM_READ_WRITE, m_kernel_options->size, NULL);
        if (m_svmData == NULL)
        {
            PrintCL("Failed to allocate the Encode buffer on the device!\n");
            return false;
        }
        m_kernel_options->dataSVM = m_svmData;
    }
#endif

    // allocate device 32bit buffer for the image source
    // idealy we want to not copy the buffer form host
    // Look into the following options clmap  and CL_MEM_AMD_PERSISTANT
    // Allocate the 32-bit source buffer in device memory.

    m_device_source_buffer = clCreateBuffer(m_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, m_source_buffer_size, (void*)m_psource, &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to allocate the source buffer on the device!\n");
        return false;
    }

    m_Source_Info_buffer = clCreateBuffer(m_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(Source_Info), (void*)&m_SourceInfo, &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to allocate the source info buffer on the device!\n");
        PrintOCLError(m_result);
        return false;
    }

    // Allocate the destination buffer in device memory.
    m_device_destination_buffer = clCreateBuffer(m_context, CL_MEM_WRITE_ONLY, m_destination_size, NULL, &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to allocate the destination buffer on the device!\n");
        PrintOCLError(m_result);
        return false;
    }
    return true;
}

//#include "bcn_common_kernel.h" use this for debugging data to kernel when using BC15

bool COpenCL::RunKernel()
{
    //    QUERY_PERFORMANCE("Run Kernel      ");

    // Get a handle to the kernel.
    m_kernel = clCreateKernel(m_program_encoder, "CMP_GPUEncoder", &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to create the kernel!\n");
        PrintOCLError(m_result);
        return false;
    }

    // Create the command queue. with profiling enabled
    const cl_queue_properties properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    m_command_queue                        = clCreateCommandQueueWithProperties(m_context, m_device_id, properties, &m_result);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to create the command queue!\n");
        return false;
    }

    //====================================================================================

    // ImageSource
    m_result = clSetKernelArg(m_kernel, KERNEL_ARG_SOURCE, sizeof(m_device_source_buffer), &m_device_source_buffer);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to set the source kernel argument!\n");
        PrintOCLError(m_result);
        return false;
    }

    //ImageDestination
    m_result = clSetKernelArg(m_kernel, KERNEL_ARG_DESTINATION, sizeof(m_device_destination_buffer), &m_device_destination_buffer);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to set the destination kernel argument!\n");
        PrintOCLError(m_result);
        return false;
    }

    //SourceInfo
    m_result = clSetKernelArg(m_kernel, KERNEL_ARG_SOURCEINFO, sizeof(m_Source_Info_buffer), (void*)&m_Source_Info_buffer);
    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to set the source info argument!\n");
        PrintOCLError(m_result);
        return false;
    }

#ifdef ENABLE_SVM  // Don not enable unless CMP_GPUEncoder parameters are updated
    if (m_svmSupport)
    {
        if (m_svmData)
        {
            /* reserve svm space for CPU update */
            m_result = clEnqueueSVMMap(m_command_queue,
                                       CL_TRUE,  //blocking call
                                       CL_MAP_WRITE_INVALIDATE_REGION,
                                       m_svmData,
                                       m_kernel_options->size,
                                       0,
                                       NULL,
                                       NULL);

            m_kernel_options->dataSVM = m_svmData;

            if (!SVMInitCodec(m_kernel_options))
            {
                PrintCL("Failed to initialize SVM Encode kernel data!\n");
                PrintOCLError(m_result);
                return false;
            }

            m_result = clEnqueueSVMUnmap(m_command_queue, m_svmData, 0, NULL, NULL);

            // Set appropriate arguments to the kernel
            m_result = clSetKernelArgSVMPointer(m_kernel, KERNEL_ARG_ENCODE, (void*)(m_svmData));
            if (m_result != CL_SUCCESS)
            {
                PrintCL("Failed to set the SVM Encode kernel argument!\n");
                PrintOCLError(m_result);
                return false;
            }
        }  // Encode ptr
        else
        {
            PrintCL("Failed to set the SVM Encode kernel argument, invalid pointer!\n");
            PrintOCLError(m_result);
            return false;
        }
    }
    else
#endif
    {
        m_Encoder_buffer = clCreateBuffer(m_context, CL_MEM_READ_WRITE, m_kernel_options->size, NULL, &m_result);

        // Set argument for the compress()
        m_result = clSetKernelArg(m_kernel, KERNEL_ARG_ENCODE, sizeof(m_Encoder_buffer), (void*)&m_Encoder_buffer);
        if (m_result != CL_SUCCESS)
        {
            PrintCL("Failed to set the Encode block argument!\n");
            PrintOCLError(m_result);
            return false;
        }

        // CMP_BC15Options *temp = reinterpret_cast<CMP_BC15Options *>(m_kernel_options->data);
        // int blocksize = sizeof(CMP_BC15Options);

        m_result = clEnqueueWriteBuffer(m_command_queue, m_Encoder_buffer, CL_TRUE, 0, m_kernel_options->size, (void*)m_kernel_options->data, 0, NULL, NULL);
        if (m_result != CL_SUCCESS)
        {
            PrintCL("Failed to set the Encode block buffer!\n");
            PrintOCLError(m_result);
            return false;
        }
    }

    // Run the kernel.
    //----------------------------------
    //# todo: max numbers of blocks to launch on low end GPUs to avoid kernel timeout
    //int maxblockslaunch = MIN(m_width_in_blocks*m_height_in_blocks, 768 * (int)m_maxComputeUnits);

    size_t local_work_size[]  = {8, 8};
    size_t global_work_size[] = {8, 8};

    // Check for smal images < 64 width x 64 height in pixels
    if (m_width_in_blocks >= 8)
    {
        global_work_size[0] = ((m_width_in_blocks + local_work_size[0] - 1) / local_work_size[0]) * local_work_size[0];
    }
    else
    {
        local_work_size[0]  = 1;
        global_work_size[0] = m_width_in_blocks;
    }

    if (m_height_in_blocks >= 8)
    {
        global_work_size[1] = ((m_height_in_blocks + local_work_size[1] - 1) / local_work_size[1]) * local_work_size[1];
    }
    else
    {
        local_work_size[1]  = 1;
        global_work_size[1] = m_height_in_blocks;
    }

#ifdef USE_CPU_PERFORMANCE_COUNTERS
    cpu_timer cputimer;
    cputimer.Start(0);
#else
    cl_event cl_perf_event = NULL;
#endif

    m_result = clEnqueueNDRangeKernel(m_command_queue,
                                      m_kernel,
                                      2,
                                      NULL,
                                      global_work_size,
                                      local_work_size,
                                      0,
                                      NULL,
#ifdef USE_CPU_PERFORMANCE_COUNTERS
                                      NULL
#else
                                      m_getPerfStats ? &cl_perf_event : NULL
#endif
    );

    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to launch the kernel!\n");
        PrintOCLError(m_result);
        return false;
    }

    // Wait until all queued commands have been processed and completed.
    if (m_getPerfStats)
        m_result = clFinish(m_command_queue);

#ifdef USE_CPU_PERFORMANCE_COUNTERS
    cputimer.Stop(0);
    m_computeShaderElapsedMS = cputimer.GetTimeMS(0);
#endif

    // Check if performance event has been set and valid
    if (m_getPerfStats &&
#ifdef USE_CPU_PERFORMANCE_COUNTERS
        (m_computeShaderElapsedMS > 0) &&
#else
        cl_perf_event &&
#endif
        (m_result == CL_SUCCESS))
    {
#ifdef USE_CPU_PERFORMANCE_COUNTERS
#else
        // Get the event data
        cl_ulong start      = 0;
        cl_ulong end        = 0;
        cl_ulong nspercount = 1;

        m_result = clGetEventProfilingInfo(cl_perf_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);

        if (m_result == CL_SUCCESS)
            m_result = clGetEventProfilingInfo(cl_perf_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

        // Get the number of nanoseconds per count
        cl_ulong resolution;
        if (m_result == CL_SUCCESS)
            m_result = clGetDeviceInfo(m_device_id, CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(cl_ulong), &resolution, NULL);

        if (m_result == CL_SUCCESS)
            m_result = clReleaseEvent(cl_perf_event);

        if (m_result != CL_SUCCESS)
        {
            PrintCL("Failed clReleaseEvent!\n");
            PrintOCLError(m_result);
            return false;
        }

        // counters are in nano second incriments 1e-9f Convert
        m_num_blocks = m_height_in_blocks * m_width_in_blocks;
        if (m_num_blocks == 0)
            m_num_blocks = 1;
        float nanoSeconds = (float)(end - start);
        // Convert nanosec to ms divide by 1e6f
        m_computeShaderElapsedMS = nanoSeconds / 1e6f;
        // time to process a single block (4x4) which is 16 texels
        m_computeShaderElapsedMS = m_computeShaderElapsedMS / (float)m_num_blocks;
#endif
        if (m_computeShaderElapsedMS > 0)
        {
            float ElapsedSeconds      = m_computeShaderElapsedMS / 1E3f;
            float ElapsedSecondsPerTx = ElapsedSeconds / 16;
            float TxPerSec            = 1 / ElapsedSecondsPerTx;
            // time to process a 1M texels in a second
            m_CmpMTxPerSec = TxPerSec / 1E6f;
        }
        else
            m_CmpMTxPerSec = 0;
    }

    return true;
}

bool COpenCL::GetResults()
{
    //    QUERY_PERFORMANCE("Get Results     ");

    // Copy the results from device to host memory.
    m_result = clEnqueueReadBuffer(m_command_queue, m_device_destination_buffer, true, 0, m_destination_size, p_destination, 0, NULL, NULL);

    if (m_result != CL_SUCCESS)
    {
        PrintCL("Failed to copy the results from the device!\n");
        PrintOCLError(m_result);
        return false;
    }

    return true;
}

float COpenCL::GetProcessElapsedTimeMS()
{
    return m_computeShaderElapsedMS;
}

float COpenCL::GetMTxPerSec()
{
    return m_CmpMTxPerSec;
}

int COpenCL::GetBlockSize()
{
    return m_num_blocks;
}

int COpenCL::GetMaxUCores()
{
    return m_maxUCores;
}

const char* COpenCL::GetDeviceName()
{
    return m_deviceName.c_str();
}

const char* COpenCL::GetVersion()
{
    return m_version.c_str();
}

CMP_ERROR COpenCL::Compress(KernelOptions* KernelOptions, MipSet& srcTexture, MipSet& destTexture, CMP_Feedback_Proc pFeedback = NULL)
{
    bool newFormat = false;
    if (m_codecFormat != destTexture.m_format)
    {
        m_codecFormat = destTexture.m_format;
        newFormat     = true;
    }

    if (m_codecFormat == CMP_FORMAT_Unknown)
    {
        // dont know how to progress this destination
        return (CMP_ERR_GENERIC);
    }

    m_source_file = KernelOptions->srcfile;
    if (m_source_file.length() == 0)
        return CMP_ERR_NOSHADER_CODE_DEFINED;

    m_getPerfStats = KernelOptions->getPerfStats && (destTexture.m_nIterations < 1);

    m_kernel_options->data    = KernelOptions->data;
    m_kernel_options->size    = KernelOptions->size;
    m_kernel_options->format  = KernelOptions->format;
    m_kernel_options->dataSVM = KernelOptions->dataSVM;
    ;

    m_source_buffer_size = srcTexture.dwDataSize;

    p_destination      = destTexture.pData;
    m_destination_size = destTexture.dwDataSize;

    if (destTexture.m_nBlockWidth > 0)
        m_width_in_blocks = (cl_uint)srcTexture.dwWidth / destTexture.m_nBlockWidth;
    else
    {
        return CMP_ERR_GENERIC;
    }

    if (destTexture.m_nBlockHeight > 0)
        m_height_in_blocks = (cl_uint)srcTexture.dwHeight / destTexture.m_nBlockHeight;
    else
    {
        return CMP_ERR_GENERIC;
    }

    m_psource                       = (CMP_Vec4uc*)srcTexture.pData;
    m_SourceInfo.m_src_height       = srcTexture.dwHeight;
    m_SourceInfo.m_src_width        = srcTexture.dwWidth;
    m_SourceInfo.m_width_in_blocks  = m_width_in_blocks;
    m_SourceInfo.m_height_in_blocks = m_height_in_blocks;
    m_SourceInfo.m_fquality         = KernelOptions->fquality;

    // Using OpenCL - Setit up and call the kernel function
    bool ok = true;

    // check for first time use on host device
    if (!m_initDeviceOk)
    {
        if (GetPlatformID() == false)
            ok = false;
        if (ok && (SearchForGPU() == false))
            ok = false;
        if (ok && (GetDeviceInfo() == false))
            ok = false;
        if (ok && (CreateContext() == false))
            ok = false;
        m_initDeviceOk = ok;
    }
    if (m_programRun)
    {
        CleanUpKernelAndIOBuffers();
        if (newFormat)
            CleanUpProgramEncoder();
        m_programRun = false;
    }
    if (newFormat)
    {
        if (ok && (CreateProgramEncoder() == false))
            ok = false;
    }
    if (ok && (CreateIOBuffers() == false))
        ok = false;
    if (ok && (RunKernel() == false))
        ok = false;
    if (ok && (GetResults() == false))
        ok = false;

    if (ok)
    {
        m_programRun = true;
        return CMP_OK;
    }

    return (CMP_ERR_GENERIC);
}
