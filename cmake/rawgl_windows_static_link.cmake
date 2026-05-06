function(rawgl_windows_append_file_library out_var release_path debug_path)
    set(rawgl_items ${${out_var}})

    if(NOT release_path AND NOT debug_path)
        set(${out_var} "${rawgl_items}" PARENT_SCOPE)
        return()
    endif()

    if(release_path AND NOT EXISTS "${release_path}")
        set(release_path "")
    endif()
    if(debug_path AND NOT EXISTS "${debug_path}")
        set(debug_path "")
    endif()

    if(NOT release_path AND NOT debug_path)
        set(${out_var} "${rawgl_items}" PARENT_SCOPE)
        return()
    endif()

    rawgl_windows_config_library_expr(rawgl_expr "${release_path}" "${debug_path}")
    list(APPEND rawgl_items ${rawgl_expr})
    set(${out_var} "${rawgl_items}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_append_vendor_library out_var release_name debug_name)
    if(NOT RAWGL_WINDOWS_DEPS_ROOT)
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
        return()
    endif()

    rawgl_windows_append_file_library(${out_var}
        "${RAWGL_WINDOWS_DEPS_ROOT}/lib/${release_name}.lib"
        "${RAWGL_WINDOWS_DEPS_ROOT}/lib/${debug_name}.lib")
    set(${out_var} "${${out_var}}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_patch_imported_interface_library target_name release_name debug_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(rawgl_iface ${target_name} INTERFACE_LINK_LIBRARIES)
    if(NOT rawgl_iface)
        return()
    endif()

    set(rawgl_release_path "${RAWGL_WINDOWS_DEPS_ROOT}/lib/${release_name}.lib")
    set(rawgl_debug_path "${RAWGL_WINDOWS_DEPS_ROOT}/lib/${debug_name}.lib")

    if(NOT EXISTS "${rawgl_release_path}")
        set(rawgl_release_path "")
    endif()
    if(NOT EXISTS "${rawgl_debug_path}")
        set(rawgl_debug_path "")
    endif()

    if(NOT rawgl_release_path AND NOT rawgl_debug_path)
        return()
    endif()

    rawgl_windows_config_library_expr(rawgl_expr "${rawgl_release_path}" "${rawgl_debug_path}")
    set(rawgl_search_paths)
    if(RAWGL_WINDOWS_DEPS_ROOT)
        list(APPEND rawgl_search_paths
            "${RAWGL_WINDOWS_DEPS_ROOT}/lib/${release_name}.lib")
    endif()
    foreach(rawgl_rewrite_root IN LISTS RAWGL_WINDOWS_IMPORTED_CONFIG_REWRITE_ROOTS)
        if(rawgl_rewrite_root)
            list(APPEND rawgl_search_paths
                "${rawgl_rewrite_root}/lib/${release_name}.lib")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES rawgl_search_paths)

    foreach(rawgl_search_path IN LISTS rawgl_search_paths)
        string(REPLACE "${rawgl_search_path}" "${rawgl_expr}" rawgl_iface "${rawgl_iface}")
    endforeach()

    set_target_properties(${target_name} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${rawgl_iface}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release)
endfunction()

foreach(rawgl_oiio_target IN LISTS RAWGL_OIIO_TARGETS)
    if(TARGET ${rawgl_oiio_target})
        set_target_properties(${rawgl_oiio_target} PROPERTIES
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release)
    endif()
endforeach()

rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO avcodec avcodecd)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO avformat avformatd)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO avutil avutild)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO swscale swscaled)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO jxl jxld)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO jxl_threads jxl_threadsd)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO raw rawd)
rawgl_windows_patch_imported_interface_library(OpenImageIO::OpenImageIO lcms2 lcms2d)

rawgl_windows_patch_imported_interface_library(heif x265-static x265-staticd)
rawgl_windows_patch_imported_interface_library(heif libde265 libde265d)
rawgl_windows_patch_imported_interface_library(heif libkvazaar libkvazaard)
rawgl_windows_patch_imported_interface_library(heif libsharpyuv libsharpyuvd)
rawgl_windows_patch_imported_interface_library(heif brotlidec brotlidecd)
rawgl_windows_patch_imported_interface_library(heif brotlienc brotliencd)

rawgl_windows_patch_imported_interface_library(freetype brotlidec brotlidecd)

set(rawgl_windows_vendor_libs)

rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs OpenColorIO OpenColorIO_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs Imath-3_2 Imath-3_2_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs Iex-3_4 Iex-3_4_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs IlmThread-3_4 IlmThread-3_4_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs OpenEXR-3_4 OpenEXR-3_4_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs OpenEXRCore-3_4 OpenEXRCore-3_4_d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs Ptex Ptexd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs avcodec avcodecd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs avformat avformatd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs avutil avutild)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs swscale swscaled)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs swresample swresampled)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs bz2_static bz2_staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs gif gifd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs heif heifd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libpng18_static libpng18_staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs zlibstatic zlibstaticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jpeg-static jpeg-staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jpeg jpegd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs uhdr uhdrd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs openjp2 openjp2d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs openjph.0.21 openjph.0.21d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jxl jxld)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jxl_cms jxl_cmsd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jxl_threads jxl_threadsd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs hwy hwyd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs hwy_contrib hwy_contribd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs raw rawd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs jasper jasperd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs turbojpeg-static turbojpeg-staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs lcms2 lcms2d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs tiff tiffd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libwebp libwebpd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libwebpdemux libwebpdemuxd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libwebpmux libwebpmuxd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs pugixml pugixmld)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs freetype freetyped)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs harfbuzz harfbuzzd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs deflatestatic deflatestaticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs lzma lzmad)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs zstd_static zstd_staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs tbb12 tbb12_debug)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libexpatMT libexpatdMT)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs pystring pystringd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs yaml-cpp yaml-cppd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs minizip-ng minizip-ngd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs x265-static x265-staticd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libde265 libde265d)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs aom aomd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libkvazaar libkvazaard)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs libsharpyuv libsharpyuvd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs brotlidec brotlidecd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs brotlienc brotliencd)
rawgl_windows_append_vendor_library(rawgl_windows_vendor_libs brotlicommon brotlicommond)

rawgl_windows_append_file_library(rawgl_windows_vendor_libs
    "${RAWGL_WINDOWS_DNG_LIBRARY_RELEASE}"
    "${RAWGL_WINDOWS_DNG_LIBRARY_DEBUG}")
rawgl_windows_append_file_library(rawgl_windows_vendor_libs
    "${RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE}"
    "${RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG}")
rawgl_windows_append_file_library(rawgl_windows_vendor_libs
    "${RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE}"
    "${RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG}")

set(rawgl_windows_system_libs
    advapi32
    Authz
    bcrypt
    comdlg32
    crypt32
    Dwmapi
    dxgi
    dxguid
    d3d11
    d3d12
    D3d9
    gdi32
    Imm32
    kernel32
    mfplat
    mfuuid
    mpr
    ncrypt
    Ole32
    ole32
    Oleaut32
    opengl32
    psapi
    RuntimeObject
    secur32
    Setupapi
    Shcore
    shell32
    Shlwapi
    strmiids
    user32
    userenv
    Uuid
    UxTheme
    Version
    WindowsApp
    windowscodecs
    Winmm
    winspool
    Ws2_32
    Wtsapi32)

set(RAWGL_EXTRA_WINDOWS_LIBS
    ${rawgl_windows_vendor_libs}
    ${rawgl_windows_system_libs})
