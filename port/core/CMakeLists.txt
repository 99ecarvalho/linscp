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
set_source_files_properties(${LINSCP_CORE_SOURCES} PROPERTIES GENERATED TRUE)

add_library(linscp-core STATIC EXCLUDE_FROM_ALL ${LINSCP_CORE_SOURCES})
add_dependencies(linscp-core transform)
target_link_libraries(linscp-core PUBLIC linscp-shim)

set(_up "${LINSCP_UPSTREAM}/source")
target_include_directories(linscp-core PUBLIC
  "${LINSCP_GEN}/source/core"
  "${_up}/resource"
  "${_up}/windows"
  "${_up}/filezilla"
  "${_up}/putty"
  # PuTTY's Windows platform layer; on Linux this is P1-09's to replace.
  "${_up}/putty/windows"
  "${_up}/components"
  "${LINSCP_UPSTREAM}/libs/openssl/include"
  "${LINSCP_UPSTREAM}/libs/neon/src"
  "${LINSCP_UPSTREAM}/libs/expat/lib"
  "${LINSCP_UPSTREAM}/libs/libs3/inc")
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
target_link_libraries(linscp-core-link PRIVATE
  -Wl,--error-limit=0
  -Wl,--whole-archive linscp-core -Wl,--no-whole-archive linscp-shim)
if(TARGET linscp-putty)
  target_link_libraries(linscp-core-link PRIVATE linscp-putty linscp-puttyvs)
endif()
