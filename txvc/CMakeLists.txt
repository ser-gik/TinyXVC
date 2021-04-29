
add_txvc_executable(TxvcExe
    SRCS
    txvc.c
    alias.c
    OUTPUT_NAME
    txvc
    DEPENDS
    Txvc
    Drivers
    )
target_link_options(TxvcExe PRIVATE -Wl,--gc-sections)

