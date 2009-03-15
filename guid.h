//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#ifndef __GUID_H__
#define __GUID_H__

typedef struct GUID {
    unsigned char byte[16];
} GUID;

struct gpt_partition_type {
    char *name;
    GUID guid;
};

extern struct gpt_partition_type gpt_partition_type[];
extern GUID gpt_partition_type_empty, bad_guid;

#define _0x(x) 0x##x##LL

// This encodes the GUID as little endian (which confusingly means the last 2 parts are big-endian)
#define GUID(a,b,c,d,e) (struct GUID) STATIC_GUID(a,b,c,d,e)
#define STATIC_GUID(a,b,c,d,e) {                \
    {   _0x(a) >>  0 & 0xff,                    \
        _0x(a) >>  8 & 0xff,                    \
        _0x(a) >> 16 & 0xff,                    \
        _0x(a) >> 24 & 0xff,                    \
        _0x(b) >>  0 & 0xff,                    \
        _0x(b) >>  8 & 0xff,                    \
        _0x(c) >>  0 & 0xff,                    \
        _0x(c) >>  8 & 0xff,                    \
        _0x(d) >>  8 & 0xff,                    \
        _0x(d) >>  0 & 0xff,                    \
        _0x(e) >> 40 & 0xff,                    \
        _0x(e) >> 32 & 0xff,                    \
        _0x(e) >> 24 & 0xff,                    \
        _0x(e) >> 16 & 0xff,                    \
        _0x(e) >>  8 & 0xff,                    \
        _0x(e) >>  0 & 0xff                     \
     } }

char *guid_str(); // convenience function. Returns a static char, so strdup before calling
                  // again. Obviously not thread safe, but it's convenient. :-)

GUID guid_from_string(char *guid);
GUID guid_create();

#include <string.h>
static inline int guid_eq(GUID a, GUID b) { return memcmp(a.byte, b.byte, sizeof(a.byte)) == 0; }

#endif /* __GUID_H__ */

