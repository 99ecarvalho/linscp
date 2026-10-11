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

#endif
