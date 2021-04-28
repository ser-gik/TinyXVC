
add_txvc_executable(TinyXVC
    SRCS
    txvc.c
    log.c
    driver.c
    alias.c
    server.c
    jtag_splitter.c
    bit_vector.c
    driver/echo.c
    driver/ft2232h.c
    driver/ftdi_generic.c
    OUTPUT_NAME
    txvc
    DEPENDS
    ftdi1
    )
target_include_directories(TinyXVC
    PRIVATE include/
    )
target_link_options(TinyXVC
    PRIVATE -Wl,--script=${CMAKE_CURRENT_LIST_DIR}/driver.ld
            -Wl,--gc-sections
    )

