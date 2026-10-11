// PuTTY's Windows help API.
//
// putty/windows/platform.h includes this, but WinSCP does not vendor the file:
// it belongs to PuTTY's own Windows GUI, which WinSCP replaces wholesale. Core
// never calls any of it, so declarations are enough to let platform.h parse.
#ifndef LINSCP_PUTTY_HELP_H
#define LINSCP_PUTTY_HELP_H

void init_help(void);
void shutdown_help(void);
bool has_help(void);
void launch_help(void * hwnd, const char * topic);
void quit_help(void * hwnd);
int has_embedded_chm(void);

#endif
