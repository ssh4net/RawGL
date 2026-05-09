if(RAWGL_TESTS)
    enable_testing()

    add_test(NAME rawgl_io_openmeta_boundary_smoke
        COMMAND ${CMAKE_COMMAND}
            -DRAWGL_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/cmake/rawgl_openmeta_boundary_check.cmake")
    set_tests_properties(rawgl_io_openmeta_boundary_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    function(rawgl_add_cpp_smoke_test target_name source_file)
        add_executable(${target_name}
            ${source_file})
        target_link_libraries(${target_name} PRIVATE rawgl_core)
        set_target_properties(${target_name} PROPERTIES
            FOLDER "tests")
        rawgl_apply_windows_debug_suffix(${target_name})
        source_group(TREE "${CMAKE_SOURCE_DIR}/tests" PREFIX "tests" FILES
            "${CMAKE_SOURCE_DIR}/${source_file}")
    endfunction()

    function(rawgl_add_cpp_io_smoke_test target_name source_file)
        add_executable(${target_name}
            ${source_file})
        target_link_libraries(${target_name} PRIVATE rawgl_io)
        set_target_properties(${target_name} PROPERTIES
            FOLDER "tests")
        rawgl_apply_windows_debug_suffix(${target_name})
        source_group(TREE "${CMAKE_SOURCE_DIR}/tests" PREFIX "tests" FILES
            "${CMAKE_SOURCE_DIR}/${source_file}")
    endfunction()

    function(rawgl_add_cpp_batch_smoke_test target_name source_file)
        add_executable(${target_name}
            ${source_file})
        target_link_libraries(${target_name} PRIVATE rawgl_batch)
        set_target_properties(${target_name} PROPERTIES
            FOLDER "tests")
        rawgl_apply_windows_debug_suffix(${target_name})
        source_group(TREE "${CMAKE_SOURCE_DIR}/tests" PREFIX "tests" FILES
            "${CMAKE_SOURCE_DIR}/${source_file}")
    endfunction()

    rawgl_add_cpp_smoke_test(rawgl_core_inspect_smoke tests/rawgl_core_inspect_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_mesh_inspect_smoke tests/rawgl_core_mesh_inspect_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_graph_smoke tests/rawgl_core_graph_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_cli_codec_options_smoke tests/rawgl_cli_codec_options_smoke.cpp)
    target_include_directories(rawgl_cli_codec_options_smoke PRIVATE
        "${CMAKE_SOURCE_DIR}/src/cli")
    rawgl_add_cpp_smoke_test(rawgl_core_default_vertex_smoke tests/rawgl_core_default_vertex_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_system_uniform_reject_smoke tests/rawgl_core_system_uniform_reject_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_shared_context_smoke tests/rawgl_core_shared_context_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_shared_file_resources_smoke tests/rawgl_core_shared_file_resources_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_host_image_capture_smoke tests/rawgl_core_host_image_capture_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_input_override_smoke tests/rawgl_core_input_override_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_array_element_smoke tests/rawgl_core_array_element_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_persistent_texture_smoke tests/rawgl_core_persistent_texture_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_persistent_atomic_counter_smoke tests/rawgl_core_persistent_atomic_counter_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_core_transient_output_reuse_smoke tests/rawgl_core_transient_output_reuse_smoke.cpp)
    rawgl_add_cpp_smoke_test(rawgl_io_workflow_smoke tests/rawgl_io_workflow_smoke.cpp)
    rawgl_add_cpp_io_smoke_test(rawgl_io_host_image_smoke tests/rawgl_io_host_image_smoke.cpp)
    rawgl_add_cpp_io_smoke_test(rawgl_io_capabilities_smoke tests/rawgl_io_capabilities_smoke.cpp)
    if(TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR)
        rawgl_add_cpp_io_smoke_test(rawgl_io_native_codec_errors_smoke tests/rawgl_io_native_codec_errors_smoke.cpp)
        target_include_directories(rawgl_io_native_codec_errors_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}/src/io")
    endif()
    if(TARGET PNG::PNG AND TARGET JPEG::JPEG AND TARGET OpenEXR::OpenEXR)
        rawgl_add_cpp_io_smoke_test(rawgl_io_native_codecs_smoke tests/rawgl_io_native_codecs_smoke.cpp)
        target_include_directories(rawgl_io_native_codecs_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}/src/io")
    endif()
    if(TARGET JPEG::JPEG AND TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR)
        rawgl_add_cpp_io_smoke_test(rawgl_io_typed_options_matrix_smoke tests/rawgl_io_typed_options_matrix_smoke.cpp)
        rawgl_add_cpp_io_smoke_test(rawgl_io_sky_codec_conversion_smoke tests/rawgl_io_sky_codec_conversion_smoke.cpp)
    endif()
    if(TARGET openjp2)
        rawgl_add_cpp_io_smoke_test(rawgl_io_jpeg2000_native_smoke tests/rawgl_io_jpeg2000_native_smoke.cpp)
    endif()
    if(TARGET TIFF::TIFF)
        rawgl_add_cpp_io_smoke_test(rawgl_io_tiff_native_smoke tests/rawgl_io_tiff_native_smoke.cpp)
        target_include_directories(rawgl_io_tiff_native_smoke PRIVATE
            "${CMAKE_SOURCE_DIR}/src/io")
    endif()
    if(RAWGL_HAS_OPENMETA)
        rawgl_add_cpp_io_smoke_test(rawgl_io_metadata_smoke tests/rawgl_io_metadata_smoke.cpp)
    endif()
    rawgl_add_cpp_batch_smoke_test(rawgl_batch_smoke tests/rawgl_batch_smoke.cpp)

    add_test(NAME rawgl_core_inspect_smoke
        COMMAND rawgl_core_inspect_smoke)
    set_tests_properties(rawgl_core_inspect_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_mesh_inspect_smoke
        COMMAND rawgl_core_mesh_inspect_smoke)
    set_tests_properties(rawgl_core_mesh_inspect_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_graph_smoke
        COMMAND rawgl_core_graph_smoke)
    set_tests_properties(rawgl_core_graph_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_cli_codec_options_smoke
        COMMAND rawgl_cli_codec_options_smoke)
    set_tests_properties(rawgl_cli_codec_options_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_default_vertex_smoke
        COMMAND rawgl_core_default_vertex_smoke)
    set_tests_properties(rawgl_core_default_vertex_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_system_uniform_reject_smoke
        COMMAND rawgl_core_system_uniform_reject_smoke)
    set_tests_properties(rawgl_core_system_uniform_reject_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_shared_context_smoke
        COMMAND rawgl_core_shared_context_smoke)
    set_tests_properties(rawgl_core_shared_context_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_host_image_capture_smoke
        COMMAND rawgl_core_host_image_capture_smoke)
    set_tests_properties(rawgl_core_host_image_capture_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_input_override_smoke
        COMMAND rawgl_core_input_override_smoke)
    set_tests_properties(rawgl_core_input_override_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_core_array_element_smoke
        COMMAND rawgl_core_array_element_smoke)
    set_tests_properties(rawgl_core_array_element_smoke PROPERTIES
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

    add_test(NAME rawgl_io_host_image_smoke
        COMMAND rawgl_io_host_image_smoke)
    set_tests_properties(rawgl_io_host_image_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_test(NAME rawgl_io_capabilities_smoke
        COMMAND rawgl_io_capabilities_smoke)
    set_tests_properties(rawgl_io_capabilities_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    if(TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR)
        add_test(NAME rawgl_io_native_codec_errors_smoke
            COMMAND rawgl_io_native_codec_errors_smoke)
        set_tests_properties(rawgl_io_native_codec_errors_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(TARGET PNG::PNG AND TARGET JPEG::JPEG AND TARGET OpenEXR::OpenEXR)
        add_test(NAME rawgl_io_native_codecs_smoke
            COMMAND rawgl_io_native_codecs_smoke)
        set_tests_properties(rawgl_io_native_codecs_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(TARGET JPEG::JPEG AND TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR)
        add_test(NAME rawgl_io_typed_options_matrix_smoke
            COMMAND rawgl_io_typed_options_matrix_smoke)
        set_tests_properties(rawgl_io_typed_options_matrix_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        add_test(NAME rawgl_io_sky_codec_conversion_smoke
            COMMAND rawgl_io_sky_codec_conversion_smoke)
        set_tests_properties(rawgl_io_sky_codec_conversion_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(TARGET openjp2)
        add_test(NAME rawgl_io_jpeg2000_native_smoke
            COMMAND rawgl_io_jpeg2000_native_smoke)
        set_tests_properties(rawgl_io_jpeg2000_native_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(TARGET TIFF::TIFF)
        add_test(NAME rawgl_io_tiff_native_smoke
            COMMAND rawgl_io_tiff_native_smoke)
        set_tests_properties(rawgl_io_tiff_native_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    add_test(NAME rawgl_io_workflow_smoke
        COMMAND rawgl_io_workflow_smoke)
    set_tests_properties(rawgl_io_workflow_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    if(RAWGL_HAS_OPENMETA)
        add_test(NAME rawgl_io_metadata_smoke
            COMMAND rawgl_io_metadata_smoke)
        set_tests_properties(rawgl_io_metadata_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    add_test(NAME rawgl_batch_smoke
        COMMAND rawgl_batch_smoke)
    set_tests_properties(rawgl_batch_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    if(NOT WIN32)
        find_program(OIIOTOOL_EXECUTABLE
            NAMES oiiotool
            HINTS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES bin)
    endif()

    function(rawgl_add_script_test test_name base_name)
        if(WIN32)
            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND} -E env
                    "RAWGL_BIN_OVERRIDE=$<TARGET_FILE:rawgl>"
                    cmd.exe /C "${CMAKE_SOURCE_DIR}/tests/${base_name}.bat")
        else()
            set(rawgl_test_env
                "RAWGL_BIN=$<TARGET_FILE:rawgl>")
            if(OIIOTOOL_EXECUTABLE)
                list(APPEND rawgl_test_env
                    "RAWGL_OIIOTOOL=${OIIOTOOL_EXECUTABLE}")
            endif()

            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND} -E env
                    ${rawgl_test_env}
                    bash "${CMAKE_SOURCE_DIR}/tests/${base_name}.sh")
        endif()

        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endfunction()

    if(TARGET rawgl)
        rawgl_add_script_test(rawgl_atomic_counter test_atomic_counter)
        rawgl_add_script_test(rawgl_compute_image_chain test_compute_image_chain)
        if(TARGET JPEG::JPEG AND TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR AND TARGET openjp2)
            rawgl_add_script_test(rawgl_cli_native_codec_outputs test_cli_native_codec_outputs)
        endif()
        rawgl_add_script_test(rawgl_cull_parser test_cull_parser)
        rawgl_add_script_test(rawgl_frag_pass test_frag_pass)
        rawgl_add_script_test(rawgl_invalid_atomic_input test_invalid_atomic_input)
        rawgl_add_script_test(rawgl_invalid_bg_color test_invalid_bg_color)
        rawgl_add_script_test(rawgl_invalid_mesh_reference test_invalid_mesh_reference)
        rawgl_add_script_test(rawgl_invalid_numeric_input test_invalid_numeric_input)
        rawgl_add_script_test(rawgl_invalid_pass_reference test_invalid_pass_reference)
        rawgl_add_script_test(rawgl_invalid_pass_size test_invalid_pass_size)
        rawgl_add_script_test(rawgl_invalid_vertfrag_arity test_invalid_vertfrag_arity)
        rawgl_add_script_test(rawgl_mesh_ao_sponge test_mesh_ao_sponge)
        rawgl_add_script_test(rawgl_mesh_cli_order test_mesh_cli_order)
        rawgl_add_script_test(rawgl_mesh_obj_material_smoke test_mesh_obj_material_smoke)
        rawgl_add_script_test(rawgl_mesh_obj_smoke test_mesh_obj_smoke)
        rawgl_add_script_test(rawgl_mesh_ply_smoke test_mesh_ply_smoke)
        rawgl_add_script_test(rawgl_missing_input_uniform test_missing_input_uniform)
        rawgl_add_script_test(rawgl_output_array_reflection test_output_array_reflection)
        rawgl_add_script_test(rawgl_single_file_vertfrag test_single_file_vertfrag)
        rawgl_add_script_test(rawgl_uint_uniform test_uint_uniform)
    endif()

    if(RAWGL_BUILD_PYTHON AND TARGET rawgl_python)
        add_test(NAME rawgl_python_multipass_batch_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_multipass_batch_smoke.py")
        set_tests_properties(rawgl_python_multipass_batch_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        add_test(NAME rawgl_python_batch_close_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_batch_close_smoke.py")
        set_tests_properties(rawgl_python_batch_close_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        add_test(NAME rawgl_python_histogram_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_histogram_smoke.py")
        set_tests_properties(rawgl_python_histogram_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        add_test(NAME rawgl_python_workflow_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_workflow_smoke.py")
        set_tests_properties(rawgl_python_workflow_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        if(TARGET JPEG::JPEG AND TARGET PNG::PNG AND TARGET TIFF::TIFF AND TARGET OpenEXR::OpenEXR)
            add_test(NAME rawgl_python_numpy_typed_io_example
                COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                    "RAWGL_NUMPY_TYPED_IO_OUTPUT_DIR=${CMAKE_SOURCE_DIR}/tests/outputs"
                    "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/examples/NumPy/NumpyTypedIoPipeline.py")
            set_tests_properties(rawgl_python_numpy_typed_io_example PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

            add_test(NAME rawgl_python_sky_codec_conversion_smoke
                COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                    "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_sky_codec_conversion_smoke.py")
            set_tests_properties(rawgl_python_sky_codec_conversion_smoke PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        endif()

        if(TARGET PNG::PNG)
            add_test(NAME rawgl_python_numpy_batch_multipass_example
                COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                    "RAWGL_NUMPY_BATCH_OUTPUT_DIR=${CMAKE_SOURCE_DIR}/tests/outputs"
                    "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/examples/NumPy/NumpyBatchMultipass.py")
            set_tests_properties(rawgl_python_numpy_batch_multipass_example PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        endif()

        if(TARGET JPEG::JPEG)
            add_test(NAME rawgl_python_obj_perspective_example
                COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                    "RAWGL_OBJ_PERSPECTIVE_OUTPUT_PATH=${CMAKE_SOURCE_DIR}/tests/outputs/RenderObjPerspectiveBaseColor_python.jpg"
                    "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/examples/Mesh/OBJ/RenderObjPerspectiveBaseColor.py")
            set_tests_properties(rawgl_python_obj_perspective_example PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        endif()

        if(RAWGL_HAS_OPENMETA)
            add_test(NAME rawgl_python_metadata_smoke
                COMMAND ${CMAKE_COMMAND} -E env
                    "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                    "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_metadata_smoke.py")
            set_tests_properties(rawgl_python_metadata_smoke PROPERTIES
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        endif()

        add_test(NAME rawgl_python_sequence_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_sequence_smoke.py")
        set_tests_properties(rawgl_python_sequence_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()

    if(RAWGL_BUILD_PYTHON AND TARGET rawgl_python)
        add_test(NAME rawgl_python_import_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "PYTHONPATH=${CMAKE_BINARY_DIR}/python"
                "${RAWGL_PYTHON_EXECUTABLE_EFFECTIVE}" "${CMAKE_SOURCE_DIR}/tests/python/rawgl_python_import_smoke.py")
        set_tests_properties(rawgl_python_import_smoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endif()
