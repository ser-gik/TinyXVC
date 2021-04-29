
add_txvc_executable(Tests
    SRCS
    main.c
    another.c
    DEPENDS
    TinyTest
    Txvc
    )
add_custom_target(UnitTest
    Tests
    COMMENT "Running unit tests"
    )

