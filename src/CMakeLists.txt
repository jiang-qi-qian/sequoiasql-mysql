# Copyright (c) 2018, SequoiaDB and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# St, Fifth Floor, Boston, MA  02110-1301 USA

# Usage: cmake -DSDB_BUILT_IN=ON -DWITH_SDB_DRIVER=/opt/sequoiadb
#
# [SDB_BUILT_IN]: compile as built-in plugin or module plugin.
# [WITH_SDB_DRIVER]: the path to SequoiaDB cpp driver, which must contain
# include/ and lib/libsdbcpp.so

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DDEBUG)
endif()
add_definitions(-Wno-error)

if(SDB_PROJECT_TYPE STREQUAL "mysql")
  add_compile_options(-DIS_MYSQL)
else()
  add_compile_options(-DIS_MARIADB)
  add_definitions(-Wno-deprecated-declarations)
endif()

include_directories(include)
aux_source_directory(handler HANDLER_FILES)
aux_source_directory(sdbwrapper SDB_FILES)
set(SEQUOIADB_SOURCES ${HANDLER_FILES} ${SDB_FILES})

set(WITH_SDB_DRIVER "" CACHE PATH "Path to SequoiaDB C++ driver")
set(SDB_DRIVER_PATH ${WITH_SDB_DRIVER})

find_path(SDB_INCLUDE_DIR NAMES client.hpp HINTS ${SDB_DRIVER_PATH}/include)

if(${SDB_INCLUDE_DIR} STREQUAL "SDB_INCLUDE_DIR-NOTFOUND")
  message(
    SEND_ERROR
      "Cannot find appropriate headers of SequoiaDB C++ driver from ${SDB_DRIVER_PATH}"
    )
endif()
message(STATUS "SDB_INCLUDE_DIR = ${SDB_INCLUDE_DIR}")
include_directories(${SDB_INCLUDE_DIR})

set(SDB_LDFLAGS "-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib")

if(SDB_BUILT_IN)
  # builtin storage engine use C++ dynamic driver
  find_library(SDB_DYNAMIC_LIBRARY
               NAMES libsdbcpp.so
               HINTS ${SDB_DRIVER_PATH}/lib)
  if(${SDB_DYNAMIC_LIBRARY} STREQUAL "SDB_DYNAMIC_LIBRARY-NOTFOUND")
    message(SEND_ERROR "Cannot find libsdbcpp.so from ${SDB_DRIVER_PATH}/lib")
  endif()
  message(STATUS "Using SequoiaDB dynamic library: ${SDB_DYNAMIC_LIBRARY}")

  mysql_add_plugin(sequoiadb
                   ${SEQUOIADB_SOURCES}
                   STORAGE_ENGINE
                   MANDATORY
                   RECOMPILE_FOR_EMBEDDED
                   LINK_LIBRARIES
                   ${SDB_DYNAMIC_LIBRARY})

  string(FIND "${WITH_MYSQLD_LDFLAGS}" "${SDB_LDFLAGS}" POS)
  if(${POS} EQUAL -1)
    set(WITH_MYSQLD_LDFLAGS "${WITH_MYSQLD_LDFLAGS} ${SDB_LDFLAGS}"
        CACHE STRING "Additional linker flags for mysqld"
        FORCE)
  endif()

  install(FILES ${SDB_DYNAMIC_LIBRARY} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
else()
  # use C++ static driver
  find_library(SDB_STATIC_LIBRARY
               NAMES libstaticsdbcpp.a
               HINTS ${SDB_DRIVER_PATH}/lib)
  if(${SDB_STATIC_LIBRARY} STREQUAL "SDB_STATIC_LIBRARY-NOTFOUND")
    message(
      SEND_ERROR "Cannot find libstaticsdbcpp.a from ${SDB_DRIVER_PATH}/lib")
  endif()
  message(STATUS "Using SequoiaDB static library: ${SDB_STATIC_LIBRARY}")

  mysql_add_plugin(sequoiadb
                   ${SEQUOIADB_SOURCES}
                   STORAGE_ENGINE
                   MODULE_ONLY
                   LINK_LIBRARIES
                   ${SDB_STATIC_LIBRARY})
  set_target_properties(sequoiadb PROPERTIES LINK_FLAGS ${SDB_LDFLAGS})
endif()
