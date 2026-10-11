/* The one frontend function PuTTY's crypto code can reach, for a test that
 * links the PuTTY libraries alone: a fatal error, reported and ended. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void modalfatalbox(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("FATAL ", stdout);
    vprintf(fmt, ap);
    fputs("\n", stdout);
    va_end(ap);
    exit(2);
}
