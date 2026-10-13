// What the programs' own halves of core's Interface.h share
// (FrontendCommon.cpp).
#ifndef LINSCP_FRONTEND_COMMON_H
#define LINSCP_FRONTEND_COMMON_H

class TConfiguration;

// The configuration's storage as upstream's CreateConfiguration sets it up:
// /ini, /rawconfig, and POSIX's INI file. Returns Configuration.
TConfiguration * SetUpConfigurationStorage(TConfiguration * Configuration);
// The temporary directory upstream's GUI uses when none is configured.
UnicodeString DefaultTemporaryDir(bool Mask);

#endif
