cmake_minimum_required(VERSION 2.8.7)

project(sdbvertool)

if(NOT SDB_DRIVER_DIR)
   message(FATAL_ERROR "SequoiaDB driver path is not set")
endif()

set(SDB_DRIVER_LIB_NAME sdbcpp)

set(SDB_DRIVER_INCLUDE_DIR "${SDB_DRIVER_DIR}/include")
set(SDB_DRIVER_LIB_DIR "${SDB_DRIVER_DIR}/lib")

if(OPENSSL_LIB_DIR)
   link_directories(${OPENSSL_LIB_DIR})
endif()

find_library(SDB_CLT_LIB
             names ${SDB_DRIVER_LIB_NAME}
             HINTS ${SDB_DRIVER_LIB_DIR})

if(${SDB_CLT_LIB} STREQUAL "SDB_CLT_LIB-NOTFOUND")
   message(FATAL_ERROR "SequoiaDB client dynamic library not found")
endif()


include_directories(${SDB_DRIVER_INCLUDE_DIR})
link_directories(${SDB_DRIVER_LIB_DIR})

#aux_source_directory('./' SOURCE_FILES)
add_executable(sdbvertool sdbVerTool.cpp)
if(OPENSSL_LIB_DIR)
   target_link_libraries(sdbvertool sdbcpp ssl crypto pthread dl)
else()
   target_link_libraries(sdbvertool sdbcpp pthread dl)
endif()

