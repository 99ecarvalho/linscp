# The headless frontend: core's Interface.h functions for a process with no
# GUI, plus upstream's TProgramParams (windows/ProgParams.h; its .cpp is
# carried in HeadlessMain.cpp, since upstream's now needs the VCL forms).
add_library(linscp-frontend-headless STATIC EXCLUDE_FROM_ALL
  HeadlessInterface.cpp
  HeadlessMain.cpp)
target_link_libraries(linscp-frontend-headless PUBLIC linscp-core)
target_include_directories(linscp-frontend-headless PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}")

# Everything a program built on core links: core, the headless frontend, and
# the libraries core calls into. lld resolves static archives regardless of
# order, so the PuTTY <-> core cycle needs no repetition.
add_library(linscp-runtime INTERFACE)
target_link_libraries(linscp-runtime INTERFACE linscp-frontend-headless linscp-core)
# Core's objects themselves, in linscp-core's source order, so their static
# initialisers run in upstream's project order (see port/core). Linked first
# and whole, the archive above is left with nothing to contribute.
target_sources(linscp-runtime INTERFACE $<TARGET_OBJECTS:linscp-core>)
foreach(_lib linscp-putty linscp-puttyvs linscp-filezilla linscp-libs3 linscp-neon)
  if(TARGET ${_lib})
    target_link_libraries(linscp-runtime INTERFACE ${_lib})
  endif()
endforeach()

# Core reads its own version from the executable's VERSIONINFO resource, which
# C++Builder writes from WinSCP.cbproj's VerInfo_* settings. Generated from
# the same settings, so it follows the pin, and linked into every executable
# built on core. POSIX has no resources; the platform layer's part (P1-07b).
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
  target_sources(linscp-runtime INTERFACE $<TARGET_OBJECTS:linscp-versioninfo>)
  add_dependencies(linscp-runtime linscp-versioninfo)
endif()
