# Build OpenSSL from the pinned release, configured by OpenSSL's own Configure.
#
# WinSCP's copy (libs/openssl) is trimmed and pre-configured for 32-bit
# Windows: x86 assembly with its C fallbacks removed, no Unix sources. The
# port builds the release of the same version, plus the WinSCP changes
# port/openssl/winscp-changes.txt adopts; port/tools/openssl_tree.py
# assembles that tree and fails if the list misses a difference.
set(LINSCP_OPENSSL_RELEASE "${CMAKE_SOURCE_DIR}/3rd-party/openssl")
if(NOT EXISTS "${LINSCP_OPENSSL_RELEASE}/Configure")
  message(FATAL_ERROR "OpenSSL release missing at ${LINSCP_OPENSSL_RELEASE}; "
                      "run: git submodule update --init 3rd-party/openssl")
endif()
find_program(LINSCP_PERL perl REQUIRED)
find_program(LINSCP_MAKE NAMES gmake make REQUIRED)

set(_ossl "${CMAKE_BINARY_DIR}/openssl")
set(_ossl_src "${_ossl}/src")
set(_ossl_bld "${_ossl}/build")

# The disabled features mirror the OPENSSL_NO_* set in WinSCP's makefile
# (libs/openssl/Makefile). no-module keeps the providers inside libcrypto,
# as WinSCP's static build has them.
set(LINSCP_OPENSSL_OPTIONS
  no-shared no-module no-tests no-apps no-docs
  no-camellia no-rc5 no-mdc2 no-whirlpool no-seed no-engine no-zlib
  no-quic no-brotli no-comp no-tfo no-unstable-qlog)
if(WIN32)
  set(_ossl_target mingw64)
  set(_ossl_cc "${CMAKE_C_COMPILER} --target=${LINSCP_TARGET_TRIPLE}")
  set(_ossl_tools "RC=${CMAKE_RC_COMPILER}")
else()
  set(_ossl_target linux-x86_64-clang)
  set(_ossl_cc "${CMAKE_C_COMPILER}")
  set(_ossl_tools)
endif()

add_custom_command(
  OUTPUT  "${_ossl}/tree.stamp"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/openssl_tree.py"
          --out "${_ossl_src}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_ossl}/tree.stamp"
  DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/openssl_tree.py"
          "${CMAKE_SOURCE_DIR}/port/openssl/winscp-changes.txt"
          "${LINSCP_OPENSSL_RELEASE}/VERSION.dat"
  COMMENT "Assembling the OpenSSL tree: release plus adopted WinSCP changes"
  VERBATIM)

add_custom_command(
  OUTPUT  "${_ossl_bld}/configdata.pm"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ossl_bld}"
  COMMAND "${CMAKE_COMMAND}" -E chdir "${_ossl_bld}"
          "${LINSCP_PERL}" "${_ossl_src}/Configure" ${_ossl_target}
          ${LINSCP_OPENSSL_OPTIONS} "CC=${_ossl_cc}"
          "AR=${CMAKE_C_COMPILER_AR}" "RANLIB=${CMAKE_C_COMPILER_RANLIB}"
          ${_ossl_tools}
  DEPENDS "${_ossl}/tree.stamp" "${CMAKE_CURRENT_LIST_FILE}"
  COMMENT "Configuring OpenSSL (${_ossl_target})"
  VERBATIM)

include(ProcessorCount)
ProcessorCount(_ossl_jobs)
if(_ossl_jobs EQUAL 0)
  set(_ossl_jobs 1)
endif()
add_custom_command(
  OUTPUT  "${_ossl_bld}/libcrypto.a" "${_ossl_bld}/libssl.a"
  COMMAND "${LINSCP_MAKE}" -C "${_ossl_bld}" -j${_ossl_jobs} build_libs
  DEPENDS "${_ossl_bld}/configdata.pm" "${_ossl}/tree.stamp"
  COMMENT "Building OpenSSL"
  VERBATIM)
add_custom_target(linscp-openssl-build
  DEPENDS "${_ossl_bld}/libcrypto.a" "${_ossl_bld}/libssl.a")

# Generated headers (configuration.h, opensslv.h, ...) are in the build
# directory; the rest are in the source tree.
file(MAKE_DIRECTORY "${_ossl_bld}/include" "${_ossl_src}/include")
add_library(linscp-openssl-crypto STATIC IMPORTED GLOBAL)
set_target_properties(linscp-openssl-crypto PROPERTIES
  IMPORTED_LOCATION "${_ossl_bld}/libcrypto.a"
  INTERFACE_INCLUDE_DIRECTORIES "${_ossl_bld}/include;${_ossl_src}/include")
add_library(linscp-openssl-ssl STATIC IMPORTED GLOBAL)
set_target_properties(linscp-openssl-ssl PROPERTIES
  IMPORTED_LOCATION "${_ossl_bld}/libssl.a"
  INTERFACE_LINK_LIBRARIES linscp-openssl-crypto)
add_dependencies(linscp-openssl-crypto linscp-openssl-build)
add_dependencies(linscp-openssl-ssl linscp-openssl-build)
if(WIN32)
  set_property(TARGET linscp-openssl-crypto APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES ws2_32 crypt32 advapi32 user32)
else()
  find_package(Threads REQUIRED)
  set_property(TARGET linscp-openssl-crypto APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS})
endif()

# A target that consumes the headers must wait for Configure to generate them.
add_library(linscp-openssl INTERFACE)
target_link_libraries(linscp-openssl INTERFACE linscp-openssl-ssl)
add_dependencies(linscp-openssl linscp-openssl-build)
