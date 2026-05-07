function(rawgl_add_windows_imported_library target_name)
    cmake_parse_arguments(ARG "" "INCLUDE_DIR;RELEASE;DEBUG" "" ${ARGN})
    if(TARGET ${target_name})
        return()
    endif()
    if(ARG_RELEASE AND NOT EXISTS "${ARG_RELEASE}")
        set(ARG_RELEASE "")
    endif()
    if(ARG_DEBUG AND NOT EXISTS "${ARG_DEBUG}")
        set(ARG_DEBUG "")
    endif()
    if(NOT ARG_RELEASE AND NOT ARG_DEBUG)
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
    set_target_properties(${target_name} PROPERTIES
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release)
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

function(rawgl_windows_config_library_expr out_var release_path debug_path)
    set(rawgl_expr)

    if(debug_path)
        list(APPEND rawgl_expr
            "$<$<CONFIG:Debug>:${debug_path}>")
    elseif(release_path)
        list(APPEND rawgl_expr
            "$<$<CONFIG:Debug>:${release_path}>")
    endif()

    if(release_path)
        list(APPEND rawgl_expr
            "$<$<NOT:$<CONFIG:Debug>>:${release_path}>")
    elseif(debug_path)
        list(APPEND rawgl_expr
            "$<$<NOT:$<CONFIG:Debug>>:${debug_path}>")
    endif()

    set(${out_var} "${rawgl_expr}" PARENT_SCOPE)
endfunction()

function(rawgl_add_imported_interface_alias alias_target target_name)
    if(TARGET ${alias_target} OR NOT TARGET ${target_name})
        return()
    endif()

    add_library(${alias_target} INTERFACE IMPORTED)
    set_target_properties(${alias_target} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${target_name}")
endfunction()

function(rawgl_configure_python_from_prefix_path)
    if(WIN32)
        set(_rawgl_python_debug_suffix "${RAWGL_WINDOWS_DEBUG_SUFFIX}")
        if(NOT _rawgl_python_debug_suffix)
            set(_rawgl_python_debug_suffix d)
        endif()
        set(_rawgl_python_names
            python.exe
            python3.exe
            "python_${_rawgl_python_debug_suffix}.exe"
            "python${_rawgl_python_debug_suffix}.exe"
            python3
            python)
        set(_rawgl_python_library_suffixes libs lib)
    else()
        set(_rawgl_python_names python3 python)
        set(_rawgl_python_library_suffixes lib lib64)
    endif()

    set(_rawgl_python_roots)
    foreach(_rawgl_python_root_hint IN ITEMS "${Python_ROOT_DIR}" "${Python3_ROOT_DIR}")
        if(_rawgl_python_root_hint)
            list(APPEND _rawgl_python_roots "${_rawgl_python_root_hint}")
        endif()
    endforeach()

    set(_rawgl_python_executable "")
    foreach(_rawgl_python_existing_executable IN ITEMS "${Python_EXECUTABLE}" "${Python3_EXECUTABLE}")
        if(_rawgl_python_existing_executable AND EXISTS "${_rawgl_python_existing_executable}")
            set(_rawgl_python_executable "${_rawgl_python_existing_executable}")
            get_filename_component(_rawgl_python_executable_dir "${_rawgl_python_existing_executable}" DIRECTORY)
            get_filename_component(_rawgl_python_executable_dir_name "${_rawgl_python_executable_dir}" NAME)
            string(TOLOWER "${_rawgl_python_executable_dir_name}" _rawgl_python_executable_dir_name_lower)
            if(_rawgl_python_executable_dir_name_lower STREQUAL "bin"
               OR _rawgl_python_executable_dir_name_lower STREQUAL "scripts")
                get_filename_component(_rawgl_python_executable_root "${_rawgl_python_executable_dir}" DIRECTORY)
            else()
                set(_rawgl_python_executable_root "${_rawgl_python_executable_dir}")
            endif()
            list(APPEND _rawgl_python_roots "${_rawgl_python_executable_root}")
            break()
        endif()
    endforeach()

    foreach(_rawgl_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(_rawgl_prefix)
            list(APPEND _rawgl_python_roots "${_rawgl_prefix}")
        endif()
    endforeach()
    if(_rawgl_python_roots)
        list(REMOVE_DUPLICATES _rawgl_python_roots)
    endif()

    if(NOT _rawgl_python_executable)
        foreach(_rawgl_prefix IN LISTS _rawgl_python_roots)
            find_program(_rawgl_python_executable_candidate
                NAMES ${_rawgl_python_names}
                PATHS "${_rawgl_prefix}"
                PATH_SUFFIXES "" bin Scripts
                NO_DEFAULT_PATH
                NO_CACHE)

            if(_rawgl_python_executable_candidate)
                set(_rawgl_python_executable "${_rawgl_python_executable_candidate}")
                file(TO_CMAKE_PATH "${_rawgl_prefix}" _rawgl_python_root)
                break()
            endif()
        endforeach()
    endif()

    if(NOT _rawgl_python_executable)
        return()
    endif()

    if(NOT _rawgl_python_root)
        get_filename_component(_rawgl_python_executable_dir "${_rawgl_python_executable}" DIRECTORY)
        get_filename_component(_rawgl_python_executable_dir_name "${_rawgl_python_executable_dir}" NAME)
        string(TOLOWER "${_rawgl_python_executable_dir_name}" _rawgl_python_executable_dir_name_lower)
        if(_rawgl_python_executable_dir_name_lower STREQUAL "bin"
           OR _rawgl_python_executable_dir_name_lower STREQUAL "scripts")
            get_filename_component(_rawgl_python_root "${_rawgl_python_executable_dir}" DIRECTORY)
        else()
            set(_rawgl_python_root "${_rawgl_python_executable_dir}")
        endif()
    endif()

    set(Python_EXECUTABLE "${_rawgl_python_executable}" CACHE FILEPATH "Python interpreter" FORCE)
    set(Python3_EXECUTABLE "${_rawgl_python_executable}" CACHE FILEPATH "Python interpreter" FORCE)
    set(Python_ROOT_DIR "${_rawgl_python_root}" CACHE PATH "Python root directory" FORCE)
    set(Python3_ROOT_DIR "${_rawgl_python_root}" CACHE PATH "Python root directory" FORCE)
    set(Python_FIND_STRATEGY LOCATION CACHE STRING "Python lookup strategy" FORCE)
    set(Python3_FIND_STRATEGY LOCATION CACHE STRING "Python lookup strategy" FORCE)
    if(WIN32)
        set(Python_FIND_REGISTRY NEVER CACHE STRING "Python registry lookup policy" FORCE)
        set(Python3_FIND_REGISTRY NEVER CACHE STRING "Python registry lookup policy" FORCE)
    endif()

    execute_process(
        COMMAND "${_rawgl_python_executable}" -c
                [=[
import sys
import sysconfig
ldversion = sysconfig.get_config_var("LDVERSION") or sysconfig.get_config_var("VERSION") or f"{sys.version_info[0]}.{sys.version_info[1]}"
print(f"{sys.version_info[0]}.{sys.version_info[1]}")
print(f"{sys.version_info[0]}{sys.version_info[1]}")
print(ldversion)
print(ldversion.replace(".", ""))
print(sys.version.split()[0])
print(getattr(sys.implementation, "name", "cpython"))
print(sysconfig.get_config_var("SOABI") or "")
print(sysconfig.get_config_var("SOSABI") or "")
print(sysconfig.get_config_var("EXT_SUFFIX") or "")
]=]
        RESULT_VARIABLE _rawgl_python_version_res
        OUTPUT_VARIABLE _rawgl_python_version_out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    set(_rawgl_python_include_suffixes include)
    set(_rawgl_python_release_names python3 python)
    set(_rawgl_python_debug_names)
    set(_rawgl_python_sabi_names python3)
    if(_rawgl_python_version_res EQUAL 0 AND _rawgl_python_version_out)
        string(REPLACE "\n" ";" _rawgl_python_version_parts "${_rawgl_python_version_out}")
        list(LENGTH _rawgl_python_version_parts _rawgl_python_version_part_count)
        if(_rawgl_python_version_part_count GREATER_EQUAL 2)
            list(GET _rawgl_python_version_parts 0 _rawgl_python_version_dot)
            list(GET _rawgl_python_version_parts 1 _rawgl_python_version_nodot)
            list(APPEND _rawgl_python_include_suffixes
                "include/python${_rawgl_python_version_dot}"
                "include/python${_rawgl_python_version_dot}m"
                "include/python${_rawgl_python_version_nodot}")
            set(_rawgl_python_release_names
                "python${_rawgl_python_version_nodot}"
                "python${_rawgl_python_version_dot}"
                python3
                python)
            set(_rawgl_python_debug_names
                "python${_rawgl_python_version_nodot}_d"
                "python${_rawgl_python_version_nodot}d"
                "python${_rawgl_python_version_dot}_d"
                "python${_rawgl_python_version_dot}d")
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 4)
            list(GET _rawgl_python_version_parts 2 _rawgl_python_ldversion)
            list(GET _rawgl_python_version_parts 3 _rawgl_python_ldversion_nodot)
            if(_rawgl_python_ldversion_nodot)
                list(PREPEND _rawgl_python_release_names "python${_rawgl_python_ldversion_nodot}")
                list(PREPEND _rawgl_python_debug_names
                    "python${_rawgl_python_ldversion_nodot}_d"
                    "python${_rawgl_python_ldversion_nodot}d")
            endif()
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 5)
            list(GET _rawgl_python_version_parts 4 _rawgl_python_full_version)
            set(Python_VERSION "${_rawgl_python_full_version}" CACHE STRING "Python version" FORCE)
            set(Python3_VERSION "${_rawgl_python_full_version}" CACHE STRING "Python version" FORCE)
            set(Python_VERSION "${_rawgl_python_full_version}" PARENT_SCOPE)
            set(Python3_VERSION "${_rawgl_python_full_version}" PARENT_SCOPE)
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 6)
            list(GET _rawgl_python_version_parts 5 _rawgl_python_interpreter_id)
            if(_rawgl_python_interpreter_id STREQUAL "cpython")
                set(_rawgl_python_interpreter_id Python)
            endif()
            set(Python_INTERPRETER_ID "${_rawgl_python_interpreter_id}" CACHE STRING "Python interpreter id" FORCE)
            set(Python3_INTERPRETER_ID "${_rawgl_python_interpreter_id}" CACHE STRING "Python interpreter id" FORCE)
            set(Python_INTERPRETER_ID "${_rawgl_python_interpreter_id}" PARENT_SCOPE)
            set(Python3_INTERPRETER_ID "${_rawgl_python_interpreter_id}" PARENT_SCOPE)
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 7)
            list(GET _rawgl_python_version_parts 6 _rawgl_python_soabi)
            set(Python_SOABI "${_rawgl_python_soabi}" CACHE STRING "Python SOABI" FORCE)
            set(Python3_SOABI "${_rawgl_python_soabi}" CACHE STRING "Python SOABI" FORCE)
            set(Python_SOABI "${_rawgl_python_soabi}" PARENT_SCOPE)
            set(Python3_SOABI "${_rawgl_python_soabi}" PARENT_SCOPE)
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 8)
            list(GET _rawgl_python_version_parts 7 _rawgl_python_sosabi)
            set(Python_SOSABI "${_rawgl_python_sosabi}" CACHE STRING "Python SOSABI" FORCE)
            set(Python3_SOSABI "${_rawgl_python_sosabi}" CACHE STRING "Python SOSABI" FORCE)
            set(Python_SOSABI "${_rawgl_python_sosabi}" PARENT_SCOPE)
            set(Python3_SOSABI "${_rawgl_python_sosabi}" PARENT_SCOPE)
        endif()
        if(_rawgl_python_version_part_count GREATER_EQUAL 9)
            list(GET _rawgl_python_version_parts 8 _rawgl_python_extension_suffix)
            set(Python_EXTENSION_SUFFIX "${_rawgl_python_extension_suffix}" CACHE STRING "Python extension suffix" FORCE)
            set(Python3_EXTENSION_SUFFIX "${_rawgl_python_extension_suffix}" CACHE STRING "Python extension suffix" FORCE)
            set(Python_EXTENSION_SUFFIX "${_rawgl_python_extension_suffix}" PARENT_SCOPE)
            set(Python3_EXTENSION_SUFFIX "${_rawgl_python_extension_suffix}" PARENT_SCOPE)
        endif()
    endif()
    if(_rawgl_python_release_names)
        list(REMOVE_DUPLICATES _rawgl_python_release_names)
    endif()
    if(_rawgl_python_debug_names)
        list(REMOVE_DUPLICATES _rawgl_python_debug_names)
    endif()

    if(NOT Python_INCLUDE_DIR AND Python3_INCLUDE_DIR)
        set(Python_INCLUDE_DIR "${Python3_INCLUDE_DIR}")
    endif()
    if(NOT Python_INCLUDE_DIR)
        find_path(_rawgl_python_include_dir
            NAMES Python.h
            PATHS "${_rawgl_python_root}"
            PATH_SUFFIXES ${_rawgl_python_include_suffixes}
            NO_DEFAULT_PATH
            NO_CACHE)
        if(_rawgl_python_include_dir)
            set(Python_INCLUDE_DIR "${_rawgl_python_include_dir}" CACHE PATH "Python include directory" FORCE)
            set(Python3_INCLUDE_DIR "${_rawgl_python_include_dir}" CACHE PATH "Python include directory" FORCE)
        endif()
    endif()
    if(Python_INCLUDE_DIR)
        set(Python_INCLUDE_DIRS "${Python_INCLUDE_DIR}" CACHE PATH "Python include directories" FORCE)
        set(Python3_INCLUDE_DIRS "${Python_INCLUDE_DIR}" CACHE PATH "Python include directories" FORCE)
        set(Python_INCLUDE_DIR "${Python_INCLUDE_DIR}" PARENT_SCOPE)
        set(Python3_INCLUDE_DIR "${Python_INCLUDE_DIR}" PARENT_SCOPE)
        set(Python_INCLUDE_DIRS "${Python_INCLUDE_DIR}" PARENT_SCOPE)
        set(Python3_INCLUDE_DIRS "${Python_INCLUDE_DIR}" PARENT_SCOPE)
    endif()

    if(NOT Python_LIBRARY AND Python3_LIBRARY)
        set(Python_LIBRARY "${Python3_LIBRARY}")
    endif()
    if(NOT Python_LIBRARY_RELEASE AND Python3_LIBRARY_RELEASE)
        set(Python_LIBRARY_RELEASE "${Python3_LIBRARY_RELEASE}")
    endif()
    if(NOT Python_LIBRARY AND NOT Python_LIBRARY_RELEASE)
        find_library(_rawgl_python_library_release
            NAMES ${_rawgl_python_release_names}
            PATHS "${_rawgl_python_root}"
            PATH_SUFFIXES ${_rawgl_python_library_suffixes}
            NO_DEFAULT_PATH
            NO_CACHE)
        if(_rawgl_python_library_release)
            set(Python_LIBRARY "${_rawgl_python_library_release}" CACHE FILEPATH "Python library" FORCE)
            set(Python_LIBRARY_RELEASE "${_rawgl_python_library_release}" CACHE FILEPATH "Python release library" FORCE)
            set(Python3_LIBRARY "${_rawgl_python_library_release}" CACHE FILEPATH "Python library" FORCE)
            set(Python3_LIBRARY_RELEASE "${_rawgl_python_library_release}" CACHE FILEPATH "Python release library" FORCE)
        endif()
    endif()

    if(NOT Python_LIBRARY_DEBUG AND Python3_LIBRARY_DEBUG)
        set(Python_LIBRARY_DEBUG "${Python3_LIBRARY_DEBUG}")
    endif()
    if(NOT Python_LIBRARY_DEBUG AND _rawgl_python_debug_names)
        find_library(_rawgl_python_library_debug
            NAMES ${_rawgl_python_debug_names}
            PATHS "${_rawgl_python_root}"
            PATH_SUFFIXES ${_rawgl_python_library_suffixes}
            NO_DEFAULT_PATH
            NO_CACHE)
        if(_rawgl_python_library_debug)
            set(Python_LIBRARY_DEBUG "${_rawgl_python_library_debug}" CACHE FILEPATH "Python debug library" FORCE)
            set(Python3_LIBRARY_DEBUG "${_rawgl_python_library_debug}" CACHE FILEPATH "Python debug library" FORCE)
        endif()
    endif()
    if(WIN32 AND Python_LIBRARY_RELEASE AND NOT Python_LIBRARY_DEBUG)
        set(Python_LIBRARY_DEBUG "${Python_LIBRARY_RELEASE}" CACHE FILEPATH "Python debug library" FORCE)
        set(Python3_LIBRARY_DEBUG "${Python_LIBRARY_RELEASE}" CACHE FILEPATH "Python debug library" FORCE)
    endif()

    if(Python_LIBRARY_RELEASE AND Python_LIBRARY_DEBUG)
        set(Python_LIBRARIES optimized "${Python_LIBRARY_RELEASE}" debug "${Python_LIBRARY_DEBUG}")
    elseif(Python_LIBRARY_RELEASE)
        set(Python_LIBRARIES "${Python_LIBRARY_RELEASE}")
    elseif(Python_LIBRARY_DEBUG)
        set(Python_LIBRARIES "${Python_LIBRARY_DEBUG}")
    elseif(Python_LIBRARY)
        set(Python_LIBRARIES "${Python_LIBRARY}")
    endif()
    if(Python_LIBRARIES)
        set(Python_LIBRARIES "${Python_LIBRARIES}" CACHE STRING "Python libraries" FORCE)
        set(Python3_LIBRARIES "${Python_LIBRARIES}" CACHE STRING "Python libraries" FORCE)
        set(Python_LIBRARY "${Python_LIBRARY}" PARENT_SCOPE)
        set(Python_LIBRARY_RELEASE "${Python_LIBRARY_RELEASE}" PARENT_SCOPE)
        set(Python_LIBRARY_DEBUG "${Python_LIBRARY_DEBUG}" PARENT_SCOPE)
        set(Python_LIBRARIES "${Python_LIBRARIES}" PARENT_SCOPE)
        set(Python3_LIBRARY "${Python_LIBRARY}" PARENT_SCOPE)
        set(Python3_LIBRARY_RELEASE "${Python_LIBRARY_RELEASE}" PARENT_SCOPE)
        set(Python3_LIBRARY_DEBUG "${Python_LIBRARY_DEBUG}" PARENT_SCOPE)
        set(Python3_LIBRARIES "${Python_LIBRARIES}" PARENT_SCOPE)
    endif()

    if(NOT Python_SABI_LIBRARY AND Python3_SABI_LIBRARY)
        set(Python_SABI_LIBRARY "${Python3_SABI_LIBRARY}")
    endif()
    if(NOT Python_SABI_LIBRARY)
        find_library(_rawgl_python_sabi_library
            NAMES ${_rawgl_python_sabi_names}
            PATHS "${_rawgl_python_root}"
            PATH_SUFFIXES ${_rawgl_python_library_suffixes}
            NO_DEFAULT_PATH
            NO_CACHE)
        if(_rawgl_python_sabi_library)
            set(Python_SABI_LIBRARY "${_rawgl_python_sabi_library}" CACHE FILEPATH "Python stable ABI library" FORCE)
            set(Python_SABI_LIBRARY_RELEASE "${_rawgl_python_sabi_library}" CACHE FILEPATH "Python stable ABI release library" FORCE)
            set(Python3_SABI_LIBRARY "${_rawgl_python_sabi_library}" CACHE FILEPATH "Python stable ABI library" FORCE)
            set(Python3_SABI_LIBRARY_RELEASE "${_rawgl_python_sabi_library}" CACHE FILEPATH "Python stable ABI release library" FORCE)
            set(Python_SABI_LIBRARY "${_rawgl_python_sabi_library}" PARENT_SCOPE)
            set(Python_SABI_LIBRARY_RELEASE "${_rawgl_python_sabi_library}" PARENT_SCOPE)
            set(Python3_SABI_LIBRARY "${_rawgl_python_sabi_library}" PARENT_SCOPE)
            set(Python3_SABI_LIBRARY_RELEASE "${_rawgl_python_sabi_library}" PARENT_SCOPE)
        endif()
    endif()

    set(Python_EXECUTABLE "${_rawgl_python_executable}" PARENT_SCOPE)
    set(Python3_EXECUTABLE "${_rawgl_python_executable}" PARENT_SCOPE)
    set(Python_ROOT_DIR "${_rawgl_python_root}" PARENT_SCOPE)
    set(Python3_ROOT_DIR "${_rawgl_python_root}" PARENT_SCOPE)
endfunction()

function(rawgl_configure_manual_python_targets)
    if(NOT Python_EXECUTABLE OR NOT EXISTS "${Python_EXECUTABLE}")
        return()
    endif()
    if(NOT Python_INCLUDE_DIR AND Python_INCLUDE_DIRS)
        list(GET Python_INCLUDE_DIRS 0 Python_INCLUDE_DIR)
    endif()
    if(NOT Python_INCLUDE_DIR OR NOT EXISTS "${Python_INCLUDE_DIR}")
        return()
    endif()

    if(WIN32 AND NOT Python_LIBRARIES)
        if(Python_LIBRARY_RELEASE AND Python_LIBRARY_DEBUG)
            set(Python_LIBRARIES optimized "${Python_LIBRARY_RELEASE}" debug "${Python_LIBRARY_DEBUG}")
        elseif(Python_LIBRARY_RELEASE)
            set(Python_LIBRARIES "${Python_LIBRARY_RELEASE}")
        elseif(Python_LIBRARY_DEBUG)
            set(Python_LIBRARIES "${Python_LIBRARY_DEBUG}")
        elseif(Python_LIBRARY)
            set(Python_LIBRARIES "${Python_LIBRARY}")
        endif()
    endif()
    if(WIN32 AND NOT Python_LIBRARIES)
        return()
    endif()

    if(NOT TARGET Python::Interpreter)
        add_executable(Python::Interpreter IMPORTED GLOBAL)
        set_target_properties(Python::Interpreter PROPERTIES
            IMPORTED_LOCATION "${Python_EXECUTABLE}")
    endif()
    if(NOT TARGET Python::Module)
        add_library(Python::Module INTERFACE IMPORTED GLOBAL)
        set_target_properties(Python::Module PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Python_INCLUDE_DIR}")
        if(Python_LIBRARIES)
            set_target_properties(Python::Module PROPERTIES
                INTERFACE_LINK_LIBRARIES "${Python_LIBRARIES}")
        endif()
    endif()
    if(NOT TARGET Python::Python)
        add_library(Python::Python INTERFACE IMPORTED GLOBAL)
        set_target_properties(Python::Python PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Python_INCLUDE_DIR}")
        if(Python_LIBRARIES)
            set_target_properties(Python::Python PROPERTIES
                INTERFACE_LINK_LIBRARIES "${Python_LIBRARIES}")
        endif()
    endif()
    if(Python_SABI_LIBRARY AND NOT TARGET Python::SABIModule)
        add_library(Python::SABIModule INTERFACE IMPORTED GLOBAL)
        set_target_properties(Python::SABIModule PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Python_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${Python_SABI_LIBRARY}")
    endif()

    set(Python_FOUND TRUE PARENT_SCOPE)
    set(Python_Interpreter_FOUND TRUE PARENT_SCOPE)
    set(Python_Development_FOUND TRUE PARENT_SCOPE)
    set(Python_Development.Module_FOUND TRUE PARENT_SCOPE)
    set(Python_INCLUDE_DIRS "${Python_INCLUDE_DIR}" PARENT_SCOPE)
    if(Python_LIBRARIES)
        set(Python_LIBRARIES "${Python_LIBRARIES}" PARENT_SCOPE)
    endif()
endfunction()

function(rawgl_map_imported_config_targets)
    foreach(rawgl_target_name ${ARGN})
        if(TARGET ${rawgl_target_name})
            set(rawgl_fallback_location)
            set(rawgl_fallback_implib)
            foreach(rawgl_config_name IN ITEMS RELEASE RELWITHDEBINFO MINSIZEREL NOCONFIG DEBUG)
                if(NOT rawgl_fallback_location)
                    get_target_property(rawgl_config_location ${rawgl_target_name} IMPORTED_LOCATION_${rawgl_config_name})
                    if(rawgl_config_location)
                        set(rawgl_fallback_location "${rawgl_config_location}")
                    endif()
                endif()
                if(NOT rawgl_fallback_implib)
                    get_target_property(rawgl_config_implib ${rawgl_target_name} IMPORTED_IMPLIB_${rawgl_config_name})
                    if(rawgl_config_implib)
                        set(rawgl_fallback_implib "${rawgl_config_implib}")
                    endif()
                endif()
            endforeach()
            if(NOT rawgl_fallback_location)
                get_target_property(rawgl_fallback_location ${rawgl_target_name} IMPORTED_LOCATION)
            endif()
            if(NOT rawgl_fallback_implib)
                get_target_property(rawgl_fallback_implib ${rawgl_target_name} IMPORTED_IMPLIB)
            endif()

            set_target_properties(${rawgl_target_name} PROPERTIES
                MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
                MAP_IMPORTED_CONFIG_MINSIZEREL Release)

            if(rawgl_fallback_location)
                set_target_properties(${rawgl_target_name} PROPERTIES
                    IMPORTED_LOCATION_RELEASE "${rawgl_fallback_location}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${rawgl_fallback_location}"
                    IMPORTED_LOCATION_MINSIZEREL "${rawgl_fallback_location}")
            endif()
            if(rawgl_fallback_implib)
                set_target_properties(${rawgl_target_name} PROPERTIES
                    IMPORTED_IMPLIB_RELEASE "${rawgl_fallback_implib}"
                    IMPORTED_IMPLIB_RELWITHDEBINFO "${rawgl_fallback_implib}"
                    IMPORTED_IMPLIB_MINSIZEREL "${rawgl_fallback_implib}")
            endif()
        endif()
    endforeach()
endfunction()

function(rawgl_apply_windows_debug_suffix target_name)
    if(NOT WIN32)
        return()
    endif()

    if(NOT TARGET ${target_name})
        return()
    endif()

    if(NOT RAWGL_WINDOWS_DEBUG_SUFFIX)
        return()
    endif()

    set_target_properties(${target_name} PROPERTIES
        DEBUG_POSTFIX "${RAWGL_WINDOWS_DEBUG_SUFFIX}")
endfunction()
