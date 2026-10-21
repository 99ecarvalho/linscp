# Reproducible, path-independent builds (R0-22).
#
# Two builds of one commit, from checkouts and build directories at different
# absolute paths, give byte-identical programs. Everything that would differ
# is pinned here, for every target defined after this file is included:
#
#  * Paths. -ffile-prefix-map rewrites the source, build and generated
#    directories in __FILE__ (OpenSSL's allocation and error macros, asserts),
#    in debug information and in coverage data: the source directory becomes
#    ".", the build directory "build", the generated trees "gen" and
#    "gen-slices" (a debugger finds the sources again with, in gdb,
#    `directory <source dir> <build dir>`). The build-tree RUNPATH
#    (port/filezilla's link directory) is made $ORIGIN-relative.
#  * Time. SOURCE_DATE_EPOCH, from the environment or, for a release build
#    (LINSCP_RELEASE), the last commit's time (from git, or from a source
#    tarball's file dates), fixes:
#      - __DATE__, __TIME__ and __TIMESTAMP__ (core's build date), defined
#        from it on the command line, so the build's own environment does not
#        matter;
#      - every custom command (rcc's resource timestamps, OpenSSL's "built on"
#        line, CPack's file times), run with SOURCE_DATE_EPOCH set.
#    Without either, the build date stays the compiler's clock: in a
#    development build a date that followed every commit would rebuild the
#    whole tree at each reconfigure, for a string only the About box shows.
#  * PE/COFF. lld and GNU ld stamp the link time into a Windows executable's
#    header and debug directory; --no-insert-timestamp writes 0 instead.
#    (llvm-ar and GNU ar write deterministic archives by default; ELF has no
#    link time, its build ID is a hash of the contents.)
#
# Configure with -DLINSCP_REPRODUCIBLE=OFF to leave all of this out.

option(LINSCP_REPRODUCIBLE "Path-independent builds, dated by SOURCE_DATE_EPOCH" ON)
if(NOT LINSCP_REPRODUCIBLE)
  return()
endif()

# --- Paths -----------------------------------------------------------------
# Most general first: debug information takes the last matching map, and
# __FILE__ the longest matching prefix, so either way the build and generated
# directories win over a source directory that contains them.
set(_linscp_prefix_maps
  "${CMAKE_SOURCE_DIR}=."
  "${CMAKE_BINARY_DIR}=build"
  "${LINSCP_GEN}=gen"
  "${LINSCP_GEN}-slices=gen-slices")
foreach(_map IN LISTS _linscp_prefix_maps)
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-ffile-prefix-map=${_map}>")
endforeach()
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
# moc's output includes its header by the path from the autogen folder, which
# for a build folder outside the source tree climbs to / and down again
# ("../../../home/me/linscp/port/qt/X.h"): no prefix map rewrites that, and
# debug information keeps it. With a path prefix, moc includes the header by
# its path below an include directory instead.
set(CMAKE_AUTOMOC_PATH_PREFIX ON)

# --- PE/COFF link timestamp ------------------------------------------------
if(WIN32 AND CMAKE_CXX_COMPILER_LINKER_ID MATCHES "^(LLD|GNU)$")
  add_link_options("LINKER:--no-insert-timestamp")
endif()

# --- Time ------------------------------------------------------------------
set(LINSCP_SOURCE_DATE_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
if(LINSCP_SOURCE_DATE_EPOCH STREQUAL "" AND LINSCP_RELEASE)
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" log -1 --format=%ct
      OUTPUT_VARIABLE LINSCP_SOURCE_DATE_EPOCH
      RESULT_VARIABLE _git_rc
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _git_rc EQUAL 0)
      set(LINSCP_SOURCE_DATE_EPOCH "")
    endif()
  endif()
  # No repository: a source tarball (port/tools/make_source_tarball.py), whose
  # every file is dated by the commit.
  if(LINSCP_SOURCE_DATE_EPOCH STREQUAL "" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    file(TIMESTAMP "${CMAKE_SOURCE_DIR}/CMakeLists.txt" LINSCP_SOURCE_DATE_EPOCH "%s" UTC)
  endif()
endif()
if(LINSCP_SOURCE_DATE_EPOCH STREQUAL "")
  message(STATUS "linscp: reproducible paths; no SOURCE_DATE_EPOCH, the build date is the build's")
  return()
endif()
if(NOT LINSCP_SOURCE_DATE_EPOCH MATCHES "^[0-9]+$")
  message(FATAL_ERROR "SOURCE_DATE_EPOCH must be seconds since 1970, not '${LINSCP_SOURCE_DATE_EPOCH}'")
endif()

# string(TIMESTAMP) reads SOURCE_DATE_EPOCH from the environment; it stays set
# for the rest of the configure, so its own generators see it too. The
# compiler's formats, in UTC as clang uses for SOURCE_DATE_EPOCH: "Nov  4 2023",
# "22:13:20", "Sat Nov  4 22:13:20 2023" (the day padded with a space).
set(ENV{SOURCE_DATE_EPOCH} "${LINSCP_SOURCE_DATE_EPOCH}")
string(TIMESTAMP _month "%b" UTC)
string(TIMESTAMP _day "%d" UTC)
string(TIMESTAMP _year "%Y" UTC)
string(TIMESTAMP _weekday "%a" UTC)
string(TIMESTAMP _time "%H:%M:%S" UTC)
string(REGEX REPLACE "^0" " " _day "${_day}")
set(_date "${_month} ${_day} ${_year}")
set(_timestamp "${_weekday} ${_month} ${_day} ${_time} ${_year}")
add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-Wno-builtin-macro-redefined>")
add_compile_definitions(
  "$<$<COMPILE_LANGUAGE:C,CXX>:__DATE__=\"${_date}\">"
  "$<$<COMPILE_LANGUAGE:C,CXX>:__TIME__=\"${_time}\">"
  "$<$<COMPILE_LANGUAGE:C,CXX>:__TIMESTAMP__=\"${_timestamp}\">")
# Ninja and Make run every custom command (and custom target) through this.
set_property(GLOBAL PROPERTY RULE_LAUNCH_CUSTOM
  "${CMAKE_COMMAND} -E env SOURCE_DATE_EPOCH=${LINSCP_SOURCE_DATE_EPOCH}")
message(STATUS "linscp: reproducible build, SOURCE_DATE_EPOCH=${LINSCP_SOURCE_DATE_EPOCH} (${_date} ${_time} UTC)")
