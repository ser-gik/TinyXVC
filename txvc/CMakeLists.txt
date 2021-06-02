
add_txvc_executable(TxvcExe
    OUTPUT_NAME txvc
    SRCS
        txvc.c
        alias.c
        driver_wrapper.c
    DEPENDS
        Txvc
        Drivers
    )
target_link_options(TxvcExe PRIVATE -Wl,--gc-sections)

