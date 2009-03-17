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
