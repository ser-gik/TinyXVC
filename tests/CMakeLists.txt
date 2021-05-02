
add_txvc_executable(Tests
    SRCS
        main.c
        bit_vector_test.c
        jtag_splitter_test.c
        log_test.c
        server_test.c
        profile_test.c
    DEPENDS
        TinyTest
        Txvc
        pthread
    )
add_custom_target(UnitTest
    Tests
    COMMENT "Running unit tests"
    )

