cmake_minimum_required(VERSION 3.25)
project(linscp LANGUAGES C CXX
        DESCRIPTION "Qt port of WinSCP for Linux and Windows")
if(WIN32)
  # Version resources (port/frontend).
  enable_language(RC)
endif()

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
  # A project file (.cbproj, .vcxproj), or a Borland make file for the
  # libraries WinSCP builds that way (neon, libs3, the MFC subset).
  if(_cbproj MATCHES "\\.(bcb|mak)$" OR _cbproj MATCHES "/Makefile$")
    set(_cmd "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py" --defines --project "${_cbproj}")
  else()
    set(_cmd "${CMAKE_SOURCE_DIR}/port/tools/upstream_defines.py"
             --target "${CMAKE_SYSTEM_NAME}" --config Base --project "${_cbproj}")
  endif()
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" ${_cmd}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _flags
    RESULT_VARIABLE _rc
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "upstream_defines.py failed reading ${_cbproj}")
  endif()
  string(REPLACE "\n" ";" _flags "${_flags}")
  list(TRANSFORM _flags REPLACE "^-D" "")
  # Upstream has two configurations: Debug defines _DEBUG, Release NDEBUG.
  # Every non-Debug build is the latter, including one with no build type,
  # for which CMake adds nothing itself. Neither would leave PuTTY's standard
  # assert live, which upstream never builds: Debug routes it to core's
  # DoAssertC, Release compiles it out -- and core relies on that, reading
  # CONF_username with conf_get_str, which asserts on its type.
  target_compile_definitions(${TARGET} ${SCOPE} ${_flags}
    "$<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>")
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
include(cmake/openssl.cmake)

add_subdirectory(port/shim)
add_subdirectory(port/putty)
add_subdirectory(port/filezilla)
add_subdirectory(port/neon)
add_subdirectory(port/libs3)
add_subdirectory(port/core)
add_subdirectory(port/frontend)

# The Qt shell, when Qt is there: Debian's qt6-base-dev natively; for the
# Windows cross-build, MSYS2's Qt (port/tools/fetch_msys2_qt.py) with
# Debian's as the host tools, the same version (ADR 0011).
# Without the host Qt, Qt's packages fall back to the target's own tools
# (moc.exe), which do not run here, so its absence stops the shell.
set(_qt_host_ok TRUE)
if(CMAKE_CROSSCOMPILING AND NOT QT_HOST_PATH)
  file(GLOB _qt_host_info LIST_DIRECTORIES true /usr/lib/*/cmake/Qt6HostInfo /usr/lib/cmake/Qt6HostInfo)
  if(_qt_host_info)
    list(GET _qt_host_info 0 _qt_host_info)
    get_filename_component(_qt_host_cmake "${_qt_host_info}" DIRECTORY)
    set(QT_HOST_PATH /usr CACHE PATH "Qt for the build host, whose moc, uic and rcc the cross-build runs")
    set(QT_HOST_PATH_CMAKE_DIR "${_qt_host_cmake}" CACHE PATH "The host Qt's CMake packages")
  else()
    set(_qt_host_ok FALSE)
    message(STATUS "linscp: no host Qt (qt6-base-dev) for the cross-build's moc, uic and rcc")
  endif()
endif()
if(_qt_host_ok)
  find_package(Qt6 6.8 QUIET COMPONENTS Widgets)
endif()
if(Qt6_FOUND)
  add_subdirectory(port/qt)
else()
  message(STATUS "linscp: no Qt 6.8 Widgets found, the Qt shell is not built")
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(port/tests)
endif()

message(STATUS "linscp: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} "
               "-> ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
