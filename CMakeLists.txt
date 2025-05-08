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

cmake_minimum_required(VERSION 2.8.7)

INCLUDE(cmake/curl.cmake)
INCLUDE(cmake/ssl.cmake)
INCLUDE(cmake/gen_version.cmake)
INCLUDE(cmake/odbc.cmake)
INCLUDE(cmake/curses.cmake)

option(ENTERPRISE "Build enterprise edition" OFF)
option(SDB_BUILT_IN "Build the sequoiadb plugin into mysqld" ON)
option(PACK_TEST "Set to true if you want to include testcases in the run
                  package" ON)
option(HYBRID "Build hybrid " OFF)

# Find out with target(s) to be build.
if(MYSQL AND MARIADB)
   message(FATAL_ERROR "Can only build one type at the same time")
elseif(MARIADB)
   set(SDB_PROJECT_TYPE "mariadb" CACHE STRING "Project type is MariaDB")
   set(PROJECT_NAME "SequoiaSQL-MariaDB")
   set(SQL_VERSION "${MARIADB}")
   set(SQL_TYPE "mariadb")
else()
   set(SDB_PROJECT_TYPE "mysql" CACHE STRING "Project type is MySQL")
   if(NOT MYSQL)
      set(MYSQL "5.7.25")
   endif()
   set(PROJECT_NAME "SequoiaSQL-MySQL")
   set(SQL_VERSION "${MYSQL}")
   set(SQL_TYPE "mysql")
endif()

#if(NOT (${CMAKE_VERSION} VERSION_LESS "3.0"))
#   cmake_policy(SET CMP0048 OLD)
#endif()

project(${PROJECT_NAME})

SDB_CHECK_SSL()
SDB_CHECK_CURL()
SDB_CHECK_ODBC()
SDB_CHECK_CURSES()

# Generate the version.info file
gen_version_file()
set(VERSION_FILE ${CMAKE_SOURCE_DIR}/version)
execute_process(
   COMMAND bash -c "awk -F. '{print $1}' ${VERSION_FILE}"
   OUTPUT_VARIABLE PROJECT_VERSION_MAJOR
)
string(STRIP ${PROJECT_VERSION_MAJOR} PROJECT_VERSION_MAJOR)
execute_process(
   COMMAND bash -c "awk -F. '{print $2}' ${VERSION_FILE}"
   OUTPUT_VARIABLE PROJECT_VERSION_MINOR
)
string(STRIP ${PROJECT_VERSION_MINOR} PROJECT_VERSION_MINOR)
execute_process(
   COMMAND bash -c "grep -o '\\.' ${VERSION_FILE}"
   COMMAND bash -c "wc -l"
   OUTPUT_VARIABLE DOT_NUMBER
)

# Check if the version number contains path version.
if(DOT_NUMBER EQUAL 2)
   execute_process(
      COMMAND bash -c "awk -F. '{print $3}' ${VERSION_FILE}"
      OUTPUT_VARIABLE PROJECT_VERSION_PATCH
   )
   string(STRIP ${PROJECT_VERSION_PATCH} PROJECT_VERSION_PATCH)
   set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
else()
   unset(PROJECT_VERSION_PATCH)
   set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
endif()

message(STATUS "SequoiaSQL version: ${CONNECTOR}")
# Set SQL source directory name: v{SQL_VERSION}-${SQL_SOURCE_BRANCH}.
# For examples: v10.4.6, v10.4.6-3.6, v5.7.39-3.6-cgb
if(CONNECTOR)
   set(CONNECTOR_VERSION "${CONNECTOR}")
endif()
if(CONNECTOR)
  set(SQL_SOURCE_BRANCH "v${SQL_VERSION}-${CONNECTOR_VERSION}")
else()
  set(SQL_SOURCE_BRANCH "v${SQL_VERSION}")
endif()
message(STATUS "SQL source branch: ${SQL_SOURCE_BRANCH}")

# set testpackage name
set(TEST_PKG_NAME "${SDB_PROJECT_TYPE}-${SQL_VERSION}")

# Set SQL source dirname
if(CMAKE_BUILD_TYPE MATCHES "Debug")
   set(SQL_SOURCE_DIR_NAME "${SQL_SOURCE_BRANCH}-debug")
else()
   set(SQL_SOURCE_DIR_NAME "${SQL_SOURCE_BRANCH}-release")
endif()

# Set SQL source root
if(CMAKE_BUILD_TYPE MATCHES "Debug")
   set(SQL_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/${SQL_SOURCE_BRANCH}-debug")
else()
   set(SQL_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/${SQL_SOURCE_BRANCH}-release")
endif()

# Remove submodule project before add it into SSQL
execute_process(
   COMMAND bash -c "git rm ${SQL_SOURCE_DIR_NAME} --force 2>/dev/null"
   WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Prepare SQL source code
execute_process(
   COMMAND bash -c "git submodule add --force http://gitlab.sequoiadb.com/sequoiadb/${SQL_TYPE}.git ${SQL_SOURCE_DIR_NAME}"
   WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# clean up
execute_process(
   COMMAND bash -c "git clean -fdx"
   WORKING_DIRECTORY "${SQL_SOURCE_ROOT}"
)

execute_process(
   COMMAND bash -c "git reset --hard"
   WORKING_DIRECTORY "${SQL_SOURCE_ROOT}"
)

execute_process(
   COMMAND bash -c "git checkout master"
   WORKING_DIRECTORY "${SQL_SOURCE_ROOT}"
)

# Fetch branch
execute_process(
   COMMAND bash -c "git fetch origin ${SQL_SOURCE_BRANCH}:${SQL_SOURCE_BRANCH}"
   WORKING_DIRECTORY "${SQL_SOURCE_ROOT}"
)

# Checkout branch
execute_process(
   COMMAND bash -c "git checkout ${SQL_SOURCE_BRANCH}"
   WORKING_DIRECTORY "${SQL_SOURCE_ROOT}"
)

# Do not add SQL source code into SSQL project
execute_process(
   COMMAND bash -c "git reset HEAD .gitmodules ${SQL_SOURCE_DIR_NAME}"
   WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(NOT WITH_SDB_DRIVER)
   message(FATAL_ERROR "SequoiaDB driver path not set")
endif()

# If WITH_BOOST is not set, use the default
if(NOT WITH_BOOST)
   set(BOOST_VERSION "1_59_0")
   set(WITH_BOOST "${CMAKE_SOURCE_DIR}/thirdparty/boost_${BOOST_VERSION}")
endif()
include_directories(SYSTEM ${WITH_BOOST})

if(NOT SDB_BUILT_IN)
   set(SDB_BUILD_IN ${SDB_BUILD_IN} CACHE BOOL
       "Build the sequoiadb plugin into mysqld")
endif()

if(NOT MYSQL_DATADIR)
   set(MYSQL_DATADIR "/opt/sequoiasql/mysql/data")
endif()

if(ENTERPRISE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSDB_ENTERPRISE=TRUE")
endif()

set(SDB_PLUGIN_VERSION "")
get_my_version(SDB_PLUGIN_VERSION 7)
message(STATUS "SDB plugin version: ${SDB_PLUGIN_VERSION}")

set(SDB_VERSION "")
set(SDB_GIT_VERSION "")
get_sdb_version(SDB_VERSION SDB_GIT_VERSION)

add_definitions(-DSDB_PLUGIN_VERSION="${SDB_PLUGIN_VERSION}")
add_definitions(-DSDB_DRIVER_VERSION="${SDB_VERSION}")

if(PACKET_TEST)
   message(STATUS "Packet testcases")
endif()

# Preprocessing
set(SQL_STORAGE_DIR "${SQL_SOURCE_ROOT}/storage")
set(SQL_SDB_CONNECTOR_DIR "${CMAKE_SOURCE_DIR}/src/sequoiadb")
set(SQL_SPK_CONNECTOR_DIR "${CMAKE_SOURCE_DIR}/src/spark")

# Replace the original mysql-test with the new one.
set(MYSQL_TEST_DIR "${SQL_SOURCE_ROOT}/mysql-test")
set(MYSQL_TEST_ORIG "${SQL_SOURCE_ROOT}/mysql-test-orig")

execute_process(
   COMMAND bash -c "file ${MYSQL_TEST_DIR}"
   COMMAND bash -c "awk '{print $2}'"
   OUTPUT_VARIABLE TEST_DIR_TYPE
)

# Remove the native mysql testcases, and create a symbol link pointing to ours.
# string(STRIP ${TEST_DIR_TYPE} TEST_DIR_TYPE)
# if(TEST_DIR_TYPE STREQUAL "directory")
#    file(REMOVE_RECURSE ${MYSQL_TEST_DIR})
#    execute_process(
#       COMMAND bash -c "ln -sf ${CMAKE_SOURCE_DIR}/testcase/${SQL_TYPE}-${SQL_VERSION} ${SQL_SOURCE_ROOT}/mysql-test"
#    )
# endif()

set(updateCmd "find -L ${SQL_SOURCE_ROOT} -type f |
               xargs grep -l CMAKE_SOURCE_DIR 2>/dev/null |
               xargs sed -i 's/CMAKE_SOURCE_DIR/SQL_SOURCE_ROOT/g' 2>/dev/null")
execute_process(COMMAND bash -c ${updateCmd})

set(SDB_PLUGIN_DIR "${SQL_STORAGE_DIR}/sequoiadb")
if(EXISTS ${SDB_PLUGIN_DIR})
   message(STATUS "Directory ${SDB_PLUGIN_DIR} exists. Ready to remove")
   file(REMOVE ${SDB_PLUGIN_DIR})
else()
   message(STATUS "Directory ${SDB_PLUGIN_DIR} does not exist")
endif()

set(SPK_PLUGIN_DIR "${SQL_STORAGE_DIR}/spark")
if(EXISTS ${SPK_PLUGIN_DIR})
   message(STATUS "Directory ${SPK_PLUGIN_DIR} exists. Ready to remove")
   file(REMOVE ${SPK_PLUGIN_DIR})
else()
   message(STATUS "Directory ${SPK_PLUGIN_DIR} does not exist")
endif()

# Add ld flag on libmysql_api_test.
set(LIB_MYSQL_CMAKE_LIST "${SQL_SOURCE_ROOT}/libmysql/CMakeLists.txt")
set(grep_cmd "grep -wn LIBMYSQL_API_TEST_LDFLAGS ${LIB_MYSQL_CMAKE_LIST} | wc -l")
execute_process(COMMAND bash -c ${grep_cmd} OUTPUT_VARIABLE IS_ADDED)
string(STRIP ${IS_ADDED} IS_ADDED)
if(IS_ADDED STREQUAL "0")
  set(addText "set(LIBMYSQL_API_TEST_LDFLAGS \"-Wl,-rpath,\${CMAKE_CURRENT_BINARY_DIR}\")\\n\
set_target_properties(libmysql_api_test PROPERTIES LINK_FLAGS \${LIBMYSQL_API_TEST_LDFLAGS})")
  set(sedCmd "sed -i '$a\\${addText}' ${LIB_MYSQL_CMAKE_LIST}")
  execute_process(COMMAND bash -c ${sedCmd})
endif()

#set(LIBMYSQL_API_TEST_LDFLAGS "-Wl,-rpath,${CMAKE_CURRENT_BINARY_DIR}")
#set_target_properties(libmysql_api_test PROPERTIES LINK_FLAGS ${LIBMYSQL_API_TEST_LDFLAGS})

# Fix compilation problem of innodb with gcc 4.8.5 in ARM64
# Refer SEQUOIADBMAINSTREAM-5417
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" AND
   SDB_PROJECT_TYPE STREQUAL "mysql")
   execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                   OUTPUT_VARIABLE GCC_VERSION)
   if(GCC_VERSION VERSION_LESS 4.9)
      set(INNO_CMAKE_LIST "${SQL_SOURCE_ROOT}/storage/innobase/CMakeLists.txt")
      message(STATUS "Updating ${INNO_CMAKE_LIST} for ARM64 with gcc < 4.9")

      set(sedCmd "sed -n '/A GCC bug causes crash when compiling these files on ARM64 with -O1+/=' ${INNO_CMAKE_LIST}")
      execute_process(COMMAND bash -c ${sedCmd} OUTPUT_VARIABLE LINE_NUM)
      string(STRIP ${LINE_NUM} LINE_NUM)

      set(sedCmd "sed -n '/ADD_COMPILE_FLAGS/{${LINE_NUM},$=}' ${INNO_CMAKE_LIST}")
      execute_process(COMMAND bash -c ${sedCmd} OUTPUT_VARIABLE LINE_NUM)
      string(STRIP ${LINE_NUM} LINE_NUM)

      set(sedCmd "sed -n '${LINE_NUM},+1p' ${INNO_CMAKE_LIST} | grep 'fts/fts0fts.cc' | wc -l")
      execute_process(COMMAND bash -c ${sedCmd} OUTPUT_VARIABLE IS_UPDATED)
      string(STRIP ${IS_UPDATED} IS_UPDATED)

      if(IS_UPDATED STREQUAL "0")
         set(sedCmd "sed -i '${LINE_NUM}a\\      fts/fts0fts.cc\\n      handler/handler0alter.cc\\n      row/row0mysql.cc\\n      srv/srv0srv.cc' ${INNO_CMAKE_LIST}")
         execute_process(COMMAND bash -c ${sedCmd})
      endif()
   endif()
endif()

set(mapSrcCmd "ln -sf ${SQL_SDB_CONNECTOR_DIR} ${SQL_STORAGE_DIR}/sequoiadb")
execute_process(COMMAND bash -c ${mapSrcCmd})

set(mapSrcCmd "ln -sf ${SQL_SPK_CONNECTOR_DIR} ${SQL_STORAGE_DIR}/spark")
execute_process(COMMAND bash -c ${mapSrcCmd})

set(SQL_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/${SQL_SOURCE_DIR_NAME}"
    CACHE STRING INTERNAL)
set(updateCmd "find ${SQL_SOURCE_ROOT} -type f |
               xargs grep -l CMAKE_BINARY_DIR 2>/dev/null |
               xargs sed -i 's#\${CMAKE_BINARY_DIR}#${SQL_OUTPUT_ROOT}#g'
               2>/dev/null")
execute_process(COMMAND bash -c ${updateCmd})

set(AUTOGEN_include_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
include_directories(${AUTOGEN_include_DIR})

option(WITH_DEBUG "USE DBUG/SAFEMUTEX" OFF)

if(WITH_DEBUG)
   message(STATUS "Build debug")
else()
   message(STATUS "Build release")
endif()

if(SDB_BUILT_IN)
   set(BUILT_IN 1)
else()
   set(BUILT_IN 0)
endif()

# Comment 'ADD_SUBDIRECTORY(mysql-test)' in SQL source code
# Use SequoiqSQL testcase instead
set(updateCmd "sed -i 's/ADD_SUBDIRECTORY(mysql-test)/# ADD_SUBDIRECTORY(mysql-test)/' ${SQL_SOURCE_ROOT}/CMakeLists.txt 2>/dev/null")
execute_process(COMMAND bash -c ${updateCmd})

# Modify testcase install information
set(updateCmd "sed -i 's/CMAKE_SOURCE_DIR/SQL_SOURCE_ROOT/g' ${CMAKE_SOURCE_DIR}/testcase/${SQL_TYPE}-${SQL_VERSION}/CMakeLists.txt 2>/dev/null")
execute_process(COMMAND bash -c ${updateCmd})

set(TARGET_SOURCE_DIR "${CMAKE_SOURCE_DIR}/${SQL_SOURCE_DIR_NAME}")
add_subdirectory(${SQL_SOURCE_ROOT} ${CMAKE_BINARY_DIR}/${SQL_SOURCE_DIR_NAME})

# Use SequoiaSQL testcase instead
add_subdirectory(testcase/${SQL_TYPE}-${SQL_VERSION})

# Generate the configuration file
execute_process(
   COMMAND python ${CMAKE_SOURCE_DIR}/tools/sdb_doc_generator.py 
           -t ${SDB_PROJECT_TYPE} -o ${CMAKE_BINARY_DIR} -b ${BUILT_IN}
)

install(FILES "${CMAKE_BINARY_DIR}/sdb_doc.cnf"
        DESTINATION "conf/sample/"
        RENAME "sample.cnf")

# Extra install options.
if(MYSQL EQUAL "5.7.28")
   set(updateCmd "sed -ie \"s@\\(^[ ]\\{1,\\}\\)\\bREADME@\\1${SQL_SOURCE_ROOT}/README@\" ${SQL_SOURCE_ROOT}/CMakeLists.txt")
   execute_process(COMMAND bash -c ${updateCmd})
   set(updateCmd "sed -ie \"s@\\(^[ ]\\{1,\\}\\)\\bLICENSE@\\1${SQL_SOURCE_ROOT}/LICENSE@\" ${SQL_SOURCE_ROOT}/CMakeLists.txt")
   execute_process(COMMAND bash -c ${updateCmd})
   set(updateCmd "")
   set(updateCmd "sed -ie 's@\${CMAKE_SOURCE_DIR}/README@\${SQL_SOURCE_ROOT}/README@' ${SQL_SOURCE_ROOT}/mysql-test/CMakeLists.txt")
   execute_process(COMMAND bash -c ${updateCmd})
endif()
set(PROJECT_PACKAGE_DIR "${CMAKE_SOURCE_DIR}/package")
set(PROJECT_PACKAGE_SUB_DIR "${PROJECT_PACKAGE_DIR}/${SDB_PROJECT_TYPE}")
install(PROGRAMS "${PROJECT_PACKAGE_SUB_DIR}/sdb_sql_ctl"
        DESTINATION "bin")
install(PROGRAMS "${PROJECT_PACKAGE_SUB_DIR}/sequoiasql-${SDB_PROJECT_TYPE}-daemon"
        DESTINATION "bin")
install(PROGRAMS "${PROJECT_PACKAGE_SUB_DIR}/sequoiasql-${SDB_PROJECT_TYPE}.sysv"
        DESTINATION "bin")
install(FILES "${PROJECT_PACKAGE_SUB_DIR}/sequoiasql-${SDB_PROJECT_TYPE}.systemd"
        DESTINATION "bin")
install(PROGRAMS "${PROJECT_PACKAGE_DIR}/common/compatible.sh"
        DESTINATION "./")
install(PROGRAMS "${PROJECT_PACKAGE_DIR}/common/preUninstall.sh"
        DESTINATION "./")
install(DIRECTORY "${PROJECT_PACKAGE_SUB_DIR}/license"
        DESTINATION "./")
install(PROGRAMS "${CMAKE_SOURCE_DIR}/tools/sdbfk/sdb_fk_init.sh"
        DESTINATION "tools/sdbfk")
install(PROGRAMS "${CMAKE_SOURCE_DIR}/tools/sdbfk/sdb_fk_notify.sh"
        DESTINATION "tools/sdbfk")
install(FILES "${CMAKE_SOURCE_DIR}/tools/sdbfk/README.md"
        DESTINATION "tools/sdbfk")
install(PROGRAMS "${CMAKE_SOURCE_DIR}/tools/diagnostic/sdb_diag_info_dump"
        DESTINATION "tools/diagnostic")


execute_process( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )
if(MYSQL)
   if( ${ARCHITECTURE} STREQUAL "x86_64" )
      if(WITH_DEBUG)
         install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/lib/x86_64/debug"
                 DESTINATION "tools/lib")
         install(FILES "${CMAKE_SOURCE_DIR}/tools/lib/x86_64/debug/server_audit.so"
                 DESTINATION "lib/plugin")
      else()
         install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/lib/x86_64/release"
                 DESTINATION "tools/lib")
         install(FILES "${CMAKE_SOURCE_DIR}/tools/lib/x86_64/release/server_audit.so"
                 DESTINATION "lib/plugin")
      endif()
   elseif( ${ARCHITECTURE} STREQUAL "aarch64" )
      if(WITH_DEBUG)
         install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/lib/aarch64/debug"
                 DESTINATION "tools/lib")
         install(FILES "${CMAKE_SOURCE_DIR}/tools/lib/aarch64/debug/server_audit.so"
                 DESTINATION "lib/plugin")
      else()
         install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/lib/aarch64/release"
                 DESTINATION "tools/lib")
         install(FILES "${CMAKE_SOURCE_DIR}/tools/lib/aarch64/release/server_audit.so"
                 DESTINATION "lib/plugin")
      endif()
   else()
      install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/lib"
              DESTINATION "tools")
   endif()
elseif(MARIADB)
# do nothing
endif()
install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/metaSync"
        DESTINATION "tools")
install(FILES "${CMAKE_BINARY_DIR}/version.info"
        DESTINATION "./")
if(MYSQL)
   install(FILES "${CMAKE_SOURCE_DIR}/tools/doc/metaSync/mysql/README.md"
           DESTINATION "tools/metaSync")
elseif(MARIADB)
   install(FILES "${CMAKE_SOURCE_DIR}/tools/doc/metaSync/mariadb/README.md"
           DESTINATION "tools/metaSync")
endif()

include("${CMAKE_SOURCE_DIR}/package/cmakecpackage.cmake")


if(PACK_TEST)
   set(CLEAN_TEST_CMD ":" CACHE STRING "Take the mysql-test when packing")
else()
   set(CLEAN_TEST_CMD "rm ${CMAKE_INSTALL_PREFIX}/mysql-test" CACHE STRING "Remove the mysql-test before packing")
endif()

if(ENTERPRISE)
   set(EDITION_VAR "-enterprise")
else()
   set(EDITION_VAR "")
endif()
set(BUILD_COMMAND "builder build ${PROJECT_PACKAGE_SUB_DIR}/sequoiamysql_server.xml --setvars project.version=${PROJECT_VERSION} --setvars platform=linux_x86_64 --setvars edition=${EDITION_VAR} --setvars project.outputDirectory=${CMAKE_BINARY_DIR}")

if(PACK_TEST)
   add_custom_target(
      runpackage
      COMMAND bash -c "cp ${PROJECT_PACKAGE_SUB_DIR}/*.sys* ${CMAKE_INSTALL_PREFIX}/bin"
      COMMAND bash -c "ln -sf ${CMAKE_INSTALL_PREFIX} ${PROJECT_PACKAGE_SUB_DIR}/${SDB_PROJECT_TYPE}"
      COMMAND bash -c ${BUILD_COMMAND}
      comment "Building .run package...")
else()
   add_custom_target(
      runpackage
      COMMAND bash -c "rm -rf ${CMAKE_INSTALL_PREFIX}/mysql-test"
      COMMAND bash -c "cp ${PROJECT_PACKAGE_SUB_DIR}/*.sys* ${CMAKE_INSTALL_PREFIX}/bin"
      COMMAND bash -c "ln -sf ${CMAKE_INSTALL_PREFIX} ${PROJECT_PACKAGE_SUB_DIR}/${SDB_PROJECT_TYPE}"
      COMMAND bash -c ${BUILD_COMMAND}
      comment "Building .run package...")
endif()

add_custom_target(testcase
   COMMAND bash -c "ln -sf ${CMAKE_INSTALL_PREFIX}/lib/libsdbcpp.so libsdbcpp.so"
   COMMAND bash -c "./mysql-test-run.pl --suite=main,json --big-test --force --max-test-fail=0 --retry=1 --retry-failure=1 --parallel=4 --xml-report=${CMAKE_BINARY_DIR}/mysql_test_report.xml"
   WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/mysql-test"
   DEPENDS ${CMAKE_INSTALL_PREFIX}
   COMMENT "Run testcases...")

add_custom_target(testpackage
   COMMAND bash -c "tar zcvf ${CMAKE_BINARY_DIR}/${TEST_PKG_NAME}-test.tar.gz *"
   WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/mysql-test"
   DEPENDS ${CMAKE_INSTALL_PREFIX}
   COMMENT "Make testcase package..."
)
