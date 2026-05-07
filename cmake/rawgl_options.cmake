include(GNUInstallDirs)

set(_rawgl_legacy_linux_prefix "${RAWGL_LINUX_PREFIX}")
set(_rawgl_legacy_windows_deps_root "${RAWGL_WINDOWS_DEPS_ROOT}")
unset(RAWGL_LINUX_PREFIX CACHE)
unset(RAWGL_WINDOWS_DEPS_ROOT CACHE)
set(RAWGL_LINUX_PREFIX "${_rawgl_legacy_linux_prefix}")
set(RAWGL_WINDOWS_DEPS_ROOT "${_rawgl_legacy_windows_deps_root}")
set(RAWGL_WINDOWS_IMPORTED_CONFIG_REWRITE_ROOTS "" CACHE STRING "Optional semicolon-separated stale roots to rewrite in imported Windows package configs.")
set(RAWGL_WINDOWS_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Windows multi-config generator configurations. Leave empty to keep the generator default.")
set(miniply_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniply" CACHE PATH "Directory containing miniply.h")
option(RAWGL_WINDOWS_UTF8 "Compile all Windows/MSVC targets with /utf-8." ON)
option(RAWGL_WINDOWS_PREFIX_ONLY_FIND "Restrict Windows third-party dependency discovery to CMake/package prefix paths and explicit hints." ON)
set(RAWGL_WINDOWS_DEBUG_SUFFIX "d" CACHE STRING "Suffix applied to Windows Debug target output names.")
option(RAWGL_BUILD_APP "Build the RawGL command-line application." ON)
option(RAWGL_BUILD_DOCS "Build Doxygen documentation targets." OFF)
option(RAWGL_BUILD_SPHINX_DOCS "Build the Sphinx HTML documentation site (requires Doxygen XML)." OFF)
option(RAWGL_DOCS_ONLY "Configure only documentation targets and skip RawGL dependency/target discovery." OFF)
option(RAWGL_BUILD_PYTHON "Build Python bindings with nanobind." OFF)
option(RAWGL_BUILD_WHEEL "Add a `rawgl_wheel` target (Python wheel build)." OFF)
option(RAWGL_WHEEL_NO_BUILD_ISOLATION "Build wheel without isolation (requires scikit-build-core installed)." OFF)
option(RAWGL_INSTALL_DEV_ARTIFACTS "Install C++ headers, libraries, and CMake package files." ON)
option(RAWGL_LINUX_USE_LIBCXX "Use libc++ instead of libstdc++ for Linux Clang builds." OFF)
if(WIN32)
    set(_rawgl_use_packaged_miniply_default ON)
else()
    set(_rawgl_use_packaged_miniply_default OFF)
endif()
option(RAWGL_USE_PACKAGED_MINIPLY "Prefer package target miniply::miniply over the vendored copy." ${_rawgl_use_packaged_miniply_default})
if(WIN32)
    set(_rawgl_python_bind_core_default ON)
else()
    set(_rawgl_python_bind_core_default OFF)
endif()
option(RAWGL_PYTHON_BIND_CORE "Link the Python module against the real RawGL workflow facade instead of scaffold-only mode." ${_rawgl_python_bind_core_default})

if(DEFINED RAWGL_MINIPLY_INCLUDE_DIR AND RAWGL_MINIPLY_INCLUDE_DIR AND NOT miniply_INCLUDE_DIR)
    set(miniply_INCLUDE_DIR "${RAWGL_MINIPLY_INCLUDE_DIR}" CACHE PATH "Directory containing miniply.h" FORCE)
endif()
unset(RAWGL_MINIPLY_INCLUDE_DIR CACHE)

foreach(_rawgl_internal_dependency_cache_var IN ITEMS
        RAWGL_ZLIB_INCLUDE_DIR
        RAWGL_ZLIB_LIBRARY_RELEASE
        RAWGL_ZLIB_LIBRARY_DEBUG
        RAWGL_BZIP2_INCLUDE_DIR
        RAWGL_BZIP2_LIBRARY_RELEASE
        RAWGL_BZIP2_LIBRARY_DEBUG
        RAWGL_ICU_INCLUDE_DIR
        RAWGL_ICU_UC_LIBRARY_RELEASE
        RAWGL_ICU_UC_LIBRARY_DEBUG
        RAWGL_ICU_I18N_LIBRARY_RELEASE
        RAWGL_ICU_I18N_LIBRARY_DEBUG
        RAWGL_ICU_DATA_LIBRARY_RELEASE
        RAWGL_ICU_DATA_LIBRARY_DEBUG
        RAWGL_LIBUHDR_INCLUDE_DIR
        RAWGL_LIBUHDR_LIBRARY_RELEASE
        RAWGL_LIBUHDR_LIBRARY_DEBUG
        RAWGL_WEBP_INCLUDE_DIR
        RAWGL_WEBP_LIBRARY_RELEASE
        RAWGL_WEBP_LIBRARY_DEBUG
        RAWGL_WEBPDEMUX_LIBRARY_RELEASE
        RAWGL_WEBPDEMUX_LIBRARY_DEBUG
        RAWGL_WEBPMUX_LIBRARY_RELEASE
        RAWGL_WEBPMUX_LIBRARY_DEBUG
        RAWGL_SHARPYUV_LIBRARY_RELEASE
        RAWGL_SHARPYUV_LIBRARY_DEBUG
        RAWGL_FMT_LIBRARY_RELEASE
        RAWGL_FMT_LIBRARY_DEBUG
        RAWGL_SPDLOG_INCLUDE_DIR
        RAWGL_LIBRAW_INCLUDE_DIR
        RAWGL_JXL_INCLUDE_DIR
        RAWGL_OPENMETA_INCLUDE_DIR
        RAWGL_OPENMETA_LIBRARY
        RAWGL_HARFBUZZ_LIBRARY
        RAWGL_GIF_LIBRARY
        RAWGL_PTEX_LIBRARY
        RAWGL_PTEX_INCLUDE_DIR)
    unset(${_rawgl_internal_dependency_cache_var} CACHE)
endforeach()

foreach(_rawgl_legacy_python_pair IN ITEMS
        "RAWGL_PYTHON_EXECUTABLE;Python_EXECUTABLE;FILEPATH;Python interpreter"
        "RAWGL_PYTHON_INCLUDE_DIR;Python_INCLUDE_DIR;PATH;Python include directory"
        "RAWGL_PYTHON_LIBRARY_RELEASE;Python_LIBRARY_RELEASE;FILEPATH;Python release library"
        "RAWGL_PYTHON_LIBRARY_DEBUG;Python_LIBRARY_DEBUG;FILEPATH;Python debug library")
    list(GET _rawgl_legacy_python_pair 0 _rawgl_legacy_python_var)
    list(GET _rawgl_legacy_python_pair 1 _rawgl_python_var)
    list(GET _rawgl_legacy_python_pair 2 _rawgl_python_cache_type)
    list(GET _rawgl_legacy_python_pair 3 _rawgl_python_cache_doc)
    if(DEFINED ${_rawgl_legacy_python_var} AND ${_rawgl_legacy_python_var} AND NOT ${_rawgl_python_var})
        set(${_rawgl_python_var} "${${_rawgl_legacy_python_var}}" CACHE ${_rawgl_python_cache_type} "${_rawgl_python_cache_doc}" FORCE)
    endif()
    unset(${_rawgl_legacy_python_var} CACHE)
endforeach()

if(WIN32)
    foreach(_rawgl_legacy_sdk_pair IN ITEMS
            "RAWGL_WINDOWS_DNG_SDK_ROOT;dng_sdk_ROOT;PATH;DNG SDK root"
            "RAWGL_WINDOWS_DNG_LIBRARY_RELEASE;DNG_SDK_LIBRARY_RELEASE;FILEPATH;DNG SDK release library"
            "RAWGL_WINDOWS_DNG_LIBRARY_DEBUG;DNG_SDK_LIBRARY_DEBUG;FILEPATH;DNG SDK debug library"
            "RAWGL_WINDOWS_XMP_CORE_LIBRARY_RELEASE;XMP_CORE_LIBRARY_RELEASE;FILEPATH;XMP Core release library"
            "RAWGL_WINDOWS_XMP_CORE_LIBRARY_DEBUG;XMP_CORE_LIBRARY_DEBUG;FILEPATH;XMP Core debug library"
            "RAWGL_WINDOWS_XMP_FILES_LIBRARY_RELEASE;XMP_FILES_LIBRARY_RELEASE;FILEPATH;XMP Files release library"
            "RAWGL_WINDOWS_XMP_FILES_LIBRARY_DEBUG;XMP_FILES_LIBRARY_DEBUG;FILEPATH;XMP Files debug library")
        list(GET _rawgl_legacy_sdk_pair 0 _rawgl_legacy_sdk_var)
        list(GET _rawgl_legacy_sdk_pair 1 _rawgl_sdk_var)
        list(GET _rawgl_legacy_sdk_pair 2 _rawgl_sdk_cache_type)
        list(GET _rawgl_legacy_sdk_pair 3 _rawgl_sdk_cache_doc)
        if(DEFINED ${_rawgl_legacy_sdk_var} AND ${_rawgl_legacy_sdk_var} AND NOT ${_rawgl_sdk_var})
            set(${_rawgl_sdk_var} "${${_rawgl_legacy_sdk_var}}" CACHE ${_rawgl_sdk_cache_type} "${_rawgl_sdk_cache_doc}" FORCE)
        endif()
        unset(${_rawgl_legacy_sdk_var} CACHE)
    endforeach()
else()
    foreach(_rawgl_windows_path_cache_var IN ITEMS
            dng_sdk_ROOT
            DNG_SDK_LIBRARY_RELEASE
            DNG_SDK_LIBRARY_DEBUG
            XMP_CORE_LIBRARY_RELEASE
            XMP_CORE_LIBRARY_DEBUG
            XMP_FILES_LIBRARY_RELEASE
            XMP_FILES_LIBRARY_DEBUG)
        if(DEFINED ${_rawgl_windows_path_cache_var} AND ${_rawgl_windows_path_cache_var} MATCHES "^[A-Za-z]:[/\\\\]")
            unset(${_rawgl_windows_path_cache_var} CACHE)
        endif()
    endforeach()
endif()

if(Python_EXECUTABLE AND NOT EXISTS "${Python_EXECUTABLE}")
    message(FATAL_ERROR "Python_EXECUTABLE does not exist: ${Python_EXECUTABLE}")
endif()

if(Python_INCLUDE_DIR)
    if(NOT IS_ABSOLUTE "${Python_INCLUDE_DIR}")
        message(FATAL_ERROR "Python_INCLUDE_DIR must be an absolute path: ${Python_INCLUDE_DIR}")
    endif()
    if(NOT EXISTS "${Python_INCLUDE_DIR}")
        message(FATAL_ERROR "Python_INCLUDE_DIR does not exist: ${Python_INCLUDE_DIR}")
    endif()
endif()

foreach(_rawgl_python_lib_var IN ITEMS Python_LIBRARY_RELEASE Python_LIBRARY_DEBUG)
    if(${_rawgl_python_lib_var})
        if(NOT IS_ABSOLUTE "${${_rawgl_python_lib_var}}")
            message(FATAL_ERROR "${_rawgl_python_lib_var} must be an absolute path: ${${_rawgl_python_lib_var}}")
        endif()
        if(NOT EXISTS "${${_rawgl_python_lib_var}}")
            message(FATAL_ERROR "${_rawgl_python_lib_var} does not exist: ${${_rawgl_python_lib_var}}")
        endif()
    endif()
endforeach()

foreach(_rawgl_sdk_path_var IN ITEMS
        DNG_SDK_LIBRARY_RELEASE
        DNG_SDK_LIBRARY_DEBUG
        XMP_CORE_LIBRARY_RELEASE
        XMP_CORE_LIBRARY_DEBUG
        XMP_FILES_LIBRARY_RELEASE
        XMP_FILES_LIBRARY_DEBUG)
    if(${_rawgl_sdk_path_var})
        if(NOT IS_ABSOLUTE "${${_rawgl_sdk_path_var}}")
            message(FATAL_ERROR "${_rawgl_sdk_path_var} must be an absolute path: ${${_rawgl_sdk_path_var}}")
        endif()
        if(NOT EXISTS "${${_rawgl_sdk_path_var}}")
            message(FATAL_ERROR "${_rawgl_sdk_path_var} does not exist: ${${_rawgl_sdk_path_var}}")
        endif()
    endif()
endforeach()

function(rawgl_cache_prepend_prefix_path prefix_path)
    if(NOT prefix_path)
        return()
    endif()

    set(rawgl_prefix_list ${CMAKE_PREFIX_PATH})
    list(REMOVE_ITEM rawgl_prefix_list "${prefix_path}")
    list(PREPEND rawgl_prefix_list "${prefix_path}")
    set(CMAKE_PREFIX_PATH "${rawgl_prefix_list}" CACHE STRING "Search path for external dependencies." FORCE)
endfunction()

if(WIN32)
    rawgl_cache_prepend_prefix_path("${RAWGL_WINDOWS_DEPS_ROOT}")
    if(CMAKE_CONFIGURATION_TYPES AND RAWGL_WINDOWS_CONFIGURATION_TYPES)
        set(CMAKE_CONFIGURATION_TYPES "${RAWGL_WINDOWS_CONFIGURATION_TYPES}" CACHE STRING "RawGL Windows generator configurations." FORCE)
    endif()
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON CACHE BOOL "Prefer package config files over Find modules on Windows static builds." FORCE)
    set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF CACHE BOOL "Do not use the user package registry for RawGL Windows dependency discovery." FORCE)
    set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF CACHE BOOL "Do not use the system package registry for RawGL Windows dependency discovery." FORCE)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release CACHE STRING "Map RelWithDebInfo imports to Release on Windows static builds." FORCE)
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release CACHE STRING "Map MinSizeRel imports to Release on Windows static builds." FORCE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "$<$<CONFIG:Debug>:MultiThreadedDebug>$<$<NOT:$<CONFIG:Debug>>:MultiThreaded>"
        CACHE STRING "MSVC runtime library selection." FORCE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    rawgl_cache_prepend_prefix_path("${RAWGL_LINUX_PREFIX}")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build position independent code on Linux." FORCE)
endif()

if(MSVC AND RAWGL_WINDOWS_UTF8)
    add_compile_options(/utf-8)
endif()

if(RAWGL_BUILD_SPHINX_DOCS AND NOT RAWGL_BUILD_DOCS)
    set(RAWGL_BUILD_DOCS ON CACHE BOOL "Build Doxygen documentation targets." FORCE)
endif()

if(RAWGL_DOCS_ONLY)
    set(RAWGL_BUILD_APP OFF CACHE BOOL "Build the RawGL command-line application." FORCE)
    set(RAWGL_BUILD_PYTHON OFF CACHE BOOL "Build Python bindings with nanobind." FORCE)
    set(RAWGL_BUILD_WHEEL OFF CACHE BOOL "Add a `rawgl_wheel` target (Python wheel build)." FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Build testing targets." FORCE)
endif()
