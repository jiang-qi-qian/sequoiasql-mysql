
# IN: COMMIT_SHA
#
# OUT:

# Trigger the sdb version tool building, and use it to get the version.
function(get_sdb_version version git_version)
   set(SDB_VERTOOL_DIR ${CMAKE_SOURCE_DIR}/misc/versiontool)
   set(SDB_VERTOOL ${SDB_VERTOOL_DIR}/sdbvertool)

   # Build the sdb version tool in misc.
   if(IS_ABSOLUTE ${WITH_SDB_DRIVER})
      set(SDB_DRIVER_DIR "${WITH_SDB_DRIVER}")
   else()
      set(SDB_DRIVER_DIR "${CMAKE_BINARY_DIR}/${WITH_SDB_DRIVER}")
   endif()

   if(OPENSSL_LIB_DIR)
      execute_process(
         COMMAND bash -c "cmake . -DSDB_DRIVER_DIR=${SDB_DRIVER_DIR} -DOPENSSL_LIB_DIR=${OPENSSL_LIB_DIR}"
         WORKING_DIRECTORY "${SDB_VERTOOL_DIR}"
      )
   else()
      execute_process(
         COMMAND bash -c "cmake . -DSDB_DRIVER_DIR=${SDB_DRIVER_DIR}"
         WORKING_DIRECTORY "${SDB_VERTOOL_DIR}"
      )
   endif()

   execute_process(
      COMMAND bash -c "make"
      WORKING_DIRECTORY "${SDB_VERTOOL_DIR}"
   )

   # Execute the sdb version tool to get the version.
   execute_process(
      COMMAND bash -c "${SDB_VERTOOL}"
      COMMAND bash -c "grep Version"
      COMMAND bash -c "awk '{print $2}'"
      OUTPUT_VARIABLE SDB_VERSION
   )

   execute_process(
      COMMAND bash -c "${SDB_VERTOOL}"
      COMMAND bash -c "grep Git"
      COMMAND bash -c "awk '{print $3}'"
      OUTPUT_VARIABLE SDB_GIT_VERSION
   )

   # The original value contains a '\n' at the end. Remove it.
   string(STRIP ${SDB_VERSION} SDB_VERSION)
   string(STRIP ${SDB_GIT_VERSION} SDB_GIT_VERSION)

   set(${version} ${SDB_VERSION} PARENT_SCOPE)
   set(${git_version} ${SDB_GIT_VERSION} PARENT_SCOPE)
endfunction()

function(get_my_version my_version length)
   IF(NOT COMMIT_SHA)
     if(${length} EQUAL 0)
         set(GIT_LOG_CMD "git log -1 --format=%H")
     else()
         set(GIT_LOG_CMD "git log -1 --format=%h --abbrev=${length}")
     endif()
     execute_process(
         COMMAND bash -c "${GIT_LOG_CMD}"
         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
         OUTPUT_VARIABLE GIT_VERSION
     )
 
     string(STRIP ${GIT_VERSION} GIT_VERSION)
     set(${my_version} ${GIT_VERSION} PARENT_SCOPE)
   ELSE()
     IF(${length} EQUAL 0)
         set(my_version "${COMMIT_SHA}" PARENT_SCOPE)
     else()
         string(SUBSTRING "${COMMIT_SHA}" 0 ${length} temp)
         set(my_version "${temp}" PARENT_SCOPE)
     ENDIF()
   ENDIF()
endfunction()

# Generate the project version file, which contains version of this project and
# the SequoiaDB driver.
MACRO(gen_version_file)
   set(SDB_PLUGIN_VERSION "")
   get_my_version(SDB_PLUGIN_VERSION 0)
   message(STATUS "SDB plugin version: ${SDB_PLUGIN_VERSION}")

   set(SDB_VERSION "")
   set(SDB_GIT_VERSION "")
   get_sdb_version(SDB_VERSION SDB_GIT_VERSION)

   string(TIMESTAMP BUILDDATE "%Y-%m-%d-%H.%M.%S")
   set(BUILD_TYPE_STR "")
   if(ENTERPRISE)
      set(BUILD_TYPE_STR "${BUILD_TYPE_STR}Enterprise")
   endif()

   if(HYBRID)
      if(BUILD_TYPE_STR)
         set(BUILD_TYPE_STR "${BUILD_TYPE_STR} Hybrid")
      else()
         set(BUILD_TYPE_STR "${BUILD_TYPE_STR}Hybrid")
      endif()
   endif()

   if(CMAKE_BUILD_TYPE MATCHES "Debug")
     if((NOT ENTERPRISE) AND (NOT HYBRID))
       set(BUILD_TYPE_STR "${BUILD_TYPE_STR}Debug")
     else()
       set(BUILD_TYPE_STR "${BUILD_TYPE_STR} Debug")
     endif()
   else()
      SET(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
          "Type of build, options are: Debug RelWithDebInfo" FORCE)
   endif()
   if(NOT (BUILD_TYPE_STR STREQUAL ""))
      set(BUILD_TYPE_STR " (${BUILD_TYPE_STR})")
   endif()

   set(FULL_VERSION "${PROJECT_NAME} version: ${SDB_VERSION}\nGit version: ${SDB_PLUGIN_VERSION}\nSDB driver: ${SDB_VERSION}(${SDB_GIT_VERSION})\n${BUILDDATE}${BUILD_TYPE_STR}\n")
   file(WRITE ${CMAKE_BINARY_DIR}/version.info ${FULL_VERSION})
   file(WRITE ${CMAKE_SOURCE_DIR}/version ${SDB_VERSION})
ENDMACRO()
