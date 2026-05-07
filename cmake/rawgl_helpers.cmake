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
