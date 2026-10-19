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

set(LINSCP_APP_ID "io.github._99ecarvalho.linscp")

# The licenses every package carries, as <file>|<name>: linscp's, WinSCP's,
# and those of what it is built from.
set(_licenses
  "${CMAKE_SOURCE_DIR}/LICENSE|LICENSE"
  "${CMAKE_SOURCE_DIR}/README.md|README.md"
  "${LINSCP_UPSTREAM}/license.txt|WinSCP-license.txt"
  "${CMAKE_SOURCE_DIR}/3rd-party/putty/LICENCE|PuTTY-LICENCE"
  "${CMAKE_SOURCE_DIR}/3rd-party/openssl/LICENSE.txt|OpenSSL-LICENSE.txt"
  "${LINSCP_UPSTREAM}/libs/expat/COPYING|expat-COPYING"
  # (WinSCP's trimmed neon keeps no COPYING: its README states the LGPL.)
  "${LINSCP_UPSTREAM}/libs/neon/README.md|neon-README.md"
  "${LINSCP_UPSTREAM}/libs/libs3/LICENSE|libs3-LICENSE"
  "${LINSCP_UPSTREAM}/libs/libs3/COPYING-LGPLv3|libs3-COPYING-LGPLv3"
  "${CMAKE_SOURCE_DIR}/assets/icons/lucide/LICENSE|icons-lucide-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/icons/tabler/LICENSE|icons-tabler-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/icons/fluent/LICENSE|icons-fluent-LICENSE"
  "${CMAKE_SOURCE_DIR}/assets/app/SOURCES.md|icon-SOURCES.md")

if(NOT WIN32)
  include(GNUInstallDirs)
  set(_libdir "${CMAKE_INSTALL_LIBDIR}/linscp")
  install(TARGETS linscp linscp-cli RUNTIME DESTINATION "${_libdir}")
  install(DIRECTORY "${LINSCP_TRANSLATIONS_DIR}/" DESTINATION "${_libdir}/Translations")
  foreach(_program linscp linscp-cli)
    install(CODE "
      set(_bin \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
      file(MAKE_DIRECTORY \"\${_bin}\")
      file(CREATE_LINK \"../${_libdir}/${_program}\" \"\${_bin}/${_program}\" SYMBOLIC)")
  endforeach()

  # WinSCP's .NET assembly (netstandard2.0), for WinSCP's PowerShell
  # extensions, when the build host has a .NET SDK (port/tools/fetch_dotnet.py);
  # <libdir>/dotnet/{WinSCPnet.dll, WinSCP.exe}, with WinSCP.exe the client the
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
      DEPENDS "${CMAKE_SOURCE_DIR}/port/tools/build_winscpnet.py"
      COMMENT "Building WinSCP's .NET assembly (netstandard2.0)" VERBATIM)
    add_custom_target(winscpnet ALL DEPENDS "${_winscpnet}")
    install(FILES "${_winscpnet}" DESTINATION "${_libdir}/dotnet")
    install(CODE "
      set(_dotnet \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_libdir}/dotnet\")
      file(CREATE_LINK \"../linscp-cli\" \"\${_dotnet}/WinSCP.exe\" SYMBOLIC)")
  else()
    message(STATUS "No .NET SDK at ${LINSCP_HOST_DOTNET}: the package goes without WinSCP's .NET assembly")
  endif()
  set(_linux "${CMAKE_CURRENT_SOURCE_DIR}/linux")
  install(FILES "${_linux}/${LINSCP_APP_ID}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES "${_linux}/${LINSCP_APP_ID}.metainfo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
  foreach(_size 16 24 32 48 64 128 256 512)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp-${_size}.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps" RENAME "${LINSCP_APP_ID}.png")
  endforeach()
  install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" RENAME "${LINSCP_APP_ID}.svg")
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
  set(CPACK_PACKAGE_CONTACT "Eduardo Correia <eduardo.correia@hexagon.com>")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SFTP, SCP, FTP, WebDAV and S3 client (a Qt port of WinSCP)")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/99ecarvalho/linscp")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_DEBIAN_PACKAGE_SECTION "net")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_STRIP_FILES ON)
  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "linscp is a Qt port of WinSCP: the same core, sessions and settings, with\n WinSCP's two-panel Commander interface. It transfers files over SFTP, SCP,\n FTP (with TLS), WebDAV and Amazon S3.")
  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")
  include(CPack)
  add_custom_target(package-deb
    COMMAND "${CMAKE_CPACK_COMMAND}" -G DEB --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS linscp linscp-cli core-translations
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building the .deb in ${CMAKE_BINARY_DIR}/packages"
    VERBATIM)
  # The AppImage (port/tools/build_appimage.py): the same install, its
  # libraries and Qt's plugins bundled.
  add_custom_target(package-appimage
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/build_appimage.py" --build "${CMAKE_BINARY_DIR}"
    DEPENDS linscp linscp-cli core-translations
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
