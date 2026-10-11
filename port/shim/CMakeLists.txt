# VCL runtime compatibility layer.
#
# std::wstring and <cwchar> are banned here: under -fshort-wchar, libstdc++'s
# char_traits<wchar_t> and libc's wide functions are compiled for a 4-byte
# wchar_t and would be an ABI mismatch. Enforced by the shim_no_wide_abi test.
add_library(linscp-shim STATIC
  vcl_unicodestring.cpp
  vcl_bytestring.cpp
  vcl_datetime.cpp
  vcl_strings.cpp
  vcl_sysutils.cpp
  vcl_strutils.cpp
  vcl_format.cpp
)

# The Win32 subset core calls directly. Only built for non-Windows targets; on
# Windows the real <windows.h> and its import libraries serve core instead.
if(NOT WIN32)
  target_sources(linscp-shim PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../platform/win32_on_posix.cpp"
  )
endif()
if(NOT WIN32)
  target_include_directories(linscp-shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../platform")
endif()
target_include_directories(linscp-shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Gaps in upstream itself, as opposed to platform emulation: needed on every
# target, so unlike ../platform this is not guarded by NOT WIN32.
target_include_directories(linscp-shim PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../compat")
target_compile_features(linscp-shim PUBLIC cxx_std_17)
target_compile_options(linscp-shim PUBLIC -fms-extensions -fshort-wchar)
