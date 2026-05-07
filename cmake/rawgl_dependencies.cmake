find_package(Threads REQUIRED)

set(RAWGL_EXTRA_WINDOWS_LIBS)

set(rawgl_dependency_prefix_hints)
set(rawgl_dependency_include_hints)
set(rawgl_dependency_library_hints)
foreach(rawgl_dependency_prefix IN LISTS CMAKE_PREFIX_PATH)
    if(rawgl_dependency_prefix)
        list(APPEND rawgl_dependency_prefix_hints "${rawgl_dependency_prefix}")
        list(APPEND rawgl_dependency_include_hints "${rawgl_dependency_prefix}/include")
        list(APPEND rawgl_dependency_library_hints "${rawgl_dependency_prefix}/lib")
    endif()
endforeach()
if(RAWGL_LINUX_PREFIX)
    list(APPEND rawgl_dependency_prefix_hints "${RAWGL_LINUX_PREFIX}")
    list(APPEND rawgl_dependency_include_hints "${RAWGL_LINUX_PREFIX}/include")
    list(APPEND rawgl_dependency_library_hints "${RAWGL_LINUX_PREFIX}/lib")
endif()
if(RAWGL_WINDOWS_DEPS_ROOT)
    list(APPEND rawgl_dependency_prefix_hints "${RAWGL_WINDOWS_DEPS_ROOT}")
    list(APPEND rawgl_dependency_include_hints "${RAWGL_WINDOWS_DEPS_ROOT}/include")
    list(APPEND rawgl_dependency_library_hints "${RAWGL_WINDOWS_DEPS_ROOT}/lib")
endif()
list(REMOVE_DUPLICATES rawgl_dependency_prefix_hints)
list(REMOVE_DUPLICATES rawgl_dependency_include_hints)
list(REMOVE_DUPLICATES rawgl_dependency_library_hints)

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
                foreach(rawgl_rewrite_root IN LISTS RAWGL_WINDOWS_IMPORTED_CONFIG_REWRITE_ROOTS)
                    if(rawgl_rewrite_root AND RAWGL_WINDOWS_DEPS_ROOT)
                        string(REPLACE "${rawgl_rewrite_root}" "${RAWGL_WINDOWS_DEPS_ROOT}" rawgl_oiio_iface_links "${rawgl_oiio_iface_links}")
                    endif()
                endforeach()
                set_target_properties(${rawgl_oiio_target} PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${rawgl_oiio_iface_links}")
            endif()
        endif()
    endif()
endforeach()

find_path(RAWGL_SPDLOG_INCLUDE_DIR
    NAMES spdlog/spdlog.h
    HINTS ${rawgl_dependency_include_hints}
    NO_CACHE)

if(NOT RAWGL_SPDLOG_INCLUDE_DIR)
    message(FATAL_ERROR "spdlog headers were not found. Add a dependency prefix containing spdlog/spdlog.h to CMAKE_PREFIX_PATH.")
endif()

if(RAWGL_USE_PACKAGED_MINIPLY AND TARGET miniply::miniply)
    set(RAWGL_MINIPLY_TARGET miniply::miniply)
    message(STATUS "RawGL miniply provider: package target miniply::miniply")
elseif(EXISTS "${miniply_INCLUDE_DIR}/miniply.h")
    set(RAWGL_MINIPLY_SOURCES
        src/third_party/miniply/miniply.cpp)
    message(STATUS "RawGL miniply provider: vendored fallback from ${miniply_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "miniply was not found. Install a miniply package in CMAKE_PREFIX_PATH or set miniply_INCLUDE_DIR to the directory that contains miniply.h.")
endif()

find_path(RAWGL_LIBRAW_INCLUDE_DIR
    NAMES libraw/libraw.h
    HINTS ${rawgl_dependency_include_hints}
    NO_CACHE)

if(NOT RAWGL_LIBRAW_INCLUDE_DIR)
    message(FATAL_ERROR "libraw headers were not found. Add a dependency prefix containing libraw/libraw.h to CMAKE_PREFIX_PATH.")
endif()

find_path(RAWGL_JXL_INCLUDE_DIR
    NAMES jxl/decode.h
    HINTS ${rawgl_dependency_include_hints}
    NO_CACHE)

find_package(OpenMeta CONFIG QUIET)
if(WIN32)
    if(NOT TARGET EXPAT::EXPAT)
        find_package(EXPAT QUIET)
    endif()
    if(NOT TARGET expat::expat)
        find_package(expat CONFIG QUIET)
    endif()
    if(TARGET expat::expat AND NOT TARGET EXPAT::EXPAT)
        rawgl_add_imported_interface_alias(EXPAT::EXPAT expat::expat)
    endif()
    rawgl_map_imported_config_targets(
        Brotli::common
        Brotli::decoder
        Brotli::encoder
        Brotli::brotlicommon
        Brotli::brotlidec
        Brotli::brotlienc
        EXPAT::EXPAT
        expat::expat
        OpenMeta::openmeta_static
        OpenMeta::openmeta_shared)
endif()

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/rawgl_windows_static_link.cmake")
endif()

find_path(RAWGL_OPENMETA_INCLUDE_DIR
    NAMES openmeta/simple_meta.h
    HINTS ${rawgl_dependency_prefix_hints}
    PATH_SUFFIXES
        include
        src/include
    NO_CACHE)
find_library(RAWGL_OPENMETA_LIBRARY
    NAMES openmeta openmetad libopenmeta
    HINTS ${rawgl_dependency_prefix_hints}
    PATH_SUFFIXES
        lib
        build
        Release
        Debug
        bld/Release
        bld/Debug
    NO_CACHE)
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
        HINTS ${rawgl_dependency_library_hints}
        NO_CACHE)
endif()
