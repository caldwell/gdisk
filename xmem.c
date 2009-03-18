//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.

#include <string.h>
#include <errno.h>
#include <err.h>
#include "xmem.h"

void *xmalloc(size_t size)
{
    void *mem = malloc(size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
void *xcalloc(size_t count, size_t size)
{
    void *mem = calloc(count, size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
void *xrealloc(void *old, size_t count)
{
    void *mem = realloc(old, count);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
char *xstrdup(char *s)
{
    char *dup = strdup(s);
    if (!dup) err(errno, "Out of memory");
    return dup;
}
void *xmemdup(void *mem, size_t size)
{
    void *dup = xmalloc(size);
    memcpy(dup, mem, size);
    return dup;
}

// Like strcat, but reallocs to make room (so dest must come from malloc)
char *xstrcat(char *dest, char *src)
{
    dest = xrealloc(dest, strlen(dest) + strlen(src) + 1);
    strcat(dest, src);
    return dest;
}

#include <stdarg.h>
#include <stdio.h>
int vxsprintf(char **out, const char *format, va_list ap)
{
    int count = vasprintf(out, format, ap);
    if (count == -1 || !*out) err(errno, "Out of memory");
    return count;
}
int xsprintf(char **out, char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int count = vxsprintf(out, format, ap);
    va_end(ap);
    return count;
}
