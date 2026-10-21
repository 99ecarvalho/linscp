# Build expat from the pinned submodule rather than linking a system copy.
#
# The system has libexpat, but the mingw sysroot does not, and upstream vendors
# expat precisely so WinSCP can build it. Compiling the vendored source keeps
# both targets on the same version and adds no external dependency.
#
# The units are read from upstream's make file (bcb5/expat_static.mak), so a
# file expat adds or splits out -- as xcs.c and random_rand_s.c were -- is
# picked up without an edit. That list is Windows': its entropy source is
# rand_s. WinSCP's expat_config.h also selects getentropy(3), which glibc has
# had since 2.25, so Linux builds the same parser with that source instead.

# The system expat (LINSCP_SYSTEM_EXPAT, POSIX only), for distributions. The
# bundled copy is the 2.9.0 release with Windows-only changes (an entropy
# source for C++Builder, its expat_config.h) and one bounds check in
# xmltok_impl.c's position tracking (doc/qt/bundled-libraries.md); core,
# neon and libs3 use only expat's public char API. neon's one 2.9.0 call is
# mapped for older versions by port/patches/neon-xml-system-expat-line-number.patch.
if(LINSCP_SYSTEM_EXPAT)
  find_package(EXPAT 2.4 REQUIRED)
  message(STATUS "expat: system ${EXPAT_VERSION_STRING} (${EXPAT_LIBRARIES})")
  add_library(linscp-expat INTERFACE)
  target_link_libraries(linscp-expat INTERFACE EXPAT::EXPAT)
  # The targets that name expat's include directory themselves (core,
  # FileZilla) take this one: none, the system's is on the default path.
  set(LINSCP_EXPAT_INCLUDE)
  return()
endif()

set(LINSCP_EXPAT_DIR "${LINSCP_UPSTREAM}/libs/expat")
set(LINSCP_EXPAT_INCLUDE "${LINSCP_EXPAT_DIR}/lib")
set(_expat_mak "${LINSCP_EXPAT_DIR}/bcb5/expat_static.mak")

if(NOT EXISTS "${_expat_mak}")
  message(FATAL_ERROR "vendored expat missing at ${LINSCP_EXPAT_DIR}")
endif()

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${_expat_mak}" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE _expat_units
  COMMAND_ERROR_IS_FATAL ANY
  OUTPUT_STRIP_TRAILING_WHITESPACE)
list(TRANSFORM _expat_units PREPEND "${LINSCP_UPSTREAM}/")
if(NOT WIN32)
  list(FILTER _expat_units EXCLUDE REGEX "/(random_rand_s|loadlibrary)\\.c$")
  list(APPEND _expat_units "${LINSCP_EXPAT_DIR}/lib/random_getentropy.c")
endif()

add_library(linscp-expat STATIC ${_expat_units})
target_include_directories(linscp-expat PUBLIC
  "${LINSCP_EXPAT_DIR}" "${LINSCP_EXPAT_DIR}/lib")
target_compile_definitions(linscp-expat PUBLIC XML_STATIC HAVE_EXPAT_CONFIG_H)
if(WIN32)
  linscp_upstream_defines(linscp-expat PRIVATE ../libs/expat/bcb5/expat_static.mak)
  # As for neon: _ASSERTE, defined to 1, is a macro mingw-w64's malloc.h calls.
  target_compile_options(linscp-expat PRIVATE -U_ASSERTE)
endif()
# Upstream's own source, not ours: do not fail the build on its warnings.
target_compile_options(linscp-expat PRIVATE -w)
