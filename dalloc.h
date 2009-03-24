//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.
#ifndef __DALLOC_H__
#define __DALLOC_H__

#include "xmem.h"

void dalloc_start();
void *dalloc_remember(void *mem);
void dalloc_free();
static inline void *dalloc(size_t size)                { return dalloc_remember(xmalloc(size)); }
static inline void *dcalloc(size_t count, size_t size) { return dalloc_remember(xcalloc(count, size)); }
static inline char *dstrdup(char *s)                   { return dalloc_remember(xstrdup(s)); }
static inline void *dmemdup(void *mem, size_t size)    { return dalloc_remember(xmemdup(mem, size)); }
void *drealloc(void *old, size_t count);
char *dsprintf(char *format, ...) __attribute__ ((format (printf, 1, 2)));

#endif /* __DALLOC_H__ */

