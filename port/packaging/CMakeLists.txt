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

set(LINSCP_APP_ID "io.github.99ecarvalho.linscp")

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
  set(_linux "${CMAKE_CURRENT_SOURCE_DIR}/linux")
  install(FILES "${_linux}/${LINSCP_APP_ID}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES "${_linux}/${LINSCP_APP_ID}.metainfo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
  foreach(_size 16 24 32 48 64 128 256 512)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp-${_size}.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps" RENAME "${LINSCP_APP_ID}.png")
  endforeach()
  install(FILES "${CMAKE_SOURCE_DIR}/assets/app/linscp.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps" RENAME "${LINSCP_APP_ID}.svg")
  # The licenses: linscp's, WinSCP's, and what it is built from.
  set(_doc "${CMAKE_INSTALL_DATADIR}/doc/linscp")
  install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/README.md" DESTINATION "${_doc}")
  install(FILES "${LINSCP_UPSTREAM}/license.txt" DESTINATION "${_doc}" RENAME "WinSCP-license.txt")
  install(FILES "${CMAKE_SOURCE_DIR}/3rd-party/putty/LICENCE" DESTINATION "${_doc}" RENAME "PuTTY-LICENCE")
  install(FILES "${CMAKE_SOURCE_DIR}/3rd-party/openssl/LICENSE.txt" DESTINATION "${_doc}" RENAME "OpenSSL-LICENSE.txt")
  install(FILES "${LINSCP_UPSTREAM}/libs/expat/COPYING" DESTINATION "${_doc}" RENAME "expat-COPYING")
  # (WinSCP's trimmed neon keeps no COPYING: its README states the LGPL.)
  install(FILES "${LINSCP_UPSTREAM}/libs/neon/README.md" DESTINATION "${_doc}" RENAME "neon-README.md")
  install(FILES "${LINSCP_UPSTREAM}/libs/libs3/LICENSE" DESTINATION "${_doc}" RENAME "libs3-LICENSE")
  install(FILES "${LINSCP_UPSTREAM}/libs/libs3/COPYING-LGPLv3" DESTINATION "${_doc}" RENAME "libs3-COPYING-LGPLv3")
  foreach(_set lucide tabler fluent)
    install(FILES "${CMAKE_SOURCE_DIR}/assets/icons/${_set}/LICENSE" DESTINATION "${_doc}" RENAME "icons-${_set}-LICENSE")
  endforeach()
  install(FILES "${CMAKE_SOURCE_DIR}/assets/app/SOURCES.md" DESTINATION "${_doc}" RENAME "icon-SOURCES.md")

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
