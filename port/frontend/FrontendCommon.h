// What the programs' own halves of core's Interface.h share
// (FrontendCommon.cpp).
#ifndef LINSCP_FRONTEND_COMMON_H
#define LINSCP_FRONTEND_COMMON_H

class TConfiguration;
class TProgramParams;

// The configuration's storage as upstream's CreateConfiguration sets it up:
// /ini, /rawconfig, and POSIX's INI file. Returns Configuration.
TConfiguration * SetUpConfigurationStorage(TConfiguration * Configuration);
// A temporary directory as upstream's GUI makes one (UniqTempDir, "scp"),
// under Base, or the system's temporary directory when none is configured.
UnicodeString DefaultTemporaryDir(bool Mask, const UnicodeString & Base = UnicodeString());

// Upstream's CheckSafe: no /unsafe.
bool CheckSafe(TProgramParams * Params);
// WinMain.cpp's (and WinSCP.com's) logging switches: /loglevel, /logsize,
// /log, /xmllog (the console's /xmllogrequired too) as the configuration's
// temporary settings.
void ApplyLogSwitches(TProgramParams * Params, bool Console);

#endif
