# Tests run on the host and, when cross-compiling, through Wine via
# CMAKE_CROSSCOMPILING_EMULATOR set by the toolchain file.
add_executable(test_property test_property.cpp)
target_link_libraries(test_property PRIVATE linscp-shim)
add_test(NAME property_forms COMMAND test_property)

add_executable(test_unicodestring test_unicodestring.cpp)
target_link_libraries(test_unicodestring PRIVATE linscp-shim)
add_test(NAME unicodestring COMMAND test_unicodestring)

add_executable(test_bytestring test_bytestring.cpp)
target_link_libraries(test_bytestring PRIVATE linscp-shim)
add_test(NAME bytestring COMMAND test_bytestring)

# When cross-compiling, the toolchain points CMAKE_CROSSCOMPILING_EMULATOR at
# wine; these tell wine where the mingw runtime DLLs live and keep its output
# off the test log.
if(LINSCP_WINE_DLLDIR_GCC)
  set(_wine_tests property_forms unicodestring bytestring)
  # The backslash-escaped ';' is a literal Windows path separator inside the
  # WINEPATH value; the unescaped ';' separates the two environment entries.
  set_tests_properties(${_wine_tests} PROPERTIES ENVIRONMENT
    "WINEPATH=${LINSCP_WINE_DLLDIR_GCC}\;${LINSCP_WINE_DLLDIR_SYSROOT};WINEDEBUG=-all")
endif()

# The transform and its invariants are verified by the same scripts CI runs.
add_test(NAME transform_verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/verify_transform.py"
          "${LINSCP_GEN}" "${LINSCP_UPSTREAM}")
add_test(NAME transform_properties_compile
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_properties.py"
          "${LINSCP_GEN}/source/core" "${CMAKE_CURRENT_BINARY_DIR}/propcheck.cpp")
add_test(NAME docs_consistent
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_docs.py")
set_tests_properties(transform_verify transform_properties_compile docs_consistent
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ADR 0001 enforcement: the wide-character ABI must stay uniform.
add_test(NAME shim_no_wide_abi
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_wide_abi.py")
set_tests_properties(shim_no_wide_abi
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
