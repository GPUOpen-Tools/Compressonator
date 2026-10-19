cmake_minimum_required(VERSION 3.10)

# Cmake build examples: for Visual Studio 2017
#
# cmake -G "Visual Studio 15 2017 Win64"   Enable building all apps and libs
# cmake -DOPTION_ENABLE_ALL_APPS=OFF -G "Visual Studio 15 2017 Win64"    Disable all builds except external libs, minimal cmake base setup 
# cmake -DOPTION_ENABLE_ALL_APPS=OFF -DOPTION_BUILD_APPS_CMP_CLI=ON -G "Visual Studio 15 2017 Win64"   Enable only CLI app build
# cmake -DOPTION_ENABLE_ALL_APPS=OFF -DOPTION_BUILD_APPS_CMP_GUI=ON -G "Visual Studio 15 2017 Win64"   Enable only GUI app build
#
# Other options are available : see OPTION_BUILDS... 
#

if(POLICY CMP0091)
   cmake_policy(SET CMP0091 NEW)
endif()

if(POLICY CMP0076)
  cmake_policy(SET CMP0076 NEW)
endif()

# Setting version information
#
# The single source of truth is cmp_compressonatorlib/version.h, which the release
# scripts also parse (see scripts/get_version.py). Hardcoding the defaults here let
# them drift: CMP_VERSION_MINOR stayed at 5 after version.h moved to 4.6, so every
# CPack package produced by a plain `cmake ..` was stamped 4.5.

# A version component of 0 is legitimate (a 5.0 release), and CMake treats the string "0" as
# false, so every test below compares against the empty string instead of using truthiness.
if ("${CMP_VERSION_MAJOR}" STREQUAL "" OR "${CMP_VERSION_MINOR}" STREQUAL "")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmp_compressonatorlib/version.h" _cmp_version_header)
    string(REGEX MATCH "#define[ \t]+VERSION_MAJOR_MAJOR[ \t]+([0-9]+)" _cmp_match "${_cmp_version_header}")
    set(_cmp_header_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH "#define[ \t]+VERSION_MAJOR_MINOR[ \t]+([0-9]+)" _cmp_match "${_cmp_version_header}")
    set(_cmp_header_minor "${CMAKE_MATCH_1}")

    if ("${_cmp_header_major}" STREQUAL "" OR "${_cmp_header_minor}" STREQUAL "")
        message(FATAL_ERROR "Could not parse VERSION_MAJOR_MAJOR/VERSION_MAJOR_MINOR from cmp_compressonatorlib/version.h")
    endif()

    if ("${CMP_VERSION_MAJOR}" STREQUAL "")
        set(CMP_VERSION_MAJOR ${_cmp_header_major})
    endif()

    if ("${CMP_VERSION_MINOR}" STREQUAL "")
        set(CMP_VERSION_MINOR ${_cmp_header_minor})
    endif()
endif()

if (NOT CMP_VERSION_BUILD_NUMBER)
    set(CMP_VERSION_BUILD_NUMBER 0)
endif()

project(Compressonator VERSION ${CMP_VERSION_MAJOR}.${CMP_VERSION_MINOR})
message("Building Compressonator version ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# ------------------------------
# Helper function for the build 
# ------------------------------
include(cmake/helperfunctions.cmake)


# ------------------------------
# Common compiler options
# ------------------------------

if (CMP_HOST_WINDOWS)
    add_compile_options(/W4 /wd4201)
#    add_compile_options(/W4 /WX /wd4201)
elseif(CMP_HOST_LINUX)
    add_compile_options(-Wall)
#    add_compile_options(-Wall -Werror)
endif()

# ------------------------------------
# Visual Studio project folder names
# ------------------------------------

set(PROJECT_FOLDER_SDK "SDK")
set(PROJECT_FOLDER_SDK_LIBS "${PROJECT_FOLDER_SDK}/Libraries")
set(PROJECT_FOLDER_SDK_EXTERNAL "${PROJECT_FOLDER_SDK}/External Libraries")
set(PROJECT_FOLDER_SDK_PLUGIN_DYNAMIC "${PROJECT_FOLDER_SDK}/Plugins/Dynamic")
set(PROJECT_FOLDER_SDK_PLUGIN_STATIC "${PROJECT_FOLDER_SDK}/Plugins/Static")
set(PROJECT_FOLDER_SDK_EXAMPLES "${PROJECT_FOLDER_SDK}/Examples")

set(PROJECT_FOLDER_APPS "Applications")
set(PROJECT_FOLDER_APPS_LIBS "${PROJECT_FOLDER_APPS}/Libraries")
set(PROJECT_FOLDER_APPS_EXTERNAL "${PROJECT_FOLDER_APPS}/External Libraries")
set(PROJECT_FOLDER_APPS_PLUGIN_DYNAMIC "${PROJECT_FOLDER_APPS}/Plugins/Dynamic")
set(PROJECT_FOLDER_APPS_PLUGIN_STATIC "${PROJECT_FOLDER_APPS}/Plugins/Static")

#----------------------------------------
# Check compiler feature support levels
# For Apple (Mac) default to C++11
#---------------------------------------
if (CMP_HOST_LINUX OR CMP_HOST_WINDOWS)
    if("cxx_std_11" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(COMPILER_SUPPORTS_CXX11 ON)
        message("Compiler Supports cxx_std_11")
    endif()
    if("cxx_std_14" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(COMPILER_SUPPORTS_CXX14 ON)
        message("Compiler Supports cxx_std_14")
    endif()
    if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(COMPILER_SUPPORTS_CXX17 ON)
        message("Compiler Supports cxx_std_17")
    endif()
else()
    message("UNIX Build: CHECK_CXX_COMPILER_FLAG defaulted to -std=c++11")
    set(COMPILER_SUPPORTS_CXX11 ON)
endif()

#---------------------------------------------------------------------------------
# Provide default values for each build option
#
# ! Cached variables should not be modified and reflect user's preference
# ! Though regular variables may be subject to change due to requirements, dependencies, etc.
#  Example:
#  do not set(OPTION_BUILD_CMP_SDK ON CACHE BOOL "Build Compressonator SDK" FORCE)
#  do     set(OPTION_BUILD_CMP_SDK ON)
#  so that user preference is $CACHE{OPTION_BUILD_CMP_SDK} and the actual value is ${OPTION_BUILD_CMP_SDK}
#---------------------------------------------------------------------------------
option(OPTION_ENABLE_ALL_APPS             "Enable all apps"                 ON) # TODO rename to OPTION_BUILD_ALL
option(OPTION_BUILD_CMP_SDK               "Build Compressonator SDK"        OFF)
option(OPTION_BUILD_APPS_CMP_CLI          "Build Compressonator CLI"        OFF)
option(OPTION_BUILD_APPS_CMP_GUI          "Build Compressonator GUI"        OFF)
option(OPTION_BUILD_APPS_CMP_UNITTESTS    "Build Compressontor UnitTests"   OFF)
option(OPTION_BUILD_APPS_CMP_EXAMPLES     "Build Compressontor Examples"    OFF)

# A local build links unsigned dependency DLLs (Qt, OpenCV, glew, dxc), which the startup
# integrity check rejects, so the CLI/GUI exit before doing any work. Turn this OFF to get a
# runnable unsigned test build; release builds must leave it ON.
option(OPTION_CMP_DLL_SECURITY_CHECK "Enforce the startup DLL signature check in the CLI and GUI" ON)

if(WIN32 AND NOT OPTION_CMP_DLL_SECURITY_CHECK)
    message(WARNING "OPTION_CMP_DLL_SECURITY_CHECK is OFF: this build does not verify DLL signatures at startup and must not be released.")
endif()

#--------------------------------------------------------------------------
# Enable or Disable Specific Build Apps
#--------------------------------------------------------------------------
if (OPTION_ENABLE_ALL_APPS)
    set(OPTION_BUILD_CMP_SDK               ON)
    set(OPTION_BUILD_APPS_CMP_CLI          ON)
    set(OPTION_BUILD_APPS_CMP_GUI          ON)
    set(OPTION_BUILD_APPS_CMP_UNITTESTS    ON)
    set(OPTION_BUILD_APPS_CMP_EXAMPLES     ON)
else()
    option(OPTION_BUILD_APPS_CMP_CLI       OFF)
    option(OPTION_BUILD_APPS_CMP_GUI       OFF)
    option(OPTION_BUILD_CMP_SDK            OFF)
    option(OPTION_BUILD_APPS_CMP_UNITTESTS OFF)
    option(OPTION_BUILD_APPS_CMP_EXAMPLES  OFF)
endif()

# Minimum Lib Dependencies for CLI, GUI, and SDK (GUI has additional lib requirements added later in this cmake)
if (OPTION_BUILD_APPS_CMP_CLI OR OPTION_BUILD_APPS_CMP_GUI)
    set(LIB_BUILD_COMPRESSONATOR_SDK    ON)
    set(LIB_BUILD_FRAMEWORK_SDK         ON)
    set(LIB_BUILD_IMAGEIO               ON)
    set(LIB_BUILD_ANALYSIS              ON)
    set(LIB_BUILD_COMMON                ON)
    set(LIB_BUILD_CORE                  ON)
    set(LIB_BUILD_GPUDECODE             ON)
elseif (OPTION_BUILD_CMP_SDK)
    set(LIB_BUILD_COMPRESSONATOR_SDK    ON)
    set(LIB_BUILD_FRAMEWORK_SDK         ON)
    set(LIB_BUILD_IMAGEIO               ON)
    set(LIB_BUILD_ANALYSIS              OFF)
    set(LIB_BUILD_COMMON                ON)
    set(LIB_BUILD_CORE                  ON)
    set(LIB_BUILD_GPUDECODE             ON)
elseif (OPTION_BUILD_APPS_CMP_UNITTESTS)
    # cmp_unittests links CMP_Compressonator, CMP_Framework, CMP_Core and the TGA image
    # plugin (tga_rle_security_tests.cpp drives the loader through CMP_LoadTexture), so a
    # unit-test-only configure has to build those libraries as well.
    set(LIB_BUILD_COMPRESSONATOR_SDK    ON)
    set(LIB_BUILD_FRAMEWORK_SDK         ON)
    set(LIB_BUILD_IMAGEIO               ON)
    set(LIB_BUILD_ANALYSIS              OFF)
    set(LIB_BUILD_COMMON                ON)
    set(LIB_BUILD_CORE                  ON)
    set(LIB_BUILD_GPUDECODE             OFF)
endif()

if (OPTION_BUILD_APPS_CMP_VISION)
    set(LIB_BUILD_COMPRESSONATOR_SDK ON)
    set(LIB_BUILD_FRAMEWORK_SDK ON)
endif()

# The following options that have been removed
set(LIB_BUILD_MESHCOMPRESSOR OFF)

set(PROJECT_EXTERNAL_LIBDIR ${PROJECT_SOURCE_DIR}/../common/lib/ext)

# ----------------------------------------------------------------------------
# GPUOpen Source Code build options : These options are been revised for v4.2
# ----------------------------------------------------------------------------
#              OPTION                    DESCRIPTION         REQUESTED VALUE          REQUIREMENT
cmp_option(OPTION_BUILD_KTX2         "Build CLI KTX2 Support"        ON     CMAKE_VERSION VERSION_GREATER "3.14.0")
cmp_option(OPTION_BUILD_EXR          "Build CLI EXR Support"         ON     CMP_HOST_WINDOWS OR CMP_HOST_LINUX)
cmp_option(OPTION_BUILD_GUI          "Build the GUI Application"     ON     CMP_HOST_WINDOWS OR OPTION_BUILD_APPS_CMP_GUI)
cmp_option(OPTION_CMP_DIRECTX        "Use Directx"                   ON     CMP_HOST_WINDOWS)
cmp_option(OPTION_CMP_VULKAN         "Use Vulkan"                    ON     OPTION_ENABLE_ALL_APPS)
cmp_option(OPTION_CMP_OPENGL         "Use OpenGL"                    ON     OPTION_BUILD_APPS_CMP_CLI OR OPTION_BUILD_APPS_CMP_GUI)
cmp_option(OPTION_CMP_QT             "Use Qt for Image Loading"      ON     OPTION_ENABLE_ALL_APPS OR OPTION_BUILD_APPS_CMP_GUI)
cmp_option(OPTION_CMP_OPENCV         "Use OpenCV"                    ON     OPTION_BUILD_APPS_CMP_CLI OR OPTION_BUILD_APPS_CMP_GUI)
cmp_option(OPTION_CMP_GTC            "Use GTC Codec"                 OFF    TRUE) # Internal R&D
cmp_option(OPTION_BUILD_BROTLIG      "Use Brotli-G Codec for CLI"    ON     CMP_HOST_WINDOWS)
cmp_option(OPTION_BUILD_BROTLIG_GUI  "Use Brotli-G Codec for GUI"    OFF    TRUE) # In development for v4.4
# Internal testing application
option(OPTION_BUILD_INTERNAL_CMP_TEST "Build Internal Application: cmp_test" ON)
# No longer supported
cmp_option(OPTION_BUILD_ASTC         "Build ASTC Support"            OFF    FALSE)
cmp_option(OPTION_BUILD_DRACO        "Build CLI Draco Support"       OFF    FALSE)
# this needs to be removed
cmp_option(NO_LEGACY_BEHAVIOR   "Disables any legacy behavior (before CMake migration)"  ON  TRUE)

# -----------------------------------------------------------------------------------
# Code build definitions and features: These options are been revised for v4.2
# -----------------------------------------------------------------------------------
add_compile_definitions(
    USE_ETCPACK                 # Use ETCPack for ETC2 else use CModel code!
    USE_MESH_CLI                # CLI Process Mesh (only support glTF and OBJ files)
    ENABLE_MAKE_COMPATIBLE_API  # Byte<->Float to make all source and dest compatible
    USE_OLD_SWIZZLE             # Remove swizzle flag and abide by CMP_Formats
    USE_QTWEBKIT                # Enable Qt Webengine interfaces for GUI welcome page

    # USE_CONVECTION_KERNELS                      # Test External Lib : ConvectionKernels for quality& performance, to enable use q=0.1 in HPC
    # USE_GPU_PIPELINE_VULKAN                     # Enable to use CMP_Core codecs on a Vulkan Pipeline
    # USE_CPU_PERFORMANCE_COUNTERS                # Enable to use performance monitoring with CPU high precision counters in place of GPU query performance counters
    # TEST_CMP_CORE_DECODER                       # Test CMP_Core Decoders using Compressonator SDK Decoders, 
    # BC7_DEBUG_TO_RESULTS_TXT                    # Send debug info to a results text file
    # DXT5_COMPDEBUGGER                           # Remote connect data to Comp Debugger views
    # BC6H_COMPDEBUGGER                           # Remote connect data to Comp Debugger views
    # BC7_COMPDEBUGGER                            # Remote connect data to Comp Debugger views
    # BC6H_NO_OPTIMIZE_ENDPOINTS                  # Turn off BC6H optimization of endpoints - useful for debugging quantization and mode checking
    # BC6H_DEBUG_TO_RESULTS_TXT                   # Generates a Results.txt file on exe working directory; MultiThreading is turned off for debuging to text file
    # BC6H_DECODE_DEBUG                           # Enables debug info on decoder
    # GT_COMPDEBUGGER                             # Remote connect data to Comp Debugger views
    # USE_MULTIPLE_MESH_DECODE                    # Enable multiple meshes and multiple primitives draco decompression
    # USE_MESH_DRACO_SETTING                      # Expose draco settings for draco mesh compression, 
                                                  # if disabled default setting will be used for mesh compression
    #USE_GLTF2_MIPSET                             # Enable Image Transcode & Compression support for GLTF files using TextureIO
    #USE_FILEIO                                   # Used for debugging code

    #New features under development
    #USE_BASIS                                    # Future release:: Universal format for transcoding codecs
    #USE_CMP_TRANSCODE                            # Future release:: Transcode BASIS/GTC to other compressed formats

    # Possible future releases features
    #SUPPORT_ETC_ALPHA                            # for ATC_RGB output enable A
    #SHOW_PROCESS_MEMORY                          # display available CPU process memory
    #USE_BCN_IMAGE_DEBUG                          # Enables Combobox in Image View for low level BCn based block compression in debug mode
    #USE_CRN                                      # Enabled .crn file output using CRUNCH encoder
    #USE_3DCONVERT                                # Enable 3D model conversion (glTF<->obj) icon
    #ENABLE_USER_ETC2S_FORMATS                    # Enable users to set these formats in CLI and GUI applications
)

if (OPTION_BUILD_BROTLIG)
    add_compile_definitions(
        USE_LOSSLESS_COMPRESSION            # Enable Lossless Compression using BrotliG
        USE_LOSSLESS_COMPRESSION_BINARY     # Change behaviour of lossless compression to load input as binary data for compression
    )

    if (OPTION_BUILD_BROTLIG_GUI)
        add_compile_definitions(
            USE_GUI_LOSSLESS_COMPRESSION            # Enable Lossless Compression using BrotliG
            USE_GUI_LOSSLESS_COMPRESSION_BINARY     # Change behaviour of lossless compression to load input as binary data for compression
        )
    endif()
endif()

if (OPTION_CMP_GTC)
    add_compile_definitions(
        USE_GTC                                      # Patented: LDR Gradient Texture Compressor 
    )
endif()

# Additional options
if (OPTION_BUILD_DRACO)
    add_compile_definitions(
        USE_MESH_DRACO_EXTENSION    # Mesh Compression with Draco support in glTF and OBJ files only
        USE_KTX_EXTENSION           # Add KTX file support
    )
endif()



if (OPTION_USE_QT)
    add_compile_definitions(CMP_USE_QT=1)
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configuration options" FORCE)
set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/build-out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# '_DEBUG' is only defined by Visual Studio. Make sure it's definied everywhere
add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)


# -----------------------------------
# Set or get all package requirements
# -----------------------------------
if (COMPILER_SUPPORTS_CXX17 AND CMP_HOST_WINDOWS)
    set(CMAKE_CXX_STANDARD 17)
    add_compile_definitions(_CMP_CPP17_=1)
else()
    # Linux
    if (COMPILER_SUPPORTS_CXX14)
        set(CMAKE_CXX_STANDARD 14)
        add_compile_definitions(_CMP_CPP14_=1)
    else()
        # UNIX or LINUX
        set(CMAKE_CXX_STANDARD 11)
    endif()
endif()

# Configure Externals, they must be added via 'include' before the applications
include(ExternalProject)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
include(external/cmake/dependencyinfo.cmake)
include(external/cmake/CMakeLists.txt)
include(external/CMakeLists.txt)

if (CMP_HOST_WINDOWS)
    add_compile_definitions(_WIN32)

    set(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL:NO")
else()
    if (CMP_HOST_LINUX)
        #gltf code needs _LINUX define
        add_compile_definitions(_LINUX=1)
    endif()

    # filesystem support for c++14 
    if (COMPILER_SUPPORTS_CXX14)
        set(CMAKE_CXX_STANDARD 14)
        #add_definitions(-std=c++14)
        link_libraries(stdc++fs)
    else()
        if (CMP_HOST_LINUX)
            set(CMAKE_CXX_STANDARD 11)
        endif()
    endif()

    add_definitions(-fPIC -Wno-write-strings)

    # ----------------------------------
    # Package requirements for the Build
    # ----------------------------------
    find_package(Threads REQUIRED)
    set(OpenGL_GL_PREFERENCE "GLVND")

    if (CMP_HOST_LINUX AND OPTION_CMP_QT)
        find_package(Qt5 COMPONENTS Gui Widgets OpenGL Qml WebEngineWidgets Xml REQUIRED HINTS ${QT_DIR})
        if (NOT Qt5Gui_FOUND)
            message(FATAL_ERROR
                "Package Qt5 (Qt5Gui) are required, but not found. "
                "In Unix, run initsetup_unix.sh or sudo apt-get install qtdeclarative5-dev. "
                "If is window, please make sure install qt and "
                "add the bin path to environment PATH.(i.e. C:\\Qt\\5.x.x\\msvcYYYY_64\\bin) "
                "where x is Qt build version and YYYY is MS Compiler release date"
            )
        endif()
    endif()
    
    if (UNIX AND (OPTION_BUILD_EXR OR OPTION_BUILD_DRACO OR OPTION_CMP_OPENCV))
        find_package(PkgConfig REQUIRED)

        if (OPTION_BUILD_EXR)
            pkg_check_modules(OpenEXR OpenEXR)
            if (NOT OpenEXR_FOUND)
                message(WARNING "Package OpenEXR not found. CMP features using OpenEXR will be disabled")
                set(OPTION_BUILD_EXR OFF)
            endif()
        endif()

        if (OPTION_BUILD_DRACO)
            find_package(Draco)
            pkg_check_modules(draco draco)
            if (NOT draco_FOUND)
                message(WARNING "Package Draco not found. CMP features using Draco will be disabled")
                add_definitions(-DCMAKE_DRACO_NOT_FOUND)
                set(OPTION_BUILD_DRACO OFF)
            endif()
        endif()

        if (OPTION_CMP_OPENCV)
            find_package(OpenCV)
            if (NOT OpenCV_FOUND)
                message(WARNING "Package OpenCV not found. CMP features using OpenCV will be disabled"
                    "In Unix, run initsetup_unix.sh or sudo apt-get install "
                    "libopencv-dev to install the libs.")
                set(OPTION_CMP_OPENCV OFF)
            endif()
        endif()
     endif()
endif()

# These are the minimum build includes for both Windows / Linux / Mac application builds
# Libs common to all OS
if (LIB_BUILD_CORE)
    add_subdirectory(cmp_core)
endif()

if (LIB_BUILD_FRAMEWORK_SDK)
    add_subdirectory(cmp_framework)
endif()

if (LIB_BUILD_COMPRESSONATOR_SDK)
    add_subdirectory(cmp_compressonatorlib)
endif()

if (LIB_BUILD_COMMON)
    add_subdirectory(applications/_plugins/common)
endif()

if (LIB_BUILD_IMAGEIO)
    if (OPTION_BUILD_ASTC)
        add_subdirectory(applications/_plugins/cimage/astc)
    endif()
    add_subdirectory(applications/_plugins/cimage/dds)
    if (OPTION_BUILD_EXR) # disable for Mac builds
        add_subdirectory(applications/_plugins/cimage/exr)
    endif()
    if (OPTION_BUILD_KTX2)
        add_subdirectory(applications/_plugins/cimage/ktx)
        if (CMP_HOST_WINDOWS) # disable for Linux & Mac builds
            add_subdirectory(applications/_plugins/cimage/ktx2)
        endif()
    endif()
    if (OPTION_BUILD_BROTLIG)
        if (CMP_HOST_WINDOWS) # disable for Linux & Mac builds
            add_subdirectory(applications/_plugins/cimage/brlg)
            add_subdirectory(applications/_plugins/cimage/binary)
        endif()
    endif()
    add_subdirectory(applications/_plugins/cimage/tga)
endif()

if (LIB_BUILD_ANALYSIS)
    add_subdirectory(applications/_plugins/canalysis)
endif()

# Add CLI common to all OS
if (OPTION_BUILD_APPS_CMP_CLI)
    message("Build CLI setup")
    add_subdirectory(applications/compressonatorcli)
endif()

if (LIB_BUILD_GPUDECODE)
        add_subdirectory(applications/_libs/gpu_decode)
endif()

# GUI - limited to Windows & Linux 
if (NOT CMP_HOST_APPLE)
    if (OPTION_BUILD_APPS_CMP_GUI)
        add_subdirectory(external/imgui)
    endif()
    if (OPTION_BUILD_APPS_CMP_GUI)
        message("Build GUI setup")
        add_subdirectory(applications/compressonatorgui)
        add_subdirectory(applications/_libs/cmp_meshoptimizer)
        add_subdirectory(applications/_plugins/common/gltf)
    endif()
endif()

# Extended Plugins and required libs on Windows
if (CMP_HOST_WINDOWS)

    if (OPTION_BUILD_APPS_CMP_GUI)
        # Windows Based Plugins
        add_subdirectory(applications/_plugins/cgpudecode)
    
        # 3D Mesh processing
        add_subdirectory(applications/_libs/cmp_mesh)
        add_subdirectory(applications/_plugins/cmesh/tootle)
        add_subdirectory(applications/_plugins/cmesh/mesh_optimizer)
        add_subdirectory(applications/_plugins/cmesh/mesh_compressor)
    
        # EncodeWith plugins (DXC, OCL and GPU)
        add_subdirectory(applications/_plugins/cmp_gpu)
        
        # mipmap filters and effects
        add_subdirectory(applications/_plugins/cfilter)
        add_subdirectory(applications/_plugins/cfilter_fx)
    
        # 3D Model Load and Views
        add_subdirectory(applications/_plugins/c3dmodel_loaders)
        add_subdirectory(applications/_plugins/c3dmodel_viewers)

        # Math Lib
        add_subdirectory(applications/_libs/cmp_math)

    endif()

    # Example code
    if (OPTION_BUILD_APPS_CMP_EXAMPLES)
        message("Build examples setup")
        add_subdirectory(examples)
    endif()

    # Unit tests
    if (OPTION_BUILD_APPS_CMP_UNITTESTS)
        message("Build cmp unittests setup")
        # cmp_unittests links CMP_Math, which is otherwise only added for the GUI.
        if (NOT TARGET CMP_Math)
            add_subdirectory(applications/_libs/cmp_math)
        endif()
        add_subdirectory(cmp_unittests)
    endif()

    # CLI + GUI Automation testing
    if (OPTION_BUILD_INTERNAL_CMP_TEST)
        if (EXISTS ${PROJECT_SOURCE_DIR}/compressonator_internal/applications/cmp_test)
            message("Building CMP Test")
            add_subdirectory(compressonator_internal/applications/cmp_test)
        else()
            message("Skipping CMP Test build because the directory could not be found")
        endif()
    endif()

endif()

# Installation

install(DIRECTORY docs/build/html DESTINATION documents/)

# Installed samples are read-only for group/world. GUI/CLI apps probe writability
# (CMP_IsDirWritable) and redirect projects/results to a per-user app-data directory when this
# tree isn't writable, so a shared world-writable install is not needed and would let any local
# user replace sample inputs or pre-place a symlink to overwrite an arbitrary writable target.
set(CMP_SAMPLE_DIR_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CMP_SAMPLE_FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
install(DIRECTORY runtime/images DESTINATION "share/compressonator"
        DIRECTORY_PERMISSIONS ${CMP_SAMPLE_DIR_PERMISSIONS}
        FILE_PERMISSIONS ${CMP_SAMPLE_FILE_PERMISSIONS})
install(DIRECTORY DESTINATION "share/compressonator/projects" DIRECTORY_PERMISSIONS ${CMP_SAMPLE_DIR_PERMISSIONS})
install(FILES runtime/bc7_compression.cprj runtime/bc6h_compression.cprj DESTINATION "share/compressonator/projects"
        PERMISSIONS ${CMP_SAMPLE_FILE_PERMISSIONS})
install(FILES license/license.txt DESTINATION "license/")
install(FILES runtime/qt.conf DESTINATION ".")

if (CMP_HOST_WINDOWS)
    set(WINDOWS_BIN_PREFIX ${CMAKE_BINARY_DIR}/bin/$<$<CONFIG:Debug>:debug>$<$<CONFIG:Release>:release>)

    set(WINDOWS_INSTALL_DLLS dxcompiler.dll dxil.dll glew32.dll ktx.dll opencv_core249.dll opencv_imgproc249.dll opencv_world420.dll Qt5Core.dll Qt5Gui.dll Qt5OpenGL.dll Qt5Widgets.dll)
    foreach(WINDOWS_INSTALL_DLL ${WINDOWS_INSTALL_DLLS})
        install(FILES ${WINDOWS_BIN_PREFIX}/${WINDOWS_INSTALL_DLL} DESTINATION ".")
    endforeach()

    install(
        DIRECTORY ${WINDOWS_BIN_PREFIX}/plugins DESTINATION "."
        PATTERN "./*.dll"
        PATTERN "imageformats" EXCLUDE
        PATTERN "shaders" EXCLUDE
        PATTERN "compute" EXCLUDE
        PATTERN "media" EXCLUDE
    )

    if (OPTION_CMP_QT)
        install(
            DIRECTORY ${QT_LIB_DIR}/plugins/imageformats DESTINATION plugins
            FILES_MATCHING
            PATTERN "qjpeg.dll"
            PATTERN "qtga.dll"
            PATTERN "qtiff.dll"
        )
    endif()

    install(
        DIRECTORY ${WINDOWS_BIN_PREFIX}/plugins/compute DESTINATION "plugins/"
        PATTERN "*.h"
        PATTERN "*.cpp"
        PATTERN "*.hlsl"
        PATTERN "compiled" EXCLUDE
        PATTERN "*.cmd" EXCLUDE
        PATTERN "*.bat" EXCLUDE
    )

    install(FILES ${WINDOWS_BIN_PREFIX}/texture.frag.spv ${WINDOWS_BIN_PREFIX}/texture.vert.spv DESTINATION ".")

elseif (CMP_HOST_LINUX)
    if (OPTION_CMP_QT)
        install(DIRECTORY ${QT_LIB_DIR}/plugins/imageformats DESTINATION plugins)
    endif()
    
    install(PROGRAMS scripts/compressonatorcli DESTINATION ".")
    
    # Qt

    if (OPTION_CMP_QT)
        set(QT_INSTALL_LIBS libQt5Core)

        foreach(QT_INSTALL_LIB ${QT_INSTALL_LIBS})
            file(GLOB QT_LIB_VARIATIONS ${QT_LIB_DIR}/lib/${QT_INSTALL_LIB}.so*)

            foreach(QT_LIB_VARIATION ${QT_LIB_VARIATIONS})
                install(FILES ${QT_LIB_VARIATION} DESTINATION qt)
            endforeach()
        endforeach()
    endif()

    # OpenCV, TBB, and extra

    set(EXTRA_INSTALL_LIBS_DIR /usr/lib/x86_64-linux-gnu)
    set(EXTRA_INSTALL_LIBS 
        libopencv_core
        libopencv_highgui
        libopencv_imgproc
        libtbb
        libHalf
        libIex-2_2
        libIexMath-2_2
        libImath-2_2
        libIlmThread-2_2
        libIlmImf-2_2
    )

    foreach(EXTRA_INSTALL_LIB ${EXTRA_INSTALL_LIBS})
        file(GLOB EXTRA_LIB_VARIATIONS ${EXTRA_INSTALL_LIBS_DIR}/${EXTRA_INSTALL_LIB}.so*)

        foreach(EXTRA_LIB_VARIATION ${EXTRA_LIB_VARIATIONS})
            install(FILES ${EXTRA_LIB_VARIATION} DESTINATION pkglibs)
        endforeach()
    endforeach()
endif()

# CPack set-up

message("Preparing CPack for version ${CMP_VERSION_MAJOR}.${CMP_VERSION_MINOR}.${CMP_VERSION_BUILD_NUMBER}")

# We only generate packages for CompressonatorCLI at the moment
set(CPACK_PACKAGE_NAME "compressonatorcli")

set(CPACK_PACKAGE_VERSION_MAJOR "${CMP_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${CMP_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${CMP_VERSION_BUILD_NUMBER}")

set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_SOURCE_DIR}/scripts/cpack_config.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "compressonatorcli_${CMP_VERSION_MAJOR}.${CMP_VERSION_MINOR}.${CMP_VERSION_BUILD_NUMBER}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/license/license.txt")

set(CPACK_PACKAGE_VENDOR "AMD")
set(CPACK_PACKAGE_DESCRIPTION "Compressonator is a set of tools to allow artists and developers to more easily work with compressed assets and easily visualize the quality impact of various compression technologies.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://gpuopen.com/compressonator/")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AMD")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://gpuopen.com/compressonator/")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)