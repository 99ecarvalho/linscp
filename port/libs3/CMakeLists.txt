# WinSCP's libs3, compiled from the generated tree (for the patch series) as
# its Makefile builds it: the same units, include path and defines. WinSCP's copy does its HTTP through
# neon rather than libcurl, so it links against linscp-neon.
#
# Windows only for now, like neon: inc/mingw supplies the POSIX headers
# libs3 expects on Windows.
if(NOT WIN32)
  return()
endif()

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

# The Makefile compiles these .c files with bcc32 -P: as C++, regardless of
# the extension. WinSCP's additions rely on it (struct names used without
# `struct`), and core declares libs3's functions with C++ linkage.
set_source_files_properties(${LINSCP_LIBS3_UNITS} PROPERTIES LANGUAGE CXX GENERATED TRUE)
add_library(linscp-libs3 STATIC EXCLUDE_FROM_ALL ${LINSCP_LIBS3_UNITS})
add_dependencies(linscp-libs3 transform)
# The Makefile's CFLAG and INCL, in its order; OpenSSL and expat come from
# their own targets.
target_compile_definitions(linscp-libs3 PRIVATE
  WIN32 WIN32_LEAN_AND_MEAN WINSCP NE_LFS)
target_include_directories(linscp-libs3 PRIVATE
  "${_s3_gen}/inc" "${_s3_gen}/inc/mingw" "${LINSCP_GEN}/libs/neon/src")
target_link_libraries(linscp-libs3 PRIVATE linscp-openssl linscp-expat)
target_link_libraries(linscp-libs3 INTERFACE linscp-neon)
# Upstream's code is compiled as it is; its warnings are not ours to fix.
target_compile_options(linscp-libs3 PRIVATE -w)
