set(GLEW_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)

find_package(OpenGL REQUIRED)
if(RAWGL_WINDOWS_PREFIX_ONLY_FIND)
    set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF CACHE BOOL "Do not use PATH/system environment for RawGL Windows dependency discovery." FORCE)
    set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF CACHE BOOL "Do not use CMake system prefixes for RawGL Windows dependency discovery." FORCE)
endif()
find_path(RAWGL_ZLIB_INCLUDE_DIR
    NAMES zlib.h)
find_library(RAWGL_ZLIB_LIBRARY_RELEASE
    NAMES zlibstatic zlib z)
find_library(RAWGL_ZLIB_LIBRARY_DEBUG
    NAMES zlibstaticd zlibd zd z_d)
rawgl_add_windows_imported_library(ZLIB::ZLIBSTATIC
    INCLUDE_DIR "${RAWGL_ZLIB_INCLUDE_DIR}"
    RELEASE "${RAWGL_ZLIB_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_ZLIB_LIBRARY_DEBUG}")
rawgl_add_windows_imported_library(ZLIB::ZLIB
    INCLUDE_DIR "${RAWGL_ZLIB_INCLUDE_DIR}"
    RELEASE "${RAWGL_ZLIB_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_ZLIB_LIBRARY_DEBUG}")
if(TARGET ZLIB::ZLIB AND NOT TARGET ZLIB::ZLIBSTATIC)
    rawgl_add_imported_interface_alias(ZLIB::ZLIBSTATIC ZLIB::ZLIB)
endif()
if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB MODULE QUIET)
endif()
if(TARGET ZLIB::ZLIB AND NOT TARGET ZLIB::ZLIBSTATIC)
    rawgl_add_imported_interface_alias(ZLIB::ZLIBSTATIC ZLIB::ZLIB)
endif()

if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ZLIB::ZLIB)
    rawgl_add_windows_imported_library(ZLIB::ZLIB
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zlibstatic.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zlibstaticd.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ZLIB::ZLIBSTATIC)
    rawgl_add_windows_imported_library(ZLIB::ZLIBSTATIC
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zlibstatic.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zlibstaticd.lib")
endif()
if(TARGET ZLIB::ZLIB)
    set(ZLIB_FOUND TRUE)
    if(RAWGL_WINDOWS_DEPS_ROOT AND NOT ZLIB_INCLUDE_DIR)
        set(ZLIB_INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include")
        set(ZLIB_INCLUDE_DIRS "${RAWGL_WINDOWS_DEPS_ROOT}/include")
    endif()
    set(ZLIB_LIBRARIES ZLIB::ZLIB)
endif()

if(RAWGL_WINDOWS_DEPS_ROOT)
    set(rawgl_png_library_release "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libpng18_static.lib")
    set(rawgl_png_library_debug "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libpng18_staticd.lib")
    if(EXISTS "${rawgl_png_library_release}" OR EXISTS "${rawgl_png_library_debug}")
        set(PNG_LIBRARY_RELEASE "${rawgl_png_library_release}" CACHE FILEPATH "libpng release library" FORCE)
        set(PNG_LIBRARY_DEBUG "${rawgl_png_library_debug}" CACHE FILEPATH "libpng debug library" FORCE)
        set(PNG_LIBRARY "optimized;${PNG_LIBRARY_RELEASE};debug;${PNG_LIBRARY_DEBUG}" CACHE STRING "libpng libraries" FORCE)
        set(PNG_INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include/libpng18" CACHE PATH "libpng include dir" FORCE)
        set(PNG_PNG_INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include/libpng18" CACHE PATH "libpng include dir (png.h)" FORCE)
        set(PNG_FOUND TRUE CACHE BOOL "libpng found manually for Windows static build" FORCE)

        if(NOT TARGET PNG::PNG)
            add_library(PNG::PNG UNKNOWN IMPORTED)
            set_target_properties(PNG::PNG PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${PNG_PNG_INCLUDE_DIR}")
            if(EXISTS "${PNG_LIBRARY_RELEASE}")
                set_property(TARGET PNG::PNG APPEND PROPERTY IMPORTED_CONFIGURATIONS Release)
                set_target_properties(PNG::PNG PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${PNG_LIBRARY_RELEASE}")
            endif()
            if(EXISTS "${PNG_LIBRARY_DEBUG}")
                set_property(TARGET PNG::PNG APPEND PROPERTY IMPORTED_CONFIGURATIONS Debug)
                set_target_properties(PNG::PNG PROPERTIES
                    IMPORTED_LOCATION_DEBUG "${PNG_LIBRARY_DEBUG}")
            endif()
        endif()
    endif()
endif()
if(TARGET PNG::PNG AND NOT TARGET PNG::png_static)
    add_library(PNG::png_static INTERFACE IMPORTED)
    set_target_properties(PNG::png_static PROPERTIES
        INTERFACE_LINK_LIBRARIES "PNG::PNG")
endif()

find_package(fmt CONFIG QUIET)
find_package(Imath CONFIG QUIET)
find_package(openjph CONFIG QUIET)
find_package(libdeflate CONFIG QUIET)
find_package(liblzma CONFIG QUIET)
find_package(zstd CONFIG QUIET)

if(TARGET zstd::libzstd_static AND NOT TARGET ZSTD::ZSTD)
    add_library(ZSTD::ZSTD INTERFACE IMPORTED)
    set_target_properties(ZSTD::ZSTD PROPERTIES
        INTERFACE_LINK_LIBRARIES "zstd::libzstd_static")
elseif(TARGET zstd::libzstd AND NOT TARGET ZSTD::ZSTD)
    add_library(ZSTD::ZSTD INTERFACE IMPORTED)
    set_target_properties(ZSTD::ZSTD PROPERTIES
        INTERFACE_LINK_LIBRARIES "zstd::libzstd")
elseif(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ZSTD::ZSTD)
    rawgl_add_windows_imported_library(ZSTD::ZSTD
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zstd_static.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/zstd_staticd.lib")
endif()

if(TARGET libdeflate::libdeflate_static AND NOT TARGET Deflate::Deflate)
    add_library(Deflate::Deflate INTERFACE IMPORTED)
    set_target_properties(Deflate::Deflate PROPERTIES
        INTERFACE_LINK_LIBRARIES "libdeflate::libdeflate_static")
elseif(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET Deflate::Deflate)
    rawgl_add_windows_imported_library(Deflate::Deflate
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/deflatestatic.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/deflatestaticd.lib")
endif()

if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET liblzma::liblzma)
    rawgl_add_windows_imported_library(liblzma::liblzma
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/lzma.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/lzmad.lib")
    set_target_properties(liblzma::liblzma PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "LZMA_API_STATIC")
endif()

if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ICU::uc)
    rawgl_add_windows_imported_library(ICU::uc
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icuuc.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icuuc.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ICU::i18n AND EXISTS "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icuin.lib")
    rawgl_add_windows_imported_library(ICU::i18n
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icuin.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icuin.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET ICU::data AND EXISTS "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icudt.lib")
    rawgl_add_windows_imported_library(ICU::data
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icudt.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/icudt.lib")
endif()

find_package(PNG CONFIG QUIET)
find_package(libjpeg-turbo CONFIG QUIET)
find_package(JPEG QUIET)
find_package(TIFF CONFIG QUIET)
find_package(WebP CONFIG QUIET)
if(NOT TARGET WebP::webp)
    find_package(webp CONFIG QUIET)
endif()
find_package(GIFLIB CONFIG QUIET)
if(NOT TARGET GIF::GIF)
    if(TARGET GIFLIB::gif)
        add_library(GIF::GIF INTERFACE IMPORTED)
        set_target_properties(GIF::GIF PROPERTIES
            INTERFACE_LINK_LIBRARIES "GIFLIB::gif")
    elseif(TARGET GIFLIB::GIFLIB)
        add_library(GIF::GIF INTERFACE IMPORTED)
        set_target_properties(GIF::GIF PROPERTIES
            INTERFACE_LINK_LIBRARIES "GIFLIB::GIFLIB")
    elseif(RAWGL_WINDOWS_DEPS_ROOT AND EXISTS "${RAWGL_WINDOWS_DEPS_ROOT}/lib/gif.lib"
       OR RAWGL_WINDOWS_DEPS_ROOT AND EXISTS "${RAWGL_WINDOWS_DEPS_ROOT}/lib/gifd.lib")
        rawgl_add_windows_imported_library(GIF::GIF
            INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
            RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/gif.lib"
            DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/gifd.lib")
    else()
        find_package(GIF QUIET)
    endif()
endif()
find_package(Ptex CONFIG QUIET)
find_package(OpenJPEG CONFIG QUIET)
find_package(AOM CONFIG QUIET)
find_package(Brotli CONFIG QUIET)
find_package(brotli CONFIG QUIET)
find_path(RAWGL_BZIP2_INCLUDE_DIR
    NAMES bzlib.h)
find_library(RAWGL_BZIP2_LIBRARY_RELEASE
    NAMES bz2_static bz2 bzip2 libbz2)
find_library(RAWGL_BZIP2_LIBRARY_DEBUG
    NAMES bz2_staticd bz2d bzip2d libbz2d bz2_d)
rawgl_add_windows_imported_library(BZip2::BZip2
    INCLUDE_DIR "${RAWGL_BZIP2_INCLUDE_DIR}"
    RELEASE "${RAWGL_BZIP2_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_BZIP2_LIBRARY_DEBUG}")
find_path(RAWGL_ICU_INCLUDE_DIR
    NAMES unicode/utypes.h)
find_library(RAWGL_ICU_UC_LIBRARY_RELEASE
    NAMES icuuc)
find_library(RAWGL_ICU_UC_LIBRARY_DEBUG
    NAMES icuucd icuuc_d)
find_library(RAWGL_ICU_I18N_LIBRARY_RELEASE
    NAMES icuin icui18n)
find_library(RAWGL_ICU_I18N_LIBRARY_DEBUG
    NAMES icuind icui18nd icuin_d icui18n_d)
find_library(RAWGL_ICU_DATA_LIBRARY_RELEASE
    NAMES icudt icudata)
find_library(RAWGL_ICU_DATA_LIBRARY_DEBUG
    NAMES icudtd icudatad icudt_d icudata_d)
rawgl_add_windows_imported_library(ICU::uc
    INCLUDE_DIR "${RAWGL_ICU_INCLUDE_DIR}"
    RELEASE "${RAWGL_ICU_UC_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_ICU_UC_LIBRARY_DEBUG}")
rawgl_add_windows_imported_library(ICU::i18n
    INCLUDE_DIR "${RAWGL_ICU_INCLUDE_DIR}"
    RELEASE "${RAWGL_ICU_I18N_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_ICU_I18N_LIBRARY_DEBUG}")
rawgl_add_windows_imported_library(ICU::data
    INCLUDE_DIR "${RAWGL_ICU_INCLUDE_DIR}"
    RELEASE "${RAWGL_ICU_DATA_LIBRARY_RELEASE}"
    DEBUG "${RAWGL_ICU_DATA_LIBRARY_DEBUG}")
find_package(harfbuzz CONFIG QUIET)
find_package(freetype CONFIG QUIET)
find_package(libheif CONFIG QUIET)
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET pystring::pystring)
    rawgl_add_windows_imported_library(pystring::pystring
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/pystring.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/pystringd.lib")
endif()
find_package(OpenColorIO CONFIG QUIET)
find_package(pugixml CONFIG QUIET)
find_package(yaml-cpp CONFIG QUIET)
find_package(minizip-ng CONFIG QUIET)
find_package(expat CONFIG QUIET)
if(NOT TARGET EXPAT::EXPAT)
    find_package(EXPAT QUIET)
endif()
if(TARGET expat::expat AND NOT TARGET EXPAT::EXPAT)
    rawgl_add_imported_interface_alias(EXPAT::EXPAT expat::expat)
endif()
find_package(glew CONFIG QUIET)
if(NOT TARGET GLEW::GLEW
   AND NOT TARGET GLEW::glew_s
   AND NOT TARGET GLEW::glew
   AND NOT TARGET glew::glew
   AND NOT TARGET libglew_static)
    find_package(GLEW CONFIG QUIET)
endif()
if(NOT TARGET GLEW::GLEW
   AND NOT TARGET GLEW::glew_s
   AND NOT TARGET GLEW::glew
   AND NOT TARGET glew::glew
   AND NOT TARGET libglew_static)
    find_package(GLEW REQUIRED)
endif()
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenEXR CONFIG QUIET)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(miniply CONFIG QUIET)
find_package(RapidObj CONFIG REQUIRED)

if(TARGET PNG::png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG INTERFACE IMPORTED)
    set_target_properties(PNG::PNG PROPERTIES
        INTERFACE_LINK_LIBRARIES "PNG::png_static")
endif()
if(TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF)
    add_library(TIFF::TIFF INTERFACE IMPORTED)
    set_target_properties(TIFF::TIFF PROPERTIES
        INTERFACE_LINK_LIBRARIES "TIFF::tiff")
endif()
if(TARGET libjpeg-turbo::jpeg-static AND NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG INTERFACE IMPORTED)
    set_target_properties(JPEG::JPEG PROPERTIES
        INTERFACE_LINK_LIBRARIES "libjpeg-turbo::jpeg-static")
endif()
rawgl_add_imported_interface_alias(WebP::webp WebP::WebP)
rawgl_add_imported_interface_alias(WebP::webp webp::webp)
rawgl_add_imported_interface_alias(WebP::webp webp)
rawgl_add_imported_interface_alias(WebP::webp libwebp)
rawgl_add_imported_interface_alias(WebP::webp libwebp::webp)
rawgl_add_imported_interface_alias(WebP::webpdemux webpdemux)
rawgl_add_imported_interface_alias(WebP::webpdemux libwebpdemux)
rawgl_add_imported_interface_alias(WebP::webpdemux libwebp::webpdemux)
rawgl_add_imported_interface_alias(WebP::libwebpmux webpmux)
rawgl_add_imported_interface_alias(WebP::libwebpmux libwebpmux)
rawgl_add_imported_interface_alias(WebP::libwebpmux libwebp::webpmux)
if(TARGET Ptex::Ptex AND NOT TARGET Ptex::Ptex_static)
    add_library(Ptex::Ptex_static ALIAS Ptex::Ptex)
elseif(TARGET Ptex AND NOT TARGET Ptex::Ptex_static)
    add_library(Ptex::Ptex_static ALIAS Ptex)
endif()
if(TARGET OpenJPEG::OpenJPEG AND NOT TARGET openjp2)
    add_library(openjp2 ALIAS OpenJPEG::OpenJPEG)
elseif(TARGET openjpeg AND NOT TARGET openjp2)
    add_library(openjp2 ALIAS openjpeg)
endif()
if(TARGET AOM::aom AND NOT TARGET aom)
    add_library(aom ALIAS AOM::aom)
endif()
if(TARGET harfbuzz::harfbuzz AND NOT TARGET HarfBuzz::HarfBuzz)
    add_library(HarfBuzz::HarfBuzz INTERFACE IMPORTED)
    set_target_properties(HarfBuzz::HarfBuzz PROPERTIES
        INTERFACE_LINK_LIBRARIES "harfbuzz::harfbuzz")
endif()

if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET BZip2::BZip2)
    rawgl_add_windows_imported_library(BZip2::BZip2
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/bz2.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/bz2d.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET WebP::webp)
    rawgl_add_windows_imported_library(WebP::webp
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebp.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebpd.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET WebP::webpdemux)
    rawgl_add_windows_imported_library(WebP::webpdemux
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebpdemux.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebpdemuxd.lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT AND NOT TARGET WebP::libwebpmux)
    rawgl_add_windows_imported_library(WebP::libwebpmux
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebpmux.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libwebpmuxd.lib")
endif()
if(NOT TARGET libuhdr::libuhdr
   AND RAWGL_WINDOWS_DEPS_ROOT
   AND EXISTS "${RAWGL_WINDOWS_DEPS_ROOT}/lib/uhdr.lib")
    rawgl_add_windows_imported_library(libuhdr::libuhdr
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/uhdr.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/uhdrd.lib")
endif()

if(NOT TARGET WebP::webp)
    find_path(RAWGL_WEBP_INCLUDE_DIR
        NAMES webp/decode.h)
    find_library(RAWGL_WEBP_LIBRARY_RELEASE
        NAMES webp libwebp)
    find_library(RAWGL_WEBP_LIBRARY_DEBUG
        NAMES webpd libwebpd webp_d)
    rawgl_add_windows_imported_library(WebP::webp
        INCLUDE_DIR "${RAWGL_WEBP_INCLUDE_DIR}"
        RELEASE "${RAWGL_WEBP_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBP_LIBRARY_DEBUG}")
endif()
if(NOT TARGET WebP::webpdemux)
    find_library(RAWGL_WEBPDEMUX_LIBRARY_RELEASE
        NAMES webpdemux libwebpdemux)
    find_library(RAWGL_WEBPDEMUX_LIBRARY_DEBUG
        NAMES webpdemuxd libwebpdemuxd webpdemux_d)
    rawgl_add_windows_imported_library(WebP::webpdemux
        INCLUDE_DIR "${RAWGL_WEBP_INCLUDE_DIR}"
        RELEASE "${RAWGL_WEBPDEMUX_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBPDEMUX_LIBRARY_DEBUG}")
endif()
if(NOT TARGET WebP::libwebpmux)
    find_library(RAWGL_WEBPMUX_LIBRARY_RELEASE
        NAMES webpmux libwebpmux)
    find_library(RAWGL_WEBPMUX_LIBRARY_DEBUG
        NAMES webpmuxd libwebpmuxd webpmux_d)
    rawgl_add_windows_imported_library(WebP::libwebpmux
        INCLUDE_DIR "${RAWGL_WEBP_INCLUDE_DIR}"
        RELEASE "${RAWGL_WEBPMUX_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBPMUX_LIBRARY_DEBUG}")
endif()

rawgl_map_imported_config_targets(
    GIF::GIF
    GIFLIB::gif
    GIFLIB::GIFLIB
    Brotli::common
    Brotli::decoder
    Brotli::encoder
    Brotli::brotlicommon
    Brotli::brotlidec
    Brotli::brotlienc
    WebP::webp
    WebP::webpdemux
    WebP::libwebpmux
    EXPAT::EXPAT
    expat::expat)

if(TARGET GLEW::GLEW)
    set(RAWGL_OPENGL_LOADER_TARGET GLEW::GLEW)
elseif(TARGET GLEW::glew_s)
    set(RAWGL_OPENGL_LOADER_TARGET GLEW::glew_s)
elseif(TARGET GLEW::glew)
    set(RAWGL_OPENGL_LOADER_TARGET GLEW::glew)
elseif(TARGET glew::glew)
    set(RAWGL_OPENGL_LOADER_TARGET glew::glew)
elseif(TARGET libglew_static)
    set(RAWGL_OPENGL_LOADER_TARGET libglew_static)
else()
    message(FATAL_ERROR
        "GLEW target was not found. Install a GLEW CMake package in CMAKE_PREFIX_PATH "
        "or set RAWGL_WINDOWS_DEPS_ROOT for the legacy static fallback layout.")
endif()

if(TARGET glfw)
    set(RAWGL_GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(RAWGL_GLFW_TARGET glfw3)
elseif(TARGET glfw::glfw)
    set(RAWGL_GLFW_TARGET glfw::glfw)
elseif(TARGET glfw3::glfw)
    set(RAWGL_GLFW_TARGET glfw3::glfw)
elseif(TARGET glfw3::glfw3)
    set(RAWGL_GLFW_TARGET glfw3::glfw3)
else()
    message(FATAL_ERROR
        "GLFW target was not found. Install a GLFW CMake package in CMAKE_PREFIX_PATH "
        "or set RAWGL_WINDOWS_DEPS_ROOT for the legacy static fallback layout.")
endif()

set(RAWGL_OIIO_TARGETS OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
set(RAWGL_EXTRA_WINDOWS_LIBS opengl32)
