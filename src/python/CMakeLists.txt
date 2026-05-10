include_guard(GLOBAL)

function(rawgl_python_override_imported_target target_name)
    if(NOT WIN32)
        return()
    endif()

    if(NOT TARGET ${target_name})
        return()
    endif()

    if(NOT Python_LIBRARY_RELEASE AND NOT Python_LIBRARY_DEBUG)
        return()
    endif()

    get_target_property(_rawgl_target_type ${target_name} TYPE)
    if(_rawgl_target_type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    get_target_property(_rawgl_imported_configs ${target_name} IMPORTED_CONFIGURATIONS)
    if(NOT _rawgl_imported_configs)
        set(_rawgl_imported_configs "")
    endif()

    foreach(_rawgl_cfg IN ITEMS RELEASE DEBUG)
        set(_rawgl_override_library "${Python_LIBRARY_${_rawgl_cfg}}")
        if(_rawgl_override_library)
            list(APPEND _rawgl_imported_configs ${_rawgl_cfg})
            if(_rawgl_target_type STREQUAL "SHARED_LIBRARY")
                set_property(TARGET ${target_name} PROPERTY IMPORTED_IMPLIB_${_rawgl_cfg} "${_rawgl_override_library}")
            else()
                set_property(TARGET ${target_name} PROPERTY IMPORTED_LOCATION_${_rawgl_cfg} "${_rawgl_override_library}")
            endif()
        endif()
    endforeach()

    if(_rawgl_imported_configs)
        list(REMOVE_DUPLICATES _rawgl_imported_configs)
        set_property(TARGET ${target_name} PROPERTY IMPORTED_CONFIGURATIONS "${_rawgl_imported_configs}")
    endif()
endfunction()

rawgl_configure_python_from_prefix_path()
find_package(Python QUIET COMPONENTS Interpreter Development.Module OPTIONAL_COMPONENTS Development.SABIModule)
if(NOT Python_EXECUTABLE OR NOT TARGET Python::Module)
    rawgl_configure_python_from_prefix_path()
    rawgl_configure_manual_python_targets()
endif()
if(NOT Python_EXECUTABLE OR NOT TARGET Python::Interpreter OR NOT TARGET Python::Module)
    message(FATAL_ERROR
        "Could not find Python interpreter and development files. Add a CPython prefix "
        "to CMAKE_PREFIX_PATH, or set standard CMake variables Python_EXECUTABLE, "
        "Python_INCLUDE_DIR, and Python_LIBRARY_RELEASE/Python_LIBRARY_DEBUG.")
endif()
set(RAWGL_PYTHON_VERSION "${Python_VERSION}" PARENT_SCOPE)
set(RAWGL_PYTHON_EXECUTABLE_EFFECTIVE "${Python_EXECUTABLE}" PARENT_SCOPE)

if(WIN32)
    if(Python_LIBRARY_RELEASE AND Python_LIBRARY_DEBUG)
        set(Python_LIBRARIES optimized "${Python_LIBRARY_RELEASE}" debug "${Python_LIBRARY_DEBUG}")
    elseif(Python_LIBRARY_RELEASE)
        set(Python_LIBRARIES "${Python_LIBRARY_RELEASE}")
    elseif(Python_LIBRARY_DEBUG)
        set(Python_LIBRARIES "${Python_LIBRARY_DEBUG}")
    endif()
endif()

if(Python_INCLUDE_DIRS)
    list(GET Python_INCLUDE_DIRS 0 _rawgl_python_include_dir_effective)
    set(RAWGL_PYTHON_INCLUDE_DIR_EFFECTIVE "${_rawgl_python_include_dir_effective}" PARENT_SCOPE)
endif()

if(WIN32)
    rawgl_python_override_imported_target(Python::Module)
    rawgl_python_override_imported_target(Python::Python)
    rawgl_python_override_imported_target(Python::SABIModule)

    if(DEFINED Python_LIBRARY_RELEASE AND Python_LIBRARY_RELEASE)
        set(RAWGL_PYTHON_LIBRARY_RELEASE_EFFECTIVE "${Python_LIBRARY_RELEASE}" PARENT_SCOPE)
    endif()

    if(DEFINED Python_LIBRARY_DEBUG AND Python_LIBRARY_DEBUG)
        set(RAWGL_PYTHON_LIBRARY_DEBUG_EFFECTIVE "${Python_LIBRARY_DEBUG}" PARENT_SCOPE)
    endif()
else()
    if(DEFINED Python_LIBRARIES AND Python_LIBRARIES)
        list(GET Python_LIBRARIES 0 _rawgl_python_library_effective)
        set(RAWGL_PYTHON_LIBRARY_EFFECTIVE "${_rawgl_python_library_effective}" PARENT_SCOPE)
    endif()
endif()

find_package(nanobind CONFIG QUIET)
if(NOT nanobind_FOUND)
    foreach(_rawgl_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_rawgl_prefix}/nanobind/cmake/nanobind-config.cmake"
           OR EXISTS "${_rawgl_prefix}/nanobind/cmake/nanobindConfig.cmake")
            set(nanobind_DIR "${_rawgl_prefix}/nanobind/cmake")
            break()
        endif()
    endforeach()
    if(nanobind_DIR)
        find_package(nanobind CONFIG QUIET)
    endif()

    set(_rawgl_nanobind_cmake_dir "")

    execute_process(
        COMMAND "${Python_EXECUTABLE}" -c
                "import nanobind, sys; p=getattr(nanobind,'cmake_dir',None); print(p() if callable(p) else '')"
        RESULT_VARIABLE _rawgl_nanobind_py_res
        OUTPUT_VARIABLE _rawgl_nanobind_py_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_rawgl_nanobind_py_res EQUAL 0 AND _rawgl_nanobind_py_dir)
        set(_rawgl_nanobind_cmake_dir "${_rawgl_nanobind_py_dir}")
    else()
        execute_process(
            COMMAND "${Python_EXECUTABLE}" -m nanobind --cmakedir
            RESULT_VARIABLE _rawgl_nanobind_mod_res
            OUTPUT_VARIABLE _rawgl_nanobind_mod_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_rawgl_nanobind_mod_res EQUAL 0 AND _rawgl_nanobind_mod_dir)
            set(_rawgl_nanobind_cmake_dir "${_rawgl_nanobind_mod_dir}")
        endif()
    endif()

    if(_rawgl_nanobind_cmake_dir AND
       (EXISTS "${_rawgl_nanobind_cmake_dir}/nanobindConfig.cmake"
        OR EXISTS "${_rawgl_nanobind_cmake_dir}/nanobind-config.cmake"))
        set(nanobind_DIR "${_rawgl_nanobind_cmake_dir}")
        find_package(nanobind CONFIG REQUIRED)
    else()
        message(FATAL_ERROR
            "Could not find nanobind. Set CMAKE_PREFIX_PATH or nanobind_DIR to a prefix containing "
            "nanobindConfig.cmake (or nanobind-config.cmake), or install nanobind into "
            "${Python_EXECUTABLE} (pip install nanobind).")
    endif()
endif()

set(RAWGL_NANOBIND_FOUND 1 PARENT_SCOPE)
if(DEFINED nanobind_DIR AND nanobind_DIR)
    set(RAWGL_NANOBIND_PROVIDER "${nanobind_DIR}" PARENT_SCOPE)
else()
    set(RAWGL_NANOBIND_PROVIDER "system (config)" PARENT_SCOPE)
endif()

nanobind_add_module(rawgl_python
    rawgl_module.cpp)

set_target_properties(rawgl_python PROPERTIES
    OUTPUT_NAME "_rawgl"
    FOLDER "python")

target_compile_definitions(rawgl_python PRIVATE RAWGL_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(rawgl_python PRIVATE rawgl_batch)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND RAWGL_LINUX_USE_LIBCXX)
    target_compile_options(rawgl_python PRIVATE -stdlib=libc++)
    target_link_options(rawgl_python PRIVATE -stdlib=libc++)
    if(TARGET nanobind-static)
        target_compile_options(nanobind-static PRIVATE -stdlib=libc++)
    endif()
endif()

if(MSVC)
    target_compile_definitions(rawgl_python PRIVATE Py_NO_LINK_LIB)

    set(_rawgl_python_defaultlibs_to_ignore "")
    foreach(_rawgl_python_release_lib IN ITEMS "${Python_LIBRARY_RELEASE}" "${Python_SABI_LIBRARY_RELEASE}")
        if(_rawgl_python_release_lib)
            get_filename_component(_rawgl_python_release_name "${_rawgl_python_release_lib}" NAME)
            if(_rawgl_python_release_name)
                list(APPEND _rawgl_python_defaultlibs_to_ignore "${_rawgl_python_release_name}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _rawgl_python_defaultlibs_to_ignore)
    foreach(_rawgl_python_release_name IN LISTS _rawgl_python_defaultlibs_to_ignore)
        target_link_options(rawgl_python PRIVATE "$<$<CONFIG:Debug>:/NODEFAULTLIB:${_rawgl_python_release_name}>")
    endforeach()
endif()

target_include_directories(rawgl_python PRIVATE
    "${PROJECT_SOURCE_DIR}/include")

if(UNIX AND NOT APPLE)
    target_link_options(rawgl_python PRIVATE "-Wl,--exclude-libs,ALL")
endif()

set(_rawgl_py_build_dir "${CMAKE_BINARY_DIR}/python/rawgl")
file(MAKE_DIRECTORY "${_rawgl_py_build_dir}")

set(_rawgl_py_init_src "${CMAKE_CURRENT_SOURCE_DIR}/rawgl/__init__.py")
set(_rawgl_py_init_dst "${_rawgl_py_build_dir}/__init__.py")

add_custom_command(
    OUTPUT "${_rawgl_py_init_dst}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_rawgl_py_build_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_rawgl_py_init_src}" "${_rawgl_py_init_dst}"
    DEPENDS "${_rawgl_py_init_src}"
    VERBATIM)

add_custom_target(rawgl_python_package_files DEPENDS "${_rawgl_py_init_dst}")
add_dependencies(rawgl_python rawgl_python_package_files)

set_target_properties(rawgl_python PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${_rawgl_py_build_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${_rawgl_py_build_dir}")

set(_rawgl_py_install_dir "rawgl")
if(DEFINED SKBUILD_PLATLIB_DIR)
    set(_rawgl_py_install_dir "${SKBUILD_PLATLIB_DIR}/rawgl")
endif()

set(_rawgl_install_python_module ON)
if(RAWGL_BUILD_WHEEL AND NOT DEFINED SKBUILD_PLATLIB_DIR)
    set(_rawgl_install_python_module OFF)
endif()

if(_rawgl_install_python_module)
    install(
        TARGETS rawgl_python
        LIBRARY DESTINATION "${_rawgl_py_install_dir}"
        RUNTIME DESTINATION "${_rawgl_py_install_dir}"
    )
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/rawgl/__init__.py"
        DESTINATION "${_rawgl_py_install_dir}"
    )
endif()
