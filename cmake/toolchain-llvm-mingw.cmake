# Cross-compile to Windows x86-64 from Linux with llvm-mingw: clang, lld,
# libc++ and the UCRT, one LLVM toolchain with no GCC parts. It is the ABI of
# MSYS2's clang64 packages, Qt among them (P2-01).
#
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
#
# The toolchain is a release of github.com/mstorsjo/llvm-mingw, unpacked by
# port/tools/fetch_llvm_mingw.py; LINSCP_LLVM_MINGW names its directory.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LINSCP_TARGET_TRIPLE x86_64-w64-mingw32)

if(NOT LINSCP_LLVM_MINGW)
  set(LINSCP_LLVM_MINGW "$ENV{LINSCP_LLVM_MINGW}")
endif()
if(NOT LINSCP_LLVM_MINGW)
  file(GLOB _llvm_mingw LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/../build/tools/llvm-mingw-*-ucrt-*")
  list(FILTER _llvm_mingw EXCLUDE REGEX "\\.tar\\.")
  list(SORT _llvm_mingw)
  list(POP_BACK _llvm_mingw LINSCP_LLVM_MINGW)
endif()
if(NOT EXISTS "${LINSCP_LLVM_MINGW}/bin/${LINSCP_TARGET_TRIPLE}-clang")
  message(FATAL_ERROR "No llvm-mingw at '${LINSCP_LLVM_MINGW}': run port/tools/fetch_llvm_mingw.py "
                      "or set LINSCP_LLVM_MINGW")
endif()
set(LINSCP_LLVM_MINGW "${LINSCP_LLVM_MINGW}" CACHE PATH "llvm-mingw toolchain directory")
# try_compile projects see only the variables named here.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LINSCP_LLVM_MINGW)

set(_bin "${LINSCP_LLVM_MINGW}/bin")
set(LINSCP_MINGW_SYSROOT "${LINSCP_LLVM_MINGW}/${LINSCP_TARGET_TRIPLE}")

set(CMAKE_C_COMPILER   "${_bin}/${LINSCP_TARGET_TRIPLE}-clang")
set(CMAKE_CXX_COMPILER "${_bin}/${LINSCP_TARGET_TRIPLE}-clang++")
set(CMAKE_RC_COMPILER  "${_bin}/${LINSCP_TARGET_TRIPLE}-windres")
set(CMAKE_AR           "${_bin}/llvm-ar")
set(CMAKE_RANLIB       "${_bin}/llvm-ranlib")
# objcopy stays binutils' (x86_64-w64-mingw32-objcopy, found on PATH):
# port/putty weakens COFF symbols, which llvm-objcopy cannot.
set(LINSCP_TARGET_NM      "${_bin}/llvm-nm" CACHE FILEPATH "")
set(LINSCP_TARGET_OBJDUMP "${_bin}/llvm-objdump" CACHE FILEPATH "")

# The Windows Qt (port/tools/fetch_msys2_qt.py) is searched as a second root.
if(LINSCP_QT_PREFIX)
  list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LINSCP_QT_PREFIX)
endif()
set(CMAKE_FIND_ROOT_PATH "${LINSCP_MINGW_SYSROOT}" ${LINSCP_QT_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Tests run under Wine, which finds libc++, libunwind and winpthreads in the
# toolchain's DLL directory (see the mingw-w64 toolchain file for why there
# are two variables).
find_program(LINSCP_WINE wine)
if(LINSCP_WINE)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${LINSCP_WINE}")
  set(LINSCP_WINE_DLLDIR_GCC "${LINSCP_MINGW_SYSROOT}/bin" CACHE PATH "llvm-mingw runtime DLL directory")
  set(LINSCP_WINE_DLLDIR_SYSROOT "${LINSCP_MINGW_SYSROOT}/bin" CACHE PATH "llvm-mingw runtime DLL directory")
endif()
