# The units of WinSCP's GUI the Qt shell uses as they are, compiled from the
# transformed tree like core (ADR 0014): windows/GUIConfiguration.cpp, the
# GUI's configuration (transfer settings presets, synchronization and queue
# options, the language).
#
# VCL side, as core: no Qt here. include/ stands in for the GUI's
# precompiled header, which upstream's would bring the whole VCL GUI with.
set(LINSCP_GUI_SOURCES "${LINSCP_GEN}/source/windows/GUIConfiguration.cpp")
set_source_files_properties(${LINSCP_GUI_SOURCES} PROPERTIES GENERATED TRUE)
add_library(linscp-gui STATIC EXCLUDE_FROM_ALL ${LINSCP_GUI_SOURCES} GuiShim.cpp)
add_dependencies(linscp-gui transform)
target_include_directories(linscp-gui BEFORE PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${LINSCP_GEN}/source/windows")
target_link_libraries(linscp-gui PUBLIC linscp-core)
if(WIN32)
  target_link_libraries(linscp-gui PUBLIC ole32 shell32)
endif()
