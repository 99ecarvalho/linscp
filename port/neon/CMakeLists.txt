# WinSCP's neon, compiled from the generated tree as its Makefile.bcb builds
# it: the same units and the same defines. WinSCP changes neon extensively
# under WINSCP, so this is the copy core is written against. It is plain C,
# which the transform's rules leave as it is; the tree gives it the patch
# series.
#
# src/config.h is neon's Win32 configuration and empty elsewhere; on POSIX
# config-posix.h, from neon's configure, is force-included in its place.

set(_neon "${LINSCP_UPSTREAM}/libs/neon")
set(_neon_gen "${LINSCP_GEN}/libs/neon")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${_neon}/Makefile.bcb" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_NEON_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for neon's Makefile.bcb")
endif()
list(TRANSFORM LINSCP_NEON_UNITS PREPEND "${LINSCP_GEN}/")
set_source_files_properties(${LINSCP_NEON_UNITS} PROPERTIES GENERATED TRUE)

add_library(linscp-neon STATIC EXCLUDE_FROM_ALL ${LINSCP_NEON_UNITS})
add_dependencies(linscp-neon transform)
# Makefile.bcb's DEFINES, and the OpenSSL flags it takes from Makefile.inc.
linscp_upstream_defines(linscp-neon PRIVATE ../libs/neon/Makefile.bcb)
target_include_directories(linscp-neon PRIVATE "${_neon_gen}/src" "${_neon_gen}")
target_link_libraries(linscp-neon PRIVATE linscp-openssl linscp-expat)
target_link_libraries(linscp-neon INTERFACE linscp-openssl-ssl linscp-expat)
if(WIN32)
  target_link_libraries(linscp-neon INTERFACE ws2_32 secur32 iphlpapi)
else()
  target_compile_options(linscp-neon PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/config-posix.h")
endif()
set_target_properties(linscp-neon PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
# Upstream's code is compiled as it is; its warnings are not ours to fix.
# Two of the make file's defines are for C++Builder's own headers and break
# mingw-w64's: _ASSERTE, defined to 1, is a macro its malloc.h calls, and
# NOUSER removes the MSG its iphlpapi.h (through ole2.h) needs. Undefined
# after the -D flags, which CMake emits first.
target_compile_options(linscp-neon PRIVATE -w -U_ASSERTE -UNOUSER)
