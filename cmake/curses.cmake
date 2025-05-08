function(build_curses type)
   # Build curses
   message(STATUS "Build Curses, call ${CMAKE_SOURCE_DIR}/thirdparty/"
            "build_thirdparty.py  --curses=5.9")
   if(type STREQUAL "debug")
      set(debug_opt " -d")
   else()
      set(debug_opt "")
   endif()
   if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set(arch_opt " --arch=aarch64")
   else()
      set(arch_opt "")
   endif()
   execute_process(
      COMMAND bash -c "python3 build_thirdparty.py --curses=5.9 ${debug_opt} ${arch_opt}"
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/thirdparty/
   )
endfunction()

MACRO(SDB_CHECK_CURSES)
   IF(NOT WITH_CURSES)
      SET(WITH_CURSES "${CMAKE_SOURCE_DIR}/thirdparty/ncurses-5.9/install_path")
   ENDIF()

   ADD_DEFINITIONS(-DHAVE_TERM_H=1)
   SET(HAVE_TERM_H 1 CACHE INTERNAL "Have term.h curses.h")
   SET(HAVE_CURSES_H 1 CACHE INTERNAL "Have curses.h")
   SET(CURSES_INCLUDE_PATH "${WITH_CURSES}/include")
   INCLUDE_DIRECTORIES(${WITH_CURSES}/include )
   INCLUDE_DIRECTORIES(${WITH_CURSES}/include/ncurses )
   # Ignore system ncurses
   # FIND_PACKAGE(Curses) cannot specific others paths, so use FIND_LIBRARY instead.
   FIND_LIBRARY(CURSES_LIBRARY
      NAMES ncurses
      PATHS ${WITH_CURSES}/lib
      NO_DEFAULT_PATH
      NO_CMAKE_ENVIRONMENT_PATH
      NO_SYSTEM_ENVIRONMENT_PATH
   )

   IF(CURSES_LIBRARY STREQUAL "CURSES_LIBRARY-NOTFOUND")
      IF(CMAKE_BUILD_TYPE MATCHES "Debug" OR WITH_DEBUG)
         build_curses("debug")
      ELSE()
         build_curses("release")
      ENDIF()
      SET(CURSES_LIBRARY "${WITH_CURSES}/lib/libncurses.a")
   ELSE()
      MESSAGE(STATUS "Curses is found in ${WITH_CURSES}, version is ${CURSES_VERSION_STRING}.")
   ENDIF()
ENDMACRO()
