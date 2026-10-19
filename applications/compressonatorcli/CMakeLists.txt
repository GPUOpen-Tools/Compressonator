cmake_minimum_required(VERSION 3.10)

if(CMP_HOST_WINDOWS)
    # this must be before add_executable
    link_directories(
        ${PROJECT_EXTERNAL_LIBDIR}/zlib/zlib-1.2.10/VS2015/x64/lib
        ${PROJECT_EXTERNAL_LIBDIR}/glew/1.9.0/lib/x64 
        ${OpenCV_LIB_DIRS}
        ${PROJECT_EXTERNAL_LIBDIR}/opencv/2.49/x64/VS2015/lib
    )

    if (OPTION_BUILD_EXR)
        link_directories(
            ${PROJECT_EXTERNAL_LIBDIR}/openexr/ilmbase-2.2.0/VS2015/x64/lib
            ${PROJECT_EXTERNAL_LIBDIR}/openexr/openexr-2.2.0/VS2015/x64/lib
        )
    endif()

    # this must be before add_executable
    if (OPTION_CMP_DIRECTX)
        link_directories(
            ${PROJECT_EXTERNAL_LIBDIR}/directxtex/DirectXTex-jun2020b/DirectXTex/Bin/Desktop_2019/x64
        )
    endif()
endif()

add_executable(CompressonatorCLI-bin "")


# allow const char* -> char*
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(CompressonatorCLI-bin 
        PRIVATE
        "-Wno-c++11-compat-deprecated-writable-strings"
    )
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(CompressonatorCLI-bin 
        PRIVATE
        "-Wno-write-strings"
    )
endif()

target_compile_definitions(CompressonatorCLI-bin PUBLIC
    -DUSE_WITH_COMMANDLINE_TOOL
#    -DUSE_CMP_SDK
#    -DUSE_AMD_PNG
)

file(GLOB_RECURSE RESOURCES
    resources/*
)

if (OPTION_CMP_QT)
    set (CLI_QTFILES
         ${PROJECT_SOURCE_DIR}/applications/_plugins/common/mipstoqimage.cpp
         ${PROJECT_SOURCE_DIR}/applications/_plugins/common/mipstoqimage.h
    )
else()
    set (CLI_QTFILES)
endif()

target_sources(CompressonatorCLI-bin
    PRIVATE
    ${RESOURCES}
    source/compressonatorcli.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/atiformats.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/atiformats.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmdline.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmdline.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmp_fileio.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmp_fileio.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/modeldata.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/modeldata.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/plugininterface.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/pluginmanager.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/pluginmanager.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cpu_timing.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cpu_timing.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/textureio.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/textureio.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/gltf/tiny_gltf2.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/gltf/tiny_gltf2_utils.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/gltf/tiny_gltf2_utils.cpp
    ${CLI_QTFILES}
)

target_include_directories(CompressonatorCLI-bin
    PRIVATE
    Source
    ${PROJECT_SOURCE_DIR}/applications/common
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/json
    ${PROJECT_SOURCE_DIR}/applications/_plugins/cmesh/mesh_optimizer
    ${PROJECT_SOURCE_DIR}/applications/_plugins/cimage
    ${PROJECT_SOURCE_DIR}/applications/_libs/gpu_decode
    ${PROJECT_SOURCE_DIR}/applications/_libs/cmp_meshoptimizer
    ${PROJECT_SOURCE_DIR}/applications/_libs/cmp_math
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib
    ${PROJECT_SOURCE_DIR}/cmp_framework/
    ${PROJECT_SOURCE_DIR}/cmp_framework/common/half
    ${PROJECT_SOURCE_DIR}/cmp_core/shaders
    ${PROJECT_SOURCE_DIR}/cmp_core/source
    ${PROJECT_SOURCE_DIR}/external/stb/
    ${PROJECT_SOURCE_DIR}/../common/lib/ext/glm
    ${OpenEXR_INCLUDE_DIRS}
)

if (OPTION_CMP_QT)
    target_include_directories(CompressonatorCLI-bin
        PRIVATE
        ${Qt5Gui_INCLUDE_DIRS}
    )
endif()

if (OPTION_BUILD_EXR)
    if (UNIX)
        target_include_directories(CompressonatorCLI-bin
            PRIVATE
            /usr/local/include/OpenEXR/
        )
    endif()
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (UNIX)
    target_compile_definitions(CompressonatorCLI-bin PRIVATE _LINUX)
endif()

set (CMP_LIBS "")
list(APPEND CMP_LIBS
    CMP_Compressonator
    CMP_Framework
    Image_Analysis
    Image_TGA
)

if (OPTION_BUILD_ASTC)
    list(APPEND CMP_LIBS Image_ASTC)
endif()

if (OPTION_BUILD_KTX2)
    list(APPEND CMP_LIBS Image_KTX)

    if (CMP_HOST_WINDOWS)
        list(APPEND CMP_LIBS Image_KTX2)
    endif()
endif()

if (OPTION_BUILD_EXR)
    list(APPEND CMP_LIBS Image_EXR)
endif()

if (OPTION_BUILD_BROTLIG)
    list(APPEND CMP_LIBS
        Image_BRLG
        Image_BINARY
    )
endif()

if (UNIX)
    if(NOT APPLE)
        list(APPEND CMP_LIBS
            Threads::Threads
            ${OpenEXR_LIBRARIES}
        )
    else()
        list(APPEND CMP_LIBS "/usr/lib/libz.dylib")
        list(APPEND CMP_LIBS "/usr/local/lib/libHalf.dylib")
        list(APPEND CMP_LIBS "/usr/local/lib/libImath.dylib")
        list(APPEND CMP_LIBS "/usr/local/lib/libIlmThread.dylib")
        list(APPEND CMP_LIBS "/usr/local/lib/libIex.dylib")
        list(APPEND CMP_LIBS "/usr/local/lib/libIlmImf.dylib")
    endif()
else()
        if (OPTION_CMP_QT)
            list(APPEND 
                CMP_LIBS
                CMP_GpuDecode
                ExtQt5 
                ExtQt5Widgets
            )
        else()
            list(APPEND 
                CMP_LIBS
                CMP_GpuDecode
            )
        endif()
endif()

list(APPEND CMP_LIBS ${OpenCV_LIBRARIES})

if (OPTION_CMP_QT)
    target_link_libraries(CompressonatorCLI-bin
        ${CMP_LIBS}
        Qt5::Gui
    )
else()
    target_link_libraries(CompressonatorCLI-bin
        ${CMP_LIBS}
    )
endif()

add_dependencies(CompressonatorCLI-bin 
    CMP_Compressonator
    CMP_Framework
    Image_Analysis
    Image_TGA
)

if (OPTION_BUILD_KTX2)
    add_dependencies(CompressonatorCLI-bin Image_KTX)
endif()

if (OPTION_BUILD_EXR)
    add_dependencies(CompressonatorCLI-bin Image_EXR)
endif()

if(CMP_HOST_WINDOWS)
    include(copyfiles.cmake)
    set_target_properties(CompressonatorCLI-bin PROPERTIES 
        WIN32_EXECUTABLE OFF             #Build an executable with a WinMain entry point on windows.
        VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:CompressonatorCLI-bin>
    )
    # cmp_fileio.cpp uses token/security descriptor APIs
    target_link_libraries(CompressonatorCLI-bin advapi32)
    # Enable runtime DLL security check for ALL DLLs (including Qt5, OpenCV, glew)
    if(OPTION_CMP_DLL_SECURITY_CHECK)
        target_compile_definitions(CompressonatorCLI-bin PRIVATE DLL_SECURITY_CHECK_ALL_DLLS)
        target_link_libraries(CompressonatorCLI-bin wintrust crypt32)
    endif()
elseif(CMP_HOST_APPLE)
    set_target_properties(CompressonatorCLI-bin PROPERTIES
        LINK_FLAGS "-Wl,-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks,-v"
    )
endif()

if(CMP_HOST_WINDOWS)
    if (OPTION_CMP_QT)
        set_target_properties(CompressonatorCLI-bin PROPERTIES 
            FOLDER ${PROJECT_FOLDER_APPS}
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$(Configuration)"
            AUTORCC ON
            AUTOMOC ON
            OUTPUT_NAME "compressonatorcli"
        )
    else()
        set_target_properties(CompressonatorCLI-bin PROPERTIES 
            FOLDER ${PROJECT_FOLDER_APPS}
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$(Configuration)"
            OUTPUT_NAME "compressonatorcli"
        )
    endif()
else()
    if (OPTION_CMP_QT)
        set_target_properties(CompressonatorCLI-bin PROPERTIES 
            FOLDER ${PROJECT_FOLDER_APPS}
            AUTORCC ON
            AUTOMOC ON
            OUTPUT_NAME "compressonatorcli-bin"
        )
    else()
        set_target_properties(CompressonatorCLI-bin PROPERTIES 
            FOLDER ${PROJECT_FOLDER_APPS}
            OUTPUT_NAME "compressonatorcli-bin"
        )
    endif()
endif()

# Installation

install(TARGETS CompressonatorCLI-bin DESTINATION ".")