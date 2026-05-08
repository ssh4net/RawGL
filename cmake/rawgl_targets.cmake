set(RAWGL_SUPPORT_SOURCES
    src/support/log.cpp)

set(RAWGL_CORE_SOURCES
    src/cli/cli_graph.cpp
    src/cli/cli_parser.cpp
    src/runtime/pass_input.cpp
    src/runtime/pass_output.cpp
    src/core/context.cpp
    src/core/graph/graph_build.cpp
    src/core/graph/graph_resources.cpp
    src/core/graph/graph_runtime_plan.cpp
    src/core/graph/graph_shared.cpp
    src/core/graph/graph_validation.cpp
    src/core/graph/shader_interface_cache.cpp
    src/runtime/sequence.cpp
    src/gl/program.cpp
    src/gl/program_manager.cpp
    src/io/mesh_io.cpp
    src/gl/gl_utils.cpp
    src/gl/texture.cpp)

list(APPEND RAWGL_CORE_SOURCES ${RAWGL_MINIPLY_SOURCES})

set(RAWGL_IO_SOURCES
    src/io/exr_backend.cpp
    src/io/image_backend.cpp
    src/io/image_io.cpp
    src/io/io_facade.cpp
    src/io/io_runtime.cpp
    src/io/jpeg2000_backend.cpp
    src/io/jpg_backend.cpp
    src/io/metadata_reader.cpp
    src/io/openmeta_bridge.cpp
    src/io/png_backend.cpp
    src/io/tiff_backend.cpp
    src/io/texture_loader.cpp
    src/io/output_writer.cpp)

set(RAWGL_BATCH_SOURCES
    src/batch/batch_runner.cpp)

add_library(rawgl_support STATIC ${RAWGL_SUPPORT_SOURCES})
add_library(RawGL::support ALIAS rawgl_support)
rawgl_apply_windows_debug_suffix(rawgl_support)

add_library(rawgl_core STATIC ${RAWGL_CORE_SOURCES})
add_library(RawGL::core ALIAS rawgl_core)
rawgl_apply_windows_debug_suffix(rawgl_core)

add_library(rawgl_io STATIC ${RAWGL_IO_SOURCES})
add_library(RawGL::io ALIAS rawgl_io)
rawgl_apply_windows_debug_suffix(rawgl_io)

add_library(rawgl_batch STATIC ${RAWGL_BATCH_SOURCES})
add_library(RawGL::batch ALIAS rawgl_batch)
rawgl_apply_windows_debug_suffix(rawgl_batch)

if(RAWGL_BUILD_APP)
    add_executable(rawgl src/app/main.cpp)
    add_executable(RawGL::rawgl ALIAS rawgl)
endif()

if(WIN32 AND RAWGL_BUILD_APP)
    enable_language(RC)
    target_sources(rawgl PRIVATE
        src/app/windows/RawGL.rc)
endif()

target_include_directories(rawgl_support
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/support
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
        "${RAWGL_SPDLOG_INCLUDE_DIR}")

target_include_directories(rawgl_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/graph
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io
        ${CMAKE_CURRENT_SOURCE_DIR}/src/support
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniply
        "${RAWGL_LIBRAW_INCLUDE_DIR}"
        "${RAWGL_SPDLOG_INCLUDE_DIR}")

target_include_directories(rawgl_io
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/graph
        ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gl
        ${CMAKE_CURRENT_SOURCE_DIR}/src/support
        "${RAWGL_LIBRAW_INCLUDE_DIR}"
        "${RAWGL_SPDLOG_INCLUDE_DIR}")

target_include_directories(rawgl_batch
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NOT TARGET miniply::miniply)
target_include_directories(rawgl_core PRIVATE
    "${miniply_INCLUDE_DIR}")
endif()
if(TARGET rapidobj::rapidobj)
    target_compile_definitions(rawgl_core PRIVATE RAWGL_HAS_RAPIDOBJ=1)
endif()

target_compile_definitions(rawgl_support PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    SPDLOG_FMT_EXTERNAL)

target_compile_definitions(rawgl_core PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    RAWGL_USE_GLEW
    SPDLOG_FMT_EXTERNAL)

target_compile_definitions(rawgl_io PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    RAWGL_USE_GLEW
    SPDLOG_FMT_EXTERNAL)

if(RAWGL_HAS_OPENMETA)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_OPENMETA=1)
endif()
if(TARGET PNG::PNG)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_LIBPNG=1)
endif()
if(TARGET JPEG::JPEG)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_LIBJPEG=1)
endif()
if(TARGET TIFF::TIFF)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_LIBTIFF=1)
endif()
if(TARGET OpenEXR::OpenEXR)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_OPENEXR=1)
endif()
if(TARGET openjp2)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_OPENJPEG=1)
endif()
if(TARGET openjph)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_OPENJPH=1)
endif()
if(RAWGL_LIBRAW_INCLUDE_DIR)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_LIBRAW_HEADERS=1)
endif()
if(RAWGL_JXL_INCLUDE_DIR)
    target_compile_definitions(rawgl_io PRIVATE RAWGL_HAS_JPEGXL_HEADERS=1)
    target_include_directories(rawgl_io PRIVATE "${RAWGL_JXL_INCLUDE_DIR}")
endif()

target_compile_definitions(rawgl_batch PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

if(WIN32)
    target_compile_definitions(rawgl_support PRIVATE
        _MSVC
        OIIO_STATIC_DEFINE=1)
    target_compile_definitions(rawgl_core PRIVATE
        _MSVC
        OIIO_STATIC_DEFINE=1)
    target_compile_definitions(rawgl_io PRIVATE
        _MSVC
        OIIO_STATIC_DEFINE=1)
    target_compile_definitions(rawgl_batch PRIVATE
        _MSVC)
    if(RAWGL_BUILD_APP)
        target_compile_definitions(rawgl PRIVATE _CONSOLE _MSVC)
    endif()
    if(RAWGL_WINDOWS_UTF8 AND RAWGL_BUILD_APP)
        target_compile_options(rawgl_support PRIVATE /utf-8)
        target_compile_options(rawgl_core PRIVATE /utf-8)
        target_compile_options(rawgl_io PRIVATE /utf-8)
        target_compile_options(rawgl_batch PRIVATE /utf-8)
        target_compile_options(rawgl PRIVATE /utf-8)
    endif()
endif()

target_link_libraries(rawgl_support PUBLIC
    Threads::Threads)

target_link_libraries(rawgl_core PUBLIC
    ${RAWGL_OPENGL_LOADER_TARGET}
    ${RAWGL_GLFW_TARGET}
    rawgl_support
    rawgl_io
    ${RAWGL_MINIPLY_TARGET}
    Threads::Threads)
if(TARGET rapidobj::rapidobj)
    target_link_libraries(rawgl_core PRIVATE rapidobj::rapidobj)
endif()

target_link_libraries(rawgl_io PUBLIC
    rawgl_support
    ${RAWGL_OPENGL_LOADER_TARGET}
    ${RAWGL_OIIO_TARGETS}
    ${RAWGL_EXTRA_WINDOWS_LIBS})

if(TARGET PNG::PNG)
    target_link_libraries(rawgl_io PUBLIC PNG::PNG)
endif()
if(TARGET JPEG::JPEG)
    target_link_libraries(rawgl_io PUBLIC JPEG::JPEG)
endif()
if(TARGET TIFF::TIFF)
    target_link_libraries(rawgl_io PUBLIC TIFF::TIFF)
endif()
if(TARGET OpenEXR::OpenEXR)
    target_link_libraries(rawgl_io PUBLIC OpenEXR::OpenEXR)
endif()
if(TARGET openjp2)
    target_link_libraries(rawgl_io PUBLIC openjp2)
endif()
if(TARGET openjph)
    target_link_libraries(rawgl_io PUBLIC openjph)
endif()

if(RAWGL_HAS_OPENMETA)
    target_link_libraries(rawgl_io PUBLIC OpenMeta::openmeta)
endif()

target_link_libraries(rawgl_batch PUBLIC
    rawgl_core
    rawgl_io)

if(TARGET fmt::fmt)
    target_link_libraries(rawgl_support PUBLIC fmt::fmt)
    target_link_libraries(rawgl_core PUBLIC fmt::fmt)
endif()

if(NOT WIN32)
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND RAWGL_LINUX_USE_LIBCXX)
    target_compile_options(rawgl_support PUBLIC -stdlib=libc++)
    target_link_options(rawgl_support PUBLIC -stdlib=libc++)
    target_compile_options(rawgl_core PUBLIC -stdlib=libc++)
    target_link_options(rawgl_core PUBLIC -stdlib=libc++)
    target_compile_options(rawgl_io PUBLIC -stdlib=libc++)
    target_link_options(rawgl_io PUBLIC -stdlib=libc++)
    target_compile_options(rawgl_batch PUBLIC -stdlib=libc++)
    target_link_options(rawgl_batch PUBLIC -stdlib=libc++)
endif()

if(RAWGL_BUILD_APP)
    target_link_libraries(rawgl PRIVATE rawgl_core)

    set_target_properties(rawgl PROPERTIES
        OUTPUT_NAME RawGL)
    rawgl_apply_windows_debug_suffix(rawgl)
endif()

if(RAWGL_INSTALL_DEV_ARTIFACTS)
    if(RAWGL_BUILD_APP)
        install(TARGETS rawgl_support rawgl_core rawgl_io rawgl_batch rawgl
            EXPORT RawGLTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    else()
        install(TARGETS rawgl_support rawgl_core rawgl_io rawgl_batch
            EXPORT RawGLTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    install(FILES
        include/rawgl/rawgl.h
        include/rawgl/rawgl_batch.h
        include/rawgl/rawgl_cli.h
        include/rawgl/rawgl_core.h
        include/rawgl/rawgl_io.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rawgl)

    install(EXPORT RawGLTargets
        NAMESPACE RawGL::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RawGL)
endif()
