# WinSCP's MFC subset and FileZilla, compiled from the generated tree.
#
# Windows only for now: FileZilla's sockets are Win32 asynchronous sockets
# driven through window messages. Linux needs them replaced (P1-12).
#
# Both are compiled with vcl_win32_prelude.h forced first, so MFC's classes
# are declared without <windows.h>'s A/W name macros, as core and the shim
# see Win32. See that header for why.
if(NOT WIN32)
  return()
endif()

set(_g "${LINSCP_GEN}")
set(_prelude "${CMAKE_SOURCE_DIR}/port/shim/vcl_win32_prelude.h")
set(_shim_includes "$<TARGET_PROPERTY:linscp-shim,INTERFACE_INCLUDE_DIRECTORIES>")

# --- WinSCP's MFC subset -----------------------------------------------------
# No project file upstream, only libs/mfc/source/borland.mak, so its sources
# and defines are mirrored here: the OBJECT, FILES, MISC and WINMISC lists, and
# TARGDEFS for the UNICODE build. _AFX_NO_DAO_SUPPORT is added because DAO is
# 32-bit only; Microsoft's own MFC turns it off for every non-x86 target, in
# the afxv_cpu.h WinSCP does not vendor.
set(_mfc_units objcore except validadd filecore filex filest strcore strex
               timecore fixalloc plex winstr apphelp)
set(LINSCP_MFC_SOURCES "")
foreach(_u IN LISTS _mfc_units)
  list(APPEND LINSCP_MFC_SOURCES "${_g}/libs/mfc/source/${_u}.cpp")
endforeach()
set_source_files_properties(${LINSCP_MFC_SOURCES} PROPERTIES GENERATED TRUE)
add_library(linscp-mfc STATIC EXCLUDE_FROM_ALL ${LINSCP_MFC_SOURCES})
add_dependencies(linscp-mfc transform)
target_compile_definitions(linscp-mfc PRIVATE
  UNICODE _UNICODE WINSCP _WINDOWS
  _AFX_NO_OLEDB_SUPPORT _AFX_NO_OCC_SUPPORT _AFX_NO_OCX_SUPPORT _AFX_NO_DB_SUPPORT
  _AFX_NO_AFXCMN_SUPPORT _AFX_NO_RICHEDIT_SUPPORT _AFX_NO_INET_SUPPORT
  _AFX_NO_DHTML_SUPPORT _AFX_NO_OLE_SUPPORT _AFX_NO_SOCKET_SUPPORT _AFX_NO_SYNC_SUPPORT
  _AFX_NO_DAO_SUPPORT
  "$<$<CONFIG:Debug>:_DEBUG>")
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
  "${_g}/source/core" "${LINSCP_UPSTREAM}/libs/openssl/include" "${_g}/libs/mfc/include"
  "${_g}/source/putty" "${_g}/source/putty/windows" "${LINSCP_UPSTREAM}/libs/neon/src"
  "${LINSCP_UPSTREAM}/libs/expat/lib"
  ${_shim_includes})
target_compile_options(linscp-filezilla PRIVATE -include "${_prelude}" -w)
target_link_libraries(linscp-filezilla PUBLIC linscp-mfc)
