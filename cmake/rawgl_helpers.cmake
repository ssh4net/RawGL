function(rawgl_add_windows_imported_library target_name)
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
