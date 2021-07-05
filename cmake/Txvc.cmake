
option(TXVC_ENABLE_GPROF "Enable gprof intstrumentation for executables" OFF)

set(TXVC_C_FLAGS
    -g
    $<$<BOOL:${TXVC_ENABLE_GPROF}>:-pg>
    -Wall
    -Wextra
    -Wpedantic
    -Wno-gnu-zero-variadic-macro-arguments
    -Werror
    -ffunction-sections
    -fdata-sections)

set(TXVC_LINK_FLAGS
    $<$<BOOL:${TXVC_ENABLE_GPROF}>:-pg>)

function(add_txvc_library target)
    cmake_parse_arguments(ARG
        ""
        ""
        "SRCS;INCDIRS;DEPENDS" ${ARGN})
    add_library(${target} OBJECT ${ARG_SRCS})
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    target_compile_options(${target} PUBLIC ${TXVC_C_FLAGS})
    if(ARG_DEPENDS)
        target_link_libraries(${target} ${ARG_DEPENDS})
    endif()
    if(ARG_INCDIRS)
        target_include_directories(${target} PUBLIC ${ARG_INCDIRS})
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
    if(ARG_DEPENDS)
        target_link_libraries(${target} ${ARG_DEPENDS})
    endif()
    target_link_libraries(${target} "$<JOIN:${TXVC_LINK_FLAGS}, >")
    if(ARG_OUTPUT_NAME)
        set_property(TARGET ${target} PROPERTY OUTPUT_NAME ${ARG_OUTPUT_NAME})
    endif()
endfunction()

