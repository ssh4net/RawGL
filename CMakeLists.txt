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

find_package(Threads REQUIRED)

if(WIN32)
    find_package(OpenGL REQUIRED)
else()
    find_package(OpenGL REQUIRED COMPONENTS OpenGL GLX)
    find_package(X11 REQUIRED)
    find_package(OpenEXR CONFIG REQUIRED)
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

if(NOT EXISTS "${RAWGL_MINIPLY_INCLUDE_DIR}/miniply.h")
    message(FATAL_ERROR "miniply.h was not found. Set RAWGL_MINIPLY_INCLUDE_DIR to the directory that contains it.")
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

set(RAWGL_SOURCES
    SRC/RawGL/src/PassInput.cpp
    SRC/RawGL/src/PassOutput.cpp
    SRC/RawGL/src/RawGL.cpp
    SRC/RawGL/src/Sequence.cpp
    SRC/RawGL/src/intern/GLProgram.cpp
    SRC/RawGL/src/intern/GLProgramManager.cpp
    SRC/RawGL/src/intern/ImageUtils.cpp
    SRC/RawGL/src/intern/Log.cpp
    SRC/RawGL/src/intern/MESH_IO.CPP
    SRC/RawGL/src/intern/OpenGLUtils.cpp
    SRC/RawGL/src/intern/Texture.cpp)

if(WIN32)
    enable_language(RC)
    list(APPEND RAWGL_SOURCES
        SRC/RawGL/RawGL.rc)
endif()

add_executable(rawgl ${RAWGL_SOURCES})
add_executable(RawGL::rawgl ALIAS rawgl)

target_include_directories(rawgl PRIVATE
    SRC/RawGL/src
    SRC/RawGL/src/intern
    "${RAWGL_MINIPLY_INCLUDE_DIR}"
    "${RAWGL_LIBRAW_INCLUDE_DIR}"
    "${RAWGL_SPDLOG_INCLUDE_DIR}")

target_compile_definitions(rawgl PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    RAWGL_USE_GLEW
    SPDLOG_FMT_EXTERNAL)

if(WIN32)
    target_compile_definitions(rawgl PRIVATE _CONSOLE _MSVC)
    target_compile_options(rawgl PRIVATE /utf-8)
endif()

target_link_libraries(rawgl PRIVATE
    ${RAWGL_OPENGL_LOADER_TARGET}
    ${RAWGL_GLFW_TARGET}
    ${RAWGL_OIIO_TARGETS}
    Threads::Threads
    ${RAWGL_EXTRA_WINDOWS_LIBS})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rawgl PRIVATE -stdlib=libc++)
    target_link_options(rawgl PRIVATE -stdlib=libc++)
endif()

set_target_properties(rawgl PROPERTIES
    OUTPUT_NAME RawGL)
