
macro(cmp_gui_copy_to_output source dest)
    cmp_copy_to_output(CompressonatorGUI-bin ${source} ${dest})
endmacro()

macro(cmp_gui_update_rpath target rpath)
    if (CMP_HOST_LINUX)
        add_custom_command(TARGET CompressonatorGUI-bin POST_BUILD
            # COMMENT "Copying ${source} -> ${dest}"
            VERBATIM
            COMMAND patchelf --set-rpath "${rpath}" ${target}
        )
    endif()
endmacro()

if (CMP_HOST_APPLE)
    # Following: https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/BundleTypes/BundleTypes.html#//apple_ref/doc/uid/10000123i-CH101-SW1
    set(ASSETS_PATH $<TARGET_BUNDLE_CONTENT_DIR:CompressonatorGUI-bin>/Resources)
    set(DYLIBS_PATH $<TARGET_BUNDLE_CONTENT_DIR:CompressonatorGUI-bin>/Frameworks)
    set(PLUGINS_PATH $<TARGET_BUNDLE_CONTENT_DIR:CompressonatorGUI-bin>/PlugIns)
else()
    set(ASSETS_PATH $<TARGET_FILE_DIR:CompressonatorGUI-bin>)
    set(DYLIBS_PATH $<TARGET_FILE_DIR:CompressonatorGUI-bin>)
    set(PLUGINS_PATH $<TARGET_FILE_DIR:CompressonatorGUI-bin>/plugins)
endif()


cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/runtime/bc7_compression.cprj ${ASSETS_PATH}/projects/bc7_compression.cprj)
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/runtime/bc6h_compression.cprj ${ASSETS_PATH}/projects/bc6h_compression.cprj)

# Image sample assets
file(GLOB_RECURSE RUNTIME_IMAGE_ASSETS  ${PROJECT_SOURCE_DIR}/runtime/images/*)
#message(STATUS "************** RUNTIME_IMAGE_ASSETS [${RUNTIME_IMAGE_ASSETS}]" )
foreach(rsc ${RUNTIME_IMAGE_ASSETS})
     file(RELATIVE_PATH asset ${PROJECT_SOURCE_DIR}/runtime/images ${rsc})
     #message(STATUS "************** COPY SHADERS [${rsc} to ${ASSETS_PATH}/${asset}]" )
     cmp_gui_copy_to_output(${rsc} ${ASSETS_PATH}/images/${asset})
 endforeach()


file(GLOB_RECURSE GUI_ASSETS WelcomePage/*)
foreach(gui_asset ${GUI_ASSETS})
    file(RELATIVE_PATH asset ${CMAKE_CURRENT_LIST_DIR} ${gui_asset})
    cmp_gui_copy_to_output(${gui_asset} ${ASSETS_PATH}/${asset})
endforeach()


cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/runtime/qt.conf ${ASSETS_PATH}/qt.conf)

cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/applications/_plugins/cgpudecode/vulkan/vk_computeshader/texture.vert.spv ${ASSETS_PATH}/texture.vert.spv)
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/applications/_plugins/cgpudecode/vulkan/vk_computeshader/texture.frag.spv ${ASSETS_PATH}/texture.frag.spv)

get_property(QT_LIB_DIR GLOBAL PROPERTY QT_LIB_DIR)

if (CMP_HOST_WINDOWS)
    cmp_gui_copy_to_output(${QT_LIB_DIR}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll ${PLUGINS_PATH}/platforms/qwindows$<$<CONFIG:Debug>:d>.dll)
    cmp_gui_copy_to_output(${QT_LIB_DIR}/plugins/imageformats/qtga$<$<CONFIG:Debug>:d>.dll  ${PLUGINS_PATH}/imageformats/qtga$<$<CONFIG:Debug>:d>.dll)
    cmp_gui_copy_to_output(${QT_LIB_DIR}/plugins/imageformats/qtiff$<$<CONFIG:Debug>:d>.dll ${PLUGINS_PATH}/imageformats/qtiff$<$<CONFIG:Debug>:d>.dll)
    cmp_gui_copy_to_output(${QT_LIB_DIR}/plugins/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll ${PLUGINS_PATH}/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll)
else ()
    # Copy the platform libraries into the bundle
    file(GLOB_RECURSE QT_PLATFORM_LIBS ${QT_LIB_DIR}/plugins/platforms/*)
    foreach(rsc ${QT_PLATFORM_LIBS})
        file(RELATIVE_PATH asset ${QT_LIB_DIR}/plugins ${rsc})
        cmp_gui_copy_to_output(${rsc} ${PLUGINS_PATH}/${asset})
        cmp_gui_update_rpath(${PLUGINS_PATH}/${asset} "\$ORIGIN/../../")
    endforeach()

    file(GLOB_RECURSE QT_IMAGEFORMAT_PLUGINS ${QT_LIB_DIR}/plugins/imageformats/*)
    foreach(rsc ${QT_IMAGEFORMAT_PLUGINS})
        file(RELATIVE_PATH asset ${QT_LIB_DIR}/plugins ${rsc})
        cmp_gui_copy_to_output(${rsc} ${PLUGINS_PATH}/${asset})
        cmp_gui_update_rpath(${PLUGINS_PATH}/${asset} "\$ORIGIN/../../")
    endforeach()

    if (CMP_HOST_LINUX)
        cmp_gui_copy_to_output(${QT_LIB_DIR}/libexec/QtWebEngineProcess ${DYLIBS_PATH}/QtWebEngineProcess)
        cmp_gui_update_rpath(${DYLIBS_PATH}/QtWebEngineProcess "\$ORIGIN")
        cmp_gui_copy_to_output(${QT_LIB_DIR}/lib/libQt5XcbQpa.so.5 ${DYLIBS_PATH}/libQt5XcbQpa.so.5)
        cmp_gui_copy_to_output(${QT_LIB_DIR}/lib/libQt5DBus.so.5 ${DYLIBS_PATH}/libQt5DBus.so.5)
    endif()
endif()

if (NOT CMP_HOST_APPLE)
    cmp_gui_copy_to_output(${QT_LIB_DIR}/translations/qtwebengine_locales/en-US.pak ${ASSETS_PATH}/translations/qtwebengine_locales/en-US.pak)
endif()

cmp_gui_copy_to_output(${QT_LIB_DIR}/translations/qtwebengine_en.qm ${ASSETS_PATH}/translations/qtwebengine_en.qm)

#Generate a list of files contained in Qt resources folder into QT_RESOURCES
#On Windows and macOS, globbing is case-insensitive
#message(STATUS "************** QT_LIB_DIR [${QT_LIB_DIR}]" )
file(GLOB_RECURSE QT_RESOURCES ${QT_LIB_DIR}/resources/*)
#message(STATUS "************** QT_RESOURCES [${QT_RESOURCES}]" )
foreach(rsc ${QT_RESOURCES})
     file(RELATIVE_PATH asset ${QT_LIB_DIR}/resources ${rsc})
     #message(STATUS "************** COPY RESOURCES [${rsc} to ${ASSETS_PATH}/${asset}]" )
     cmp_gui_copy_to_output(${rsc} ${ASSETS_PATH}/resources/${asset})
 endforeach()

include(${PROJECT_SOURCE_DIR}/applications/_plugins/c3dmodel_viewers/gltf_dx12_ex/copyfiles.cmake)
gltf_dx12_copy_files(CompressonatorGUI-bin ${PLUGINS_PATH})

include(${PROJECT_SOURCE_DIR}/applications/_plugins/c3dmodel_viewers/vulkan/copyfiles.cmake)
gltf_vulkan_copy_files(CompressonatorGUI-bin ${PLUGINS_PATH})

#copy glew32.dll
cmp_gui_copy_to_output(${ExtGLEW_BIN_PATH}/glew32.dll ${ASSETS_PATH}/glew32.dll)

# OpenCV dll
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/../common/lib/ext/opencv/2.49/x64/VS2015/bin/$<$<CONFIG:Debug>:debug>$<$<CONFIG:Release>:release>/opencv_core249$<$<CONFIG:Debug>:d>.dll ${ASSETS_PATH}/opencv_core249$<$<CONFIG:Debug>:d>.dll)
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/../common/lib/ext/opencv/2.49/x64/VS2015/bin/$<$<CONFIG:Debug>:debug>$<$<CONFIG:Release>:release>/opencv_imgproc249$<$<CONFIG:Debug>:d>.dll ${ASSETS_PATH}/opencv_imgproc249$<$<CONFIG:Debug>:d>.dll)

# New OpenCV support
if (CMP_HOST_WINDOWS)
    cmp_gui_copy_to_output(C:/opencv/build/x64/vc15/bin/opencv_world420$<$<CONFIG:Debug>:d>.dll ${ASSETS_PATH}/opencv_world420$<$<CONFIG:Debug>:d>.dll)
endif()


#KTX2 Features dll
if (OPTION_BUILD_KTX2)
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/../common/lib/ext/ktx/build/$<$<CONFIG:Debug>:debug>$<$<CONFIG:Release>:release>/ktx.dll ${ASSETS_PATH}/ktx.dll)
else()
# Use a null dll so that installers can build
cmp_gui_copy_to_output(${PROJECT_SOURCE_DIR}/runtime/ktx.dll ${ASSETS_PATH}/ktx.dll)  
endif()

# GPU Shaders
file(GLOB_RECURSE GPUCOMPUTE_SHADERS ${PROJECT_SOURCE_DIR}/cmp_core/shaders/*)
#message(STATUS "************** GPUCOMPUTE_SHADERS [${GPUCOMPUTE_SHADERS}]" )
foreach(rsc ${GPUCOMPUTE_SHADERS})
     file(RELATIVE_PATH asset ${PROJECT_SOURCE_DIR}/cmp_core/shaders ${rsc})
     #message(STATUS "************** COPY SHADERS [${rsc} to ${ASSETS_PATH}/${asset}]" )
     cmp_gui_copy_to_output(${rsc} ${ASSETS_PATH}/plugins/compute/${asset})
 endforeach()

if (OPTION_BUILD_BROTLIG)
    set(BRLG_SHADER_SRC ${BROTLIG_ROOT_PATH}/src/decoder/BrotliGCompute.hlsl)
    message(STATUS "************** BROTLIG SHADER ${BRLG_SHADER_SRC}")
    cmp_gui_copy_to_output(${BRLG_SHADER_SRC} ${ASSETS_PATH}/plugins/compute/)

    set(BRLG_DX_CMP ${BROTLIG_ROOT_PATH}/sample/external/dxc_2021_12_08/bin/x64/dxcompiler.dll)
    cmp_gui_copy_to_output(${BRLG_DX_CMP} ${ASSETS_PATH}/)

    set(BRLG_DX_IL ${BROTLIG_ROOT_PATH}/sample/external/dxc_2021_12_08/bin/x64/dxil.dll)
    cmp_gui_copy_to_output(${BRLG_DX_IL} ${ASSETS_PATH}/)
endif()

executable_post_build_dylibs(CompressonatorGUI-bin ${DYLIBS_PATH})
