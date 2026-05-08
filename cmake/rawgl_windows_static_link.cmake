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

function(rawgl_windows_find_prefixed_library_ordered out_var)
    set(rawgl_found_library)
    foreach(rawgl_library_name ${ARGN})
        if(NOT rawgl_library_name)
            continue()
        endif()
        string(REGEX REPLACE "\\.lib$" "" rawgl_library_stem "${rawgl_library_name}")
        rawgl_windows_find_prefixed_library(rawgl_candidate "${rawgl_library_stem}")
        if(rawgl_candidate)
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

function(rawgl_windows_find_dual_config_library out_var config_name release_name debug_name)
    set(rawgl_debug_postfix "${RAWGL_WINDOWS_DEBUG_SUFFIX}")
    if(NOT rawgl_debug_postfix)
        set(rawgl_debug_postfix "d")
    endif()

    set(rawgl_candidate_names)
    if(config_name STREQUAL "Debug")
        if(debug_name)
            list(APPEND rawgl_candidate_names
                "${debug_name}"
                "lib${debug_name}")
        endif()
        if(release_name)
            list(APPEND rawgl_candidate_names
                "${release_name}${rawgl_debug_postfix}"
                "lib${release_name}${rawgl_debug_postfix}"
                "${release_name}_${rawgl_debug_postfix}"
                "lib${release_name}_${rawgl_debug_postfix}"
                "${release_name}d"
                "lib${release_name}d"
                "${release_name}_d"
                "lib${release_name}_d"
                "${release_name}"
                "lib${release_name}")
        endif()
    else()
        if(release_name)
            list(APPEND rawgl_candidate_names
                "${release_name}"
                "lib${release_name}")
        endif()
        if(debug_name)
            list(APPEND rawgl_candidate_names
                "${debug_name}"
                "lib${debug_name}")
        endif()
        if(release_name)
            list(APPEND rawgl_candidate_names
                "${release_name}${rawgl_debug_postfix}"
                "lib${release_name}${rawgl_debug_postfix}"
                "${release_name}_${rawgl_debug_postfix}"
                "lib${release_name}_${rawgl_debug_postfix}"
                "${release_name}d"
                "lib${release_name}d"
                "${release_name}_d"
                "lib${release_name}_d")
        endif()
    endif()
    list(REMOVE_DUPLICATES rawgl_candidate_names)

    rawgl_windows_find_prefixed_library_ordered(rawgl_library "${rawgl_candidate_names}")
    set(${out_var} "${rawgl_library}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_append_path_variants list_var path)
    if(NOT path)
        set(${list_var} "${${list_var}}" PARENT_SCOPE)
        return()
    endif()

    list(APPEND ${list_var} "${path}")
    string(REPLACE "\\" "/" rawgl_normalized_path "${path}")
    list(APPEND ${list_var} "${rawgl_normalized_path}")

    if(rawgl_normalized_path MATCHES "^([A-Za-z]):/(.*)$")
        set(rawgl_path_drive "${CMAKE_MATCH_1}")
        set(rawgl_path_tail "${CMAKE_MATCH_2}")
        string(TOUPPER "${rawgl_path_drive}" rawgl_path_drive_upper)
        string(TOLOWER "${rawgl_path_drive}" rawgl_path_drive_lower)
        list(APPEND ${list_var}
            "${rawgl_path_drive_upper}:/${rawgl_path_tail}"
            "${rawgl_path_drive_lower}:/${rawgl_path_tail}")
    endif()

    set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_rewrite_prefixed_library_paths out_var input_value)
    set(rawgl_value "${input_value}")
    set(rawgl_search_value "${input_value}")

    while(rawgl_search_value MATCHES "([A-Za-z]:[/\\\\][^;$<>]+[/\\\\]([^/\\\\;$<>]+\\.lib))")
        set(rawgl_old_path "${CMAKE_MATCH_1}")
        set(rawgl_file_name "${CMAKE_MATCH_2}")
        string(REGEX REPLACE "\\.lib$" "" rawgl_library_stem "${rawgl_file_name}")

        rawgl_windows_find_prefixed_library(rawgl_new_path "${rawgl_library_stem}")
        if(rawgl_new_path AND NOT rawgl_new_path STREQUAL rawgl_old_path)
            string(REPLACE "${rawgl_old_path}" "${rawgl_new_path}" rawgl_value "${rawgl_value}")
        endif()

        string(REPLACE "${rawgl_old_path}" "" rawgl_search_value "${rawgl_search_value}")
    endwhile()

    set(${out_var} "${rawgl_value}" PARENT_SCOPE)
endfunction()

function(rawgl_windows_rewrite_target_library_paths target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(rawgl_aliased_target ${target_name} ALIASED_TARGET)
    if(rawgl_aliased_target)
        set(target_name "${rawgl_aliased_target}")
    endif()

    foreach(rawgl_property IN ITEMS
            INTERFACE_LINK_LIBRARIES
            IMPORTED_LOCATION
            IMPORTED_LOCATION_DEBUG
            IMPORTED_LOCATION_RELEASE
            IMPORTED_IMPLIB
            IMPORTED_IMPLIB_DEBUG
            IMPORTED_IMPLIB_RELEASE)
        get_target_property(rawgl_property_value ${target_name} ${rawgl_property})
        if(NOT rawgl_property_value)
            continue()
        endif()

        set(rawgl_rewritten_value)
        foreach(rawgl_item IN LISTS rawgl_property_value)
            rawgl_windows_rewrite_prefixed_library_paths(rawgl_rewritten_item "${rawgl_item}")
            list(APPEND rawgl_rewritten_value "${rawgl_rewritten_item}")
        endforeach()

        if(rawgl_rewritten_value)
            list(REMOVE_DUPLICATES rawgl_rewritten_value)
            set_target_properties(${target_name} PROPERTIES
                ${rawgl_property} "${rawgl_rewritten_value}")
        endif()
    endforeach()
endfunction()

function(rawgl_windows_set_imported_target_library target_name release_name debug_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(rawgl_aliased_target ${target_name} ALIASED_TARGET)
    if(rawgl_aliased_target)
        set(target_name "${rawgl_aliased_target}")
    endif()

    rawgl_windows_find_dual_config_library(rawgl_release_path Release "${release_name}" "${debug_name}")
    rawgl_windows_find_dual_config_library(rawgl_debug_path Debug "${release_name}" "${debug_name}")

    if(NOT rawgl_release_path AND NOT rawgl_debug_path)
        return()
    endif()

    get_target_property(rawgl_imported_configs ${target_name} IMPORTED_CONFIGURATIONS)
    if(NOT rawgl_imported_configs)
        set(rawgl_imported_configs)
    endif()

    if(rawgl_release_path)
        list(APPEND rawgl_imported_configs Release)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_RELEASE "${rawgl_release_path}")
    endif()

    if(rawgl_debug_path)
        list(APPEND rawgl_imported_configs Debug)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_DEBUG "${rawgl_debug_path}")
    endif()

    if(rawgl_imported_configs)
        list(REMOVE_DUPLICATES rawgl_imported_configs)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_CONFIGURATIONS "${rawgl_imported_configs}")
    endif()

    if(rawgl_release_path)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION "${rawgl_release_path}")
    elseif(rawgl_debug_path)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION "${rawgl_debug_path}")
    endif()

    set_target_properties(${target_name} PROPERTIES
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release)
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
    rawgl_windows_find_dual_config_library(rawgl_release_path Release "${release_name}" "${debug_name}")
    rawgl_windows_find_dual_config_library(rawgl_debug_path Debug "${release_name}" "${debug_name}")

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

    rawgl_windows_find_dual_config_library(rawgl_release_path Release "${release_name}" "${debug_name}")
    rawgl_windows_find_dual_config_library(rawgl_debug_path Debug "${release_name}" "${debug_name}")

    if(NOT rawgl_release_path AND NOT rawgl_debug_path)
        return()
    endif()

    rawgl_windows_config_library_expr(rawgl_expr "${rawgl_release_path}" "${rawgl_debug_path}")
    set(rawgl_search_paths)
    rawgl_windows_append_path_variants(rawgl_search_paths "${rawgl_release_path}")
    rawgl_windows_append_path_variants(rawgl_search_paths "${rawgl_debug_path}")
    foreach(rawgl_prefix IN LISTS rawgl_windows_library_prefixes)
        rawgl_windows_append_path_variants(rawgl_search_paths
            "${rawgl_prefix}/lib/${release_name}.lib")
        rawgl_windows_append_path_variants(rawgl_search_paths
            "${rawgl_prefix}/lib/${debug_name}.lib")
    endforeach()
    foreach(rawgl_rewrite_root IN LISTS RAWGL_WINDOWS_IMPORTED_CONFIG_REWRITE_ROOTS)
        if(rawgl_rewrite_root)
            rawgl_windows_append_path_variants(rawgl_search_paths
                "${rawgl_rewrite_root}/lib/${release_name}.lib")
            rawgl_windows_append_path_variants(rawgl_search_paths
                "${rawgl_rewrite_root}/lib/${debug_name}.lib")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES rawgl_search_paths)

    foreach(rawgl_search_path IN LISTS rawgl_search_paths)
        string(REPLACE "${rawgl_search_path}" "${rawgl_expr}" rawgl_iface "${rawgl_iface}")
    endforeach()

    string(TOLOWER "${release_name}.lib" rawgl_release_file_name)
    string(TOLOWER "${debug_name}.lib" rawgl_debug_file_name)
    set(rawgl_rewritten_iface)
    foreach(rawgl_iface_item IN LISTS rawgl_iface)
        string(TOLOWER "${rawgl_iface_item}" rawgl_iface_item_lower)
        if(rawgl_iface_item_lower MATCHES "(^|[/\\\\])${rawgl_release_file_name}(>|$)"
           OR rawgl_iface_item_lower MATCHES "(^|[/\\\\])${rawgl_debug_file_name}(>|$)")
            list(APPEND rawgl_rewritten_iface ${rawgl_expr})
        else()
            list(APPEND rawgl_rewritten_iface "${rawgl_iface_item}")
        endif()
    endforeach()
    set(rawgl_iface ${rawgl_rewritten_iface})
    list(REMOVE_DUPLICATES rawgl_iface)

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

foreach(rawgl_imported_target IN ITEMS
        OpenImageIO::OpenImageIO
        OpenImageIO::OpenImageIO_Util
        heif
        freetype
        Freetype::Freetype
        harfbuzz::harfbuzz
        HarfBuzz::HarfBuzz
        OpenColorIO::OpenColorIO
        OpenEXR::OpenEXR
        OpenEXR::OpenEXRCore
        Imath::Imath
        PNG::PNG
        JPEG::JPEG
        TIFF::TIFF
        ZLIB::ZLIB
        libjpeg-turbo::jpeg
        libjpeg-turbo::jpeg-static
        WebP::webp
        WebP::webpdemux
        WebP::libwebpmux
        openjp2
        openjph
        Ptex::Ptex_static
        GIF::GIF
        BZip2::BZip2
        Deflate::Deflate
        liblzma::liblzma
        ZSTD::ZSTD
        OpenMeta::openmeta
        OpenMeta::openmeta_static
        pystring::pystring
        yaml-cpp
        yaml-cpp::yaml-cpp
        pugixml::pugixml)
    rawgl_windows_rewrite_target_library_paths(${rawgl_imported_target})
endforeach()

foreach(rawgl_oiio_target IN LISTS RAWGL_OIIO_TARGETS)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} avcodec avcodecd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} avdevice avdeviced)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} avfilter avfilterd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} avformat avformatd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} avutil avutild)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} swscale swscaled)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} swresample swresampled)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} jxl jxld)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} jxl_cms jxl_cmsd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} jxl_extras_codec jxl_extras_codecd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} jxl_threads jxl_threadsd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} raw rawd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} jasper jasperd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} turbojpeg-static turbojpeg-staticd)
    rawgl_windows_patch_imported_interface_library(${rawgl_oiio_target} lcms2 lcms2_staticd)
    rawgl_windows_remove_interface_library(${rawgl_oiio_target} libpng18_static libpng18_staticd)
    rawgl_windows_remove_interface_library(${rawgl_oiio_target} zlibstatic zlibstaticd)
    rawgl_remove_interface_link_libraries(${rawgl_oiio_target}
        "$<LINK_ONLY:PNG::PNG>"
        "$<LINK_ONLY:ZLIB::ZLIB>"
        "$<LINK_ONLY:$<TARGET_NAME_IF_EXISTS:JPEG::JPEG>>"
        "$<LINK_ONLY:$<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>>")
endforeach()

rawgl_windows_set_imported_target_library(pystring::pystring pystring pystringd)
rawgl_windows_set_imported_target_library(Brotli::common brotlicommon brotlicommond)
rawgl_windows_set_imported_target_library(Brotli::decoder brotlidec brotlidecd)
rawgl_windows_set_imported_target_library(Brotli::encoder brotlienc brotliencd)
rawgl_windows_set_imported_target_library(Brotli::brotlicommon brotlicommon brotlicommond)
rawgl_windows_set_imported_target_library(Brotli::brotlidec brotlidec brotlidecd)
rawgl_windows_set_imported_target_library(Brotli::brotlienc brotlienc brotliencd)
rawgl_windows_patch_imported_interface_library(Brotli::decoder brotlicommon brotlicommond)
rawgl_windows_patch_imported_interface_library(Brotli::encoder brotlicommon brotlicommond)

rawgl_windows_patch_imported_interface_library(heif x265-static x265-staticd)
rawgl_windows_patch_imported_interface_library(heif libde265 libde265d)
rawgl_windows_patch_imported_interface_library(heif libkvazaar libkvazaard)
rawgl_windows_patch_imported_interface_library(heif libsharpyuv libsharpyuvd)
rawgl_windows_patch_imported_interface_library(heif brotlidec brotlidecd)
rawgl_windows_patch_imported_interface_library(heif brotlienc brotliencd)

rawgl_windows_remove_interface_library(freetype brotlidec brotlidecd)
rawgl_windows_remove_interface_library(harfbuzz::harfbuzz freetype freetyped)

set(rawgl_windows_oiio_extra_libs)

if(TARGET dng_sdk::dng_sdk)
    list(APPEND rawgl_windows_oiio_extra_libs dng_sdk::dng_sdk)
endif()

rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs avdevice avdeviced)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs avfilter avfilterd)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs swresample swresampled)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs jxl_cms jxl_cmsd)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs hwy hwyd)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs hwy_contrib hwy_contribd)
if(NOT TARGET OpenMeta::openmeta AND NOT TARGET OpenMeta::openmeta_static)
    rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs brotlidec brotlidecd)
endif()
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs brotlienc brotliencd)
rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs brotlicommon brotlicommond)
if(NOT TARGET TBB::tbb)
    rawgl_windows_append_vendor_library(rawgl_windows_oiio_extra_libs tbb12 tbb12_debug)
endif()

if(NOT DNG_SDK_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(DNG_SDK_LIBRARY_RELEASE
        dng_sdk
        dngsdk
        dng
        dng_sdk_static
        dngsdk_static)
    if(DNG_SDK_LIBRARY_RELEASE)
        set(DNG_SDK_LIBRARY_RELEASE "${DNG_SDK_LIBRARY_RELEASE}" CACHE FILEPATH "DNG SDK release library" FORCE)
    endif()
endif()
if(NOT DNG_SDK_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(DNG_SDK_LIBRARY_DEBUG
        dng_sdkd
        dngsdkd
        dngd
        dng_sdk_debug
        dngsdk_debug
        dng_sdk_staticd
        dngsdk_staticd)
    if(DNG_SDK_LIBRARY_DEBUG)
        set(DNG_SDK_LIBRARY_DEBUG "${DNG_SDK_LIBRARY_DEBUG}" CACHE FILEPATH "DNG SDK debug library" FORCE)
    endif()
endif()
if(NOT XMP_CORE_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(XMP_CORE_LIBRARY_RELEASE
        XMPCoreStaticRelease
        XMPCoreStatic
        XMPCore
        xmp_core)
    if(XMP_CORE_LIBRARY_RELEASE)
        set(XMP_CORE_LIBRARY_RELEASE "${XMP_CORE_LIBRARY_RELEASE}" CACHE FILEPATH "XMP Core release library" FORCE)
    endif()
endif()
if(NOT XMP_CORE_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(XMP_CORE_LIBRARY_DEBUG
        XMPCoreStaticDebug
        XMPCoreStaticd
        XMPCored
        xmp_cored)
    if(XMP_CORE_LIBRARY_DEBUG)
        set(XMP_CORE_LIBRARY_DEBUG "${XMP_CORE_LIBRARY_DEBUG}" CACHE FILEPATH "XMP Core debug library" FORCE)
    endif()
endif()
if(NOT XMP_FILES_LIBRARY_RELEASE)
    rawgl_windows_find_prefixed_library_any(XMP_FILES_LIBRARY_RELEASE
        XMPFilesStaticRelease
        XMPFilesStatic
        XMPFiles
        xmp_files)
    if(XMP_FILES_LIBRARY_RELEASE)
        set(XMP_FILES_LIBRARY_RELEASE "${XMP_FILES_LIBRARY_RELEASE}" CACHE FILEPATH "XMP Files release library" FORCE)
    endif()
endif()
if(NOT XMP_FILES_LIBRARY_DEBUG)
    rawgl_windows_find_prefixed_library_any(XMP_FILES_LIBRARY_DEBUG
        XMPFilesStaticDebug
        XMPFilesStaticd
        XMPFilesd
        xmp_filesd)
    if(XMP_FILES_LIBRARY_DEBUG)
        set(XMP_FILES_LIBRARY_DEBUG "${XMP_FILES_LIBRARY_DEBUG}" CACHE FILEPATH "XMP Files debug library" FORCE)
    endif()
endif()

rawgl_windows_append_file_library(rawgl_windows_oiio_extra_libs
    "${DNG_SDK_LIBRARY_RELEASE}"
    "${DNG_SDK_LIBRARY_DEBUG}")
rawgl_windows_append_file_library(rawgl_windows_oiio_extra_libs
    "${XMP_CORE_LIBRARY_RELEASE}"
    "${XMP_CORE_LIBRARY_DEBUG}")
rawgl_windows_append_file_library(rawgl_windows_oiio_extra_libs
    "${XMP_FILES_LIBRARY_RELEASE}"
    "${XMP_FILES_LIBRARY_DEBUG}")

if(rawgl_windows_oiio_extra_libs)
    list(REMOVE_DUPLICATES rawgl_windows_oiio_extra_libs)
    if(TARGET OpenImageIO::OpenImageIO)
        rawgl_append_unique_interface_link_libraries(OpenImageIO::OpenImageIO
            ${rawgl_windows_oiio_extra_libs})
    endif()
endif()

set(rawgl_windows_system_libs
    Authz
    bcrypt
    crypt32
    Dwmapi
    dxgi
    dxguid
    d3d11
    d3d12
    D3d9
    Imm32
    mfplat
    mfuuid
    mpr
    ncrypt
    opengl32
    psapi
    RuntimeObject
    secur32
    Setupapi
    Shcore
    strmiids
    userenv
    UxTheme
    Version
    WindowsApp
    windowscodecs
    Winmm
    Ws2_32
    Wtsapi32)

set(RAWGL_EXTRA_WINDOWS_LIBS
    ${rawgl_windows_system_libs})
