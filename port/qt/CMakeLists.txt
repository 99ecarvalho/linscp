# linscp, the Qt shell (P2). Qt's translation units and core's never meet:
# CoreBridge.cpp is the only file that includes the VCL shim, and the only one
# the Qt sources reach core through.

# The Qt side, moc's output included. On Linux Qt is built with a 4-byte
# wchar_t and its headers inline wide conversions (QString::fromWCharArray,
# std::wstring), so these units must have Qt's wchar_t, not the -fshort-wchar
# every core unit has. Nothing wide crosses CoreBridge.h. Windows' wchar_t is
# 2 bytes either way. See ADR 0001.
add_library(linscp-qt-ui STATIC EXCLUDE_FROM_ALL MainWindow.cpp MainWindow.h CoreBridge.h)
set_target_properties(linscp-qt-ui PROPERTIES AUTOMOC ON)
target_link_libraries(linscp-qt-ui PUBLIC Qt6::Widgets)
if(NOT WIN32)
  target_compile_options(linscp-qt-ui PRIVATE -fno-short-wchar)
endif()

# The core side.
add_library(linscp-qt-core STATIC EXCLUDE_FROM_ALL CoreBridge.cpp)
target_link_libraries(linscp-qt-core PRIVATE linscp-runtime)

add_executable(linscp WIN32 EXCLUDE_FROM_ALL main.cpp)
set_target_properties(linscp PROPERTIES AUTOMOC ON)
if(NOT WIN32)
  target_compile_options(linscp PRIVATE -fno-short-wchar)
endif()
target_link_libraries(linscp PRIVATE linscp-qt-ui linscp-qt-core linscp-runtime)

# A folder that runs on Windows as it is (port/tools/deploy_windows.py):
#   cmake --build <build> --target deploy   ->   <build>/deploy
if(WIN32 AND LINSCP_QT_PREFIX)
  add_custom_target(deploy
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/port/tools/deploy_windows.py"
            --objdump "${LINSCP_TARGET_OBJDUMP}" --qt-prefix "${LINSCP_QT_PREFIX}"
            --dest "${CMAKE_BINARY_DIR}/deploy" "$<TARGET_FILE:linscp>" "$<TARGET_FILE:linscp-cli>"
    DEPENDS linscp linscp-cli
    COMMENT "Assembling a runnable Windows folder in ${CMAKE_BINARY_DIR}/deploy"
    VERBATIM)
endif()
