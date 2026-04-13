include(CTest)

if(BUILD_TESTING)
    function(rawgl_add_cpp_smoke_test target_name source_file)
        add_executable(${target_name}
            ${source_file})
        target_link_libraries(${target_name} PRIVATE rawgl_core)
        target_include_directories(${target_name} PRIVATE
            SRC/RawGL/src
            SRC/RawGL/src/intern)
        set_target_properties(${target_name} PROPERTIES
            FOLDER "tests")
        source_group(TREE "${CMAKE_SOURCE_DIR}/tests" PREFIX "tests" FILES
            "${CMAKE_SOURCE_DIR}/${source_file}")
    endfunction()

    rawgl_add_cpp_smoke_test(rawgl_core_inspect_smoke tests/rawgl_core_inspect_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_graph_smoke tests/rawgl_core_graph_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_system_uniform_reject_smoke tests/rawgl_core_system_uniform_reject_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_shared_context_smoke tests/rawgl_core_shared_context_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_shared_file_resources_smoke tests/rawgl_core_shared_file_resources_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_input_override_smoke tests/rawgl_core_input_override_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_persistent_texture_smoke tests/rawgl_core_persistent_texture_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_persistent_atomic_counter_smoke tests/rawgl_core_persistent_atomic_counter_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_transient_output_reuse_smoke tests/rawgl_core_transient_output_reuse_smoke.cpp)

    add_test(NAME rawgl_core_inspect_smoke
        COMMAND rawgl_core_inspect_smoke)
    set_tests_properties(rawgl_core_inspect_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_graph_smoke
        COMMAND rawgl_core_graph_smoke)
    set_tests_properties(rawgl_core_graph_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_system_uniform_reject_smoke
        COMMAND rawgl_core_system_uniform_reject_smoke)
    set_tests_properties(rawgl_core_system_uniform_reject_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_shared_context_smoke
        COMMAND rawgl_core_shared_context_smoke)
    set_tests_properties(rawgl_core_shared_context_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_input_override_smoke
        COMMAND rawgl_core_input_override_smoke)
    set_tests_properties(rawgl_core_input_override_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_shared_file_resources_smoke
        COMMAND rawgl_core_shared_file_resources_smoke)
    set_tests_properties(rawgl_core_shared_file_resources_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_persistent_texture_smoke
        COMMAND rawgl_core_persistent_texture_smoke)
    set_tests_properties(rawgl_core_persistent_texture_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_persistent_atomic_counter_smoke
        COMMAND rawgl_core_persistent_atomic_counter_smoke)
    set_tests_properties(rawgl_core_persistent_atomic_counter_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_transient_output_reuse_smoke
        COMMAND rawgl_core_transient_output_reuse_smoke)
    set_tests_properties(rawgl_core_transient_output_reuse_smoke PROPERTIES
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
