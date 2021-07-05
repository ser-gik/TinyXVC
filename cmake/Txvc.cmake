
#
# Utilities to abstract of toolchain' and platform' specifics
#

set(TXVC_C_FLAGS
    -g
    -Wall
    -Wextra
    -Wpedantic
    -Wno-gnu-zero-variadic-macro-arguments
    -Werror
    -ffunction-sections
    -fdata-sections
    )

set(TXVC_LINK_EXE_FLAGS
    -Wl,--no-undefined
    -Wl,--gc-sections
    -Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/exe_exports.txt
    )

function(add_txvc_library target)
    cmake_parse_arguments(ARG
        ""
        ""
        "SRCS;INCDIRS;DEPENDS" ${ARGN})
    add_library(${target} OBJECT ${ARG_SRCS})
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    target_compile_options(${target} PUBLIC ${TXVC_C_FLAGS})
    if(ARG_INCDIRS)
        target_include_directories(${target} PUBLIC ${ARG_INCDIRS})
    endif()
    if(ARG_DEPENDS)
        target_link_libraries(${target} ${ARG_DEPENDS})
    endif()
endfunction()

function(add_txvc_executable target)
    cmake_parse_arguments(ARG
        ""
        "OUTPUT_NAME"
        "SRCS;DEPENDS" ${ARGN})
    add_executable(${target} ${ARG_SRCS})
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    target_compile_options(${target} PRIVATE ${TXVC_C_FLAGS})
    target_link_options(${target} PRIVATE ${TXVC_LINK_EXE_FLAGS})
    if(ARG_OUTPUT_NAME)
        set_property(TARGET ${target} PROPERTY OUTPUT_NAME ${ARG_OUTPUT_NAME})
    endif()
    if(ARG_DEPENDS)
        target_link_libraries(${target} ${ARG_DEPENDS})
    endif()
endfunction()

function(txvc_detect_arch output_var)
    execute_process(
        COMMAND echo [===[
#if defined(__x86_64__)
"amd64"
#else
#error Unknown architecture
#endif
]===]
        COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} ${TXVC_C_FLAGS} -P -E -
        RESULT_VARIABLE status
        OUTPUT_VARIABLE stdout
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE stderr
        ERROR_STRIP_TRAILING_WHITESPACE)
    if (NOT (status EQUAL 0))
        message(FATAL_ERROR "Can not detect arch: \"${stderr}\"")
    endif()
    string(REGEX REPLACE "[ \r\n\t\"]" "" arch "${stdout}")
    message(STATUS "Architecture is: ${arch}")
    set(${output_var} ${arch} PARENT_SCOPE)
endfunction()

txvc_detect_arch(TXVC_ARCH)

