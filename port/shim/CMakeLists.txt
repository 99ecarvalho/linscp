# VCL runtime compatibility layer.
#
# std::wstring and <cwchar> are banned here: under -fshort-wchar, libstdc++'s
# char_traits<wchar_t> and libc's wide functions are compiled for a 4-byte
# wchar_t and would be an ABI mismatch. Enforced by the shim_no_wide_abi test.
# Resource strings, generated from upstream's own .rc tables so they cannot
# drift from the submodule. Declared here rather than at the top level because
# a custom command only creates a build dependency for targets in the same
# directory scope.
set(LINSCP_RESOURCES "${CMAKE_BINARY_DIR}/gen_resources.cpp")
add_custom_command(
  OUTPUT  "${LINSCP_RESOURCES}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/gen_resources.py"
          --src "${LINSCP_UPSTREAM}/source" --out "${LINSCP_RESOURCES}"
  DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/gen_resources.py"
          "${LINSCP_UPSTREAM}/source/resource/TextsCore.h"
          "${LINSCP_UPSTREAM}/source/resource/TextsCore1.rc"
          "${LINSCP_UPSTREAM}/source/resource/TextsCore2.rc"
          "${LINSCP_UPSTREAM}/source/resource/TextsFileZilla.rc"
          "${LINSCP_UPSTREAM}/source/resource/TextsWin.h"
          "${LINSCP_UPSTREAM}/source/resource/TextsWin1.rc"
  COMMENT "Generating resource string table from upstream .rc files"
  VERBATIM)

add_library(linscp-shim STATIC
  vcl_unicodestring.cpp
  vcl_bytestring.cpp
  vcl_datetime.cpp
  vcl_strings.cpp
  vcl_sysutils.cpp
  vcl_strutils.cpp
  vcl_format.cpp
  vcl_streams.cpp
  vcl_containers.cpp vcl_menus.cpp
  vcl_resources.cpp
  vcl_comobj.cpp
  vcl_inifiles.cpp
  vcl_registry.cpp
  vcl_misc.cpp
  vcl_exceptions.cpp
  vcl_xml.cpp
  vcl_thread.cpp
  vcl_variant.cpp
  vcl_json.cpp
  vcl_assert.cpp
  "${CMAKE_BINARY_DIR}/gen_resources.cpp"
)

# The Win32 subset core calls directly. Only built for non-Windows targets; on
# Windows the real <windows.h> and its import libraries serve core instead.
if(NOT WIN32)
  target_sources(linscp-shim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_on_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_kernel_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_file_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_misc_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_crypt_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_system_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_winsock_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_window_posix.cpp"
  )
endif()
if(NOT WIN32)
  target_include_directories(linscp-shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../platform")
  # wincrypt.h's certificate chains are verified with OpenSSL.
  target_link_libraries(linscp-shim PUBLIC linscp-openssl-crypto)
  add_dependencies(linscp-shim linscp-openssl-build)
endif()
target_include_directories(linscp-shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(linscp-shim PUBLIC linscp-expat)
find_package(Threads REQUIRED)
target_link_libraries(linscp-shim PUBLIC Threads::Threads)
if(WIN32)
  # version: GetFileVersion's VERSIONINFO reading.
  target_link_libraries(linscp-shim PUBLIC ole32 version)
endif()
target_compile_features(linscp-shim PUBLIC cxx_std_17)
# Core's configuration, publicly: the shim must see exactly what core sees, and
# so must everything built against it.
linscp_upstream_defines(linscp-shim PUBLIC ScpCore.cbproj)
target_compile_options(linscp-shim PUBLIC -fms-extensions -fshort-wchar)
# Every allocation starts zeroed (vcl_exceptions.cpp's global operator new),
# and upstream reads fields no constructor sets. The optimizer must not know
# what a new-expression returns, or it takes the memory as uninitialized:
# once a new-expression and such a read meet in one function (after
# inlining), it folds the read to undef and deletes the path as unreachable
# -- at -O2 and up only, so a Release build crashes where an unoptimized one
# works. LLVM gets that knowledge from its library-function table, which
# recognises _Znwm and friends on any call clang marks `builtin` (every
# new-expression). It uses it twice: a `noalias` result reads as undef until
# stored to, and -- since LLVM 23, noalias or not -- a new/delete pair whose
# object does not escape is removed along with the reads from it.
# -fno-assume-sane-operator-new only drops clang's own noalias; libstdc++ 16's
# <new> puts it back with __malloc__ on every form, and the removal never
# needed it. -fno-builtin turns the table off; nothing narrower reaches it
# (-fno-builtin-<name> silently ignores names that are not clang builtins,
# operator new among them). C++ only: the C sources never meet the shim's
# operator new. The cost is codegen, not meaning: plain memcpy, strlen and
# the like become ordinary calls (the standard library's __builtin_* ones
# are still inlined); a Release build grows by about 0.3%.
target_compile_options(linscp-shim PUBLIC
  $<$<COMPILE_LANGUAGE:CXX>:-fno-builtin>)
# Thrown objects start zeroed, as Delphi's instances do (vcl_exceptions.cpp).
target_link_options(linscp-shim INTERFACE "LINKER:--wrap=__cxa_allocate_exception")
