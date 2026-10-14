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
add_executable(test_xml test_xml.cpp)
target_link_libraries(test_xml PRIVATE linscp-shim)
add_test(NAME xml COMMAND test_xml)

add_executable(test_tryfinally test_tryfinally.cpp)
target_link_libraries(test_tryfinally PRIVATE linscp-shim)
add_test(NAME tryfinally COMMAND test_tryfinally)

add_executable(test_misc test_misc.cpp)
target_link_libraries(test_misc PRIVATE linscp-shim)
add_test(NAME misc COMMAND test_misc)

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

add_executable(test_winsock test_winsock.cpp)
target_link_libraries(test_winsock PRIVATE linscp-shim)
if(WIN32)
  target_link_libraries(test_winsock PRIVATE ws2_32)
endif()
add_test(NAME winsock COMMAND test_winsock)
set_tests_properties(winsock PROPERTIES TIMEOUT 60)

# Windows, message queues and window-message sockets (FileZilla's model):
# user32 and ws2_32 on Windows, the emulation on POSIX, same assertions.
add_executable(test_messages test_messages.cpp)
target_link_libraries(test_messages PRIVATE linscp-shim)
if(WIN32)
  target_compile_definitions(test_messages PRIVATE UNICODE _UNICODE)
  target_link_libraries(test_messages PRIVATE ws2_32)
endif()
add_test(NAME messages COMMAND test_messages)
set_tests_properties(messages PROPERTIES TIMEOUT 60)

add_executable(test_win32_posix test_win32_posix.cpp)
target_link_libraries(test_win32_posix PRIVATE linscp-shim)
if(WIN32)
  target_link_libraries(test_win32_posix PRIVATE shlwapi psapi urlmon secur32)
endif()
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

add_executable(test_thread test_thread.cpp)
target_link_libraries(test_thread PRIVATE linscp-shim)
add_test(NAME thread COMMAND test_thread)

add_executable(test_json test_json.cpp)
target_link_libraries(test_json PRIVATE linscp-shim)
add_test(NAME json COMMAND test_json)

# Core, every library it uses and the headless frontend, linked into one
# executable with every core object forced in: the link proves nothing is
# undefined, and running it runs every static initialiser in them.
add_test(NAME core_link_build
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target linscp-core-link)
add_test(NAME core_link_run COMMAND linscp-core-link)
set_tests_properties(core_link_run PROPERTIES DEPENDS core_link_build)

add_executable(test_core_init EXCLUDE_FROM_ALL test_core_init.cpp)
target_link_libraries(test_core_init PRIVATE linscp-runtime)
add_test(NAME core_init_build
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_core_init)
add_test(NAME core_init COMMAND test_core_init /ini=nul)
set_tests_properties(core_init PROPERTIES DEPENDS core_init_build)

# Sites in WinSCP's INI file, read through core's storage as CoreLoad does.
add_executable(test_ini_sites EXCLUDE_FROM_ALL test_ini_sites.cpp)
target_link_libraries(test_ini_sites PRIVATE linscp-runtime)
add_test(NAME ini_sites_build
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_ini_sites)
add_test(NAME ini_sites COMMAND test_ini_sites /ini=nul "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(ini_sites PROPERTIES DEPENDS ini_sites_build)

# A real session against a throwaway loopback sshd: connect, list, download.
add_executable(test_sftp_session EXCLUDE_FROM_ALL test_sftp_session.cpp)
target_link_libraries(test_sftp_session PRIVATE linscp-runtime)
add_test(NAME sftp_session_build
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_sftp_session)
# Once as negotiated, then pinned to each cipher family: each runs through
# different PuTTY code (AES-NI or software AES, GCM, ChaCha20-Poly1305).
foreach(_case "default;" "aes_ctr;Ciphers aes128-ctr" "aes_gcm;Ciphers aes256-gcm@openssh.com"
              "chacha;Ciphers chacha20-poly1305@openssh.com")
  list(GET _case 0 _name)
  list(GET _case 1 _line)
  set(_extra)
  if(_line)
    set(_extra --sshd "${_line}")
  endif()
  set(_test sftp_session)
  if(NOT _name STREQUAL "default")
    set(_test sftp_session_${_name})
  endif()
  add_test(NAME ${_test}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_sftp_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" ${_extra}
            "$<TARGET_FILE:test_sftp_session>")
  set_tests_properties(${_test} PROPERTIES
    DEPENDS sftp_session_build SKIP_RETURN_CODE 77 TIMEOUT 180)
endforeach()


# The scripting client against the same loopback sshd: /keygen, a session
# by /command, a failing command's exit code, commands on standard input,
# /stdout and /script with /parameter.
add_test(NAME cli_session_build
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target linscp-cli)
add_test(NAME cli_session
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_cli_session.py"
          --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:linscp-cli>")
set_tests_properties(cli_session PROPERTIES
  DEPENDS cli_session_build SKIP_RETURN_CODE 77 TIMEOUT 300)
# keepuptodate: local changes made while the client watches must reach the
# server, Ctrl+C must end it. Native POSIX here; the Windows build is run
# natively on a Windows host (run_keepuptodate.py works there as well).
if(NOT WIN32)
  add_test(NAME keepuptodate
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_keepuptodate.py" "$<TARGET_FILE:linscp-cli>")
  set_tests_properties(keepuptodate PROPERTIES
    DEPENDS cli_session_build SKIP_RETURN_CODE 77 TIMEOUT 300)
endif()
# winscp.com's console protocol over /consolepipes, the script in the
# caller's place (ADR 0010). POSIX's transport; Windows' is /consoleinstance.
if(NOT WIN32)
  add_test(NAME console_pipes
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_console_pipes.py" "$<TARGET_FILE:linscp-cli>")
  set_tests_properties(console_pipes PROPERTIES
    DEPENDS cli_session_build SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()
# WinSCP's .NET assembly driving linscp-cli (dotnet/run_dotnet.py): the
# assembly built from the pinned source with linscp's patches
# (port/patches/dotnet: console pipes off Windows, ADR 0010), by the .NET SDK
# port/tools/fetch_dotnet.py puts at LINSCP_DOTNET. Both skip without it.
# Native builds only: the SDK does not run under Wine.
if(NOT CMAKE_CROSSCOMPILING)
  set(LINSCP_DOTNET "${CMAKE_BINARY_DIR}/tools/dotnet/dotnet${CMAKE_EXECUTABLE_SUFFIX}" CACHE FILEPATH
      "the .NET SDK's dotnet, for ctest dotnet (port/tools/fetch_dotnet.py)")
  add_test(NAME winscpnet_build
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_winscpnet.py"
            --dotnet "${LINSCP_DOTNET}" --upstream "${LINSCP_UPSTREAM}" --dest "${CMAKE_BINARY_DIR}/winscpnet")
  set_tests_properties(winscpnet_build PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
  add_test(NAME dotnet
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/dotnet/run_dotnet.py"
            --dotnet "${LINSCP_DOTNET}" --winscpnet "${CMAKE_BINARY_DIR}/winscpnet/bin/WinSCPnet.dll"
            --client "$<TARGET_FILE:linscp-cli>")
  set_tests_properties(dotnet PROPERTIES
    DEPENDS "cli_session_build;winscpnet_build" SKIP_RETURN_CODE 77 TIMEOUT 900)
endif()
# One everyday script over every protocol whose server is installed here,
# each checked on the server's side (run_protocols.py, fixtures.py).
add_test(NAME protocols
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_protocols.py"
          --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:linscp-cli>")
set_tests_properties(protocols PROPERTIES
  DEPENDS cli_session_build SKIP_RETURN_CODE 77 TIMEOUT 600)
# The golden comparison: the cases in golden/cases run by linscp-cli and
# compared with the logs real WinSCP wrote for them on Windows
# (golden/expected, captured with run_golden.py capture). Skipped until the
# corpus exists.
# The servers must be the corpus' (golden/expected/SERVERS.txt): rclone of
# that version at LINSCP_RCLONE, which port/tools/fetch_rclone.py fetches;
# otherwise the test skips and says so.
set(LINSCP_RCLONE "${CMAKE_BINARY_DIR}/tools/rclone" CACHE FILEPATH
    "rclone of the golden corpus' version, for ctest golden")
add_test(NAME golden
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/golden/run_golden.py" check
          --client "$<TARGET_FILE:linscp-cli>" --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}"
          --rclone "${LINSCP_RCLONE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/golden/expected")
set_tests_properties(golden PROPERTIES
  DEPENDS cli_session_build SKIP_RETURN_CODE 77 TIMEOUT 1800)

# PuTTY's crypto primitives against published vectors, on both targets: the
# PuTTY libraries alone, with a stub for the one frontend function they need.
if(TARGET linscp-putty)
  add_executable(test_putty_crypto EXCLUDE_FROM_ALL test_putty_crypto.c test_putty_crypto_stubs.c)
  target_include_directories(test_putty_crypto PRIVATE
    $<TARGET_PROPERTY:linscp-putty,INCLUDE_DIRECTORIES>)
  linscp_upstream_defines(test_putty_crypto PRIVATE Putty.cbproj)
  target_link_libraries(test_putty_crypto PRIVATE linscp-putty linscp-puttyvs linscp-shim)
  set_target_properties(test_putty_crypto PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME putty_crypto_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_putty_crypto)
  add_test(NAME putty_crypto COMMAND test_putty_crypto)
  set_tests_properties(putty_crypto PROPERTIES DEPENDS putty_crypto_build)
endif()

# Executable tests go above this line.
# The Qt shell starts core, shows its main window and quits from the event
# loop (port/qt/main.cpp --smoke), offscreen on Linux; under Wine on Windows,
# where Qt finds its plugins beside its DLLs. Qt's -platform argument, as
# Wine does not hand QT_QPA_PLATFORM on.
if(TARGET linscp)
  # The bridge's VCL side against a real server, a scripted frontend in Qt's
  # place: questions core asks while a session opens, and the answers.
  add_executable(test_qt_session EXCLUDE_FROM_ALL test_qt_session.cpp)
  target_include_directories(test_qt_session PRIVATE "${CMAKE_SOURCE_DIR}/port/qt")
  target_link_libraries(test_qt_session PRIVATE linscp-qt-core)
  add_test(NAME qt_session_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_qt_session)
  add_test(NAME qt_session
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_session>")
  set_tests_properties(qt_session PROPERTIES DEPENDS qt_session_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  # Stored sites over core's INI storage, saved in one process and checked in
  # the next, the stored one opened.
  add_executable(test_qt_sites EXCLUDE_FROM_ALL test_qt_sites.cpp)
  target_include_directories(test_qt_sites PRIVATE "${CMAKE_SOURCE_DIR}/port/qt")
  target_link_libraries(test_qt_sites PRIVATE linscp-qt-core)
  add_test(NAME qt_sites_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_qt_sites)
  add_test(NAME qt_sites
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_sites.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_sites>")
  set_tests_properties(qt_sites PROPERTIES DEPENDS qt_sites_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  # In WinSCP's Portuguese translation: core's texts come back translated.
  add_dependencies(test_qt_session core-translations)
  # The tests that read the shell's translations run after gui_build, which
  # may write them (the translations' generation is part of building the
  # shell).
  add_test(NAME qt_session_pt_BR
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_session>"
            /ini=nul {port} sftp pt_BR "${LINSCP_TRANSLATIONS_DIR}")
  set_tests_properties(qt_session_pt_BR PROPERTIES DEPENDS "qt_session_build;gui_build" SKIP_RETURN_CODE 77 TIMEOUT 300)
  # Over FTP too: FileZilla's engine, whose sockets run on window messages.
  add_test(NAME qt_session_ftp
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" --protocol ftp "$<TARGET_FILE:test_qt_session>")
  set_tests_properties(qt_session_ftp PROPERTIES DEPENDS qt_session_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  # The Qt side: the dialogs alone, then the main window opening a session
  # with every question answered through them.
  add_executable(test_qt_gui EXCLUDE_FROM_ALL test_qt_gui.cpp)
  target_include_directories(test_qt_gui PRIVATE "${CMAKE_SOURCE_DIR}/port/qt")
  find_package(Qt6 REQUIRED COMPONENTS Test)
  target_link_libraries(test_qt_gui PRIVATE linscp-qt-ui linscp-qt-core Qt6::Test)
  if(NOT WIN32)
    target_compile_options(test_qt_gui PRIVATE -fno-short-wchar)  # a Qt unit: ADR 0001
  endif()
  add_test(NAME qt_gui_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test_qt_gui)
  add_test(NAME qt_dialogs COMMAND test_qt_gui dialogs -platform offscreen)
  set_tests_properties(qt_dialogs PROPERTIES DEPENDS qt_gui_build TIMEOUT 120)
  add_test(NAME qt_language COMMAND test_qt_gui language "${LINSCP_TRANSLATIONS_DIR}" -platform offscreen)
  set_tests_properties(qt_language PROPERTIES DEPENDS "qt_gui_build;gui_build" TIMEOUT 120)
  # The shell's .ts files keep up with its texts (port/tools/check_ts_current.py).
  add_test(NAME translations_current
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_ts_current.py"
            --lupdate "${LINSCP_LUPDATE}" --ts ${LINSCP_SHELL_TS} --sources ${LINSCP_SHELL_SOURCES})
  add_test(NAME qt_gui_sites
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            sites "/ini={tmp}/WinSCP.ini" {port} {key} {tmp} -platform offscreen)
  set_tests_properties(qt_gui_sites PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_panel
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            panel /ini=nul {port} -platform offscreen)
  set_tests_properties(qt_gui_panel PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_local
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            local /ini=nul {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_local PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_keys
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            keys /ini=nul {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_keys PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_preferences
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            preferences "/ini={tmp}/WinSCP.ini" "${LINSCP_TRANSLATIONS_DIR}" {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_preferences PROPERTIES DEPENDS "qt_gui_build;gui_build" SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_transfer
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            transfer /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_transfer PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_drop
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            drop /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_drop PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_sync
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            sync /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_sync PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_edit
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            edit /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_edit PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_properties
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" --protocol ftp "$<TARGET_FILE:test_qt_gui>"
            properties /ini=nul {port} -platform offscreen)
  set_tests_properties(qt_gui_properties PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_bookmarks
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            bookmarks /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_bookmarks PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_tools
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            tools /ini=nul {port} {tmp}/local -platform offscreen)
  set_tests_properties(qt_gui_tools PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_tags
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" --protocol s3 "$<TARGET_FILE:test_qt_gui>"
            tags /ini=nul {port} -platform offscreen)
  set_tests_properties(qt_gui_tags PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_keep
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            keep /ini=nul {port} {tmp} -platform offscreen)
  set_tests_properties(qt_gui_keep PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_commands
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" --protocol sshd "$<TARGET_FILE:test_qt_gui>"
            commands /ini=nul {port} {key} {user} {tmp} {hosttmp} -platform offscreen)
  set_tests_properties(qt_gui_commands PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME qt_gui_session
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_qt_session.py"
            --emulator "${CMAKE_CROSSCOMPILING_EMULATOR}" "$<TARGET_FILE:test_qt_gui>"
            session /ini=nul {port} -platform offscreen)
  set_tests_properties(qt_gui_session PROPERTIES DEPENDS qt_gui_build SKIP_RETURN_CODE 77 TIMEOUT 300)
  add_test(NAME gui_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target linscp)
  add_test(NAME gui_smoke COMMAND linscp /ini=nul --smoke -platform offscreen)
  set_tests_properties(gui_smoke PROPERTIES DEPENDS gui_build TIMEOUT 120
    PASS_REGULAR_EXPRESSION ", Qt 6[.0-9]+, offscreen: ok")
endif()

# When cross-compiling, the toolchain points CMAKE_CROSSCOMPILING_EMULATOR at
# wine; these tell wine where the mingw runtime DLLs live and keep its output
# off the test log.
if(LINSCP_WINE_DLLDIR_GCC)
  # Every test registered so far, rather than a hand-kept list: a list let a new
  # test silently run without the DLL path. Script-driven tests are added below
  # this block and run on the host, so they are excluded by position.
  get_property(_wine_tests DIRECTORY PROPERTY TESTS)
  # The backslash-escaped ';' is a literal Windows path separator inside the
  # WINEPATH value; the unescaped ';' separates the two environment entries.
  set_tests_properties(${_wine_tests} PROPERTIES ENVIRONMENT
    "WINEPATH=${LINSCP_WINE_DLLDIR_GCC}\;${LINSCP_WINE_DLLDIR_SYSROOT};WINEDEBUG=-all")
endif()

# What Windows checks at load time and Wine does not: every import from a DLL
# the programs ship with resolves in it (port/tools/check_dll_imports.py).
if(WIN32 AND LINSCP_WINE_DLLDIR_GCC)
  set(_programs "$<TARGET_FILE:linscp-cli>")
  set(_deps cli_session_build)
  if(TARGET linscp)
    list(APPEND _programs "$<TARGET_FILE:linscp>")
    list(APPEND _deps gui_build)
  endif()
  add_test(NAME windows_imports
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_dll_imports.py"
            --objdump "${LINSCP_TARGET_OBJDUMP}" --dlls "${LINSCP_WINE_DLLDIR_GCC}" ${_programs})
  set_tests_properties(windows_imports PROPERTIES DEPENDS "${_deps}")
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
# The recorded closure sites must still match the source. An upstream change
# that moves one of these lines fails here rather than silently wrapping the
# wrong token.
add_test(NAME patch_series
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_patchset.py")
# WinSCP's OpenSSL changes are all reviewed: adopted or knowingly skipped.
add_test(NAME openssl_tree
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_openssl_tree.py")
# WinSCP's PuTTY changes are all reviewed (doc/qt/putty-deltas.md): a bump of
# WinSCP or of the stock PuTTY that changes any of them fails here.
add_test(NAME putty_deltas
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/putty_deltas.py"
          --winscp "${LINSCP_UPSTREAM}/source/putty" --stock "${CMAKE_SOURCE_DIR}/3rd-party/putty"
          --check "${CMAKE_SOURCE_DIR}/port/putty/deltas.txt")
add_test(NAME upstream_units
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_upstream_units.py")
if(TARGET linscp-putty)
  # Builds core and PuTTY first: the check reads the two archives.
  add_test(NAME putty_overrides_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target linscp-core linscp-putty)
  add_test(NAME putty_overrides
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_overrides.py"
            "${LINSCP_TARGET_NM}" "$<TARGET_FILE:linscp-core>" "$<TARGET_FILE:linscp-putty>"
            "${CMAKE_SOURCE_DIR}/port/putty/overrides.txt")
  set_tests_properties(putty_overrides PROPERTIES DEPENDS putty_overrides_build)
  if(WIN32)
    find_program(LINSCP_TARGET_OBJDUMP NAMES "${LINSCP_TARGET_TRIPLE}-objdump" REQUIRED)
  else()
    find_program(LINSCP_TARGET_OBJDUMP NAMES objdump llvm-objdump REQUIRED)
  endif()
  add_test(NAME aes_ni_guard_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target linscp-puttyvs)
  add_test(NAME aes_ni_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_isa_free.py"
            "${LINSCP_TARGET_OBJDUMP}" "$<TARGET_FILE:linscp-puttyvs>" "aes-ni.c${CMAKE_C_OUTPUT_EXTENSION}" aes_ni_available)
  set_tests_properties(aes_ni_guard PROPERTIES DEPENDS aes_ni_guard_build)
endif()
add_test(NAME tryfinally_inline
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_tryfinally_inline.py")
add_test(NAME lp64_long
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_lp64_long.py")
# ADR 0001 at link level: nothing linked calls the C library's 4-byte wide
# functions. POSIX only; Windows' runtime has a 2-byte wchar_t of its own.
if(NOT WIN32)
  set(_wide_archives)
  foreach(_lib linscp-core linscp-putty linscp-puttyvs linscp-filezilla linscp-mfc linscp-libs3
               linscp-neon linscp-shim linscp-frontend-headless)
    list(APPEND _wide_archives "$<TARGET_FILE:${_lib}>")
  endforeach()
  add_test(NAME wide_imports
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_wide_imports.py"
            "${CMAKE_NM}" ${_wide_archives})
  set_tests_properties(wide_imports PROPERTIES DEPENDS core_link_build)
endif()
add_test(NAME closure_shapes
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_closure_shapes.py")
add_test(NAME closure_sites
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/bind_closures.py"
          --gen "${LINSCP_GEN}" --require-bound)
set_tests_properties(closure_sites
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(NAME census_ratchet
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/census_ratchet.py")
set_tests_properties(census_ratchet
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_test(NAME shim_no_wide_abi
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/check_wide_abi.py")
set_tests_properties(shim_no_wide_abi
  PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# The *_build tests each run a build of this tree; two at once contend for
# Ninja's lock and fail spuriously under ctest -j, so they take turns.
get_property(_all_tests DIRECTORY PROPERTY TESTS)
set(_build_tests)
foreach(_t IN LISTS _all_tests)
  if(_t MATCHES "_build$")
    list(APPEND _build_tests ${_t})
  endif()
endforeach()
set_tests_properties(${_build_tests} PROPERTIES RESOURCE_LOCK linscp_build_tree)
