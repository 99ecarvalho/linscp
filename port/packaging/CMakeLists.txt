# Packaging (P2-10): what `cmake --install` lays out on Linux, which the .deb
# (CPack), the AppImage and the Flatpak all package; and the Windows
# installer (Inno Setup, as WinSCP's) over the deploy folder.
#
# Linux layout, FHS-style:
#   <prefix>/lib/linscp/{linscp, linscp-cli, Translations/}
#       core finds its translations beside the real executable, as WinSCP's
#       GUI does, so the programs live together in their own directory;
#   <prefix>/bin/{linscp, linscp-cli}  symlinks to them;
#   <prefix>/share/{applications, icons/hicolor, metainfo, doc/linscp}.

set(LINSCP_APP_ID "org.linscp.linscp")

# The licenses every package carries, as <file>|<name>: linscp's, WinSCP's,
# and those of what it is built from.
set(_licenses
  "${CMAKE_SOURCE_DIR}/LICENSE|LICENSE"
  "${CMAKE_SOURCE_DIR}/README.md|README.md"
  "${CMAKE_SOURCE_DIR}/NEWS.md|NEWS.md"
  "${LINSCP_UPSTREAM}/license.txt|WinSCP-license.txt"
  "${CMAKE_SOURCE_DIR}/3rd-party/putty/LICENCE|PuTTY-LICENCE"
  # The FTP engine, from FileZilla: GPL-2.0-or-later.
  "${LINSCP_UPSTREAM}/source/filezilla/license-filezilla.txt|FileZilla-license.txt"
  # neon's licence text (src/COPYING.LIB) and its README, which lists the
  # copyright holders.
  "${LINSCP_UPSTREAM}/libs/neon/src/COPYING.LIB|neon-COPYING.LIB"
  "${LINSCP_UPSTREAM}/libs/neon/README.md|neon-README.md"
  # libs3: LGPL-3.0-or-later or GPL-2.0-or-later, at the user's choice.
  "${LINSCP_UPSTREAM}/libs/libs3/LICENSE|libs3-LICENSE"
  "${LINSCP_UPSTREAM}/libs/libs3/COPYING-LGPLv3|libs3-COPYING-LGPLv3"
  "${LINSCP_UPSTREAM}/libs/libs3/COPYING-GPLv2|libs3-COPYING-GPLv2"
  "${CMAKE_SOURCE_DIR}/assets/icons/lucide/LICENSE|icons-lucide-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/icons/tabler/LICENSE|icons-tabler-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/icons/fluent/LICENSE|icons-fluent-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/app/SOURCES.md|icon-SOURCES.md")
# The bundled OpenSSL and expat, unless the system's are used (their licences
# then come with the system's packages).
if(NOT LINSCP_SYSTEM_OPENSSL)
  list(APPEND _licenses "${CMAKE_SOURCE_DIR}/3rd-party/openssl/LICENSE.txt|OpenSSL-LICENSE.txt")
endif()
if(NOT LINSCP_SYSTEM_EXPAT)
  list(APPEND _licenses "${LINSCP_UPSTREAM}/libs/expat/COPYING|expat-COPYING")
endif()

if(NOT WIN32)
  include(GNUInstallDirs)
  set(_libdir "${CMAKE_INSTALL_LIBDIR}/linscp")
  install(TARGETS linscp linscp-cli RUNTIME DESTINATION "${_libdir}")
  install(DIRECTORY "${LINSCP_TRANSLATIONS_DIR}/" DESTINATION "${_libdir}/Translations")
  # The install directories may be absolute (GNUInstallDirs allows it; Nix
  # passes them so): resolved at install time, against the prefix then in
  # effect (cmake --install --prefix), only when relative.
  set(_install_dirs "
    set(_libdir_abs \"${_libdir}\")
    set(_bindir_abs \"${CMAKE_INSTALL_BINDIR}\")
    foreach(_dir _libdir_abs _bindir_abs)
      if(NOT IS_ABSOLUTE \"\${\${_dir}}\")
        set(\${_dir} \"\${CMAKE_INSTALL_PREFIX}/\${\${_dir}}\")
      endif()
    endforeach()")
  foreach(_program linscp linscp-cli)
    install(CODE "${_install_dirs}
      set(_bin \"\$ENV{DESTDIR}\${_bindir_abs}\")
      file(RELATIVE_PATH _target \"\${_bindir_abs}\" \"\${_libdir_abs}/${_program}\")
      file(MAKE_DIRECTORY \"\${_bin}\")
      file(CREATE_LINK \"\${_target}\" \"\${_bin}/${_program}\" SYMBOLIC)")
  endforeach()

  # WinSCP's .NET assembly (netstandard2.0), for WinSCP's PowerShell
  # extensions, when the build host has a .NET SDK (port/tools/fetch_dotnet.py);
  # <libdir>/dotnet/{WinSCPnet.dll, linscp-cli}, with linscp-cli the client the
  # assembly starts (as the Windows deploy has it). %WINSCP_PATH% points here
  # (port/qt/main.cpp, CliMain.cpp). Skipped, with a note, when no SDK.
  set(LINSCP_HOST_DOTNET "${CMAKE_BINARY_DIR}/tools/dotnet/dotnet" CACHE FILEPATH
      "the build host's .NET SDK, to build WinSCP's .NET assembly into the package")
  if(EXISTS "${LINSCP_HOST_DOTNET}")
    set(_winscpnet "${CMAKE_BINARY_DIR}/winscpnet/bin/WinSCPnet.dll")
    add_custom_command(OUTPUT "${_winscpnet}"
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_winscpnet.py"
              --dotnet "${LINSCP_HOST_DOTNET}" --upstream "${CMAKE_SOURCE_DIR}/3rd-party/winscp"
              --dest "${CMAKE_BINARY_DIR}/winscpnet" --framework netstandard2.0
              --product-version "${PROJECT_VERSION}"
      DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/build_winscpnet.py"
      COMMENT "Building WinSCP's .NET assembly (netstandard2.0)" VERBATIM)
    add_custom_target(winscpnet DEPENDS "${_winscpnet}")
    # The package targets build it first (below). The install tolerates its
    # absence, so a plain `cmake --install` without it still succeeds.
    install(CODE "${_install_dirs}
      if(EXISTS \"${_winscpnet}\")
        set(_dotnet \"\$ENV{DESTDIR}\${_libdir_abs}/dotnet\")
        file(MAKE_DIRECTORY \"\${_dotnet}\")
        file(COPY \"${_winscpnet}\" DESTINATION \"\${_dotnet}\")
        file(REMOVE \"\${_dotnet}/linscp-cli\")
        file(CREATE_LINK \"../linscp-cli\" \"\${_dotnet}/linscp-cli\" SYMBOLIC)
      else()
        message(STATUS \"no WinSCPnet.dll built; package without WinSCP's .NET assembly\")
      endif()")
    set(_winscpnet_dep winscpnet)
  else()
    message(STATUS "No .NET SDK at ${LINSCP_HOST_DOTNET}: the package goes without WinSCP's .NET assembly")
    set(_winscpnet_dep)
  endif()
  set(_linux "${CMAKE_CURRENT_SOURCE_DIR}/linux")
  install(FILES "${_linux}/${LINSCP_APP_ID}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  # The screenshots' addresses: this version's tag.
  set(PROJECT_VERSION "${CMAKE_PROJECT_VERSION}")
  configure_file("${_linux}/${LINSCP_APP_ID}.metainfo.xml" "${CMAKE_CURRENT_BINARY_DIR}/${LINSCP_APP_ID}.metainfo.xml" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${LINSCP_APP_ID}.metainfo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
  foreach(_size 16 24 32 48 64 128 256 512)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp-${_size}.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps" RENAME "${LINSCP_APP_ID}.png")
  endforeach()
  install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" RENAME "${LINSCP_APP_ID}.svg")
  # The manual pages (generated by port/tools/gen_manpages.py, committed).
  install(FILES "${CMAKE_SOURCE_DIR}/doc/man/linscp.1" "${CMAKE_SOURCE_DIR}/doc/man/linscp-cli.1"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
  # The licenses.
  set(_doc "${CMAKE_INSTALL_DATADIR}/doc/linscp")
  foreach(_license ${_licenses})
    string(REPLACE "|" ";" _license "${_license}")
    list(GET _license 0 _from)
    list(GET _license 1 _name)
    install(FILES "${_from}" DESTINATION "${_doc}" RENAME "${_name}")
  endforeach()

  # The .deb: CPack's, its dependencies read from the binaries by
  # dpkg-shlibdeps, so it is for the distribution it is built on.
  #   cmake --build <build> --target package-deb
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "linscp")
  set(CPACK_PACKAGE_VENDOR "linscp")
  set(CPACK_PACKAGE_CONTACT "Eduardo Correia <ecorreia@apliant.com.br>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SFTP, SCP, FTP, WebDAV and S3 file-transfer client")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://linscp.org")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_SECTION "net")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_STRIP_FILES ON)
  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "linscp transfers files between your computer and remote servers over\n SFTP, SCP, FTP (with TLS), WebDAV and S3. Its two-panel interface shows\n local and remote files side by side, with the same sessions, settings and\n scripting on Linux and Windows.\n .\n It synchronizes and compares directories, keeps a remote directory up to\n date, edits remote files, and runs commands on the server.")
  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")
  include(CPack)
  add_custom_target(package-deb
    COMMAND "${CMAKE_CPACK_COMMAND}" -G DEB --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS linscp linscp-cli core-translations ${_winscpnet_dep}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building the .deb in ${CMAKE_BINARY_DIR}/packages"
    VERBATIM)
  # The AppImage (port/tools/build_appimage.py): the same install, its
  # libraries and Qt's plugins bundled.
  add_custom_target(package-appimage
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_appimage.py" --build "${CMAKE_BINARY_DIR}"
    DEPENDS linscp linscp-cli core-translations ${_winscpnet_dep}
    COMMENT "Building the AppImage in ${CMAKE_BINARY_DIR}/packages"
    VERBATIM)
endif()

# The Windows installer (port/tools/build_installer.py): the deploy folder
# and the licenses, compiled by the pinned Inno Setup under Wine.
#   cmake --build <build> --target installer
if(WIN32 AND TARGET deploy)
  set(_license_args)
  foreach(_license ${_licenses})
    list(APPEND _license_args --license "${_license}")
  endforeach()
  add_custom_target(installer
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_installer.py"
            --script "${CMAKE_CURRENT_SOURCE_DIR}/windows/linscp.iss" --version "${PROJECT_VERSION}"
            --deploy "${CMAKE_BINARY_DIR}/deploy" --work "${CMAKE_BINARY_DIR}/installer"
            --tools "${CMAKE_BINARY_DIR}/tools" --out "${CMAKE_BINARY_DIR}/packages" ${_license_args}
    DEPENDS deploy
    COMMENT "Building the installer in ${CMAKE_BINARY_DIR}/packages"
    VERBATIM)
  # The portable package (port/tools/build_portable.py): the same folder,
  # zipped, its configuration kept beside it.
  add_custom_target(package-portable
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_portable.py" --version "${PROJECT_VERSION}"
            --deploy "${CMAKE_BINARY_DIR}/deploy" --out "${CMAKE_BINARY_DIR}/packages" ${_license_args}
    DEPENDS deploy
    COMMENT "Building the portable package in ${CMAKE_BINARY_DIR}/packages"
    VERBATIM)
endif()
