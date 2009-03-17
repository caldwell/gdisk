//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.
#ifndef __XMEM_H__
#define __XMEM_H__

#include <stdlib.h>

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *old, size_t count);
char *xstrdup(char *s);
void *xmemdup(void *mem, size_t size);
// Like strcat, but reallocs to make room (so dest must come from malloc)
char *xstrcat(char *dest, char *src);

#endif /* __XMEM_H__ */

