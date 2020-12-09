cmake_minimum_required(VERSION 3.10)

project(TinyXVC C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE DEBUG)

add_executable(TinyXVC
    src/txvc.c
    src/fixtures.c
    src/module/echo.c
    src/module/ft2232h.c
    )
target_include_directories(TinyXVC
    PRIVATE include/
    )
target_compile_options(TinyXVC
    PRIVATE -Wall -Werror
    )
set_property(TARGET TinyXVC PROPERTY C_STANDARD 11)
set_property(TARGET TinyXVC PROPERTY LINK_FLAGS "${CMAKE_CURRENT_LIST_DIR}/src/txvc.ld")
set_property(TARGET TinyXVC PROPERTY OUTPUT_NAME txvc)

