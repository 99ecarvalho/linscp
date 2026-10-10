# Cross-compile to Windows x86-64 from Linux using clang with the mingw-w64
# sysroot. GCC's mingw packages supply the sysroot and libstdc++; clang does the
# compiling, because only clang implements __declspec(property).
#
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-w64.cmake
#
# Install prerequisites with:
#   sudo apt-get install --no-install-recommends \
#       mingw-w64-x86-64-dev binutils-mingw-w64-x86-64 g++-mingw-w64-x86-64-posix

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LINSCP_TARGET_TRIPLE x86_64-w64-mingw32)
set(LINSCP_MINGW_SYSROOT /usr/${LINSCP_TARGET_TRIPLE})

set(CMAKE_C_COMPILER   clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET   ${LINSCP_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${LINSCP_TARGET_TRIPLE})
set(CMAKE_RC_COMPILER ${LINSCP_TARGET_TRIPLE}-windres)

# lld, not GNU ld: GNU ld fails to resolve libwinpthread when static-linking
# the mingw libstdc++.
add_link_options(-fuse-ld=lld -L${LINSCP_MINGW_SYSROOT}/lib)

set(CMAKE_FIND_ROOT_PATH ${LINSCP_MINGW_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run Windows test binaries through Wine. CMAKE_COMMAND is not reliably defined
# while a toolchain file is being processed, so the emulator is just wine and the
# DLL search path is handed to tests as an environment property instead.
# libwinpthread-1.dll lives in the sysroot while libstdc++/libgcc live in the
# GCC directory, so both are needed.
find_program(LINSCP_WINE wine)
if(LINSCP_WINE)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${LINSCP_WINE}")
  file(GLOB _mingw_gcc_dirs /usr/lib/gcc/${LINSCP_TARGET_TRIPLE}/*-posix)
  if(_mingw_gcc_dirs)
    list(GET _mingw_gcc_dirs 0 _mingw_gcc_dir)
    # Kept as two variables rather than one WINEPATH string: Windows separates
    # path entries with ';', which CMake would treat as a list separator and
    # silently shred the environment.
    set(LINSCP_WINE_DLLDIR_GCC "${_mingw_gcc_dir}"
        CACHE PATH "mingw libstdc++/libgcc DLL directory")
    set(LINSCP_WINE_DLLDIR_SYSROOT "${LINSCP_MINGW_SYSROOT}/lib"
        CACHE PATH "mingw sysroot DLL directory (libwinpthread)")
  endif()
endif()
