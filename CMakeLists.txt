cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(RawGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(RAWGL_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include("${RAWGL_CMAKE_DIR}/rawgl_options.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_helpers.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_dependencies.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_targets.cmake")
include("${RAWGL_CMAKE_DIR}/rawgl_testing.cmake")
