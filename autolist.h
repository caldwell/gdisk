// Copyright (c) 2004-2007 David Caldwell,  All Rights Reserved.
// Copyright (c) 2005,2007 Jim Radford,  All Rights Reserved.

#ifndef __AUTOLIST_H__
#define __AUTOLIST_H__

#include <string.h>
#include <stdbool.h>
#include <stddef.h>

enum autolist_sort_state { as_unsorted, as_sorted, as_sorting };

#define __autolist_kind(kind) __autolist_kind_##kind
#define __autolist_struct(kind) __autolist_##kind
#define __autolist_order(kind) __autolist_order_##kind

#define autolist_declare(type, kind)                          \
    struct __autolist_order(kind) {                           \
        struct __autolist_order(kind) *next;                  \
        struct __autolist_struct(kind) *dependency;           \
        char *description;                                    \
    };                                                        \
    struct __autolist_struct(kind) {                          \
        struct __autolist_struct(kind) *next;                 \
        struct __autolist_order(kind) *after;                 \
        enum autolist_sort_state state;                       \
        typeof(type) data;                                    \
    };                                                        \
    struct __autolist_kind(kind) {                            \
        struct __autolist_struct(kind) *first;                \
        bool unsorted;                                        \
        char *name;                                           \
    };                                                        \
    extern struct __autolist_kind(kind) __autolist_kind(kind)

#define autolist_define(kind)                                \
    struct __autolist_kind(kind) __autolist_kind(kind) = { .name=#kind }


#include "cat.h"
struct autolist *__autolist_chain(void *kind, void *data, size_t offset_of_first);

#define __autolist_item(_kind, _name) __autolist_struct__##_kind##__##_name

#define _autolist_add_with_name(_kind, _name, _data, _static)                                   \
    _static struct __autolist_struct(_kind) __autolist_item(_kind,_name) = { .data = _data };   \
    static void CAT2(__autofunc__##_kind##__,_name)() __attribute__((constructor));             \
    static void CAT2(__autofunc__##_kind##__,_name)()                                           \
    {                                                                                           \
        __autolist_chain(&__autolist_kind(_kind), &__autolist_item(_kind, _name), offsetof(struct __autolist_kind(_kind), first)); \
    }

#define autolist_add_with_name(_kind, _name, _data)                                             \
    _autolist_add_with_name(_kind, _name, _data, )

#define autolist_add(_kind,_data,...) /* "..." can be used for disambiguation */    \
    _autolist_add_with_name(_kind, Unique(__VA_ARGS__), _data, static)

#define autolist_add_named(_kind,_data)         \
    autolist_add_with_name(_kind, _data, _data)

#define autolist_order(_kind, _before, _after)                                                                      \
    static void __autofunc_order__##_kind##__##_before##__##_after() __attribute__((constructor));                  \
    static void __autofunc_order__##_kind##__##_before##__##_after()                                                \
    {                                                                                                               \
        extern struct __autolist_struct(_kind) __autolist_item(_kind,_before), __autolist_item(_kind, _after);      \
        static struct __autolist_order(_kind) __a = { .dependency = &__autolist_item(_kind,_before),                \
                                                      .description = #_before " -> " #_after };                     \
        __autolist_chain(&__autolist_item(_kind, _after), &__a, offsetof(struct __autolist_struct(_kind), after));  \
        __autolist_kind(_kind).unsorted = true;                                                                     \
    }

void *__autolist__first(void *list);

#define foreach_autolist(var, _kind)                                    \
    for (struct __autolist_struct(_kind) *__a = __autolist__first(&__autolist_kind(_kind)), *__c = __a; __c && __a; __a = __a->next) \
        if ((__c = NULL)) ; else                                        \
            for (var = __a->data; !__c; __c = __a)

#ifdef __cplusplus
extern "C"{
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AUTOLIST_H__ */
