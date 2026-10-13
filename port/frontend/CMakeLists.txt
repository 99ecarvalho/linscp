# The frontend half of core's Interface.h every program shares
# (FrontendCommon.cpp), process startup, and upstream's TProgramParams
# (windows/ProgParams.h; its .cpp is carried in HeadlessMain.cpp, since
# upstream's now needs the VCL forms).
add_library(linscp-frontend-common STATIC EXCLUDE_FROM_ALL
  FrontendCommon.cpp
  HeadlessMain.cpp)
target_link_libraries(linscp-frontend-common PUBLIC linscp-core)
target_include_directories(linscp-frontend-common PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}")
# The rest of it for a process with no GUI: errors to stderr, no event loop,
# no clipboard. The Qt shell has its own (port/qt/QtInterface.cpp).
add_library(linscp-frontend-headless STATIC EXCLUDE_FROM_ALL HeadlessInterface.cpp)
target_link_libraries(linscp-frontend-headless PUBLIC linscp-frontend-common)

# Everything a program built on core links but its own Interface.h half:
# core, the shared frontend, and the libraries core calls into. lld resolves
# static archives regardless of order, so the PuTTY <-> core cycle needs no
# repetition.
add_library(linscp-runtime-base INTERFACE)
target_link_libraries(linscp-runtime-base INTERFACE linscp-frontend-common linscp-core)
# Core's objects themselves, in linscp-core's source order, so their static
# initialisers run in upstream's project order (see port/core). Linked first
# and whole, the archive above is left with nothing to contribute.
target_sources(linscp-runtime-base INTERFACE $<TARGET_OBJECTS:linscp-core>)
foreach(_lib linscp-putty linscp-puttyvs linscp-filezilla linscp-libs3 linscp-neon)
  if(TARGET ${_lib})
    target_link_libraries(linscp-runtime-base INTERFACE ${_lib})
  endif()
endforeach()
# A headless program: the command-line client and the tests.
add_library(linscp-runtime INTERFACE)
target_link_libraries(linscp-runtime INTERFACE linscp-runtime-base linscp-frontend-headless)

# Core reads its own version from the executable's VERSIONINFO resource, which
# C++Builder writes from WinSCP.cbproj's VerInfo_* settings. Generated from
# the same settings, so it follows the pin, and linked into every executable
# built on core. POSIX has no resources: there the same data is generated as
# C++, which the platform layer serves GetFileVersionInfo from.
if(WIN32)
  set(_verrc "${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc")
  add_custom_command(
    OUTPUT  "${_verrc}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/gen_versioninfo.py"
            --project "${LINSCP_UPSTREAM}/source/WinSCP.cbproj" --out "${_verrc}"
    DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/gen_versioninfo.py"
            "${LINSCP_UPSTREAM}/source/WinSCP.cbproj"
    COMMENT "Generating VERSIONINFO from WinSCP.cbproj"
    VERBATIM)
  add_library(linscp-versioninfo OBJECT EXCLUDE_FROM_ALL "${_verrc}")
  # An object library's objects reach only targets linking it directly, so
  # they are handed on as sources instead.
  target_sources(linscp-runtime-base INTERFACE $<TARGET_OBJECTS:linscp-versioninfo>)
  add_dependencies(linscp-runtime-base linscp-versioninfo)
endif()
if(NOT WIN32)
  set(_vercpp "${CMAKE_CURRENT_BINARY_DIR}/versioninfo.cpp")
  add_custom_command(
    OUTPUT  "${_vercpp}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/gen_versioninfo.py"
            --project "${LINSCP_UPSTREAM}/source/WinSCP.cbproj" --out "${_vercpp}" --cxx
            --platform "${CMAKE_SYSTEM_NAME}"
    DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/gen_versioninfo.py"
            "${LINSCP_UPSTREAM}/source/WinSCP.cbproj"
    COMMENT "Generating version information from WinSCP.cbproj"
    VERBATIM)
  add_library(linscp-versioninfo OBJECT EXCLUDE_FROM_ALL "${_vercpp}")
  target_link_libraries(linscp-versioninfo PRIVATE linscp-shim)
  target_sources(linscp-runtime-base INTERFACE $<TARGET_OBJECTS:linscp-versioninfo>)
  add_dependencies(linscp-runtime-base linscp-versioninfo)
endif()

# The scripting client, winscp.com's counterpart: core's script on standard
# input and output (CliMain.cpp, StdioConsole.cpp).
add_executable(linscp-cli EXCLUDE_FROM_ALL CliMain.cpp StdioConsole.cpp ExternalConsole.cpp
  DirectoryWatcher.cpp)
# console/Console.h: winscp.com's protocol structure, used as upstream's.
target_include_directories(linscp-cli PRIVATE "${LINSCP_UPSTREAM}/source/resource"
  "${LINSCP_UPSTREAM}/source/console")
target_link_libraries(linscp-cli PRIVATE linscp-runtime)
