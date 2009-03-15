//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include "guid.h"

#include <stdio.h>
char *guid_str(GUID g)
{
    // Obviously not thread safe, but convenient. Don't use 2 in the same print. :-)
    static char str[sizeof(GUID)*2+4+1]; // four '-'s and a null
    snprintf(str, sizeof(str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             g.byte[3], g.byte[2], g.byte[1], g.byte[0],
             g.byte[5], g.byte[4],
             g.byte[7], g.byte[6],
             g.byte[8], g.byte[9],
             g.byte[10], g.byte[11], g.byte[12], g.byte[13], g.byte[14], g.byte[15]);
    return str;
}

// This GUID isn't "bad" per-se, but since GUIDs are globally unique I hereby designate this one to represent unparsable GUID strings.
GUID bad_guid = STATIC_GUID(a3805766,111e,11de,9b0f,001cc0952d53);

#include <stdlib.h>
#include <ctype.h>
GUID guid_from_string(char *guid)
{
    GUID g;
    if (strlen(guid) == 32) { // hex stream
        for (int i=0; i<16; i++) {
            char hex[3] = { guid[i*2+0], guid[i*2+1] };
            if (!isxdigit(hex[0]) || !isxdigit(hex[1]))
                return bad_guid;
            g.byte[i] = strtoul(hex, NULL, 16);
        }
        return g;
    }
    int byte[16];
    if (sscanf(guid, "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
               &byte[3], &byte[2], &byte[1], &byte[0],
               &byte[5], &byte[4],
               &byte[7], &byte[6],
               &byte[8], &byte[9],
               &byte[10], &byte[11], &byte[12], &byte[13], &byte[14], &byte[15]) == 16) {
        for (int i=0; i<16; i++)
            g.byte[i] = byte[i];
        return g;
    }
    return bad_guid;
}

#include <uuid/uuid.h>
GUID guid_create()
{
    GUID g;
    uuid_generate(g.byte);
    return g;
}
