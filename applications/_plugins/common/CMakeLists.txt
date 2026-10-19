
set (PLUGIN_COMMON_SRC "")
set (PLUGIN_COMMON_H   "")

if (OPTION_CMP_QT)
    set(PLUGIN_COMMON_SRC_QT
        mipstoqimage.cpp
    )
    set(PLUGIN_COMMON_INC_QT
        mipstoqimage.h
    )
else()
    set(PLUGIN_COMMON_SRC_QT)
    set(PLUGIN_COMMON_INC_QT)
endif()


list(APPEND PLUGIN_COMMON_SRC
    atiformats.cpp
    cmdline.cpp
    cmp_fileio.cpp
    misc.cpp
    modeldata.cpp
    pluginmanager.cpp
    cpu_timing.cpp
    ssim.cpp
    tc_plugininternal.cpp
    textureio.cpp
    userinterface.cpp
    utilfuncs.cpp
)

list(APPEND PLUGIN_COMMON_H
    atiformats.h
    cmdline.h
    cmp_fileio.h
    common_kerneldef.h
    crc32.h
    hpc_compress.h
    misc.h
    modeldata.h
    namespacealias.h
    pluginbase.h
    plugininterface.h
    pluginmanager.h
    cpu_timing.h
    scoped_temp_files.h
    ssim.h
    tc_pluginapi.h
    tc_plugininternal.h
    testreport.h
    texture.h
    textureio.h
    userinterface.h
    utilfuncs.h
    vectypes.h
)

if (OPTION_BUILD_EXR)
    list(APPEND PLUGIN_COMMON_SRC cexr.cpp)
    list(APPEND PLUGIN_COMMON_H   cexr.h)
endif()

add_library(CMP_Common STATIC 
    ${PLUGIN_COMMON_SRC} 
    ${PLUGIN_COMMON_SRC_QT}
    ${PLUGIN_COMMON_H}
    ${PLUGIN_COMMON_INC_QT}
    )

target_include_directories(CMP_Common PRIVATE
  .
  ${PROJECT_SOURCE_DIR}/cmp_framework
  ${PROJECT_SOURCE_DIR}/cmp_framework/common/half
  ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib
  ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/common
  ${PROJECT_SOURCE_DIR}/applications/_plugins/common/gltf
  ${PROJECT_SOURCE_DIR}/cmp_core/shaders
  ${PROJECT_SOURCE_DIR}/cmp_core/source
  ${PROJECT_SOURCE_DIR}/applications/_plugins/cimage
  ${PROJECT_SOURCE_DIR}/applications/_plugins/cmesh
  ${PROJECT_SOURCE_DIR}/applications/_plugins/cmesh/mesh_optimizer
  ${PROJECT_SOURCE_DIR}/applications/_libs/gpu_decode
  ${PROJECT_SOURCE_DIR}/applications/_libs/cmp_math
  ${PROJECT_SOURCE_DIR}/applications/_libs/cmp_meshoptimizer
  ${PROJECT_SOURCE_DIR}/external/stb/
  ${PROJECT_SOURCE_DIR}/../common/lib/ext/rapidxml
  ${PROJECT_SOURCE_DIR}/../common/lib/ext/glm
  ${OpenCV_INCLUDE_DIRS}
  ${OpenEXR_INCLUDE_DIRS}
)

if (OPTION_CMP_QT)
    target_link_libraries(CMP_Common Qt5::Widgets)
endif()

# cmp_fileio.cpp uses token/security descriptor APIs
if (WIN32)
    target_link_libraries(CMP_Common advapi32)
endif()

if (OPTION_BUILD_BROTLIG)
    target_include_directories(CMP_Common PUBLIC ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib/brotlig)

    # should there be an error if OPTION_CMP_DIRECTX isn't ON?
    target_link_libraries(CMP_Common
        ExtBrotlig
        dxcompiler.lib
        dxgi.lib
        d3d12.lib
        dxguid.lib
    )
endif()

set_target_properties(CMP_Common PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_LIBS})

