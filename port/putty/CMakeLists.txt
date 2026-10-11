# WinSCP's vendored PuTTY, compiled from the generated tree (the include rule
# and the patch series apply to it), as upstream's Putty.cbproj builds it: the
# same units, the same include path, the same defines. C, as upstream's is.
#
# On Linux too, with its Windows platform layer, over port/platform; the few
# units that are Windows services rather than platform code are replaced or
# dropped by port/putty/linux/units.txt (see the decisions table).
set(_putty_rules)
if(NOT WIN32)
  set(_putty_rules --rules "${CMAKE_CURRENT_SOURCE_DIR}/linux/units.txt")
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
               "${CMAKE_CURRENT_SOURCE_DIR}/linux/units.txt")
endif()
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${LINSCP_UPSTREAM}/source/Putty.cbproj" --separator ";" ${_putty_rules}
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_PUTTY_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for Putty.cbproj")
endif()

set(LINSCP_PUTTY_SOURCES "")
foreach(_unit IN LISTS LINSCP_PUTTY_UNITS)
  if(IS_ABSOLUTE "${_unit}")
    list(APPEND LINSCP_PUTTY_SOURCES "${_unit}")    # a Linux replacement
  else()
    list(APPEND LINSCP_PUTTY_SOURCES "${LINSCP_GEN}/${_unit}")
    set_source_files_properties("${LINSCP_GEN}/${_unit}" PROPERTIES GENERATED TRUE)
  endif()
endforeach()

add_library(linscp-putty STATIC EXCLUDE_FROM_ALL ${LINSCP_PUTTY_SOURCES})
add_dependencies(linscp-putty transform)
linscp_upstream_defines(linscp-putty PRIVATE Putty.cbproj)

# Putty.cbproj's IncludePath, in its order.
set(_p "${LINSCP_GEN}/source/putty")
target_include_directories(linscp-putty PRIVATE
  "${_p}/windows/utils" "${_p}/utils" "${_p}/stubs" "${_p}/ssh"
  "${_p}/proxy" "${_p}/crypto" "${_p}" "${_p}/windows")
set_target_properties(linscp-putty PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
# Upstream's code is compiled as it is; its warnings are not ours to fix.
target_compile_options(linscp-putty PRIVATE -w)
if(NOT WIN32)
  target_include_directories(linscp-putty PRIVATE "${CMAKE_SOURCE_DIR}/port/platform")
  # These diagnostics are errors on Linux, where they mean a mismatch with the
  # POSIX layer -- a narrow string passed to a wide function, a 64-bit long
  # where Win32 has 32 bits -- that would otherwise compile and misbehave.
  # dupprintf.c names vsnprintf _vsnprintf under _WINDOWS, and handles C99's
  # return convention explicitly.
  target_compile_options(linscp-putty PRIVATE
    -Werror=incompatible-pointer-types -Werror=int-conversion
    -Werror=implicit-function-declaration -Werror=incompatible-function-pointer-types)
  target_compile_definitions(linscp-putty PRIVATE _vsnprintf=vsnprintf)
  # The runtime's wide functions under 2-byte wchar_t, in every unit: some of
  # PuTTY's utilities (dupwcs.c, burnwcs.c) include no platform header, and
  # reached glibc's 4-byte functions.
  target_compile_options(linscp-putty PRIVATE
    -include "${CMAKE_SOURCE_DIR}/port/platform/win32_on_posix_wide.h")
endif()

# PuTTY's performance-critical crypto -- the AES and SHA-256 loops, AES-NI and
# Argon2 -- which WinSCP compiles a second time with MSVC (libs/puttyvs),
# defining WINSCP_VS, because C++Builder cannot build or optimise them well.
# The same files with the same defines, from upstream's own PuTTYVS.vcxproj;
# clang needs no second compiler.
set(_vcxproj "${LINSCP_UPSTREAM}/libs/puttyvs/PuTTYVS.vcxproj")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${_vcxproj}" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_PUTTYVS_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for PuTTYVS.vcxproj")
endif()
set(LINSCP_PUTTYVS_SOURCES "")
foreach(_unit IN LISTS LINSCP_PUTTYVS_UNITS)
  list(APPEND LINSCP_PUTTYVS_SOURCES "${LINSCP_GEN}/${_unit}")
endforeach()
set_source_files_properties(${LINSCP_PUTTYVS_SOURCES} PROPERTIES GENERATED TRUE)
add_library(linscp-puttyvs STATIC EXCLUDE_FROM_ALL ${LINSCP_PUTTYVS_SOURCES})
add_dependencies(linscp-puttyvs transform)
linscp_upstream_defines(linscp-puttyvs PRIVATE ../libs/puttyvs/PuTTYVS.vcxproj)
target_include_directories(linscp-puttyvs PRIVATE "${_p}")
set_target_properties(linscp-puttyvs PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_compile_options(linscp-puttyvs PRIVATE -w)
if(NOT WIN32)
  target_compile_options(linscp-puttyvs PRIVATE
    -include "${CMAKE_SOURCE_DIR}/port/platform/win32_on_posix_wide.h")
endif()
# aes-ni.c uses SSE4.1 and AES-NI intrinsics without the per-function target
# attributes vanilla PuTTY carries -- WinSCP dropped them, as MSVC does not
# need them. The file is compiled with the ISA enabled instead, as MSVC
# effectively treats it. That is only safe if the CPU check that guards the
# whole file stays free of those instructions; check_isa_free.py verifies it.
set_source_files_properties("${LINSCP_GEN}/source/putty/crypto/aes-ni.c"
  PROPERTIES COMPILE_OPTIONS "-msse4.1;-maes")

# core/PuttyIntf.cpp overrides a few PuTTY definitions, relying on
# C++Builder's linker letting the first definition win; mark PuTTY's copies
# weak in the archive so a standard linker reaches the same result. GNU objcopy
# for the target: llvm-objcopy cannot weaken COFF symbols.
if(WIN32)
  find_program(LINSCP_TARGET_OBJCOPY NAMES "${LINSCP_TARGET_TRIPLE}-objcopy" REQUIRED)
  find_program(LINSCP_TARGET_NM NAMES "${LINSCP_TARGET_TRIPLE}-nm" REQUIRED)
else()
  find_program(LINSCP_TARGET_OBJCOPY NAMES llvm-objcopy objcopy REQUIRED)
  find_program(LINSCP_TARGET_NM NAMES llvm-nm nm REQUIRED)
endif()
set(LINSCP_PUTTY_OVERRIDES "${CMAKE_CURRENT_SOURCE_DIR}/overrides.txt")
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${LINSCP_PUTTY_OVERRIDES}")
add_custom_command(TARGET linscp-putty POST_BUILD
  COMMAND "${LINSCP_TARGET_OBJCOPY}" "--weaken-symbols=${LINSCP_PUTTY_OVERRIDES}"
          "$<TARGET_FILE:linscp-putty>"
  COMMENT "Weakening PuTTY symbols core overrides"
  VERBATIM)
