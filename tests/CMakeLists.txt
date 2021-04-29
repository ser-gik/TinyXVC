
add_txvc_executable(Tests
    SRCS
    main.c
    another.c
    DEPENDS
    TestSupport
    Txvc
    )
add_custom_target(UnitTest
    Tests
    )

