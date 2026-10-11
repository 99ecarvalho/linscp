# WinSCP's vendored PuTTY, compiled from the generated tree (the include rule
# and the patch series apply to it), as upstream's Putty.cbproj builds it: the
# same units, the same include path, the same defines. C, as upstream's is.
#
# Windows only for now: the units include PuTTY's Windows platform layer.
# Linux needs PuTTY's Unix one instead, which is P1-09.
if(NOT WIN32)
  return()
endif()

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/upstream_units.py"
          --project "${LINSCP_UPSTREAM}/source/Putty.cbproj" --separator ";"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE LINSCP_PUTTY_UNITS
  RESULT_VARIABLE _units_rc
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _units_rc EQUAL 0)
  message(FATAL_ERROR "upstream_units.py failed for Putty.cbproj")
endif()

set(LINSCP_PUTTY_SOURCES "")
foreach(_unit IN LISTS LINSCP_PUTTY_UNITS)
  list(APPEND LINSCP_PUTTY_SOURCES "${LINSCP_GEN}/${_unit}")
endforeach()
set_source_files_properties(${LINSCP_PUTTY_SOURCES} PROPERTIES GENERATED TRUE)

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
find_program(LINSCP_TARGET_OBJCOPY NAMES "${LINSCP_TARGET_TRIPLE}-objcopy" REQUIRED)
find_program(LINSCP_TARGET_NM NAMES "${LINSCP_TARGET_TRIPLE}-nm" REQUIRED)
set(LINSCP_PUTTY_OVERRIDES "${CMAKE_CURRENT_SOURCE_DIR}/overrides.txt")
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${LINSCP_PUTTY_OVERRIDES}")
add_custom_command(TARGET linscp-putty POST_BUILD
  COMMAND "${LINSCP_TARGET_OBJCOPY}" "--weaken-symbols=${LINSCP_PUTTY_OVERRIDES}"
          "$<TARGET_FILE:linscp-putty>"
  COMMENT "Weakening PuTTY symbols core overrides"
  VERBATIM)
