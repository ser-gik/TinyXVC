
#
# Use d2xx driver from FTDI website
#

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(D2XX_URL "https://www.ftdichip.com/Drivers/D2XX/Linux/libftd2xx-x86_64-1.4.24.gz")
    set(D2XX_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(D2XX_ARCHIVE ${D2XX_DIR}/libftd2xx.tar.gz)

    if (NOT EXISTS ${D2XX_ARCHIVE})
        message(STATUS "Downloading libd2xx")
        file(DOWNLOAD ${D2XX_URL} ${D2XX_ARCHIVE}
            SHOW_PROGRESS
            STATUS D2XX_DOWNLOAD_STATUS
            TLS_VERIFY ON
            TIMEOUT 60)
        list(GET D2XX_DOWNLOAD_STATUS 0 D2XX_DOWNLOAD_STATUS_CODE)
        list(GET D2XX_DOWNLOAD_STATUS 1 D2XX_DOWNLOAD_STATUS_ERROR)
        if (NOT (D2XX_DOWNLOAD_STATUS_CODE EQUAL 0))
            message(FATAL_ERROR "Failed to download libd2xx: ${D2XX_DOWNLOAD_STATUS_ERROR}")
        endif()
        message(STATUS "Unpacking libd2xx")
        execute_process(COMMAND tar zx
            WORKING_DIRECTORY ${D2XX_DIR}
            INPUT_FILE ${D2XX_ARCHIVE}
            RESULT_VARIABLE D2XX_UNPACK_STATUS_CODE
            TIMEOUT 60)
        if (NOT (D2XX_UNPACK_STATUS_CODE EQUAL 0))
            message(FATAL_ERROR "Failed to unpack libd2xx")
        endif()
    endif()

    add_library(libftd2xx STATIC IMPORTED GLOBAL)
    set_target_properties(libftd2xx
        PROPERTIES
        IMPORTED_LOCATION ${D2XX_DIR}/release/build/libftd2xx.a
        INTERFACE_INCLUDE_DIRECTORIES ${D2XX_DIR}/release
        INTERFACE_COMPILE_DEFINITIONS FTD2XX_STATIC
        INTERFACE_LINK_LIBRARIES pthread)

else()
    message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

