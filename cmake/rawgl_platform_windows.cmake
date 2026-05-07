find_package(OpenGL REQUIRED)
find_package(ZLIB QUIET)

set(GLEW_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)

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
