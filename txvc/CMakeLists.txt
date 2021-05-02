
add_txvc_executable(TxvcExe
    OUTPUT_NAME txvc
    SRCS
        txvc.c
        alias.c
    DEPENDS
        Txvc
        Drivers
    )
target_link_options(TxvcExe PRIVATE -Wl,--gc-sections)

