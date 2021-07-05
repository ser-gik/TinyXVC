
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

configure_file(txvc.h.in config/txvc.h @ONLY)
target_include_directories(TxvcExe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS TxvcExe)

