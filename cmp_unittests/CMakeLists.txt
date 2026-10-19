
add_executable(cmp_unittests)

target_sources(cmp_unittests
    PRIVATE

    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/utilfuncs.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/utilfuncs.cpp

    test_main.cpp
    
    core_tests.h
    core_tests.cpp

    core_simd_tests.cpp
    framework_tests.cpp
    sdk_tests.cpp
    mipmap_tests.cpp
    codecbuffer_tests.cpp
    codec_tests.cpp
    fileio_test.cpp

    blockconstants.h
    bc6h_tests.cpp

    # Security regression tests. These guard the hardening applied to publicly
    # shipped code (see security_tests/docs/), so they must build in the public
    # project rather than only in the internal staged test tree.
    bc1_compress_security_tests.cpp
    codec_thread_lifecycle_tests.cpp
    dds_loader_security_tests.cpp
    float_to_byte_security_tests.cpp
    tga_rle_security_tests.cpp
)

target_include_directories(cmp_unittests
    PUBLIC

    ./
    ${PROJECT_SOURCE_DIR}/cmp_core/shaders
    ${PROJECT_SOURCE_DIR}/cmp_core/source
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/
    ${PROJECT_SOURCE_DIR}/../common/lib/ext/catch2
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/buffer/
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/common/
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/block/
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/etc/
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/etc/etcpack/
    ${PROJECT_SOURCE_DIR}/cmp_framework/common/
    ${PROJECT_SOURCE_DIR}/cmp_framework/common/half
)

target_link_libraries(cmp_unittests 
    CMP_Math
    CMP_Core
    CMP_Framework
    CMP_Compressonator
)

# tga_rle_security_tests.cpp drives the TGA loader through CMP_LoadTexture, so the
# plugin has to be linked into the test binary. The root CMakeLists enables
# LIB_BUILD_IMAGEIO whenever the unit tests are built, so this target always exists here.
target_link_libraries(cmp_unittests Image_TGA)

if (OPTION_BUILD_BROTLIG)
    target_sources(cmp_unittests PRIVATE brlg_data.h)

    target_link_libraries(cmp_unittests ExtBrotlig)
endif()

if(CMP_HOST_WINDOWS)
    include(copyfiles.cmake)
endif()

set_target_properties(cmp_unittests PROPERTIES 
    FOLDER ${PROJECT_FOLDER_APPS}
    OUTPUT_NAME "cmp_unittests" 
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$(Configuration)"
)
