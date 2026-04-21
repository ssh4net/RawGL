include_guard(GLOBAL)

if(NOT RAWGL_BUILD_DOCS AND NOT RAWGL_BUILD_SPHINX_DOCS)
    return()
endif()

find_package(Doxygen REQUIRED)

set(_rawgl_docs_root_dir "${CMAKE_BINARY_DIR}/docs")
set(_rawgl_doxygen_out_dir "${_rawgl_docs_root_dir}/doxygen")
set(_rawgl_doxygen_cfg "${CMAKE_CURRENT_BINARY_DIR}/RawGLDoxyfile")

set(RAWGL_DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
set(RAWGL_DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(RAWGL_DOXYGEN_OUTPUT_DIR "${_rawgl_doxygen_out_dir}")
set(RAWGL_DOXYGEN_INPUT_MAINPAGE "${PROJECT_SOURCE_DIR}/README.md")
set(RAWGL_DOXYGEN_INPUT_PUBLIC_HEADERS "${PROJECT_SOURCE_DIR}/include/rawgl")

configure_file(
    "${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
    "${_rawgl_doxygen_cfg}"
    @ONLY
)

add_custom_target(rawgl_docs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_rawgl_doxygen_out_dir}"
    COMMAND "${DOXYGEN_EXECUTABLE}" "${_rawgl_doxygen_cfg}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating RawGL Doxygen documentation"
    VERBATIM
)

if(NOT RAWGL_BUILD_SPHINX_DOCS)
    return()
endif()

find_package(Python REQUIRED COMPONENTS Interpreter)

execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import sphinx, breathe"
    RESULT_VARIABLE _rawgl_sphinx_import_res
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT _rawgl_sphinx_import_res EQUAL 0)
    message(FATAL_ERROR
        "RAWGL_BUILD_SPHINX_DOCS requires Python packages 'sphinx' and 'breathe' "
        "for: ${Python_EXECUTABLE}. Install them (see docs/requirements.txt) and reconfigure.")
endif()

set(_rawgl_doxygen_xml_dir "${_rawgl_doxygen_out_dir}/xml")
file(TO_CMAKE_PATH "${_rawgl_doxygen_xml_dir}" RAWGL_DOXYGEN_XML_DIR)

set(_rawgl_sphinx_source_dir "${PROJECT_SOURCE_DIR}/docs/sphinx")
set(_rawgl_sphinx_conf_dir "${CMAKE_CURRENT_BINARY_DIR}/docs/sphinx")
set(_rawgl_sphinx_out_dir "${_rawgl_docs_root_dir}/html")
file(MAKE_DIRECTORY "${_rawgl_sphinx_conf_dir}")

configure_file(
    "${PROJECT_SOURCE_DIR}/docs/sphinx/conf.py.in"
    "${_rawgl_sphinx_conf_dir}/conf.py"
    @ONLY
)

add_custom_target(rawgl_docs_sphinx
    DEPENDS rawgl_docs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_rawgl_sphinx_out_dir}"
    COMMAND "${Python_EXECUTABLE}" -m sphinx -b html
            -c "${_rawgl_sphinx_conf_dir}"
            "${_rawgl_sphinx_source_dir}"
            "${_rawgl_sphinx_out_dir}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating RawGL Sphinx documentation"
    VERBATIM
)
