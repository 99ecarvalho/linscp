# WinSCP's MFC subset and FileZilla, compiled from the generated tree.
#
# FileZilla's sockets are Win32 asynchronous sockets driven through window
# messages; on POSIX the platform layer emulates those (P1-12).
#
# Both are compiled with vcl_win32_prelude.h forced first, so MFC's classes
# are declared without <windows.h>'s A/W name macros, as core and the shim
# see Win32. See that header for why.
set(_g "${LINSCP_GEN}")
set(_prelude "${CMAKE_SOURCE_DIR}/port/shim/vcl_win32_prelude.h")
set(_shim_includes "$<TARGET_PROPERTY:linscp-shim,INTERFACE_INCLUDE_DIRECTORIES>")

# --- WinSCP's MFC subset -----------------------------------------------------
# No project file upstream, only libs/mfc/source/borland.mak: its objects and
# DEFS are read from it.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${LINSCP_UPSTREAM}/libs/mfc/source/borland.mak" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_MFC_SOURCES
  COMMAND_ERROR_IS_FATAL ANY
  OUTPUT_STRIP_TRAILING_WHITESPACE)
list(TRANSFORM LINSCP_MFC_SOURCES PREPEND "${_g}/")
set_source_files_properties(${LINSCP_MFC_SOURCES} PROPERTIES GENERATED TRUE)
add_library(linscp-mfc STATIC EXCLUDE_FROM_ALL ${LINSCP_MFC_SOURCES})
add_dependencies(linscp-mfc transform)
linscp_upstream_defines(linscp-mfc PRIVATE ../libs/mfc/source/borland.mak)
target_include_directories(linscp-mfc PRIVATE
  "${_g}/libs/mfc/include" "${_g}/libs/mfc/source" ${_shim_includes})
target_compile_options(linscp-mfc PRIVATE -include "${_prelude}" -w)
# FileZillaIntf.cpp asks for this library by name -- #pragma comment(lib,
# "uafxcw.lib") -- meaning WinSCP's own subset, which borland.mak builds under
# that name. Named the same here, and on the link path, the request resolves
# to exactly the library upstream means (lld looks up libuafxcw.a).
set_target_properties(linscp-mfc PROPERTIES OUTPUT_NAME uafxcw)
target_link_directories(linscp-mfc INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

# --- FileZilla -----------------------------------------------------------------
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${LINSCP_UPSTREAM}/source/FileZilla.cbproj" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_FILEZILLA_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for FileZilla.cbproj")
endif()
set(LINSCP_FILEZILLA_SOURCES "")
foreach(_unit IN LISTS LINSCP_FILEZILLA_UNITS)
  list(APPEND LINSCP_FILEZILLA_SOURCES "${_g}/${_unit}")
endforeach()
set_source_files_properties(${LINSCP_FILEZILLA_SOURCES} PROPERTIES GENERATED TRUE)
add_library(linscp-filezilla STATIC EXCLUDE_FROM_ALL ${LINSCP_FILEZILLA_SOURCES})
add_dependencies(linscp-filezilla transform)
# FileZilla.cbproj's own configuration; the shim's include paths but not its
# defines, which are core's (OPENSSL_NO_DEPRECATED_1_1_0 would hide OpenSSL
# API FileZilla's SSL layer is entitled to).
linscp_upstream_defines(linscp-filezilla PRIVATE FileZilla.cbproj)
# FileZilla.cbproj's IncludePath, in its order, then what core's headers need.
target_include_directories(linscp-filezilla PRIVATE
  "${_g}/source/filezilla/misc" "${_g}/source/filezilla" "${LINSCP_UPSTREAM}/source/resource"
  "${_g}/source/core" "${_g}/libs/mfc/include"
  "${_g}/source/putty" "${_g}/source/putty/windows" "${LINSCP_GEN}/libs/neon/src"
  "${LINSCP_UPSTREAM}/libs/expat/lib"
  ${_shim_includes})
target_compile_options(linscp-filezilla PRIVATE -include "${_prelude}" -w)
# OpenSSL from the configured release, as for core.
target_link_libraries(linscp-filezilla PUBLIC linscp-mfc linscp-openssl)
