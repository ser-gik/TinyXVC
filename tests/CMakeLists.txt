
add_txvc_executable(Tests
    SRCS
    main.c
    bit_vector_test.c
    DEPENDS
    TinyTest
    Txvc
    )
add_custom_target(UnitTest
    Tests
    COMMENT "Running unit tests"
    )

