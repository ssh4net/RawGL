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
    src/io/image_io.cpp
    src/support/log.cpp
    src/io/mesh_io.cpp
    src/gl/gl_utils.cpp
    src/gl/texture.cpp)

list(APPEND RAWGL_CORE_SOURCES ${RAWGL_MINIPLY_SOURCES})

add_library(rawgl_core STATIC ${RAWGL_CORE_SOURCES})
add_library(RawGL::core ALIAS rawgl_core)

add_executable(rawgl src/app/main.cpp)
add_executable(RawGL::rawgl ALIAS rawgl)

if(WIN32)
    enable_language(RC)
    target_sources(rawgl PRIVATE
        src/app/windows/RawGL.rc)
endif()

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
    target_compile_definitions(rawgl_core PRIVATE
        _MSVC
        OIIO_STATIC_DEFINE=1)
    target_compile_definitions(rawgl PRIVATE _CONSOLE _MSVC)
    if(RAWGL_WINDOWS_UTF8)
        target_compile_options(rawgl_core PRIVATE /utf-8)
        target_compile_options(rawgl PRIVATE /utf-8)
    endif()
endif()

target_link_libraries(rawgl_core PUBLIC
    ${RAWGL_OPENGL_LOADER_TARGET}
    ${RAWGL_GLFW_TARGET}
    ${RAWGL_OIIO_TARGETS}
    ${RAWGL_MINIPLY_TARGET}
    Threads::Threads
    ${RAWGL_EXTRA_WINDOWS_LIBS})

if(TARGET fmt::fmt)
    target_link_libraries(rawgl_core PUBLIC fmt::fmt)
endif()

if(NOT WIN32)
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

install(TARGETS rawgl_core rawgl
    EXPORT RawGLTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
    include/rawgl/rawgl_core.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rawgl)

install(EXPORT RawGLTargets
    NAMESPACE RawGL::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RawGL)
