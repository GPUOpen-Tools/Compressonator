# -------------------------------------------------------------------------
# Vulkan

if (NOT OPTION_CMP_VULKAN)
    message("OPTION_CMP_VULKAN=OFF")
    return()
endif()

add_library(ExtVulkan INTERFACE)

if (NOT DEFINED ENV{VULKAN_DIR})
    message(FATAL_ERROR "A 'VULKAN_DIR' environment variable is required to indicate where the Vulkan SDK is installed.")
endif()

set(VULKAN_SDK_PATH $ENV{VULKAN_DIR})

message(STATUS "++++++++++++++++++ External-VULKAN  ${VULKAN_SDK_PATH}" )

if (NOT EXISTS ${VULKAN_SDK_PATH})
    message(FATAL_ERROR "The given Vulkan SDK path ${VULKAN_SDK_PATH} could not be found")
endif()

# Get Vulkan SDK version from file path

set(VULKAN_VERSION_STRING "1.2.141.2")

string(LENGTH ${VULKAN_SDK_PATH} VULKAN_VERSION_LAST_CHAR)
MATH(EXPR VULKAN_VERSION_LAST_CHAR "${VULKAN_VERSION_LAST_CHAR}-1")

string(FIND ${VULKAN_SDK_PATH} "/" VULKAN_VERSION_INDEX REVERSE)

# try again checking for \ instead of /
if (${VULKAN_VERSION_INDEX} EQUAL -1)
    string(FIND ${VULKAN_SDK_PATH} "\\" VULKAN_VERSION_INDEX REVERSE)
endif()

message("SDK path length: ${VULKAN_VERSION_LAST_CHAR}")
message("Version string index: ${VULKAN_VERSION_INDEX}")

if (${VULKAN_VERSION_INDEX} EQUAL ${VULKAN_VERSION_LAST_CHAR})
    # There was a '/' or '\' at the end, so we remove it
    string(SUBSTRING ${VULKAN_SDK_PATH} 0 ${VULKAN_VERSION_LAST_CHAR} VULKAN_SDK_PATH)

    message("New SDK path after removing final separator: ${VULKAN_SDK_PATH}")

    # then try again to find version string
    string(FIND ${VULKAN_SDK_PATH} "/" VULKAN_VERSION_INDEX REVERSE)

    # try again checking for \ instead of /
    if (${VULKAN_VERSION_INDEX} EQUAL -1)
        string(FIND ${VULKAN_SDK_PATH} "\\" VULKAN_VERSION_INDEX REVERSE)
    endif()
endif()

if (${VULKAN_VERSION_INDEX} EQUAL -1)
    message("No Vulkan SDK version string could be extracted, assuming 1.2.141.2")
else()
    MATH(EXPR VULKAN_VERSION_INDEX "${VULKAN_VERSION_INDEX}+1")
    string(SUBSTRING ${VULKAN_SDK_PATH} ${VULKAN_VERSION_INDEX} -1 VULKAN_VERSION_STRING)
endif()

message(STATUS "Vulkan version string = ${VULKAN_VERSION_STRING}")

string(REPLACE "." ";" VULKAN_VERSION_LIST ${VULKAN_VERSION_STRING})

list(GET VULKAN_VERSION_LIST 0 VULKAN_MAJOR_VERSION)
list(GET VULKAN_VERSION_LIST 1 VULKAN_MINOR_VERSION)
list(GET VULKAN_VERSION_LIST 2 VULKAN_PATCH_VERSION)

#message("VULKAN DEBUG: major version ${VULKAN_MAJOR_VERSION}, minor version ${VULKAN_MINOR_VERSION}, patch version ${VULKAN_PATCH_VERSION}")

if (${VULKAN_MAJOR_VERSION} LESS_EQUAL 1 AND ${VULKAN_MINOR_VERSION} LESS_EQUAL 2 AND ${VULKAN_PATCH_VERSION} LESS_EQUAL 141)
    set(USE_OLD_VULKAN_LIBS ON)
endif()

# glslang merged the HLSL and OGLCompiler libraries into glslang.lib starting with
# Vulkan SDK 1.3.280.0; older SDKs still ship them as separate import libraries.
set(VULKAN_HAS_LEGACY_GLSLANG_LIBS ON)
if (${VULKAN_MAJOR_VERSION} GREATER 1
    OR (${VULKAN_MAJOR_VERSION} EQUAL 1 AND ${VULKAN_MINOR_VERSION} GREATER 3)
    OR (${VULKAN_MAJOR_VERSION} EQUAL 1 AND ${VULKAN_MINOR_VERSION} EQUAL 3 AND ${VULKAN_PATCH_VERSION} GREATER_EQUAL 280))
    set(VULKAN_HAS_LEGACY_GLSLANG_LIBS OFF)
endif()

if (CMP_HOST_APPLE)

    set(MOLTEN_VK_PATHS

        ${VULKAN_SDK_PATH}/macOS/framework
        #${VULKAN_SDK}/macOS/static
        #${VULKAN_SDK}/macOS/dynamic
    )

    find_library(
        MOLTEN_VK_FRAMEWORK

        NAMES MoltenVK
        PATHS ${MOLTEN_VK_PATHS}
        NO_DEFAULT_PATH
    )

    find_library(IO_KIT_FRAMEWORK       IOKit)
    find_library(IO_SURFACE_FRAMEWORK   IOSurface)
    find_library(METAL_FRAMEWORK        Metal)
    find_library(QUARTZ_CORE_FRAMEWORK  QuartzCore)

    set(
        Vulkan_LIBRARIES

        ${IO_KIT_FRAMEWORK}
        ${IO_SURFACE_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${MOLTEN_VK_FRAMEWORK}
        ${QUARTZ_CORE_FRAMEWORK}
    )

    find_path(MVK_INCLUDE_DIR
        NAMES MoltenVK/mvk_vulkan.h
        PATHS ${VULKAN_DIR}/include
    )

    set(
        Vulkan_INCLUDE_DIRS
        ${MVK_INCLUDE_DIR}
        ${MOLTEN_VK_FRAMEWORK}
    )

elseif(CMP_HOST_LINUX)

# More details can be found on this issue: https://vulkan.lunarg.com/issue/view/5ed6eca8e0d2a37f6e19cefc
#    set(RENAME_HACK bash -c "find . -type f -exec sed -i 's,root/sdk-build/${VULKAN_VERSION},${VULKAN_OUTPUT_DIR},' {} \\\;")
#    cmp_execute_process(RENAME_HACK ${VULKAN_OUTPUT_DIR})
#
#    get_filename_component(VULKAN_DIR ${COMPRESSONATOR_PROJECT_DIR}/External/build-out/Linux/vulkan ABSOLUTE)
#    if (NOT EXISTS VULKAN_DIR)
#        file(COPY $ENV{VULKAN_DIR} ${VULKAN_DIR})
#    endif()
#    set(ENV{VULKAN_DIR} ${VULKAN_DIR})
#
#    find_package(Vulkan REQUIRED)

elseif(CMP_HOST_WINDOWS)

    find_package(Vulkan REQUIRED)

    target_compile_definitions(ExtVulkan INTERFACE VK_USE_PLATFORM_WIN32_KHR)

endif()

target_link_libraries(ExtVulkan INTERFACE ${Vulkan_LIBRARIES})
target_include_directories(ExtVulkan INTERFACE
    ${Vulkan_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}/glslang
)

add_library(ExtGlslang INTERFACE)

target_link_libraries(ExtGlslang INTERFACE
    ${VULKAN_SDK_PATH}/lib/glslang$<$<CONFIG:Debug>:d>.lib
    ${VULKAN_SDK_PATH}/lib/SPIRV$<$<CONFIG:Debug>:d>.lib
    ${VULKAN_SDK_PATH}/lib/OSDependent$<$<CONFIG:Debug>:d>.lib
)

if (VULKAN_HAS_LEGACY_GLSLANG_LIBS)
    # Removed in the glslang shipped with Vulkan SDK 1.3.280.0+ (merged into glslang.lib)
    target_link_libraries(ExtGlslang INTERFACE
        ${VULKAN_SDK_PATH}/lib/HLSL$<$<CONFIG:Debug>:d>.lib
        ${VULKAN_SDK_PATH}/lib/OGLCompiler$<$<CONFIG:Debug>:d>.lib
    )
endif()

if (DEFINED USE_OLD_VULKAN_LIBS)
    message("Old version of Vulkan detected, using older libraries")

    # Enable for Vulkan version 1.2.141.2 and earlier
    target_link_libraries(ExtGlslang INTERFACE
        ${VULKAN_SDK_PATH}/lib/SPIRV-Tools.lib
        ${VULKAN_SDK_PATH}/lib/SPIRV-Tools-opt.lib
    )
else()
    message("Newer Vulkan version detected, using newer libraries")

    # Enable for Vulkan version 1.2.148.0 and later
    target_link_libraries(ExtGlslang INTERFACE
        ${VULKAN_SDK_PATH}/lib/MachineIndependent$<$<CONFIG:Debug>:d>.lib
        ${VULKAN_SDK_PATH}/lib/GenericCodeGen$<$<CONFIG:Debug>:d>.lib
        ${VULKAN_SDK_PATH}/lib/SPIRV-Tools$<$<CONFIG:Debug>:d>.lib
        ${VULKAN_SDK_PATH}/lib/SPIRV-Tools-opt$<$<CONFIG:Debug>:d>.lib
    )
endif()