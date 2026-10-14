# linscp, the Qt shell (P2). Qt's translation units and core's never meet:
# CoreBridge.cpp is the only file that includes the VCL shim, and the only one
# the Qt sources reach core through.

# The Qt side, moc's output included. On Linux Qt is built with a 4-byte
# wchar_t and its headers inline wide conversions (QString::fromWCharArray,
# std::wstring), so these units must have Qt's wchar_t, not the -fshort-wchar
# every core unit has. Nothing wide crosses CoreBridge.h. Windows' wchar_t is
# 2 bytes either way. See ADR 0001.
add_library(linscp-qt-ui STATIC EXCLUDE_FROM_ALL MainWindow.cpp AboutDialog.cpp EditorWindow.cpp QtFrontend.cpp QueryDialog.cpp
  PromptDialog.cpp Language.cpp SiteManager.cpp SiteAdvanced.cpp
  FileList.cpp RemotePanel.cpp LocalPanel.cpp
  TransferDialog.cpp QueuePanel.cpp SynchronizeDialog.cpp PreferencesDialog.cpp TransferSettingsDialog.cpp
  RightsFrame.cpp PropertiesDialog.cpp CreateDirectoryDialog.cpp LinkDialog.cpp RemoteTransferDialog.cpp OpenDirectoryDialog.cpp CoreBridge.h)
set_target_properties(linscp-qt-ui PROPERTIES AUTOMOC ON)
target_link_libraries(linscp-qt-ui PUBLIC Qt6::Widgets)
if(NOT WIN32)
  target_compile_options(linscp-qt-ui PRIVATE -fno-short-wchar)
endif()

# The core side: the bridge (CoreBridge.cpp, Session.cpp) and the shell's
# half of core's Interface.h (QtInterface.cpp), in place of the headless one.
add_library(linscp-qt-core STATIC EXCLUDE_FROM_ALL CoreBridge.cpp QtInterface.cpp Session.cpp Sites.cpp
  Keys.cpp ShellConfiguration.cpp TransferSettings.cpp)
target_link_libraries(linscp-qt-core PUBLIC linscp-runtime-base linscp-gui)
# The generated GUIConfiguration.h before core's include path, which has
# upstream's own windows/ (ADR 0014).
target_include_directories(linscp-qt-core BEFORE PUBLIC "${LINSCP_GEN}/source/windows")
# linscp's own version (the project's); core keeps WinSCP's.
target_compile_definitions(linscp-qt-core PRIVATE LINSCP_VERSION="${PROJECT_VERSION}")

add_executable(linscp WIN32 EXCLUDE_FROM_ALL main.cpp)
set_target_properties(linscp PROPERTIES AUTOMOC ON)
if(NOT WIN32)
  target_compile_options(linscp PRIVATE -fno-short-wchar)
endif()
target_link_libraries(linscp PRIVATE linscp-qt-ui linscp-qt-core)

# Translations (P2-09, doc/qt/translations.md).
#
# Core's texts: WinSCP's own translations, one file per language generated
# into Translations/ beside the executable, where upstream's GUI looks for its
# own (port/tools/gen_translations.py),
# which CoreBridge::SetLanguage loads into the shim.
file(GLOB _winscp_translations CONFIGURE_DEPENDS "${LINSCP_UPSTREAM}/translations/*.ini")
set(_translations_dir "${CMAKE_CURRENT_BINARY_DIR}/Translations")
set(_translations_stamp "${CMAKE_CURRENT_BINARY_DIR}/translations.stamp")
add_custom_command(
  OUTPUT "${_translations_stamp}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/gen_translations.py"
          --src "${LINSCP_UPSTREAM}/source" --translations "${LINSCP_UPSTREAM}/translations"
          --out "${_translations_dir}"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_translations_stamp}"
  DEPENDS ${_winscp_translations} "${CMAKE_SOURCE_DIR}/port/tools/gen_translations.py"
          "${CMAKE_SOURCE_DIR}/port/tools/gen_resources.py"
  COMMENT "Generating core's translations from WinSCP's"
  VERBATIM)
add_custom_target(core-translations DEPENDS "${_translations_stamp}")
add_dependencies(linscp core-translations)
set(LINSCP_TRANSLATIONS_DIR "${_translations_dir}" PARENT_SCOPE)

# The shell's own texts: Qt Linguist's .ts files, one per language
# (translations/linscp_<locale>.ts), compiled into the UI library -- so the
# tests that link it have them too. The build machine's lupdate and lrelease
# run them, the same for both targets: MSYS2 has Qt's Linguist CMake package
# only in its qt6-tools. Run the update_translations target after changing
# the shell's texts; ctest translations_current fails while a .ts lags behind.
find_program(LINSCP_LUPDATE NAMES lupdate-qt6 lupdate HINTS /usr/lib/qt6/bin NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_program(LINSCP_LRELEASE NAMES lrelease-qt6 lrelease HINTS /usr/lib/qt6/bin NO_CMAKE_FIND_ROOT_PATH REQUIRED)
file(GLOB _shell_ts CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/translations/linscp_*.ts")
file(GLOB _shell_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
set(_shell_qm)
foreach(_ts ${_shell_ts})
  get_filename_component(_name "${_ts}" NAME_WE)
  set(_qm "${CMAKE_CURRENT_BINARY_DIR}/i18n/${_name}.qm")
  add_custom_command(OUTPUT "${_qm}"
    COMMAND "${LINSCP_LRELEASE}" -silent "${_ts}" -qm "${_qm}"
    DEPENDS "${_ts}" VERBATIM)
  list(APPEND _shell_qm "${_qm}")
endforeach()
qt_add_resources(linscp-qt-ui shell_translations
  PREFIX /i18n BASE "${CMAKE_CURRENT_BINARY_DIR}/i18n" FILES ${_shell_qm})
# linscp's icon: WinSCP's with an "L" on the padlock (port/tools/make_icon.py).
qt_add_resources(linscp-qt-ui shell_icons
  PREFIX /icons BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons" FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/linscp.ico")
add_custom_target(update_translations
  COMMAND "${LINSCP_LUPDATE}" -no-obsolete -locations none ${_shell_sources} -ts ${_shell_ts}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Updating the shell's .ts files from its sources"
  VERBATIM)
set(LINSCP_LUPDATE "${LINSCP_LUPDATE}" PARENT_SCOPE)
set(LINSCP_SHELL_SOURCES ${_shell_sources} PARENT_SCOPE)
set(LINSCP_SHELL_TS ${_shell_ts} PARENT_SCOPE)

# A folder that runs on Windows as it is (port/tools/deploy_windows.py):
#   cmake --build <build> --target deploy   ->   <build>/deploy
if(WIN32 AND LINSCP_QT_PREFIX)
  add_custom_target(deploy
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/deploy_windows.py"
            --objdump "${LINSCP_TARGET_OBJDUMP}" --qt-prefix "${LINSCP_QT_PREFIX}"
            --dest "${CMAKE_BINARY_DIR}/deploy" --translations "${_translations_dir}"
            "$<TARGET_FILE:linscp>" "$<TARGET_FILE:linscp-cli>"
    DEPENDS linscp linscp-cli core-translations
    COMMENT "Assembling a runnable Windows folder in ${CMAKE_BINARY_DIR}/deploy"
    VERBATIM)
endif()
