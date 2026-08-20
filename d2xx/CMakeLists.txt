#
# Download and export official FTDI's prebuilt d2xx driver as IMPORTED library
#

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (TXVC_ARCH STREQUAL "amd64")
        set(D2XX_ARCHIVE ${CMAKE_CURRENT_LIST_DIR}/libftd2xx-linux-x86_64-1.4.35.tgz)
        set(D2XX_DIR ${CMAKE_CURRENT_BINARY_DIR})
        set(D2XX_EXTRACTED_ROOT ${D2XX_DIR}/linux-x86_64)
        set(D2XX_UNPACK_MARKER ${D2XX_EXTRACTED_ROOT}/release-notes.txt)

        if (NOT EXISTS ${D2XX_UNPACK_MARKER})
            message(STATUS "Unpacking ${D2XX_ARCHIVE}")
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
            IMPORTED_LOCATION ${D2XX_EXTRACTED_ROOT}/libftd2xx.a
            INTERFACE_INCLUDE_DIRECTORIES ${D2XX_EXTRACTED_ROOT}
            INTERFACE_COMPILE_DEFINITIONS FTD2XX_STATIC
            INTERFACE_LINK_LIBRARIES pthread)
        return()
    endif()
endif()

message(FATAL_ERROR "Do not know where to find D2XX driver ")

