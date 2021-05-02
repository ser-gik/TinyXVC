
add_txvc_executable(Tests
    SRCS
        main.c
        bit_vector_test.c
        log_test.c
        server_test.c
    DEPENDS
        TinyTest
        Txvc
        pthread
    )
add_custom_target(UnitTest
    Tests
    COMMENT "Running unit tests"
    )

