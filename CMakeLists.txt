cmake_minimum_required(VERSION 2.8)

project(cnc)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/local/lib/libnetconf2.so") #-O0 -ggdb

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
message(STATUS "Source directory: ${CMAKE_SOURCE_DIR}")

add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/src/main.c")

# dependencies - libssh
# https://www.libssh.org/2019/11/07/libssh-0-9-2/
find_package(libssh REQUIRED)
if (NOT TARGET ssh)
     add_library(ssh SHARED IMPORTED)
     set_target_properties(ssh
                           PROPERTIES
                 ​          IMPORTED_LOCATION "${LIBSSH_LIBRARIES}"
                           INTERFACE_INCLUDE_DIRECTORIES "${LIBSSH_INCLUDE_DIR}")
endif()

# dependencies - libnetconf2
find_package(LibNETCONF2 REQUIRED)
if (NOT LIBNETCONF2_ENABLED_SSH AND NOT LIBNETCONF2_ENABLED_TLS)
    message(SEND_ERROR "Missing SSH and TLS support in libnetconf2, CLI requires at least one them to be supported.")
endif()
target_link_libraries(${PROJECT_NAME} ${LIBNETCONF2_LIBRARIES})
include_directories(${LIBNETCONF2_INCLUDE_DIRS})


#find_package(libnetconf2 REQUIRED)
#include_directories(${LIBNETCONF2_INCLUDE_DIRS})
#message(STATUS "-------> ${LIBNETCONF2_INCLUDE_DIRS}")


target_link_libraries(${PROJECT_NAME} ${LIBNETCON2_LIBRARIES})

