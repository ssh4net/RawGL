# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2022-2026 Erium Vladlen.

if(NOT DEFINED RAWGL_SOURCE_DIR)
    message(FATAL_ERROR "RAWGL_SOURCE_DIR was not provided")
endif()

set(rawgl_openmeta_bridge_file "${RAWGL_SOURCE_DIR}/src/io/openmeta_bridge.cpp")

file(GLOB rawgl_io_boundary_files
    "${RAWGL_SOURCE_DIR}/src/io/*.cpp"
    "${RAWGL_SOURCE_DIR}/src/io/*.h")

set(rawgl_openmeta_boundary_failures "")

foreach(rawgl_candidate IN LISTS rawgl_io_boundary_files)
    if(rawgl_candidate STREQUAL rawgl_openmeta_bridge_file)
        continue()
    endif()

    file(READ "${rawgl_candidate}" rawgl_candidate_text)

    if(rawgl_candidate_text MATCHES "#[ \t]*include[ \t]*<openmeta/")
        string(APPEND rawgl_openmeta_boundary_failures
            "Direct OpenMeta include outside openmeta_bridge.cpp: ${rawgl_candidate}\n")
    endif()

    if(rawgl_candidate_text MATCHES "openmeta::")
        string(APPEND rawgl_openmeta_boundary_failures
            "Direct OpenMeta symbol use outside openmeta_bridge.cpp: ${rawgl_candidate}\n")
    endif()
endforeach()

if(NOT rawgl_openmeta_boundary_failures STREQUAL "")
    message(FATAL_ERROR "${rawgl_openmeta_boundary_failures}")
endif()
