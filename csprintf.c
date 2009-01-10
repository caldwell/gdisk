#include <stdarg.h>
#include <stdio.h>
#include "csprintf.h"
#include "lengthof.h"
char *csprintf(char *format, ...)
{
    static char s[5][1000];
    static int i;
    i = i % lengthof(s);
    va_list ap;
    va_start(ap, format);
    if (vsnprintf(s[i], sizeof(s[i]), format, ap) >= sizeof(s[i]))
        fprintf(stderr, "csprintf output truncated\n");
    va_end(ap);
    return s[i++];
}
