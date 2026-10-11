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
# Upstream's preprocessor configuration, per project, read from its own .cbproj
# (port/tools/upstream_defines.py) so it follows upstream. Per project because
# they differ where it matters: core and FileZilla are built with UNICODE and
# PuTTY without, and UNICODE decides which Win32 A/W function every macro names.
# Applied with linscp_upstream_defines(target scope project).
# ---------------------------------------------------------------------------
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_SOURCE_DIR}/port/tools/upstream_defines.py")
function(linscp_upstream_defines TARGET SCOPE PROJECT)
  # Normalised: Ninja folds "source/../libs/x" and "libs/x" into one path and
  # rejects the dependency as registered twice.
  get_filename_component(_cbproj "${LINSCP_UPSTREAM}/source/${PROJECT}" ABSOLUTE)
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_cbproj}")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_defines.py"
            --target "${CMAKE_SYSTEM_NAME}" --config Base --project "${_cbproj}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _flags
    RESULT_VARIABLE _rc
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "upstream_defines.py failed reading ${_cbproj}")
  endif()
  string(REPLACE "\n" ";" _flags "${_flags}")
  list(TRANSFORM _flags REPLACE "^-D" "")
  # Debug adds _DEBUG as upstream's Debug configurations do; Release gets
  # NDEBUG from CMake itself.
  target_compile_definitions(${TARGET} ${SCOPE} ${_flags} "$<$<CONFIG:Debug>:_DEBUG>")
  message(STATUS "${TARGET}: upstream defines from ${PROJECT}: ${_flags}")
endfunction()

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
# Every generated file is a byproduct. Otherwise Ninja stats them before the
# transform runs, sees nothing newer than the objects, and compiles nothing:
# a new patch reached build/gen but not the libraries built from it.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${LINSCP_TRANSFORM}"
          --src "${LINSCP_UPSTREAM}" --list-outputs
  OUTPUT_VARIABLE _gen_files OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" _gen_files "${_gen_files}")
list(TRANSFORM _gen_files PREPEND "${LINSCP_GEN}/")
add_custom_command(
  OUTPUT  "${LINSCP_GEN_STAMP}"
  BYPRODUCTS ${_gen_files}
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
add_subdirectory(port/putty)
add_subdirectory(port/core)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(port/tests)
endif()

message(STATUS "linscp: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
               "-> ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
