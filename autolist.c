/*
 * Copyright 2004 Jim Radford <radford@blackbean.org>
 * Copyright 2007 David Caldwell <david@porkrind.org>
 */

#include "autolist.h"

struct autolist {
    struct autolist *next;
};

struct autolist *__autolist_chain(void *_head, void *_next, size_t offset_of_first)
{
    struct autolist *head = _head + offset_of_first, *next = _next, **start = &head->next;
    //while (*start) start = &(*start)->next; // call in order of construction
    struct autolist *orig = *start;
    *start = next;
    next->next = orig;
    return orig;
}

autolist_declare(void *, generic); // Not a real autolist, just used to get structures defined.

typedef struct __autolist_struct(generic) autolist; // Trust me, it's impossible to read without this.

static autolist *unhook(autolist *item, autolist **rest)
{
    // Remove item from rest.
    for (autolist *i = *rest, *last = NULL; i; last=i, i=i->next)
        if (i == item) {
            if (last)
                last->next = i->next;
            else
                *rest = (*rest)->next;
            break;
        }
    item->next = NULL;
    return item;
}

static void append(autolist ***next, autolist *list)
{
    **next = list;
    for (autolist *i=list; i; i=i->next)
        if (!i->next) // only advance next when we hit the end of list
            *next = &i->next;
}

#include <stdio.h>
static autolist *autolist_deps(autolist *item, autolist **rest)
{
    unhook(item, rest);
    item->state = as_sorting;

    autolist *deps, **next=&deps;
    for (struct __autolist_order(generic) *after = item->after; after; after=after->next) {
        if (after->dependency->state == as_sorting)
            fprintf(stderr, "Circular dependency! Dropping \"%s\"\n", after->description);
        if (after->dependency->state == as_unsorted)
            append(&next, autolist_deps(after->dependency, rest));
    }

    item->state = as_sorted;
    append(&next, item);

    return deps;
}

static autolist *autolist_sort(autolist *rest)
{
    autolist *list, **next=&list;

    while (rest)
        append(&next, autolist_deps(rest, &rest));

    return list;
}

void *__autolist__first(void *_list)
{
    struct __autolist_kind(generic) *list = _list;
    if (!list->unsorted) return list->first;

    list->first = autolist_sort(list->first);

    list->unsorted = false;
    return list->first;
}
