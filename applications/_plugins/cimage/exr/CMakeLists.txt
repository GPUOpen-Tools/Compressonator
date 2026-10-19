cmake_minimum_required(VERSION 3.10)

add_library(Image_EXR STATIC "")

target_sources(Image_EXR
    PRIVATE
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/tc_pluginapi.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/tc_plugininternal.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/tc_plugininternal.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/utilfuncs.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/utilfuncs.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmp_fileio.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cmp_fileio.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cexr.h
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/cexr.cpp
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common/namespacealias.h
    ./exr.h
    ./exr.cpp
    )

target_include_directories(Image_EXR
    PRIVATE
    ${PROJECT_SOURCE_DIR}/cmp_compressonatorlib
    ${PROJECT_SOURCE_DIR}/cmp_framework/common/half
    ${PROJECT_SOURCE_DIR}/applications/_plugins/common
    ${OpenEXR_INCLUDE_DIRS}
    )

if (APPLE)
    target_include_directories(Image_EXR
                               PRIVATE
                               /usr/local/include/OpenEXR/)
endif()

if (UNIX)
    target_compile_definitions(Image_EXR PRIVATE _LINUX)
endif()

# cmp_fileio.cpp uses token/security descriptor APIs
if (WIN32)
    target_link_libraries(Image_EXR PUBLIC advapi32)
endif()

set_target_properties(Image_EXR PROPERTIES FOLDER ${PROJECT_FOLDER_SDK_PLUGIN_STATIC})
