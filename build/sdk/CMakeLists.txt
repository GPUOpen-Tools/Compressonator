cmake_minimum_required(VERSION 3.15)

# Derive the version from cmp_compressonatorlib/version.h so the standalone SDK build
# cannot drift from the main build and the release scripts (see scripts/get_version.py).
#
# A version component of 0 is legitimate (a 5.0 release), and CMake treats the string "0" as
# false, so every test below compares against the empty string instead of using truthiness.
if ("${CMP_VERSION_MAJOR}" STREQUAL "" OR "${CMP_VERSION_MINOR}" STREQUAL "")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../cmp_compressonatorlib/version.h" _cmp_version_header)
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

project(CompressonatorSDK VERSION ${CMP_VERSION_MAJOR}.${CMP_VERSION_MINOR})

set(PROJECT_FOLDER_SDK "SDK")
set(PROJECT_FOLDER_SDK_LIBS "${PROJECT_FOLDER_SDK}/Libraries")
set(PROJECT_FOLDER_SDK_EXTERNAL "${PROJECT_FOLDER_SDK}/External Libraries")
set(PROJECT_FOLDER_SDK_PLUGIN_DYNAMIC "${PROJECT_FOLDER_SDK}/Plugins/Dynamic")
set(PROJECT_FOLDER_SDK_PLUGIN_STATIC "${PROJECT_FOLDER_SDK}/Plugins/Static")

message(PLATFORM "${CMAKE_GENERATOR_PLATFORM}")

# Host platform detection
set(CMP_HOST_APPLE OFF)
set(CMP_HOST_LINUX OFF)
set(CMP_HOST_WINDOWS OFF)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMP_HOST_APPLE ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(CMP_HOST_LINUX ON)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(CMP_HOST_WINDOWS ON)
else()
    message(FATAL_ERROR "Unknown platform")
endif()

include(ExternalProject)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#----------------------------------------
# Check compiler feature support levels
# for Apple (Mac) default to C++11
#---------------------------------------
if (CMP_HOST_LINUX OR CMP_HOST_WINDOWS)
    foreach(i ${CMAKE_CXX_COMPILE_FEATURES})
        if("${i}" STREQUAL "cxx_std_11")
            set(COMPILER_SUPPORTS_CXX11 ON)
            message("Complier Supports ${i}")
        endif()
        if("${i}" STREQUAL "cxx_std_14")
            set(COMPILER_SUPPORTS_CXX14 ON)
            message("Complier Supports ${i}")
        endif()
        if("${i}" STREQUAL "cxx_std_17")
            set(COMPILER_SUPPORTS_CXX17 ON)
            message("Complier Supports ${i}")
        endif()
    endforeach()
else()
    message("UNIX Build: CHECK_CXX_COMPILER_FLAG defaulted to -std=c++11")
    set(COMPILER_SUPPORTS_CXX11 ON)
endif()

set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    "Debug_DLL"
    "Debug_MD"
    "Debug_MD_DLL"
    "Release_DLL"
    "Release_MD"
    "Release_MD_DLL"
    CACHE STRING "" FORCE
)

if(MSVC)
    # remove default flags provided with CMake for MSVC
    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_DEBUG_DLL "")
    set(CMAKE_CXX_FLAGS_DEBUG_MD "")
    set(CMAKE_CXX_FLAGS_DEBUG_MD_DLL "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_RELEASE_DLL "")
    set(CMAKE_CXX_FLAGS_RELEASE_MD "")
    set(CMAKE_CXX_FLAGS_RELEASE_MD_DLL "")

    # Setup required variables for custom build types
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG_DLL "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG_DLL "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG_DLL "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG_DLL "${CMAKE_STATIC_LINKER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG_MD "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG_MD "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG_MD "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG_MD "${CMAKE_STATIC_LINKER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG_MD_DLL "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG_MD_DLL "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG_MD_DLL "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG_MD_DLL "${CMAKE_STATIC_LINKER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE_DLL "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_DLL "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE_DLL "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE_DLL "${CMAKE_STATIC_LINKER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE_MD "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_MD "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE_MD "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE_MD "${CMAKE_STATIC_LINKER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE_MD_DLL "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE_MD_DLL "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE_MD_DLL "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE_MD_DLL "${CMAKE_STATIC_LINKER_FLAGS}")
endif()

add_compile_definitions(
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Debug_DLL>:_DEBUG>
    $<$<CONFIG:Debug_MD>:_DEBUG>
    $<$<CONFIG:Debug_MD_DLL>:_DEBUG>
)

if (CMP_HOST_WINDOWS)
    add_compile_options(
        $<$<CONFIG:Debug>:/MTd>
        $<$<CONFIG:Debug_DLL>:/MTd>
        $<$<CONFIG:Debug_MD>:/MDd>
        $<$<CONFIG:Debug_MD_DLL>:/MDd>
        $<$<CONFIG:Release>:/MT>
        $<$<CONFIG:Release_DLL>:/MT>
        $<$<CONFIG:Release_MD>:/MD>
        $<$<CONFIG:Release_MD_DLL>:/MD>
    )
endif()

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

if (CMP_HOST_WINDOWS)
    add_compile_definitions(_WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL:NO")
else()
    if (CMP_HOST_LINUX)
        #gltf code needs _LINUX define
        add_compile_definitions(_LINUX=1)
    endif()

    # filesystem support for c++14 
    if (COMPILER_SUPPORTS_CXX14)
        add_definitions(-std=c++14)
        link_libraries(stdc++fs)
    else()
        if (CMP_HOST_LINUX)
            add_definitions(-std=c++11)
        endif()
    endif()
endif()

# Helper function for setting persistent CMake options
macro(cmp_option OPTION_NAME DESCRIPTION EXPRESSION)
    set(expression ${ARGV})
    list(REMOVE_AT expression 0)
    list(REMOVE_AT expression 0)

    if (${expression})
        set(${OPTION_NAME} ON CACHE BOOL ${DESCRIPTION} FORCE)
        add_compile_definitions(${OPTION_NAME}=1)
    else()
        set(${OPTION_NAME} OFF CACHE BOOL ${DESCRIPTION} FORCE)
        add_compile_definitions(${OPTION_NAME}=0)
    endif()

    message(STATUS "${OPTION_NAME} ${${OPTION_NAME}}")
endmacro()

# option(OPTION_BUILD_SHARED_LIBS "Build the shared library" OFF)
# message(STATUS "OPTION_BUILD_SHARED_LIBS ${OPTION_BUILD_SHARED_LIBS}")

cmp_option(OPTION_BUILD_BROTLIG   "Use Brotli-G Codec for Commandline" OFF)
cmp_option(OPTION_CMP_GTC       "Use GTC Codec" OFF) # Internal R&D
cmp_option(OPTION_BUILD_ASTC    "Build ASTC Support" OFF)
cmp_option(OPTION_CMP_APC    "APC" OFF)

if (OPTION_BUILD_BROTLIG)
    add_compile_definitions(
        USE_LOSSLESS_COMPRESSION            # Enable Lossless Compression using BrotliG
        USE_LOSSLESS_COMPRESSION_BINARY     # Change behaviour of lossless compression to load input as binary data for compression
    )
endif()

set(COMPRESSONATOR_ROOT_PATH ${PROJECT_SOURCE_DIR}/../..)

add_subdirectory(external)

add_subdirectory(cmp_core)

add_subdirectory(cmp_compressonatorlib)

add_subdirectory(cmp_framework)

add_subdirectory(directx)

add_subdirectory(opencl)