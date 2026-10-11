cmake_minimum_required(VERSION 3.25)
project(linscp LANGUAGES C CXX
        DESCRIPTION "Qt port of WinSCP for Linux and Windows")

# ---------------------------------------------------------------------------
# clang is mandatory: the VCL property shim relies on __declspec(property),
# which GCC does not implement. Fail early and clearly rather than at the
# first transformed header.
# ---------------------------------------------------------------------------
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "linscp requires clang (found ${CMAKE_CXX_COMPILER_ID}). "
    "GCC has no __declspec(property) equivalent. "
    "Configure with -DCMAKE_CXX_COMPILER=clang++")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -fms-extensions: __declspec(property) for the VCL property shim.
# -fshort-wchar:   UnicodeString is UTF-16 and upstream assumes a 2-byte
#                  wchar_t. Must be uniform across every translation unit --
#                  a single TU without it is an ABI landmine. See ADR 0001.
add_compile_options(-fms-extensions -fshort-wchar)

set(LINSCP_UPSTREAM "${CMAKE_SOURCE_DIR}/3rd-party/winscp"
    CACHE PATH "Pristine WinSCP submodule")
set(LINSCP_GEN "${CMAKE_BINARY_DIR}/gen"
    CACHE PATH "Generated (transformed) upstream tree")

if(NOT EXISTS "${LINSCP_UPSTREAM}/source/core/Common.cpp")
  message(FATAL_ERROR
    "Upstream submodule missing. Run: git submodule update --init --recursive")
endif()

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

# ---------------------------------------------------------------------------
# The preprocessor configuration upstream compiles core with, read from its
# own ScpCore.cbproj (port/tools/upstream_defines.py) so it follows upstream.
# Applied to everything, shim included: on Windows UNICODE decides whether each
# A/W Win32 macro means the narrow or the wide function, so the shim must see
# exactly what core sees.
# ---------------------------------------------------------------------------
set(LINSCP_CBPROJ "${LINSCP_UPSTREAM}/source/ScpCore.cbproj")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${LINSCP_CBPROJ}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_defines.py")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_defines.py"
          --target "${CMAKE_SYSTEM_NAME}" --config Base
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_UPSTREAM_FLAGS
  RESULT_VARIABLE _defines_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _defines_rc EQUAL 0)
  message(FATAL_ERROR "upstream_defines.py failed reading ${LINSCP_CBPROJ}")
endif()
string(REPLACE "\n" ";" LINSCP_UPSTREAM_FLAGS "${LINSCP_UPSTREAM_FLAGS}")
foreach(_flag IN LISTS LINSCP_UPSTREAM_FLAGS)
  string(REGEX REPLACE "^-D" "" _def "${_flag}")
  add_compile_definitions("${_def}")
endforeach()
# Debug adds _DEBUG as upstream's Debug configuration does; Release gets
# NDEBUG from CMake itself.
add_compile_definitions("$<$<CONFIG:Debug>:_DEBUG>")
message(STATUS "Upstream defines: ${LINSCP_UPSTREAM_FLAGS}")

# ---------------------------------------------------------------------------
# Transform the pristine upstream tree. Never edited in place; the generated
# tree is a build artefact.
# ---------------------------------------------------------------------------
set(LINSCP_TRANSFORM "${CMAKE_SOURCE_DIR}/port/tools/transform.py")
# The stamp lives outside LINSCP_GEN so the generated tree stays a faithful
# mirror of upstream; verify_transform.py treats any extra file as a violation.
set(LINSCP_GEN_STAMP "${CMAKE_BINARY_DIR}/transform.stamp")
# The transform's inputs include the reviewed patch series, not just the
# scripts. Without this, a change to closure_sites.json left the generated tree
# silently stale.
file(GLOB LINSCP_PATCHES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/port/patches/*")
add_custom_command(
  OUTPUT  "${LINSCP_GEN_STAMP}"
  COMMAND "${Python3_EXECUTABLE}" "${LINSCP_TRANSFORM}"
          --src "${LINSCP_UPSTREAM}" --out "${LINSCP_GEN}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/verify_transform.py"
          "${LINSCP_GEN}" "${LINSCP_UPSTREAM}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${LINSCP_GEN_STAMP}"
  DEPENDS "${LINSCP_TRANSFORM}" "${CMAKE_SOURCE_DIR}/port/tools/verify_transform.py"
          ${LINSCP_PATCHES}
  COMMENT "Transforming upstream core and verifying the diff is property-only"
  VERBATIM)
add_custom_target(transform ALL DEPENDS "${LINSCP_GEN_STAMP}")

include(cmake/expat.cmake)

add_subdirectory(port/shim)
add_subdirectory(port/core)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(port/tests)
endif()

message(STATUS "linscp: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
               "-> ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
