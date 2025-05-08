function(build_curl type)
   # Build curl
   message(STATUS "Build Curl, call ${CMAKE_SOURCE_DIR}/thirdparty/"
            "build_thirdparty.py  --curl=7.88.1")
   if(type STREQUAL "debug")
      set(debug_opt " -d")
   else()
      set(debug_opt "")
   endif()
   execute_process(
      COMMAND bash -c "python3 build_thirdparty.py --curl=7.88.1 ${debug_opt}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/
   )
endfunction()

MACRO(SDB_CHECK_CURL)
   IF(NOT WITH_CURL)
      SET(WITH_CURL "${CMAKE_SOURCE_DIR}/thirdparty/curl-7.88.1/install_path/")
      SET(CURL_LIBRARY "${WITH_CURL}/lib")
      SET(CURL_INCLUDE_DIR "${WITH_CURL}/include")
   ENDIF()

   # Find thirdparty, then system. If system curl version not required, then build thirdparty.
   SET(CURL_ROOT_DIR "${WITH_CURL}")
   set(_SDB_CURL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
   set(CMAKE_FIND_LIBRARY_SUFFIXES .a )
   FIND_PACKAGE(CURL)
   set(CMAKE_FIND_LIBRARY_SUFFIXES ${_SDB_CURL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})

   IF(CURL_LIBRARY STREQUAL "CURL_LIBRARY-NOTFOUND" OR
      NOT "${CURL_VERSION_STRING}" STREQUAL "7.88.1")
      IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR WITH_DEBUG)
         build_curl("debug")
      ELSE()
         build_curl("release")
      ENDIF()
      FIND_PACKAGE(CURL)
   ELSE()
      MESSAGE(STATUS "Curl is found in ${WITH_CURL}, version is ${CURL_VERSION_STRING}.")
   ENDIF()
ENDMACRO()
