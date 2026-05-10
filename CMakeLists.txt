cmake_minimum_required(VERSION 3.28)

set(_rawgl_version_file "${CMAKE_CURRENT_LIST_DIR}/VERSION")
if(NOT EXISTS "${_rawgl_version_file}")
    message(FATAL_ERROR "Missing version file: ${_rawgl_version_file}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rawgl_version_file}")
file(READ "${_rawgl_version_file}" RAWGL_VERSION)
string(STRIP "${RAWGL_VERSION}" RAWGL_VERSION)
if(NOT RAWGL_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "Invalid RawGL version in ${_rawgl_version_file}: '${RAWGL_VERSION}' (expected MAJOR.MINOR.PATCH)")
endif()
set(RAWGL_VERSION "${RAWGL_VERSION}" CACHE STRING "RawGL project version" FORCE)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(RawGL VERSION "${RAWGL_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(RAWGL_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include("${RAWGL_CMAKE_DIR}/rawgl_options.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_helpers.cmake")
if(NOT RAWGL_DOCS_ONLY)
    include("${RAWGL_CMAKE_DIR}/rawgl_dependencies.cmake")
    include("${RAWGL_CMAKE_DIR}/rawgl_targets.cmake")
endif()
include("${RAWGL_CMAKE_DIR}/rawgl_docs.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_report.cmake")
if(RAWGL_BUILD_PYTHON)
    add_subdirectory(src/python)
endif()

if(RAWGL_BUILD_WHEEL)
    if(NOT RAWGL_BUILD_PYTHON)
        message(FATAL_ERROR "RAWGL_BUILD_WHEEL requires RAWGL_BUILD_PYTHON=ON.")
    endif()

    rawgl_configure_python_from_prefix_path()
    find_package(Python REQUIRED COMPONENTS Interpreter)
    find_program(RAWGL_UV_EXECUTABLE NAMES uv)

    set(_rawgl_wheel_out_dir "${CMAKE_BINARY_DIR}/dist")

    set(_rawgl_wheel_config_settings "")
    function(rawgl_append_wheel_config_define rawgl_define_name rawgl_define_value)
        if("${rawgl_define_value}" STREQUAL "")
            return()
        endif()
        set(rawgl_escaped_define_value "${rawgl_define_value}")
        string(REPLACE ";" "\\;" rawgl_escaped_define_value "${rawgl_escaped_define_value}")
        list(APPEND _rawgl_wheel_config_settings
            "-C" "cmake.define.${rawgl_define_name}=${rawgl_escaped_define_value}")
        set(_rawgl_wheel_config_settings "${_rawgl_wheel_config_settings}" PARENT_SCOPE)
    endfunction()

    rawgl_append_wheel_config_define(RAWGL_BUILD_PYTHON ON)
    rawgl_append_wheel_config_define(RAWGL_TESTS OFF)
    rawgl_append_wheel_config_define(RAWGL_INSTALL_DEV_ARTIFACTS OFF)

    foreach(_rawgl_wheel_define_var IN ITEMS
            BUILD_SHARED_LIBS
            CMAKE_C_STANDARD
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_CXX_STANDARD
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_MSVC_RUNTIME_LIBRARY
            CMAKE_POLICY_VERSION_MINIMUM
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_MODULE_LINKER_FLAGS
            RAWGL_WINDOWS_UTF8
            RAWGL_WINDOWS_PREFIX_ONLY_FIND
            RAWGL_WINDOWS_CONFIGURATION_TYPES
            RAWGL_WINDOWS_DEBUG_SUFFIX
            RAWGL_WINDOWS_IMPORTED_CONFIG_REWRITE_ROOTS
            RAWGL_LINUX_USE_LIBCXX
            RAWGL_USE_PACKAGED_MINIPLY
            miniply_INCLUDE_DIR
            dng_sdk_ROOT
            DNG_SDK_LIBRARY_RELEASE
            DNG_SDK_LIBRARY_DEBUG
            XMP_CORE_LIBRARY_RELEASE
            XMP_CORE_LIBRARY_DEBUG
            XMP_FILES_LIBRARY_RELEASE
            XMP_FILES_LIBRARY_DEBUG
            Python_EXECUTABLE
            Python_ROOT_DIR
            Python_INCLUDE_DIR
            Python_LIBRARY
            Python_LIBRARY_RELEASE
            Python_LIBRARY_DEBUG
            Python_SABI_LIBRARY
            Python_SABI_LIBRARY_RELEASE)
        if(DEFINED ${_rawgl_wheel_define_var})
            rawgl_append_wheel_config_define("${_rawgl_wheel_define_var}" "${${_rawgl_wheel_define_var}}")
        endif()
    endforeach()

    set(_rawgl_install_wheel_script "${CMAKE_CURRENT_BINARY_DIR}/RawGLInstallWheel.cmake")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/RawGLInstallWheel.cmake.in"
        "${_rawgl_install_wheel_script}"
        @ONLY
    )

    add_custom_target(rawgl_wheel
        COMMAND "${CMAKE_COMMAND}"
                -DRAWGL_WHEEL_INSTALL=OFF
                "-DRAWGL_WHEEL_CONFIG=$<CONFIG>"
                -P "${_rawgl_install_wheel_script}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Building RawGL Python wheel"
        VERBATIM
    )
endif()
if(NOT RAWGL_DOCS_ONLY)
    include("${RAWGL_CMAKE_DIR}/rawgl_testing.cmake")
endif()

if(RAWGL_BUILD_WHEEL)
    install(SCRIPT "${_rawgl_install_wheel_script}")
endif()

rawgl_print_config_summary()
