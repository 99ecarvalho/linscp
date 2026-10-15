# WinSCP's core library, compiled from the transformed tree.
#
# The unit list is upstream's own (ScpCore.cbproj, via upstream_units.py), and
# the include paths and defines are the census's, so the two measure the same
# thing. EXCLUDE_FROM_ALL while it is being brought up: build it explicitly
# with --target linscp-core, and link-check it with --target linscp-core-link.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_CORE_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed")
endif()

set(LINSCP_CORE_SOURCES "")
foreach(_unit IN LISTS LINSCP_CORE_UNITS)
  list(APPEND LINSCP_CORE_SOURCES "${LINSCP_GEN}/${_unit}")
endforeach()
# Core's globals depend on each other across units, and C++Builder runs their
# constructors in project order. mingw-w64 runs them in reverse link order and
# Linux in link order, so the units are listed reversed for Windows: the
# executables link these objects directly, in this order (port/frontend's
# linscp-runtime), rather than leaving it to archive member extraction.
if(WIN32)
  list(REVERSE LINSCP_CORE_SOURCES)
endif()
set_source_files_properties(${LINSCP_CORE_SOURCES} PROPERTIES GENERATED TRUE)

add_library(linscp-core STATIC EXCLUDE_FROM_ALL ${LINSCP_CORE_SOURCES})
add_dependencies(linscp-core transform)
# A release build (the packages): core's BUILD_OFFICIAL, so its version
# strings drop upstream's "Dev Build -- Do NOT distribute" marker, which is
# for anyone's build of WinSCP from source. Only the version strings (and the
# update check's "unofficial" flag) depend on it.
option(LINSCP_RELEASE "A release build: what the packages ship" OFF)
if(LINSCP_RELEASE)
  target_compile_definitions(linscp-core PRIVATE BUILD_OFFICIAL)
endif()
# OpenSSL's headers come from the configured release (cmake/openssl.cmake),
# not WinSCP's copy, whose configuration.h describes 32-bit Windows.
target_link_libraries(linscp-core PUBLIC linscp-shim linscp-openssl)

set(_up "${LINSCP_UPSTREAM}/source")
target_include_directories(linscp-core PUBLIC
  "${LINSCP_GEN}/source/core"
  "${_up}/resource"
  "${_up}/windows"
  "${LINSCP_GEN}/source/filezilla"
  "${LINSCP_GEN}/source/putty"
  # PuTTY's Windows platform layer; on Linux this is P1-09's to replace.
  "${LINSCP_GEN}/source/putty/windows"
  "${_up}/components"
  "${LINSCP_GEN}/libs/neon/src"
  "${LINSCP_UPSTREAM}/libs/expat/lib"
  "${LINSCP_GEN}/libs/libs3/inc")
# The Win32 import libraries core calls into directly: sockets, CryptoAPI
# certificate chains, shell path helpers, version resources, GetUserNameEx and
# MIME sniffing.
if(WIN32)
  target_link_libraries(linscp-core PUBLIC ws2_32 crypt32 shlwapi version secur32 urlmon)
endif()

# Upstream's code is compiled as it is; its warnings are not ours to fix.
target_compile_options(linscp-core PRIVATE -w)

# Links every core object into one executable, so the linker names every
# symbol core uses and nothing defines -- which a static library alone never
# reports. It is a measurement, not a program: main() does nothing.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/link_check_main.cpp" "int main() { return 0; }\n")
add_executable(linscp-core-link EXCLUDE_FROM_ALL "${CMAKE_CURRENT_BINARY_DIR}/link_check_main.cpp")
# linscp-runtime links every core object (in initialisation order) and every
# library core uses; defined later, in port/frontend, and resolved when CMake
# generates.
# lld stops after 20 errors unless told otherwise; GNU ld reports them all.
target_link_libraries(linscp-core-link PRIVATE
  $<$<STREQUAL:${CMAKE_CXX_COMPILER_LINKER_ID},LLD>:-Wl,--error-limit=0> linscp-runtime)
