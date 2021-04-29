
add_txvc_library(Drivers
    SRCS
    drivers.c
    echo.c
    ft2232h.c
    ftdi_generic.c
    INCDIRS
    include/
    DEPENDS
    Txvc
    ftdi1
    "-Wl,--script=${CMAKE_CURRENT_LIST_DIR}/driver.ld"
    )

