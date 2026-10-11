# WinSCP's neon, compiled from the generated tree as its Makefile.bcb builds
# it: the same units and the same defines. WinSCP changes neon extensively
# under WINSCP, so this is the copy core is written against. It is plain C,
# which the transform's rules leave as it is; the tree gives it the patch
# series.
#
# Windows only for now: src/config.h is neon's Win32 configuration.
if(NOT WIN32)
  return()
endif()

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
# Makefile.bcb's CFLAG, less what only bcc32 needs (_RTLDLL, the
# _stricmp=stricmp renames) and NOUSER: mingw-w64's iphlpapi.h, which
# ne_socket.c includes, reaches ole2.h, and that needs user32's MSG.
target_compile_definitions(linscp-neon PRIVATE
  WIN32 WIN32_LEAN_AND_MEAN NE_HAVE_SSL HAVE_OPENSSL HAVE_EXPAT HAVE_EXPAT_H
  NE_HAVE_DAV NE_LFS USE_GETADDRINFO NOGDI COMPILED_FROM_DSP
  XML_STATIC HAVE_STRTOLL WINSCP)
target_include_directories(linscp-neon PRIVATE "${_neon_gen}/src" "${_neon_gen}")
target_link_libraries(linscp-neon PRIVATE linscp-openssl linscp-expat)
target_link_libraries(linscp-neon INTERFACE linscp-openssl-ssl linscp-expat
  ws2_32 secur32 iphlpapi)
set_target_properties(linscp-neon PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
# Upstream's code is compiled as it is; its warnings are not ours to fix.
# ne_socket.c calls WinSCP's ne_init_ssl_session without including the header
# that declares it, which bcc32 compiles as C89 does; clang makes an implicit
# declaration an error unless told otherwise. The call matches the implicit
# signature's ABI: pointer arguments, result unused.
target_compile_options(linscp-neon PRIVATE -w
  -Wno-error=implicit-function-declaration)
