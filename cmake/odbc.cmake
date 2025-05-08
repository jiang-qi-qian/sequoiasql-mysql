# variable:: ODBC_FOUND
# variable:: ODBC_INCLUDE_DIRS
# variable:: ODBC_LIBRARIES
# variable:: ODBC_CONFIG

# cache variables
# varaible:: ODBC_INCLUDE_DIR
# variable:: ODBC_LIBRARY

# output variables
# varaible:: ODBC_INCLUDE_DIR
# variable:: ODBC_LIBRARY

cmake_minimum_required(VERSION 2.8.7)
function(build_unix_odbc type)
   # Build openssl
   if(type STREQUAL "debug")
      set(debug_opt " -d")
   else()
      set(debug_opt "")
   endif()
   if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set(arch " --arch=aarch64")
   endif()
   message(STATUS "Build UnixODBC, call ${CMAKE_SOURCE_DIR}/thirdparty/"
           "build_thirdparty.py  --odbc=2.3.1 ${debug_opt} ${arch}")
   execute_process(
      COMMAND bash -c "python3 build_thirdparty.py --odbc=2.3.1 ${debug_opt} ${arch}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/
   )
endfunction()



MACRO(SDB_CHECK_ODBC)
  set(_odbc_include_paths)
  set(_odbc_lib_paths)
  set(_odbc_lib_names)
  set(_odbc_required_libs_names)

  IF(NOT UNIX_ODBC_DIR)
  SET(UNIX_ODBC_DIR "${CMAKE_SOURCE_DIR}/thirdparty/unixODBC-2.3.1/install_path/")
  SET(ODBC_INCLUDE_DIR ${UNIX_ODBC_DIR}/include)
  ENDIF()

  find_program(ODBC_CONFIG NAMES odbc_config iodbc-config
               PATHS ${UNIX_ODBC_DIR}/bin
               DOC "Path to unixODBC or iODBC config program"
               NO_CMAKE_SYSTEM_PATH
               NO_DEFAULT_PATH
               NO_CMAKE_ENVIRONMENT_PATH)
  mark_as_advanced(ODBC_CONFIG)

  if(NOT ODBC_CONFIG)
    set(_odbc_lib_names odbc;iodbc;unixodbc;)
    IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR WITH_DEBUG)
      build_unix_odbc("debug")
    ELSE()
      build_unix_odbc("release")
    ENDIF()
    find_program(ODBC_CONFIG NAMES odbc_config iodbc-config
    PATHS ${UNIX_ODBC_DIR}/bin
    DOC "Path to unixODBC or iODBC config program")
    mark_as_advanced(ODBC_CONFIG)
  endif()

  # unixODBC and iODBC accept unified command line options
  execute_process(COMMAND ${ODBC_CONFIG} --cflags OUTPUT_VARIABLE _cflags
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND ${ODBC_CONFIG} --libs OUTPUT_VARIABLE _libs
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND ${ODBC_CONFIG} --version OUTPUT_VARIABLE _version
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Collect paths of include directories from CFLAGS
  separate_arguments(_cflags UNIX_COMMAND "${_cflags}")
  foreach(arg IN LISTS _cflags)
    if("${arg}" MATCHES "^-I(.*)$")
      list(APPEND _odbc_include_paths "${CMAKE_MATCH_1}")
    endif()
  endforeach(arg IN LISTS _cflags)
  unset(_cflags)

  # Collect paths of library names and directories from LIBS
  separate_arguments(_libs UNIX_COMMAND "${_libs}")
  MESSAGE(STATUS "_libs is ${_libs}")
  foreach(arg IN LISTS _libs)
    if("${arg}" MATCHES "^-L(.*)$")
      list(APPEND _odbc_lib_paths "${CMAKE_MATCH_1}")
    elseif("${arg}" MATCHES "^-l(.*)$")
      set(_lib_name ${CMAKE_MATCH_1})
      string(REGEX MATCH "odbc" _is_odbc ${_lib_name})
      if(_is_odbc)
        list(APPEND _odbc_lib_names ${_lib_name})
      else()
        list(APPEND _odbc_required_libs_names ${_lib_name})
      endif()
      unset(_lib_name)
    endif()
  endforeach()
  unset(_libs)

  MESSAGE(STATUS "_odbc_lib_paths is ${_odbc_lib_paths}, "
          "_odbc_lib_names:${_odbc_lib_names}")
  # Find include directories
  find_path(ODBC_INCLUDE_DIR NAMES sql.h PATHS ${_odbc_include_paths})
  MESSAGE(STATUS "ODBC_INCLUDE_DIR is ${ODBC_INCLUDE_DIR}")
  # Find libraries
  if(NOT ODBC_LIBRARIES)
    find_library(ODBC_LIBRARY
                NAMES ${_odbc_lib_names}
                HINTS ${_odbc_lib_paths}
                PATH_SUFFIXES odbc)
  endif()
  MESSAGE(STATUS "ODBC_LIBRARY is ${ODBC_LIBRARY}")
  unset(_odbc_include_paths)
  unset(_odbc_lib_paths)
  unset(_odbc_lib_names)
  unset(_odbc_required_libs_names)
endmacro()
