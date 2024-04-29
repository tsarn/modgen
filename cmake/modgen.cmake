add_custom_target(modgen_all_modules)

function(modgen_define_module)
    cmake_parse_arguments(
            ARG
            "NO_BUILD;IGNORE_NONEXISTENT_INCLUDES"
            "TARGET;NAME;FILTER;EXCLUDE"
            "INCLUDES;NAMESPACES;DEPENDS"
            ${ARGN}
    )

    if (NOT DEFINED ARG_NAME)
        message(FATAL_ERROR "modgen_define_module: NAME not specified")
    endif ()

    if (NOT DEFINED ARG_TARGET)
        set(ARG_TARGET "${ARG_NAME}")
    endif ()

    if (DEFINED ARG_NAMESPACES)
        if (DEFINED ARG_FILTER)
            message(WARNING "modgen_define_module: both NAMESPACES and FILTER defined, ignoring NAMESPACES")
            unset(ARG_NAMESPACES)
        else()
            string(REGEX REPLACE ";" "," ARG_NAMESPACES "${ARG_NAMESPACES}")
        endif()
    endif()

    set(INCLUDES_STRING "")
    foreach (include_file ${ARG_INCLUDES})
        if (ARG_IGNORE_NONEXISTENT_INCLUDES)
            string(APPEND INCLUDES_STRING "
#if __has_include(\"${include_file}\")
#include \"${include_file}\"
#endif
")
        else()
            string(APPEND INCLUDES_STRING "#include \"${include_file}\"\n")
        endif()
    endforeach ()

    set(MODULE_STRING "\
#ifdef MODGEN_NO_BUILD
export module ${ARG_NAME};
#else
module;
#include \"./_includes.cpp\"
export module ${ARG_NAME};

#if __has_include(\"./_exports.cpp\")
    #include \"./_exports.cpp\"
#endif
#endif
")

    add_library("${ARG_TARGET}" STATIC)

    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/modgen_tempdir/${ARG_TARGET}")
    set(INCLUDES_FILE_PATH "${OUTPUT_DIR}/_includes.cpp")
    set(MODULE_FILE_PATH "${OUTPUT_DIR}/_module.cppm")
    set(EXPORTS_FILE_PATH "${OUTPUT_DIR}/_exports.cpp")
    set(AST_FILE_PATH "${OUTPUT_DIR}/_includes.ast")
    set(FINAL_MODULE_FILE_PATH "${CMAKE_BINARY_DIR}/modgen_generated_modules/${ARG_NAME}.cppm")

    file(CONFIGURE OUTPUT "${INCLUDES_FILE_PATH}" CONTENT "${INCLUDES_STRING}")
    file(CONFIGURE OUTPUT "${MODULE_FILE_PATH}" CONTENT "${MODULE_STRING}")

    target_sources("${ARG_TARGET}" PUBLIC
            FILE_SET CXX_MODULES
            FILES "${MODULE_FILE_PATH}"
            BASE_DIRS "${OUTPUT_DIR}"
    )

    set(PARSER_CMD
        "$<TARGET_FILE:modgen_ast_parser>"
        -o "${EXPORTS_FILE_PATH}"
        "${AST_FILE_PATH}"
    )

    if (DEFINED ARG_NAMESPACES)
        list(APPEND PARSER_CMD -n "${ARG_NAMESPACES}")
    endif()

    if (DEFINED ARG_FILTER)
        list(APPEND PARSER_CMD -f "${ARG_FILTER}")
    endif()

    if (DEFINED ARG_EXCLUDE)
        list(APPEND PARSER_CMD -e "${ARG_EXCLUDE}")
    endif()

    set(COMPILER_WRAPPER_CMD
#            "$<TARGET_FILE:modgen_compiler_wrapper>"
            "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/compiler_wrapper.cmake" --
            -modgen_begin_parser_cmd
            ${PARSER_CMD}
            -modgen_end_parser_cmd
            -modgen_ast_file_path "${AST_FILE_PATH}"
            -modgen_includes_file_path "${INCLUDES_FILE_PATH}"
            -modgen_final_module_file_path "${FINAL_MODULE_FILE_PATH}"
            -modgen_exports_file_path "${EXPORTS_FILE_PATH}"
            -modgen_module_name "${ARG_NAME}"
    )
    set_target_properties("${ARG_TARGET}" PROPERTIES
            CXX_COMPILER_LAUNCHER "${COMPILER_WRAPPER_CMD}"
    )
    add_dependencies("${ARG_TARGET}"
            modgen_ast_parser
    )
    target_compile_features("${ARG_TARGET}" PUBLIC cxx_std_20)
    add_dependencies(modgen_all_modules "${ARG_TARGET}")
    target_link_libraries("${ARG_TARGET}" PUBLIC ${ARG_DEPENDS})
    if (ARG_NO_BUILD)
        target_compile_definitions("${ARG_TARGET}" PRIVATE MODGEN_NO_BUILD)
    endif()
endfunction()
