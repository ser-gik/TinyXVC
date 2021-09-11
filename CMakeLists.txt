cmake_minimum_required(VERSION 3.13)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/.revision_change_marker)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

include(TxvcGit)

get_txvc_version_from_git(TXVC_VERSION TXVC_VERSION_SHORT)

project(TinyXVC
    VERSION ${TXVC_VERSION_SHORT}
    DESCRIPTION "Virtual cable server for Xilinx FPGAs"
    HOMEPAGE_URL "https://github.com/ser-gik/TinyXVC"
    LANGUAGES C)

include(Txvc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(d2xx)
add_subdirectory(libtxvc)
add_subdirectory(libdrivers)
add_subdirectory(txvc)
add_subdirectory(libtinytest)
add_subdirectory(tests)
add_subdirectory(player)
add_subdirectory(udev)

add_subdirectory(package)

