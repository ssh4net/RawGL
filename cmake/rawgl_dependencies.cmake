find_package(Threads REQUIRED)

set(RAWGL_EXTRA_WINDOWS_LIBS)

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/rawgl_platform_windows.cmake")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/rawgl_platform_linux.cmake")
endif()

foreach(rawgl_oiio_target OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
    if(TARGET ${rawgl_oiio_target})
        if(TARGET BZip2::BZip2)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES BZip2::BZip2)
        endif()
        if(TARGET pugixml::pugixml)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES pugixml::pugixml)
        elseif(TARGET pugixml)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES pugixml)
        endif()
        if(TARGET libuhdr::libuhdr)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES libuhdr::libuhdr)
        endif()
        if(TARGET Freetype::Freetype)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES Freetype::Freetype)
        endif()
        if(TARGET sharpyuv::sharpyuv)
            set_property(TARGET ${rawgl_oiio_target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES sharpyuv::sharpyuv)
        endif()

        if(WIN32)
            get_target_property(rawgl_oiio_iface_links ${rawgl_oiio_target} INTERFACE_LINK_LIBRARIES)
            if(rawgl_oiio_iface_links)
                string(REPLACE "E:/DVS" "${RAWGL_WINDOWS_DEPS_ROOT}" rawgl_oiio_iface_links "${rawgl_oiio_iface_links}")
                string(REPLACE "e:/DVS" "${RAWGL_WINDOWS_DEPS_ROOT}" rawgl_oiio_iface_links "${rawgl_oiio_iface_links}")
                set_target_properties(${rawgl_oiio_target} PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${rawgl_oiio_iface_links}")
            endif()
        endif()
    endif()
endforeach()

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/rawgl_windows_static_link.cmake")
endif()

find_path(RAWGL_SPDLOG_INCLUDE_DIR
    NAMES spdlog/spdlog.h
    HINTS
        "${RAWGL_LINUX_PREFIX}/include"
        "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        "/mnt/e/DVS/include")

if(NOT RAWGL_SPDLOG_INCLUDE_DIR)
    message(FATAL_ERROR "spdlog headers were not found. Set CMAKE_PREFIX_PATH or RAWGL_WINDOWS_DEPS_ROOT so spdlog/spdlog.h is available.")
endif()

if(RAWGL_USE_PACKAGED_MINIPLY AND TARGET miniply::miniply)
    set(RAWGL_MINIPLY_TARGET miniply::miniply)
    message(STATUS "RawGL miniply provider: package target miniply::miniply")
elseif(EXISTS "${RAWGL_MINIPLY_INCLUDE_DIR}/miniply.h")
    set(RAWGL_MINIPLY_SOURCES
        src/third_party/miniply/miniply.cpp)
    message(STATUS "RawGL miniply provider: vendored fallback from ${RAWGL_MINIPLY_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "miniply was not found. Install a miniply package in CMAKE_PREFIX_PATH or set RAWGL_MINIPLY_INCLUDE_DIR to the directory that contains miniply.h.")
endif()

find_path(RAWGL_LIBRAW_INCLUDE_DIR
    NAMES libraw/libraw.h
    HINTS
        "${RAWGL_LINUX_PREFIX}/include"
        "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        "/mnt/e/DVR/include")

if(NOT RAWGL_LIBRAW_INCLUDE_DIR)
    message(FATAL_ERROR "libraw headers were not found. Set CMAKE_PREFIX_PATH or RAWGL_WINDOWS_DEPS_ROOT so libraw/libraw.h is available.")
endif()

find_package(OpenMeta CONFIG QUIET)
find_path(RAWGL_OPENMETA_INCLUDE_DIR
    NAMES openmeta/simple_meta.h
    HINTS
        "${RAWGL_LINUX_PREFIX}"
        "${RAWGL_WINDOWS_DEPS_ROOT}"
    PATH_SUFFIXES
        include
        src/include)
find_library(RAWGL_OPENMETA_LIBRARY
    NAMES openmeta openmetad libopenmeta
    HINTS
        "${RAWGL_LINUX_PREFIX}"
        "${RAWGL_WINDOWS_DEPS_ROOT}"
    PATH_SUFFIXES
        lib
        build
        Release
        Debug
        bld/Release
        bld/Debug)
set(RAWGL_HAS_OPENMETA OFF)
if(NOT TARGET OpenMeta::openmeta AND RAWGL_OPENMETA_INCLUDE_DIR AND RAWGL_OPENMETA_LIBRARY)
    add_library(OpenMeta::openmeta UNKNOWN IMPORTED)
    set_target_properties(OpenMeta::openmeta PROPERTIES
        IMPORTED_LOCATION "${RAWGL_OPENMETA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RAWGL_OPENMETA_INCLUDE_DIR}")
endif()
if(TARGET OpenMeta::openmeta)
    set(RAWGL_HAS_OPENMETA ON)
endif()

if(NOT WIN32)
    find_library(RAWGL_HARFBUZZ_LIBRARY
        NAMES harfbuzz libharfbuzz
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
endif()
