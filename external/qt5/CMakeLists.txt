# CMakeCustomTargets CMakeLists.txt

message(STATUS "++++++++++++++++++ External-QT " $ENV{QT_DIR})

if (NOT OPTION_CMP_QT)
    message("OPTION_CMP_QT=OFF")
    return()
endif()

if (NOT DEFINED ENV{QT_DIR})
    message(FATAL_ERROR "A 'QT_DIR' environment variable is required to indicate the location where Qt5 is installed.")
endif()

# We set Qt5_DIR because it is needed for find_package to work, but we want to keep the consistency of the user environment variables
set(ENV{Qt5_DIR} $ENV{QT_DIR})

find_package(Qt5 COMPONENTS REQUIRED Core PATHS $ENV{QT_DIR})

get_target_property(QtCore_location Qt5::Core LOCATION)

set(QT_LIB_DIR ${_qt5Core_install_prefix})

message("Qt install library directory set to " ${QT_LIB_DIR})

# ====================================================
# QT_LIB_DIR
# ====================================================
set_property(GLOBAL PROPERTY QT_LIB_DIR "${QT_LIB_DIR}")
list(APPEND CMAKE_FIND_ROOT_PATH ${QT_LIB_DIR})
set(PATH_SUFFIXES lib plugins/imageformats)

set(QT_FIND_ROOT_PATH
    ${CMAKE_FIND_ROOT_PATH}
    ${QT_LIB_DIR}
)

# ====================================================
#                       Q T 5
# ====================================================

find_library(LIB_PNG png png.a libpng.a libqtpng.a libqtlibpng.a qtlibpng.lib
    PATHS ${QT_FIND_ROOT_PATH}
    PATH_SUFFIXES ${PATH_SUFFIXES}
)

if (${LIB_PNG} STREQUAL "LIB_PNG-NOTFOUND")
    message(FATAL_ERROR "png library not found")
endif()

set(CMP_QT_COMPONENTS Core Widgets)

if (OPTION_BUILD_APPS_CMP_GUI)
    list(APPEND CMP_QT_COMPONENTS Gui Qml QuickWidgets WebEngineWidgets OpenGL Xml)
endif()

find_package(Qt5 REQUIRED QUIET
    COMPONENTS ${CMP_QT_COMPONENTS}
    PATHS ${QT_FIND_ROOT_PATH}
    HINTS ${QT_DIR}
)

add_library(ExtQt5 INTERFACE)

target_link_libraries(ExtQt5
    INTERFACE
        Qt5::Core
        ${LIB_PNG}
)

if (OPTION_BUILD_APPS_CMP_GUI)
    target_link_libraries(ExtQt5 INTERFACE Qt5::Gui)
endif()

if (CMP_HOST_APPLE)
    # find_package(LibLZMA REQUIRED QUIET)
    target_link_libraries(ExtQt5 INTERFACE
        # LibLZMA::LibLZMA
        "-framework Cocoa"
    )
endif()

add_library(ExtQt5Widgets INTERFACE)
target_link_libraries(ExtQt5Widgets
    INTERFACE
        Qt5::Widgets
)

if (OPTION_BUILD_APPS_CMP_GUI)
    add_library(ExtQt5OpenGL INTERFACE)
    target_link_libraries(ExtQt5OpenGL
        INTERFACE
        Qt5::OpenGL
    )

    add_library(ExtQt5ForGUI INTERFACE)
    target_link_libraries(ExtQt5ForGUI
        INTERFACE
        ExtQt5
        Qt5::Qml
        Qt5::QuickWidgets
        Qt5::WebEngineWidgets
        Qt5::Xml
)
endif()