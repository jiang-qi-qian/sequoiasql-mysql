# Build thirdparty.
function(build_openssl type)
   # Build openssl
   message(STATUS "Build OpenSSL, call ${CMAKE_SOURCE_DIR}/thirdparty/"
           "build_thirdparty.py  --openssl=1.1.1t")
   if(type STREQUAL "debug")
      set(debug_opt " -d")
   else()
      set(debug_opt "")
   endif()
   execute_process(
      COMMAND bash -c "python3 build_thirdparty.py --openssl=1.1.1t ${debug_opt}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/
   )
endfunction()

MACRO(SDB_CHECK_SSL)
   IF(NOT WITH_SSL)
      SET(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/thirdparty/openssl-1.1.1t/install_path")
   ELSE()
      SET(OPENSSL_ROOT_DIR "${WITH_SSL}")
   ENDIF()

   SET(OPENSSL_USE_STATIC_LIBS ON)
   SET(THREADS_PREFER_PTHREAD_FLAG ON)
   set(_SDB_OPENSSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
   set(CMAKE_FIND_LIBRARY_SUFFIXES .a )

   FIND_LIBRARY(OpenSSL NAMES libssl.a
                PATHS "${OPENSSL_ROOT_DIR}/lib"
                NO_CMAKE_SYSTEM_PATH
                NO_DEFAULT_PATH
                NO_CMAKE_ENVIRONMENT_PATH)
   set(CMAKE_FIND_LIBRARY_SUFFIXES ${_SDB_OPENSSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
   IF(NOT OpenSSL STREQUAL OpenSSL-NOTFOUND)
      MESSAGE(STATUS "OpenSSL is found in ${OPENSSL_ROOT_DIR}, version is ${OPENSSL_VERSION}.")
   ELSE()
      IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR WITH_DEBUG)
         build_openssl("debug")
      ELSE()
         build_openssl("release")
      ENDIF()
      SET(OPENSSL_USE_STATIC_LIBS ON)
      SET(THREADS_PREFER_PTHREAD_FLAG ON)
      set(_SDB_OPENSSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
      set(CMAKE_FIND_LIBRARY_SUFFIXES .a )
      FIND_PACKAGE(OpenSSL)
      set(CMAKE_FIND_LIBRARY_SUFFIXES ${_SDB_OPENSSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
   ENDIF()
   IF(NOT WITH_SSL)
      SET(WITH_SSL "${OPENSSL_ROOT_DIR}" CACHE STRING "")
   ENDIF()
   SET(OPENSSL_LIB_DIR "${OPENSSL_ROOT_DIR}/lib" CACHE STRING "")
   SET(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE STRING "")
ENDMACRO(SDB_CHECK_SSL)
