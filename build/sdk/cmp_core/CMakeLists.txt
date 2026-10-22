# foreach(lib_type SHARED)

    add_library(CMP_Core SHARED)

    target_sources(CMP_Core
                PRIVATE
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc1_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc1_common_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc1_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc2_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc2_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc3_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc3_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc4_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc4_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc5_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc5_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc6_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc6_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc7_encode_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc7_common_encoder.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bc7_encode_kernel.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bcn_common_kernel.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/bcn_common_api.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/common_def.h
                    
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/cmp_core.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/cmp_core.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/cmp_math_vec4.h
                    ${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/cmp_math_func.h

                    ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cpu_extensions.cpp
                    ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/cmp_math_common.cpp
                )

    if (OPTION_CMP_GTC)
    target_sources(
        CMP_Core
        PRIVATE
        ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/gtc_encode_kernel.h
        ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/gtc_encode_kernel.cpp
    )
    endif()

    if (OPTION_BUILD_BROTLIG)
        target_sources(
            CMP_Core
            PRIVATE
            ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/brlg_encode_kernel.h
            ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders/brlg_encode_kernel.cpp
        )
    endif()

    target_include_directories(
        CMP_Core
        PUBLIC
        ${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders
        ${COMPRESSONATOR_ROOT_PATH}/cmp_core/source
        ${COMPRESSONATOR_ROOT_PATH}/applications/_libs/cmp_math/
    )
    if (UNIX)
        target_compile_definitions(CMP_Core PRIVATE _LINUX ASPM_GPU)
    endif()

    set_target_properties(CMP_Core PROPERTIES 
        FOLDER ${PROJECT_FOLDER_SDK_LIBS}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

# endforeach()

# Core SIMD options

# Metallicafan212: Actually check for support before linking it
# 				   This fixes ARM builds
if(AMD_COMPRESSONATOR_AMD64 OR AMD_COMPRESSONATOR_X86)
	# SSE
	add_library(CMP_Core_SSE OBJECT)
	target_sources(
		CMP_Core_SSE 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/core_simd_sse.cpp
	)

	target_include_directories(
		CMP_Core_SSE 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders
	)

	if (UNIX)
		target_compile_options(CMP_Core_SSE PRIVATE -march=nehalem)
	endif()

	set_target_properties(CMP_Core_SSE PROPERTIES 
		FOLDER ${PROJECT_FOLDER_SDK_LIBS}
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	)

	# AVX
	add_library(CMP_Core_AVX OBJECT)
	target_sources(
		CMP_Core_AVX 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/core_simd_avx.cpp
	)
	target_include_directories(
		CMP_Core_AVX 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders
	)

	if (WIN32)
		target_compile_options(CMP_Core_AVX PRIVATE /arch:AVX2)
	else()
		target_compile_options(CMP_Core_AVX PRIVATE -march=haswell)
	endif()

	set_target_properties(CMP_Core_AVX PROPERTIES 
		FOLDER ${PROJECT_FOLDER_SDK_LIBS}
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	)

	# AVX-512
	add_library(CMP_Core_AVX512 OBJECT)
	target_sources(
		CMP_Core_AVX512 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source/core_simd_avx512.cpp
	)
	target_include_directories(
		CMP_Core_AVX512 
		PRIVATE 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/source 
		${COMPRESSONATOR_ROOT_PATH}/cmp_core/shaders
	)

	if (WIN32)
		target_compile_options(CMP_Core_AVX512 PRIVATE /arch:AVX-512)
	else()
		target_compile_options(CMP_Core_AVX512 PRIVATE -march=knl)
	endif()

	set_target_properties(CMP_Core_AVX512 PROPERTIES 
		FOLDER ${PROJECT_FOLDER_SDK_LIBS}
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	)

	# Link SIMD libraries to CMP_Core
	target_link_libraries(CMP_Core PRIVATE CMP_Core_SSE CMP_Core_AVX CMP_Core_AVX512)
endif()