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

add_executable(test_datetime test_datetime.cpp)
target_link_libraries(test_datetime PRIVATE linscp-shim)
add_test(NAME datetime COMMAND test_datetime)

add_executable(test_strings test_strings.cpp)
target_link_libraries(test_strings PRIVATE linscp-shim)
add_test(NAME strings COMMAND test_strings)

# Runs on both targets on purpose: on Windows it exercises the real Win32 API,
# which is what the POSIX implementation is being measured against.
add_executable(test_dynarray test_dynarray.cpp)
target_link_libraries(test_dynarray PRIVATE linscp-shim)
add_test(NAME dynarray COMMAND test_dynarray)

add_executable(test_storage test_storage.cpp)
target_link_libraries(test_storage PRIVATE linscp-shim)
add_test(NAME storage COMMAND test_storage)

add_executable(test_resources test_resources.cpp)
target_link_libraries(test_resources PRIVATE linscp-shim)
# Ids come from upstream's own header, so a renumbering there fails the test.
target_include_directories(test_resources PRIVATE "${LINSCP_UPSTREAM}/source/resource")
add_test(NAME resources COMMAND test_resources)

add_executable(test_containers test_containers.cpp)
target_link_libraries(test_containers PRIVATE linscp-shim)
add_test(NAME containers COMMAND test_containers)

add_executable(test_streams test_streams.cpp)
target_link_libraries(test_streams PRIVATE linscp-shim)
add_test(NAME streams COMMAND test_streams)

add_executable(test_win32_posix test_win32_posix.cpp)
target_link_libraries(test_win32_posix PRIVATE linscp-shim)
add_test(NAME win32_on_posix COMMAND test_win32_posix)

add_executable(test_strutils test_strutils.cpp)
target_link_libraries(test_strutils PRIVATE linscp-shim)
add_test(NAME strutils COMMAND test_strutils)

add_executable(test_closure test_closure.cpp)
target_link_libraries(test_closure PRIVATE linscp-shim)
add_test(NAME closure COMMAND test_closure)

add_executable(test_sysutils test_sysutils.cpp)
target_link_libraries(test_sysutils PRIVATE linscp-shim)
add_test(NAME sysutils COMMAND test_sysutils)

add_executable(test_object test_object.cpp)
target_link_libraries(test_object PRIVATE linscp-shim)
find_package(Threads REQUIRED)
target_link_libraries(test_object PRIVATE Threads::Threads)
add_test(NAME object_exceptions COMMAND test_object)

# When cross-compiling, the toolchain points CMAKE_CROSSCOMPILING_EMULATOR at
# wine; these tell wine where the mingw runtime DLLs live and keep its output
# off the test log.
if(LINSCP_WINE_DLLDIR_GCC)
  set(_wine_tests property_forms unicodestring bytestring datetime object_exceptions strings sysutils closure strutils win32_on_posix streams containers resources storage dynarray)
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
# Locks in the compile-census numbers so shim progress cannot slip back
# unnoticed: nothing in core compiles cleanly yet, so a reintroduced missing
# type would otherwise look like a passing build.
add_test(NAME census_ratchet
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/census_ratchet.py")
set_tests_properties(census_ratchet
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(NAME shim_no_wide_abi
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_wide_abi.py")
set_tests_properties(shim_no_wide_abi
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
