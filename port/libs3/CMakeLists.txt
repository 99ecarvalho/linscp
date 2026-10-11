# WinSCP's libs3, compiled from the generated tree (for the patch series) as
# its Makefile builds it: the same units, include path and defines. WinSCP's copy does its HTTP through
# neon rather than libcurl, so it links against linscp-neon.

set(_s3 "${LINSCP_UPSTREAM}/libs/libs3")
set(_s3_gen "${LINSCP_GEN}/libs/libs3")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${_s3}/Makefile" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_LIBS3_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for libs3's Makefile")
endif()
list(TRANSFORM LINSCP_LIBS3_UNITS PREPEND "${LINSCP_GEN}/")
set(_mingw_functions "${_s3_gen}/src/mingw_functions.c")
if(NOT WIN32)
  # mingw_functions.c and inc/mingw are pthreads and uname over Win32; POSIX
  # has the real ones.
  list(REMOVE_ITEM LINSCP_LIBS3_UNITS "${_mingw_functions}")
endif()

# The Makefile compiles these .c files with bcc32 -P: as C++, regardless of
# the extension. WinSCP's additions rely on it (struct names used without
# `struct`), and core declares libs3's functions with C++ linkage.
set_source_files_properties(${LINSCP_LIBS3_UNITS} PROPERTIES LANGUAGE CXX GENERATED TRUE)
add_library(linscp-libs3 STATIC EXCLUDE_FROM_ALL ${LINSCP_LIBS3_UNITS})
add_dependencies(linscp-libs3 transform)
# The Makefile's DEFINES (and Makefile.inc's OpenSSL flags) and INCL, in its
# order; OpenSSL and expat come from their own targets.
linscp_upstream_defines(linscp-libs3 PRIVATE ../libs/libs3/Makefile)
target_include_directories(linscp-libs3 PRIVATE
  "${_s3_gen}/inc" "${LINSCP_GEN}/libs/neon/src")
# inc/mingw stands in for POSIX headers mingw lacks: sys/utsname.h, and a
# pthread.h over Win32 that mingw_functions.c implements. That unit gets it
# first, as upstream's include path has it. Every other unit searches it only
# after the system's: request.c now includes <memory>, whose libstdc++
# threading support needs the real winpthreads pthread.h (C++Builder's library
# never asks for one). request.c's own mutex calls then reach winpthreads,
# linked anyway; libs3's versions, compiled as C++, have other names.
if(WIN32)
foreach(_unit IN LISTS LINSCP_LIBS3_UNITS)
  if(_unit STREQUAL _mingw_functions)
    set_property(SOURCE "${_unit}" APPEND PROPERTY INCLUDE_DIRECTORIES "${_s3_gen}/inc/mingw")
  else()
    set_property(SOURCE "${_unit}" APPEND PROPERTY COMPILE_OPTIONS -idirafter "${_s3_gen}/inc/mingw")
  endif()
endforeach()
endif()
target_link_libraries(linscp-libs3 PRIVATE linscp-openssl linscp-expat)
if(NOT WIN32)
  # WinSCP's additions use a little Win32 (GetSystemTime, strnicmp), which
  # on Windows arrives through OpenSSL's headers including <windows.h>.
  target_link_libraries(linscp-libs3 PRIVATE linscp-shim)
  target_compile_options(linscp-libs3 PRIVATE -include windows.h)
endif()
target_link_libraries(linscp-libs3 INTERFACE linscp-neon)
# Upstream's code is compiled as it is; its warnings are not ours to fix.
target_compile_options(linscp-libs3 PRIVATE -w)
