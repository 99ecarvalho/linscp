// What the Qt shell asks of core, in types both sides share: plain C++ and
// UTF-8. Qt's translation units never include the VCL shim, and core's never
// include Qt -- the two disagree on too much (wchar_t's width on Linux, Win32
// names, macros) to meet in one file. CoreBridge.cpp is the VCL side.
#ifndef LINSCP_QT_CORE_BRIDGE_H
#define LINSCP_QT_CORE_BRIDGE_H

#include <string>

namespace CoreBridge
{

// Starts core as upstream's WinSCP.exe does, with this process's command
// line. Throws std::runtime_error with core's message when it cannot.
void Initialize();
// Saves the configuration and stops core.
void Finalize();

struct TInfo
{
  std::string AppName;
  std::string Version;
  // Where the configuration is kept: an INI file or the registry.
  std::string ConfigurationStorage;
};
TInfo Info();

}

#endif
