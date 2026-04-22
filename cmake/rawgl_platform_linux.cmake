find_package(OpenGL REQUIRED COMPONENTS OpenGL GLX)
find_package(X11 REQUIRED)
find_package(PNG CONFIG QUIET)
if(NOT TARGET PNG::PNG)
    find_package(PNG QUIET)
endif()
find_package(libjpeg-turbo CONFIG QUIET)
find_package(JPEG QUIET)
find_package(TIFF CONFIG QUIET)
if(NOT TARGET TIFF::TIFF AND NOT TARGET TIFF::tiff)
    find_package(TIFF QUIET)
endif()
find_package(OpenEXR CONFIG REQUIRED)
find_package(BZip2 QUIET)
find_package(fmt CONFIG QUIET)
find_package(freetype CONFIG QUIET)
find_package(pugixml CONFIG QUIET)

find_package(GIF QUIET)
if(NOT TARGET GIF::GIF)
    find_package(GIFLIB CONFIG QUIET)
    if(TARGET GIFLIB::gif)
        add_library(GIF::GIF ALIAS GIFLIB::gif)
    elseif(TARGET GIFLIB::GIFLIB)
        add_library(GIF::GIF ALIAS GIFLIB::GIFLIB)
    else()
        find_library(RAWGL_GIF_LIBRARY
            NAMES gif libgif giflib gif_static libgif_static giflib_static
            HINTS
                "${RAWGL_LINUX_PREFIX}/lib")
        if(RAWGL_GIF_LIBRARY)
            add_library(GIF::GIF UNKNOWN IMPORTED)
            set_target_properties(GIF::GIF PROPERTIES
                IMPORTED_LOCATION "${RAWGL_GIF_LIBRARY}")
        endif()
    endif()
endif()

find_package(Ptex CONFIG QUIET)
if(NOT TARGET Ptex::Ptex_static)
    if(TARGET Ptex::Ptex)
        add_library(Ptex::Ptex_static ALIAS Ptex::Ptex)
    elseif(TARGET Ptex)
        add_library(Ptex::Ptex_static ALIAS Ptex)
    else()
        find_library(RAWGL_PTEX_LIBRARY
            NAMES Ptex libPtex ptex libptex
            HINTS
                "${RAWGL_LINUX_PREFIX}/lib")
        if(RAWGL_PTEX_LIBRARY)
            add_library(Ptex::Ptex_static UNKNOWN IMPORTED)
            set_target_properties(Ptex::Ptex_static PROPERTIES
                IMPORTED_LOCATION "${RAWGL_PTEX_LIBRARY}")
            find_path(RAWGL_PTEX_INCLUDE_DIR
                NAMES Ptexture.h
                HINTS
                    "${RAWGL_LINUX_PREFIX}/include")
            if(RAWGL_PTEX_INCLUDE_DIR)
                set_target_properties(Ptex::Ptex_static PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${RAWGL_PTEX_INCLUDE_DIR}")
            endif()
        endif()
    endif()
endif()

find_package(dng_sdk CONFIG QUIET)
find_package(libheif CONFIG QUIET)
find_package(libuhdr QUIET CONFIG)
find_package(OpenJPEG CONFIG QUIET)
if(NOT TARGET openjp2)
    find_package(OpenJPEG QUIET)
    if(TARGET OpenJPEG::OpenJPEG)
        add_library(openjp2 ALIAS OpenJPEG::OpenJPEG)
    elseif(TARGET openjpeg)
        add_library(openjp2 ALIAS openjpeg)
    endif()
endif()

if(NOT TARGET libuhdr::libuhdr)
    find_path(RAWGL_LIBUHDR_INCLUDE_DIR
        NAMES ultrahdr_api.h
        HINTS
            "${RAWGL_LINUX_PREFIX}/include")
    find_library(RAWGL_LIBUHDR_LIBRARY_RELEASE
        NAMES uhdr libuhdr
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
    find_library(RAWGL_LIBUHDR_LIBRARY_DEBUG
        NAMES uhdrd libuhdrd uhdr_d
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
    if(RAWGL_LIBUHDR_LIBRARY_RELEASE OR RAWGL_LIBUHDR_LIBRARY_DEBUG)
        rawgl_add_linux_imported_library(libuhdr::libuhdr
            INCLUDE_DIR "${RAWGL_LIBUHDR_INCLUDE_DIR}"
            RELEASE "${RAWGL_LIBUHDR_LIBRARY_RELEASE}"
            DEBUG "${RAWGL_LIBUHDR_LIBRARY_DEBUG}")
    endif()
endif()

find_library(RAWGL_WEBP_LIBRARY_RELEASE
    NAMES webp libwebp
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_WEBP_LIBRARY_DEBUG
    NAMES webpd libwebpd webp_d
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_WEBPDEMUX_LIBRARY_RELEASE
    NAMES webpdemux libwebpdemux
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_WEBPDEMUX_LIBRARY_DEBUG
    NAMES webpdemuxd libwebpdemuxd webpdemux_d
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_WEBPMUX_LIBRARY_RELEASE
    NAMES webpmux libwebpmux
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_WEBPMUX_LIBRARY_DEBUG
    NAMES webpmuxd libwebpmuxd webpmux_d
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_SHARPYUV_LIBRARY_RELEASE
    NAMES sharpyuv libsharpyuv
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")
find_library(RAWGL_SHARPYUV_LIBRARY_DEBUG
    NAMES sharpyuvd libsharpyuvd sharpyuv_d
    HINTS
        "${RAWGL_LINUX_PREFIX}/lib")

if(NOT TARGET WebP::webp AND (RAWGL_WEBP_LIBRARY_RELEASE OR RAWGL_WEBP_LIBRARY_DEBUG))
    rawgl_add_linux_imported_library(WebP::webp
        INCLUDE_DIR "${RAWGL_LINUX_PREFIX}/include"
        RELEASE "${RAWGL_WEBP_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBP_LIBRARY_DEBUG}")
endif()
if(NOT TARGET WebP::webpdemux AND (RAWGL_WEBPDEMUX_LIBRARY_RELEASE OR RAWGL_WEBPDEMUX_LIBRARY_DEBUG))
    rawgl_add_linux_imported_library(WebP::webpdemux
        INCLUDE_DIR "${RAWGL_LINUX_PREFIX}/include"
        RELEASE "${RAWGL_WEBPDEMUX_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBPDEMUX_LIBRARY_DEBUG}")
endif()
if(NOT TARGET WebP::libwebpmux AND (RAWGL_WEBPMUX_LIBRARY_RELEASE OR RAWGL_WEBPMUX_LIBRARY_DEBUG))
    rawgl_add_linux_imported_library(WebP::libwebpmux
        INCLUDE_DIR "${RAWGL_LINUX_PREFIX}/include"
        RELEASE "${RAWGL_WEBPMUX_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_WEBPMUX_LIBRARY_DEBUG}")
endif()
if(NOT TARGET sharpyuv::sharpyuv AND (RAWGL_SHARPYUV_LIBRARY_RELEASE OR RAWGL_SHARPYUV_LIBRARY_DEBUG))
    rawgl_add_linux_imported_library(sharpyuv::sharpyuv
        INCLUDE_DIR "${RAWGL_LINUX_PREFIX}/include"
        RELEASE "${RAWGL_SHARPYUV_LIBRARY_RELEASE}"
        DEBUG "${RAWGL_SHARPYUV_LIBRARY_DEBUG}")
endif()

if(NOT TARGET fmt::fmt)
    find_library(RAWGL_FMT_LIBRARY_RELEASE
        NAMES fmt libfmt
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
    find_library(RAWGL_FMT_LIBRARY_DEBUG
        NAMES fmtd libfmtd fmtd libfmtd fmt_d
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
    if(RAWGL_FMT_LIBRARY_RELEASE OR RAWGL_FMT_LIBRARY_DEBUG)
        rawgl_add_linux_imported_library(fmt::fmt
            INCLUDE_DIR "${RAWGL_LINUX_PREFIX}/include"
            RELEASE "${RAWGL_FMT_LIBRARY_RELEASE}"
            DEBUG "${RAWGL_FMT_LIBRARY_DEBUG}")
    endif()
endif()

find_package(glew CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(miniply CONFIG QUIET)

if(TARGET PNG::png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG INTERFACE IMPORTED)
    set_target_properties(PNG::PNG PROPERTIES
        INTERFACE_LINK_LIBRARIES "PNG::png_static")
elseif(PNG_FOUND AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG INTERFACE IMPORTED)
    if(PNG_INCLUDE_DIRS)
        set_target_properties(PNG::PNG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIRS}")
    elseif(PNG_PNG_INCLUDE_DIR)
        set_target_properties(PNG::PNG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PNG_PNG_INCLUDE_DIR}")
    endif()
    if(PNG_LIBRARIES)
        set_target_properties(PNG::PNG PROPERTIES
            INTERFACE_LINK_LIBRARIES "${PNG_LIBRARIES}")
    elseif(PNG_LIBRARY)
        set_target_properties(PNG::PNG PROPERTIES
            INTERFACE_LINK_LIBRARIES "${PNG_LIBRARY}")
    endif()
endif()
if(TARGET libjpeg-turbo::jpeg-static AND NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG INTERFACE IMPORTED)
    set_target_properties(JPEG::JPEG PROPERTIES
        INTERFACE_LINK_LIBRARIES "libjpeg-turbo::jpeg-static")
elseif(JPEG_FOUND AND NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG INTERFACE IMPORTED)
    if(JPEG_INCLUDE_DIRS)
        set_target_properties(JPEG::JPEG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIRS}")
    elseif(JPEG_INCLUDE_DIR)
        set_target_properties(JPEG::JPEG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${JPEG_INCLUDE_DIR}")
    endif()
    if(JPEG_LIBRARIES)
        set_target_properties(JPEG::JPEG PROPERTIES
            INTERFACE_LINK_LIBRARIES "${JPEG_LIBRARIES}")
    elseif(JPEG_LIBRARY)
        set_target_properties(JPEG::JPEG PROPERTIES
            INTERFACE_LINK_LIBRARIES "${JPEG_LIBRARY}")
    endif()
endif()
if(TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF)
    add_library(TIFF::TIFF INTERFACE IMPORTED)
    set_target_properties(TIFF::TIFF PROPERTIES
        INTERFACE_LINK_LIBRARIES "TIFF::tiff")
elseif(TIFF_FOUND AND NOT TARGET TIFF::TIFF)
    add_library(TIFF::TIFF INTERFACE IMPORTED)
    if(TIFF_INCLUDE_DIR)
        set_target_properties(TIFF::TIFF PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIR}")
    elseif(TIFF_INCLUDE_DIRS)
        set_target_properties(TIFF::TIFF PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${TIFF_INCLUDE_DIRS}")
    endif()
    if(TIFF_LIBRARY)
        set_target_properties(TIFF::TIFF PROPERTIES
            INTERFACE_LINK_LIBRARIES "${TIFF_LIBRARY}")
    elseif(TIFF_LIBRARIES)
        set_target_properties(TIFF::TIFF PROPERTIES
            INTERFACE_LINK_LIBRARIES "${TIFF_LIBRARIES}")
    endif()
endif()

set(RAWGL_OPENGL_LOADER_TARGET libglew_static)
set(RAWGL_GLFW_TARGET glfw)
set(RAWGL_OIIO_TARGETS OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
