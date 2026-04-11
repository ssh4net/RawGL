cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(RawGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RAWGL_LINUX_PREFIX "" CACHE PATH "Linux dependency prefix, for example /mnt/e/UBc/Release or /mnt/e/UBc/Debug")
set(RAWGL_WINDOWS_DEPS_ROOT "E:/DVS" CACHE PATH "Windows dependency root used by the static CMake build")
set(RAWGL_MINIPLY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SRC/RawGL/src/intern" CACHE PATH "Directory containing miniply.h")

if(RAWGL_LINUX_PREFIX)
    list(PREPEND CMAKE_PREFIX_PATH "${RAWGL_LINUX_PREFIX}")
endif()

function(rawgl_add_windows_imported_library target_name)
    cmake_parse_arguments(ARG "" "INCLUDE_DIR;RELEASE;DEBUG" "" ${ARGN})
    add_library(${target_name} UNKNOWN IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ARG_INCLUDE_DIR}"
        IMPORTED_CONFIGURATIONS "Release;Debug"
        IMPORTED_LOCATION_RELEASE "${ARG_RELEASE}"
        IMPORTED_LOCATION_DEBUG "${ARG_DEBUG}")
endfunction()

function(rawgl_add_linux_imported_library target_name)
    cmake_parse_arguments(ARG "" "INCLUDE_DIR;RELEASE;DEBUG" "" ${ARGN})
    if(TARGET ${target_name})
        return()
    endif()
    add_library(${target_name} UNKNOWN IMPORTED)
    if(ARG_INCLUDE_DIR)
        set_target_properties(${target_name} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ARG_INCLUDE_DIR}")
    endif()
    if(ARG_RELEASE)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS Release)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_RELEASE "${ARG_RELEASE}")
    endif()
    if(ARG_DEBUG)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS Debug)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_DEBUG "${ARG_DEBUG}")
    endif()
    if(ARG_RELEASE AND NOT ARG_DEBUG)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION "${ARG_RELEASE}")
    elseif(ARG_DEBUG AND NOT ARG_RELEASE)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION "${ARG_DEBUG}")
    endif()
endfunction()

find_package(Threads REQUIRED)

if(WIN32)
    find_package(OpenGL REQUIRED)
else()
    find_package(OpenGL REQUIRED COMPONENTS OpenGL GLX)
    find_package(X11 REQUIRED)
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
endif()

if(WIN32)
    rawgl_add_windows_imported_library(RawGLWindowsGLEW
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libglew32.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/libglew32d.lib")
    target_compile_definitions(RawGLWindowsGLEW INTERFACE GLEW_STATIC GLEW_NO_GLU)

    rawgl_add_windows_imported_library(RawGLWindowsGLFW
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/glfw3.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/glfw3d.lib")

    rawgl_add_windows_imported_library(RawGLWindowsOIIO
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/OpenImageIO.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/OpenImageIO_d.lib")
    rawgl_add_windows_imported_library(RawGLWindowsOIIOUtil
        INCLUDE_DIR "${RAWGL_WINDOWS_DEPS_ROOT}/include"
        RELEASE "${RAWGL_WINDOWS_DEPS_ROOT}/lib/OpenImageIO_Util.lib"
        DEBUG "${RAWGL_WINDOWS_DEPS_ROOT}/lib/OpenImageIO_Util_d.lib")

    set(RAWGL_OPENGL_LOADER_TARGET RawGLWindowsGLEW)
    set(RAWGL_GLFW_TARGET RawGLWindowsGLFW)
    set(RAWGL_OIIO_TARGETS RawGLWindowsOIIO RawGLWindowsOIIOUtil)
    set(RAWGL_EXTRA_WINDOWS_LIBS opengl32)
else()
    find_package(glew CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenImageIO CONFIG REQUIRED)
    find_package(miniply CONFIG QUIET)
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
        endif()
    endforeach()
    set(RAWGL_OPENGL_LOADER_TARGET libglew_static)
    set(RAWGL_GLFW_TARGET glfw)
    set(RAWGL_OIIO_TARGETS OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
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

if(TARGET miniply::miniply)
    set(RAWGL_MINIPLY_TARGET miniply::miniply)
    message(STATUS "RawGL miniply provider: package target miniply::miniply")
elseif(EXISTS "${RAWGL_MINIPLY_INCLUDE_DIR}/miniply.h")
    set(RAWGL_MINIPLY_SOURCES
        SRC/RawGL/src/intern/miniply.cpp)
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

if(NOT WIN32)
    find_library(RAWGL_HARFBUZZ_LIBRARY
        NAMES harfbuzz libharfbuzz
        HINTS
            "${RAWGL_LINUX_PREFIX}/lib")
endif()

set(RAWGL_CORE_SOURCES
    SRC/RawGL/src/PassInput.cpp
    SRC/RawGL/src/PassOutput.cpp
    SRC/RawGL/src/RawGLCore.cpp
    SRC/RawGL/src/Sequence.cpp
    SRC/RawGL/src/intern/GLProgram.cpp
    SRC/RawGL/src/intern/GLProgramManager.cpp
    SRC/RawGL/src/intern/ImageUtils.cpp
    SRC/RawGL/src/intern/Log.cpp
    SRC/RawGL/src/intern/MESH_IO.CPP
    SRC/RawGL/src/intern/OpenGLUtils.cpp
    SRC/RawGL/src/intern/Texture.cpp)

list(APPEND RAWGL_CORE_SOURCES ${RAWGL_MINIPLY_SOURCES})

add_library(rawgl_core STATIC ${RAWGL_CORE_SOURCES})
add_library(RawGL::core ALIAS rawgl_core)

add_executable(rawgl SRC/RawGL/src/RawGL.cpp)
add_executable(RawGL::rawgl ALIAS rawgl)

if(WIN32)
    enable_language(RC)
    target_sources(rawgl PRIVATE
        SRC/RawGL/RawGL.rc)
endif()

target_include_directories(rawgl_core PUBLIC
    SRC/RawGL/src
    SRC/RawGL/src/intern
    "${RAWGL_LIBRAW_INCLUDE_DIR}"
    "${RAWGL_SPDLOG_INCLUDE_DIR}")

if(NOT TARGET miniply::miniply)
    target_include_directories(rawgl_core PRIVATE
        "${RAWGL_MINIPLY_INCLUDE_DIR}")
endif()

target_compile_definitions(rawgl_core PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    RAWGL_USE_GLEW
    SPDLOG_FMT_EXTERNAL)

if(WIN32)
    target_compile_definitions(rawgl_core PRIVATE _MSVC)
    target_compile_definitions(rawgl PRIVATE _CONSOLE _MSVC)
    target_compile_options(rawgl_core PRIVATE /utf-8)
    target_compile_options(rawgl PRIVATE /utf-8)
endif()

target_link_libraries(rawgl_core PUBLIC
    ${RAWGL_OPENGL_LOADER_TARGET}
    ${RAWGL_GLFW_TARGET}
    ${RAWGL_OIIO_TARGETS}
    ${RAWGL_MINIPLY_TARGET}
    Threads::Threads
    ${RAWGL_EXTRA_WINDOWS_LIBS})

if(NOT WIN32)
    if(TARGET fmt::fmt)
        target_link_libraries(rawgl_core PUBLIC fmt::fmt)
    endif()
    if(TARGET BZip2::BZip2)
        target_link_libraries(rawgl_core PUBLIC
            "-Wl,--whole-archive"
            "$<TARGET_FILE:BZip2::BZip2>"
            "-Wl,--no-whole-archive")
    endif()
    if(TARGET pugixml::pugixml)
        target_link_libraries(rawgl_core PUBLIC pugixml::pugixml)
    elseif(TARGET pugixml)
        target_link_libraries(rawgl_core PUBLIC pugixml)
    endif()
    if(TARGET libuhdr::libuhdr)
        target_link_libraries(rawgl_core PUBLIC libuhdr::libuhdr)
    endif()
    if(TARGET Freetype::Freetype)
        target_link_libraries(rawgl_core PUBLIC Freetype::Freetype)
    endif()
    if(RAWGL_HARFBUZZ_LIBRARY)
        target_link_libraries(rawgl_core PUBLIC "${RAWGL_HARFBUZZ_LIBRARY}")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rawgl_core PUBLIC -stdlib=libc++)
    target_link_options(rawgl_core PUBLIC -stdlib=libc++)
endif()

target_link_libraries(rawgl PRIVATE rawgl_core)

set_target_properties(rawgl PROPERTIES
    OUTPUT_NAME RawGL)

include(CTest)

if(BUILD_TESTING)
    add_executable(rawgl_core_inspect_smoke
        tests/rawgl_core_inspect_smoke.cpp)
    target_link_libraries(rawgl_core_inspect_smoke PRIVATE rawgl_core)
    target_include_directories(rawgl_core_inspect_smoke PRIVATE
        SRC/RawGL/src
        SRC/RawGL/src/intern)

    add_executable(rawgl_core_graph_smoke
        tests/rawgl_core_graph_smoke.cpp)
    target_link_libraries(rawgl_core_graph_smoke PRIVATE rawgl_core)
    target_include_directories(rawgl_core_graph_smoke PRIVATE
        SRC/RawGL/src
        SRC/RawGL/src/intern)

    add_test(NAME rawgl_core_inspect_smoke
        COMMAND rawgl_core_inspect_smoke)
    set_tests_properties(rawgl_core_inspect_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_graph_smoke
        COMMAND rawgl_core_graph_smoke)
    set_tests_properties(rawgl_core_graph_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    function(rawgl_add_script_test test_name base_name)
        if(WIN32)
            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND} -E env
                    "RAWGL_BIN_OVERRIDE=$<TARGET_FILE:rawgl>"
                    cmd.exe /C "${CMAKE_SOURCE_DIR}/tests/${base_name}.bat")
        else()
            set(rawgl_test_env
                "RAWGL_BIN=$<TARGET_FILE:rawgl>")
            if(RAWGL_LINUX_PREFIX)
                list(APPEND rawgl_test_env
                    "RAWGL_OIIOTOOL=${RAWGL_LINUX_PREFIX}/bin/oiiotool")
            endif()

            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND} -E env
                    ${rawgl_test_env}
                    bash "${CMAKE_SOURCE_DIR}/tests/${base_name}.sh")
        endif()

        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endfunction()

    rawgl_add_script_test(rawgl_atomic_counter test_atomic_counter)
    rawgl_add_script_test(rawgl_compute_image_chain test_compute_image_chain)
    rawgl_add_script_test(rawgl_cull_parser test_cull_parser)
    rawgl_add_script_test(rawgl_invalid_atomic_input test_invalid_atomic_input)
    rawgl_add_script_test(rawgl_invalid_bg_color test_invalid_bg_color)
    rawgl_add_script_test(rawgl_invalid_mesh_reference test_invalid_mesh_reference)
    rawgl_add_script_test(rawgl_invalid_numeric_input test_invalid_numeric_input)
    rawgl_add_script_test(rawgl_invalid_pass_reference test_invalid_pass_reference)
    rawgl_add_script_test(rawgl_invalid_pass_size test_invalid_pass_size)
    rawgl_add_script_test(rawgl_invalid_vertfrag_arity test_invalid_vertfrag_arity)
    rawgl_add_script_test(rawgl_mesh_ao_sponge test_mesh_ao_sponge)
    rawgl_add_script_test(rawgl_mesh_cli_order test_mesh_cli_order)
    rawgl_add_script_test(rawgl_mesh_ply_smoke test_mesh_ply_smoke)
    rawgl_add_script_test(rawgl_missing_input_uniform test_missing_input_uniform)
    rawgl_add_script_test(rawgl_output_array_reflection test_output_array_reflection)
    rawgl_add_script_test(rawgl_single_file_vertfrag test_single_file_vertfrag)
    rawgl_add_script_test(rawgl_uint_uniform test_uint_uniform)
endif()
