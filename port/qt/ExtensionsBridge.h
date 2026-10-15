// The extensions' part of the custom commands' bridge, VCL side: shared by
// CustomCommands.cpp and Session.cpp.
#ifndef LINSCP_QT_EXTENSIONS_BRIDGE_H
#define LINSCP_QT_EXTENSIONS_BRIDGE_H

#include "BridgeVcl.h"
#include "CustomCommandsSlice.h"
#include "CustomCommandType.h"

// WinConfiguration.cpp's WinSCPExtensionExt: its slice's has internal
// linkage (the header's slice ends before its extern).
extern const UnicodeString ExtensionFileExt;

namespace CoreBridge
{

// The listed command a definition is: an extension by its id, else the
// custom command of its name, unless the command was changed.
const TCustomCommandType * FindListedCommand(const TCustomCommandDefinition & Definition);
// Its options with these flags and their values (Values: the
// CustomCommandOptions), their defaults' patterns filled in by ForOptions,
// when given; and the values set back.
std::vector<TExtensionOption> ExtensionOptionsOf(const TCustomCommandType * Command, TStrings * Values, unsigned Flags,
  const UnicodeString & Site, TCustomCommand * ForOptions);
void SetExtensionOptionsOf(const TCustomCommandType * Command, TStrings * Values, unsigned Flags, const UnicodeString & Site,
  const std::vector<std::string> & New);

}

#endif
