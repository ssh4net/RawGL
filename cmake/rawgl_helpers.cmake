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
    if(Python_EXECUTABLE)
        return()
    endif()

    if(WIN32)
        set(_rawgl_python_names python.exe python3.exe python3 python)
        set(_rawgl_python_library_suffixes libs lib)
    else()
        set(_rawgl_python_names python3 python)
        set(_rawgl_python_library_suffixes lib lib64)
    endif()

    foreach(_rawgl_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(NOT _rawgl_prefix)
            continue()
        endif()

        find_program(_rawgl_python_executable
            NAMES ${_rawgl_python_names}
            PATHS "${_rawgl_prefix}"
            PATH_SUFFIXES "" bin Scripts
            NO_DEFAULT_PATH
            NO_CACHE)

        if(_rawgl_python_executable)
            file(TO_CMAKE_PATH "${_rawgl_prefix}" _rawgl_python_root)
            break()
        endif()
    endforeach()

    if(NOT _rawgl_python_executable)
        return()
    endif()

    set(Python_EXECUTABLE "${_rawgl_python_executable}" CACHE FILEPATH "Python interpreter" FORCE)
    set(Python_ROOT_DIR "${_rawgl_python_root}" CACHE PATH "Python root directory" FORCE)
    set(Python_FIND_STRATEGY LOCATION CACHE STRING "Python lookup strategy" FORCE)

    execute_process(
        COMMAND "${_rawgl_python_executable}" -c
                "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}'); print(f'{sys.version_info[0]}{sys.version_info[1]}')"
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
        endif()
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
        endif()
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
        endif()
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
        endif()
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
