cmake_minimum_required(VERSION 3.13)

project(TinyXVC C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(LibFTDI1 NO_MODULE REQUIRED)
include(${LIBFTDI_USE_FILE})


function(add_txvc_executable target)
    cmake_parse_arguments(ARG
        ""
        "OUTPUT_NAME"
        "DEPENDS;SRCS" ${ARGN})
    add_executable(${target} ${ARG_SRCS})
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    if(ARG_OUTPUT_NAME)
        set_property(TARGET ${target} PROPERTY OUTPUT_NAME ${ARG_OUTPUT_NAME})
    endif()
    target_compile_options(${target}
        PRIVATE -Wall -Wextra -Wpedantic -Werror -ffunction-sections -fdata-sections)
    if(ARG_DEPENDS)
        target_link_libraries(${target} ${ARG_DEPENDS})
    endif()
endfunction()


add_txvc_executable(TinyXVC
    SRCS
    src/txvc.c
    src/log.c
    src/driver.c
    src/alias.c
    src/server.c
    src/jtag_splitter.c
    src/bit_vector.c
    src/driver/echo.c
    src/driver/ft2232h.c
    src/driver/ftdi_generic.c
    OUTPUT_NAME
    txvc
    DEPENDS
    ftdi1
    )
target_include_directories(TinyXVC
    PRIVATE include/
    )
target_link_options(TinyXVC
    PRIVATE -Wl,--script=${CMAKE_CURRENT_LIST_DIR}/src/driver.ld
            -Wl,--gc-sections
    )

add_txvc_executable(Tester
    SRCS
    test/support/main.c
    test/dummy.c
    )
add_custom_target(UnitTest
    Tester
    )

