// Process startup and shutdown for a program on core without a GUI: the part
// of upstream's wWinMain (WinSCP.cpp) that is not the VCL's.
#ifndef LINSCP_HEADLESS_MAIN_H
#define LINSCP_HEADLESS_MAIN_H

// Creates the application log (enabled by /applog=<path>, as upstream) and
// initialises core. Program options come from the command line, through
// TProgramParams. Raises what CoreInitialize raises.
void HeadlessInitialize();
// Saves the configuration and tears core and the log down.
void HeadlessFinalize();
// The command line TProgramParams parses, instead of the process's: for a
// program whose own options (Qt's) are no business of core's. Before
// HeadlessInitialize.
void SetProgramCommandLine(const wchar_t * CommandLine);

class UnicodeString;
// How the console client asks for the master password when a stored
// password is encrypted with it, as WinSCP.com's MasterPasswordPrompt: the
// password read into Password (Incorrect: the last one did not match, which
// the prompt says first), false when none can be read. Without one such
// passwords cannot be decrypted.
typedef bool (*THeadlessMasterPasswordPrompt)(UnicodeString & Password, bool Incorrect);
void SetHeadlessMasterPasswordPrompt(THeadlessMasterPasswordPrompt Prompt);

#endif
