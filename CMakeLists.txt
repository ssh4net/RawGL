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
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -c "import pip"
        RESULT_VARIABLE _rawgl_python_has_pip
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(RAWGL_WHEEL_NO_BUILD_ISOLATION)
        execute_process(
            COMMAND "${Python_EXECUTABLE}" -c "import scikit_build_core"
            RESULT_VARIABLE _rawgl_python_has_scikit_build_core
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT _rawgl_python_has_scikit_build_core EQUAL 0)
            message(FATAL_ERROR
                "RAWGL_WHEEL_NO_BUILD_ISOLATION requires Python package 'scikit-build-core' for: "
                "${Python_EXECUTABLE}. Install it into that interpreter, or set RAWGL_WHEEL_NO_BUILD_ISOLATION=OFF.")
        endif()
    endif()

    set(_rawgl_wheel_env_prefix_path "${CMAKE_PREFIX_PATH}")
    string(REPLACE ";" "\\;" _rawgl_wheel_env_prefix_path "${_rawgl_wheel_env_prefix_path}")
    set(_rawgl_wheel_out_dir "${CMAKE_BINARY_DIR}/dist")
    set(_rawgl_wheel_env_args
        "CMAKE_PREFIX_PATH=${_rawgl_wheel_env_prefix_path}"
        "SKBUILD_CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    if(CMAKE_C_COMPILER)
        list(APPEND _rawgl_wheel_env_args
            "CC=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND _rawgl_wheel_env_args
            "CXX=${CMAKE_CXX_COMPILER}")
    endif()

    set(_rawgl_wheel_pip_isolation_args "")
    if(RAWGL_WHEEL_NO_BUILD_ISOLATION)
        set(_rawgl_wheel_pip_isolation_args --no-build-isolation)
    endif()

    set(_rawgl_wheel_config_settings "")
    list(APPEND _rawgl_wheel_config_settings
        "-C" "cmake.define.RAWGL_BUILD_PYTHON=${RAWGL_BUILD_PYTHON}"
        "-C" "cmake.define.RAWGL_PYTHON_BIND_CORE=${RAWGL_PYTHON_BIND_CORE}"
        "-C" "cmake.define.RAWGL_INSTALL_DEV_ARTIFACTS=OFF")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND RAWGL_LINUX_PREFIX)
        list(APPEND _rawgl_wheel_config_settings
            "-C" "cmake.define.RAWGL_LINUX_PREFIX=${RAWGL_LINUX_PREFIX}")
        list(APPEND _rawgl_wheel_config_settings
            "-C" "cmake.define.RAWGL_LINUX_USE_LIBCXX=${RAWGL_LINUX_USE_LIBCXX}"
            "-C" "cmake.define.RAWGL_USE_PACKAGED_MINIPLY=${RAWGL_USE_PACKAGED_MINIPLY}")
    endif()
    if(WIN32)
        if(RAWGL_WINDOWS_DEPS_ROOT)
            list(APPEND _rawgl_wheel_config_settings
                "-C" "cmake.define.RAWGL_WINDOWS_DEPS_ROOT=${RAWGL_WINDOWS_DEPS_ROOT}")
        endif()
        if(dng_sdk_ROOT)
            list(APPEND _rawgl_wheel_config_settings
                "-C" "cmake.define.dng_sdk_ROOT=${dng_sdk_ROOT}")
        endif()
    endif()
    foreach(_rawgl_link_flag_var IN ITEMS
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_MODULE_LINKER_FLAGS)
        if(${_rawgl_link_flag_var})
            list(APPEND _rawgl_wheel_config_settings
                "-C" "cmake.define.${_rawgl_link_flag_var}=${${_rawgl_link_flag_var}}")
        endif()
    endforeach()

    set(_rawgl_wheel_cmd "")
    set(_rawgl_wheel_tool "pip")
    if(_rawgl_python_has_pip EQUAL 0)
        list(APPEND _rawgl_wheel_cmd
            "${Python_EXECUTABLE}" -m pip wheel "${PROJECT_SOURCE_DIR}"
            --no-deps
            ${_rawgl_wheel_config_settings}
            ${_rawgl_wheel_pip_isolation_args}
            -w "${_rawgl_wheel_out_dir}"
        )
    else()
        if(NOT RAWGL_UV_EXECUTABLE)
            message(FATAL_ERROR
                "RAWGL_BUILD_WHEEL: '${Python_EXECUTABLE}' has no pip module, and 'uv' was not found on PATH. "
                "Either install pip into that Python, or install uv.")
        endif()
        set(_rawgl_wheel_tool "uv")
        set(_rawgl_wheel_uv_args "")
        if(RAWGL_WHEEL_NO_BUILD_ISOLATION)
            set(_rawgl_wheel_uv_args --no-build-isolation)
        endif()
        list(APPEND _rawgl_wheel_cmd
            "${RAWGL_UV_EXECUTABLE}" --no-cache build
            --wheel
            ${_rawgl_wheel_config_settings}
            ${_rawgl_wheel_uv_args}
            -p "${Python_EXECUTABLE}"
            -o "${_rawgl_wheel_out_dir}"
            "${PROJECT_SOURCE_DIR}"
        )
    endif()

    add_custom_target(rawgl_wheel
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_rawgl_wheel_out_dir}"
        COMMAND "${CMAKE_COMMAND}" -E env
                ${_rawgl_wheel_env_args}
                ${_rawgl_wheel_cmd}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Building RawGL Python wheel (${_rawgl_wheel_tool})"
        VERBATIM
    )

    set(_rawgl_install_wheel_script "${CMAKE_CURRENT_BINARY_DIR}/RawGLInstallWheel.cmake")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/RawGLInstallWheel.cmake.in"
        "${_rawgl_install_wheel_script}"
        @ONLY
    )
endif()
if(NOT RAWGL_DOCS_ONLY)
    include("${RAWGL_CMAKE_DIR}/rawgl_testing.cmake")
endif()

if(RAWGL_BUILD_WHEEL)
    install(SCRIPT "${_rawgl_install_wheel_script}")
endif()

rawgl_print_config_summary()
