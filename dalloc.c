//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.

#include "dalloc.h"

struct dalloc_memory {
    struct dalloc_memory *next;
    void *mem;
};

struct dalloc_head {
    struct dalloc_head *next;
    struct dalloc_memory *list;
};

static struct dalloc_head *dalloc_head_list;

void dalloc_start()
{
    struct dalloc_head *head = xcalloc(1, sizeof(*head));
    head->next = dalloc_head_list;
    dalloc_head_list = head;
}

void *dalloc_remember(void *mem)
{
    if (!mem) return mem;
    struct dalloc_memory *m = xmalloc(sizeof(*m));
    m->mem = mem;
    m->next = dalloc_head_list->list;
    dalloc_head_list->list = m;
    return mem;
}

void dalloc_free()
{
    struct dalloc_head *head = dalloc_head_list;
    dalloc_head_list = dalloc_head_list->next;
    for (struct dalloc_memory *m=head->list, *next; m; m=next) {
        next = m->next;
        free(m->mem);
        free(m);
    }
    free(head);
}


void *drealloc(void *old, size_t count)
{
    for (struct dalloc_memory *m=dalloc_head_list->list; m; m=m->next)
        if (m->mem == old) m->mem = NULL; // Taking it out of the list is hard. Let's go shopping.
    return dalloc_remember(xrealloc(old,count));
}

#include <stdarg.h>
#include <stdio.h>
char *dsprintf(char *format, ...)
{
    char *out;
    va_list ap;
    va_start(ap, format);
    vxsprintf(&out, format, ap);
    va_end(ap);
    return dalloc_remember(out);
}
