# command line parsing

set(ARGS "")
math(EXPR LAST_ARG_NUM "${CMAKE_ARGC} - 1")
foreach(IDX RANGE 4 ${LAST_ARG_NUM})
    list(APPEND ARGS "${CMAKE_ARGV${IDX}}")
endforeach()
list(LENGTH ARGS ARGC)
set(CUR_ARG 0)

macro(has_arg var)
    if (CUR_ARG LESS ARGC)
        set(${var} ON)
    else()
        set(${var} OFF)
    endif()
endmacro()

macro(next_arg var)
    if (CUR_ARG LESS ARGC)
        list(GET ARGS ${CUR_ARG} ${var})
        math(EXPR CUR_ARG "${CUR_ARG} + 1")
    else()
        message(FATAL_ERROR "No next arg")
    endif()
endmacro()

set(PARSER_CMD "")
set(INCLUDES_FILE_PATH "")
set(AST_FILE_PATH "")
set(OUTPUT_FILE_PATH "")
set(COMPILER_CMD "")
set(FINAL_MODULE_FILE_PATH "")
set(EXPORTS_FILE_PATH "")
set(MODULE_NAME "")

while("1")
    has_arg(HAS_ARG)
    if (NOT HAS_ARG)
        break()
    endif()

    next_arg(ARG)
    if (ARG STREQUAL "-modgen_begin_parser_cmd")
        while ("1")
            next_arg(ARG)

            if (ARG STREQUAL "-modgen_end_parser_cmd")
                break()
            endif()

            list(APPEND PARSER_CMD "${ARG}")

            has_arg(HAS_ARG)
            if (NOT HAS_ARG)
                break()
            endif()
        endwhile()
        continue()
    endif()

    if (ARG STREQUAL "-modgen_includes_file_path")
        next_arg(INCLUDES_FILE_PATH)
        continue()
    endif()

    if (ARG STREQUAL "-modgen_ast_file_path")
        next_arg(AST_FILE_PATH)
        continue()
    endif()

    if (ARG STREQUAL "-modgen_final_module_file_path")
        next_arg(FINAL_MODULE_FILE_PATH)
        continue()
    endif()

    if (ARG STREQUAL "-modgen_exports_file_path")
        next_arg(EXPORTS_FILE_PATH)
        continue()
    endif()

    if (ARG STREQUAL "-modgen_module_name")
        next_arg(MODULE_NAME)
        continue()
    endif()

    if (ARG STREQUAL "-o")
        next_arg(OUTPUT_FILE_PATH)
        continue()
    endif()

    list(APPEND COMPILER_CMD "${ARG}")
endwhile()

# 1. Build AST

set(AST_CMD "")
foreach(ARG ${COMPILER_CMD})
    if (ARG MATCHES "^@.*\\.modmap$")
        continue()
    endif()
    if (ARG MATCHES "^.*_module\\.cppm$")
        continue()
    endif()
    list(APPEND AST_CMD "${ARG}")
endforeach()
list(APPEND AST_CMD "-emit-ast" "-o" "${AST_FILE_PATH}" "${INCLUDES_FILE_PATH}")
execute_process(COMMAND ${AST_CMD} COMMAND_ERROR_IS_FATAL ANY)

# 2. Generate exports
execute_process(COMMAND ${PARSER_CMD} COMMAND_ERROR_IS_FATAL ANY)

# 3. Compile the module
list(APPEND COMPILER_CMD "-o" "${OUTPUT_FILE_PATH}")
execute_process(COMMAND ${COMPILER_CMD} COMMAND_ERROR_IS_FATAL ANY)

# 4. Create complete module file
file(READ "${EXPORTS_FILE_PATH}" EXPORTS_CONTENTS)
file(READ "${INCLUDES_FILE_PATH}" INCLUDES_CONTENTS)
file(WRITE "${FINAL_MODULE_FILE_PATH}" "module;
${INCLUDES_CONTENTS}
export module ${MODULE_NAME};
${EXPORTS_CONTENTS}
")