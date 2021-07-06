
add_txvc_library(Drivers
    SRCS
        drivers.c
        echo.c
        ftdi_generic.c
    INCDIRS
        include/
    DEPENDS
        Txvc
        libftd2xx
    )

