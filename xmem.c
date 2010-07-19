//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.
#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#include <err.h>
#include <gc/gc.h>
#include "xmem.h"

void *xmalloc(size_t size)
{
    void *mem = GC_MALLOC(size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
void *xcalloc(size_t count, size_t size)
{
    void *mem = GC_MALLOC(count * size);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
void *xrealloc(void *old, size_t count)
{
    void *mem = GC_REALLOC(old, count);
    if (!mem) err(errno, "Out of memory");
    return mem;
}
char *xstrdup(char *s)
{
    char *dup = xmalloc(strlen(s));
    strcpy(dup,s);
    return dup;
}
char *xstrdupfree(char *s)
{
    char *dup = xstrdup(s);
    free(s);
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
    if (!dest) return xstrdup(src);
    dest = xrealloc(dest, strlen(dest) + strlen(src) + 1);
    strcat(dest, src);
    return dest;
}

#include <stdarg.h>
#include <stdio.h>
int vxsprintf(char **out, const char *format, va_list ap)
{
    char *tmp;
    int count = vasprintf(&tmp, format, ap);
    if (count == -1 || !tmp) err(errno, "Out of memory");
    *out = xstrdup(tmp); // Get it into a buffer that's managed by the garbage collector
    free(tmp);
    return count;
}
char *xsprintf(char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char *out;
    /*int count = */vxsprintf(&out, format, ap);
    va_end(ap);
    return out;
}
