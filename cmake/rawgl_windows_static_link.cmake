set(rawgl_windows_library_prefixes)
if(RAWGL_WINDOWS_DEPS_ROOT)
    list(APPEND rawgl_windows_library_prefixes "${RAWGL_WINDOWS_DEPS_ROOT}")
endif()
foreach(rawgl_windows_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(rawgl_windows_prefix)
        list(APPEND rawgl_windows_library_prefixes "${rawgl_windows_prefix}")
    endif()
endforeach()
list(REMOVE_DUPLICATES rawgl_windows_library_prefixes)

function(rawgl_windows_find_prefixed_library out_var library_name)
    set(rawgl_found_library)
    foreach(rawgl_prefix IN LISTS rawgl_windows_library_prefixes)
        set(rawgl_candidate "${rawgl_prefix}/lib/${library_name}.lib")
        if(EXISTS "${rawgl_candidate}")
            set(rawgl_found_library "${rawgl_candidate}")
            break()
        endif()
    endforeach()
    set(${out_var} "${rawgl_found_library}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_find_prefixed_library_any out_var)
    set(rawgl_found_library)
    foreach(rawgl_library_name ${ARGN})
        rawgl_windows_find_prefixed_library(rawgl_candidate "${rawgl_library_name}")
        if(rawgl_candidate)
            set(rawgl_found_library "${rawgl_candidate}")
            break()
        endif()
    endforeach()
    set(${out_var} "${rawgl_found_library}" PARENT_SCOPE)
endfunction()

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
    rawgl_windows_find_prefixed_library(rawgl_release_path "${release_name}")
    rawgl_windows_find_prefixed_library(rawgl_debug_path "${debug_name}")

    rawgl_windows_append_file_library(${out_var}
        "${rawgl_release_path}"
        "${rawgl_debug_path}")
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

    rawgl_windows_find_prefixed_library(rawgl_release_path "${release_name}")
    rawgl_windows_find_prefixed_library(rawgl_debug_path "${debug_name}")

    if(NOT rawgl_release_path AND NOT rawgl_debug_path)
        return()
    endif()

    rawgl_windows_config_library_expr(rawgl_expr "${rawgl_release_path}" "${rawgl_debug_path}")
    set(rawgl_search_paths)
    foreach(rawgl_prefix IN LISTS rawgl_windows_library_prefixes)
        list(APPEND rawgl_search_paths
            "${rawgl_prefix}/lib/${release_name}.lib")
    endforeach()
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

if(NOT RAWGL_WINDOWS_DNG_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_DNG_LIBRARY_RELEASE
        dng_sdk
        dngsdk
        dng
        dng_sdk_static
        dngsdk_static)
    if(RAWGL_WINDOWS_DNG_LIBRARY_RELEASE)
        set(RAWGL_WINDOWS_DNG_LIBRARY_RELEASE "${RAWGL_WINDOWS_DNG_LIBRARY_RELEASE}" CACHE FILEPATH "Windows release DNG SDK library" FORCE)
    endif()
endif()
if(NOT RAWGL_WINDOWS_DNG_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_DNG_LIBRARY_DEBUG
        dng_sdkd
        dngsdkd
        dngd
        dng_sdk_debug
        dngsdk_debug
        dng_sdk_staticd
        dngsdk_staticd)
    if(RAWGL_WINDOWS_DNG_LIBRARY_DEBUG)
        set(RAWGL_WINDOWS_DNG_LIBRARY_DEBUG "${RAWGL_WINDOWS_DNG_LIBRARY_DEBUG}" CACHE FILEPATH "Windows debug DNG SDK library" FORCE)
    endif()
endif()
if(NOT RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE
        XMPCoreStaticRelease
        XMPCoreStatic
        XMPCore
        xmp_core)
    if(RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE)
        set(RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE "${RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE}" CACHE FILEPATH "Windows release XMP core library" FORCE)
    endif()
endif()
if(NOT RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG
        XMPCoreStaticDebug
        XMPCoreStaticd
        XMPCored
        xmp_cored)
    if(RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG)
        set(RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG "${RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG}" CACHE FILEPATH "Windows debug XMP core library" FORCE)
    endif()
endif()
if(NOT RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE
        XMPFilesStaticRelease
        XMPFilesStatic
        XMPFiles
        xmp_files)
    if(RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE)
        set(RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE "${RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE}" CACHE FILEPATH "Windows release XMP files library" FORCE)
    endif()
endif()
if(NOT RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG
        XMPFilesStaticDebug
        XMPFilesStaticd
        XMPFilesd
        xmp_filesd)
    if(RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG)
        set(RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG "${RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG}" CACHE FILEPATH "Windows debug XMP files library" FORCE)
    endif()
endif()

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
