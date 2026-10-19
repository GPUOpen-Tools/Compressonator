add_library(CMP_Framework SHARED)

if(CMP_HOST_WINDOWS)
    target_compile_definitions(CMP_Framework PUBLIC
        -DCMP_USE_XMMINTRIN
        )
endif()

file(GLOB_RECURSE HALF_SRC
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/half/*.h
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/half/*.cpp
)

file(GLOB_RECURSE DDS_SRC
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/cimage/dds/*.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/cimage/dds/*.cpp
)

file(GLOB_RECURSE HPC_SRC
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_encode/hpc/*.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_encode/hpc/*.cpp
)

file(GLOB_RECURSE CMP_FRAMEWORK_SRC
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/*.h
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/*.cpp
)

file(GLOB_RECURSE CMP_FRAMEWORK_COMMON_SRC
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/*.h
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/*.cpp
)

target_sources(CMP_Framework
        PRIVATE
        ${HALF_SRC}
        ${DDS_SRC}
        ${HPC_SRC}
        ${CMP_FRAMEWORK_SRC}
        ${CMP_FRAMEWORK_COMMON_SRC}
        
        # CMP_Math
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cmp_math_common.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cmp_math_common.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cpu_extensions.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cpu_extensions.cpp
        
        # CMP_COMMON
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/atiformats.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/atiformats.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/pluginbase.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/plugininterface.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/pluginmanager.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/pluginmanager.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/cpu_timing.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/cpu_timing.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/tc_pluginapi.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/tc_plugininternal.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/tc_plugininternal.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/utilfuncs.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/utilfuncs.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/cmp_fileio.h 
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/cmp_fileio.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/format_conversion.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/format_conversion.cpp

        # CMP_SDK Lossy Compression
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bcn.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bcn.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc1/bc1.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc1/bc1.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc2/bc2.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc2/bc2.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc3/bc3.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc3/bc3.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc4/bc4.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc4/bc4.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc5/bc5.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc5/bc5.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc6/bc6h.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc6/bc6h.cpp
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc7/bc7.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/bc7/bc7.cpp

        # External Sources
        ${COMPRESSONATOR_ROOT_PATH}/external/stb/stb_image.h
        ${COMPRESSONATOR_ROOT_PATH}/external/stb/stb_image_write.h

        )


if (OPTION_CMP_GTC)
target_sources(CMP_Framework
        PRIVATE
        # Extra 
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/gtc/gtc.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/gtc/gtc.cpp
        )
endif()

if (OPTION_BUILD_BROTLIG)
    target_sources(CMP_Framework
        PRIVATE
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/brlg/codec_brlg.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/brlg/codec_brlg.cpp
    )

    target_link_libraries(CMP_Framework
        PRIVATE
        ExtBrotlig
    )
endif()

if (OPTION_CMP_APC)
target_sources(CMP_Framework
        PRIVATE
        # Extra 
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/apc/apc.h
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk/apc/apc.cpp
        )
endif()

target_include_directories(CMP_Framework
        PRIVATE
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/half
        ${COMPRESSONATOR_ROOT_PATH}/cmp_framework/common/
        ${COMPRESSONATOR_ROOT_PATH}/cmp_compressonatorlib
        ${COMPRESSONATOR_ROOT_PATH}/cmp_compressonatorlib/buffer
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/ccmp_sdk
        ${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common
        #${COMPRESSONATOR_ROOT_PATH}/applications/_plugins/common/gltf
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/gpu_decode
        ${COMPRESSONATOR_ROOT_PATH}/external/stb
        )

if (UNIX)
    target_compile_definitions(CMP_Framework PRIVATE _LINUX)
endif()

set_target_properties(CMP_Framework PROPERTIES 
        FOLDER ${PROJECT_FOLDER_SDK_LIBS}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(CMP_Framework PRIVATE CMP_Core)

# cmp_fileio.cpp uses token/security descriptor APIs
if (WIN32)
    target_link_libraries(CMP_Framework PRIVATE advapi32)
endif()