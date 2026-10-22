cmake_minimum_required(VERSION 3.10)

add_library(CMP_Core STATIC)

target_sources(CMP_Core
    PRIVATE
    shaders/bc1_encode_kernel.h
    shaders/bc1_common_kernel.h
    shaders/bc1_encode_kernel.cpp
    shaders/bc2_encode_kernel.h
    shaders/bc2_encode_kernel.cpp
    shaders/bc3_encode_kernel.h
    shaders/bc3_encode_kernel.cpp
    shaders/bc4_encode_kernel.h
    shaders/bc4_encode_kernel.cpp
    shaders/bc5_encode_kernel.h
    shaders/bc5_encode_kernel.cpp
    shaders/bc6_encode_kernel.h
    shaders/bc6_encode_kernel.cpp
    shaders/bc7_encode_kernel.h
    shaders/bc7_common_encoder.h
    shaders/bc7_encode_kernel.cpp
    shaders/bcn_common_kernel.h
    shaders/bcn_common_api.h
    shaders/common_def.h

    source/cmp_core.h
    source/cmp_core.cpp
    source/cmp_math_vec4.h
    source/cmp_math_func.h

    ../applications/_libs/cmp_math/cpu_extensions.cpp
    ../applications/_libs/cmp_math/cmp_math_common.cpp
)

if (OPTION_CMP_GTC)
    target_sources(
        CMP_Core
        PRIVATE
        shaders/gtc_encode_kernel.h
        shaders/gtc_encode_kernel.cpp
    )
endif()

if (OPTION_BUILD_BROTLIG)
    target_sources(
        CMP_Core
        PRIVATE
        shaders/brlg_encode_kernel.h
        shaders/brlg_encode_kernel.cpp
    )
endif()

target_include_directories(CMP_Core
    PUBLIC
    shaders
    source
    ../applications/_libs/cmp_math/
)

if (UNIX)
    target_compile_definitions(CMP_Core PRIVATE _LINUX ASPM_GPU)
endif()

set_target_properties(CMP_Core PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_LIBS})

# Core SIMD options

# Metallicafan212: Actually check for support before linking it
# 				   This fixes ARM builds
if(AMD_COMPRESSONATOR_AMD64 OR AMD_COMPRESSONATOR_X86)
	# SSE
	add_library(CMP_Core_SSE STATIC)
	target_sources(CMP_Core_SSE PRIVATE source/core_simd_sse.cpp)
	target_include_directories(CMP_Core_SSE PRIVATE source shaders)

	if (UNIX)
		target_compile_options(CMP_Core_SSE PRIVATE -march=nehalem)
	endif()

	set_target_properties(CMP_Core_SSE PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_LIBS})

	# AVX
	add_library(CMP_Core_AVX STATIC)
	target_sources(CMP_Core_AVX PRIVATE source/core_simd_avx.cpp)
	target_include_directories(CMP_Core_AVX PRIVATE source shaders)

	if (WIN32)
		target_compile_options(CMP_Core_AVX PRIVATE /arch:AVX2)
	else()
		target_compile_options(CMP_Core_AVX PRIVATE -march=haswell)
	endif()

	set_target_properties(CMP_Core_AVX PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_LIBS})

	# AVX-512
	add_library(CMP_Core_AVX512 STATIC)
	target_sources(CMP_Core_AVX512 PRIVATE source/core_simd_avx512.cpp)
	target_include_directories(CMP_Core_AVX512 PRIVATE source shaders)

	if (WIN32)
		target_compile_options(CMP_Core_AVX512 PRIVATE /arch:AVX-512)
	else()
		target_compile_options(CMP_Core_AVX512 PRIVATE -march=knl)
	endif()

	set_target_properties(CMP_Core_AVX512 PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_LIBS})

	# Link SIMD libraries to CMP_Core
	target_link_libraries(CMP_Core PRIVATE CMP_Core_SSE CMP_Core_AVX CMP_Core_AVX512)
endif()