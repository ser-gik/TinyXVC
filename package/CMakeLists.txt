
configure_file(CMakeCPackOptions.cmake.in CMakeCPackOptions.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/CMakeCPackOptions.cmake)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR DEB)
    endif()
endif()

include(CPack)

