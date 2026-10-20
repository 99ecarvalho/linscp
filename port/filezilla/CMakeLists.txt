# FileZilla, compiled from the generated tree, and the MFC-compatible layer
# it is written against.
#
# FileZilla's sockets are Win32 asynchronous sockets driven through window
# messages; on POSIX the platform layer emulates those (P1-12).
#
# Both are compiled with vcl_win32_prelude.h forced first, so the MFC classes
# are declared without <windows.h>'s A/W name macros, as core and the shim
# see Win32. See that header for why.
set(_g "${LINSCP_GEN}")
set(_prelude "${CMAKE_SOURCE_DIR}/port/shim/vcl_win32_prelude.h")
set(_shim_includes "$<TARGET_PROPERTY:linscp-shim,INTERFACE_INCLUDE_DIRECTORIES>")

# --- The MFC-compatible layer --------------------------------------------------
# FileZilla is written against a small part of MFC (CString, CTime, CFile),
# which it reaches through <afx.h>. linscp provides that part as original code
# in port/mfc-compat (ADR 0017); nothing of upstream's libs/mfc is built.
set(_mfc "${CMAKE_SOURCE_DIR}/port/mfc-compat")
add_library(linscp-mfc-compat STATIC EXCLUDE_FROM_ALL
  "${_mfc}/mfc_string.cpp" "${_mfc}/mfc_time.cpp" "${_mfc}/mfc_file.cpp")
target_include_directories(linscp-mfc-compat PUBLIC "${_mfc}/include")
# Built against the shim as core is (its defines and options), but privately:
# FileZilla takes the shim's include paths, not its defines (see below).
target_link_libraries(linscp-mfc-compat PRIVATE linscp-shim)
target_compile_options(linscp-mfc-compat PRIVATE -include "${_prelude}")
# FileZillaIntf.cpp asks for this library by name -- #pragma comment(lib,
# "uafxcw.lib") -- so it keeps that name, on the link path (lld looks up
# libuafxcw.a), and the request resolves to this layer.
set_target_properties(linscp-mfc-compat PROPERTIES OUTPUT_NAME uafxcw)
target_link_directories(linscp-mfc-compat INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

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
  "${_g}/source/core" "${_mfc}/include"
  "${_g}/source/putty" "${_g}/source/putty/windows" "${LINSCP_GEN}/libs/neon/src"
  "${LINSCP_UPSTREAM}/libs/expat/lib"
  ${_shim_includes})
target_compile_options(linscp-filezilla PRIVATE -include "${_prelude}" -w)
# OpenSSL from the configured release, as for core.
target_link_libraries(linscp-filezilla PUBLIC linscp-mfc-compat linscp-openssl)
