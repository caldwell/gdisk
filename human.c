//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.

#include <stdint.h>
#include "csprintf.h"
#include "human.h"

float human_number(long long x)
{
    float n = x;
    while (n > 1024)
        n /= 1024;
    return n;
}
char *human_units(long long x)
{
    char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
    for (char **u = units; ; x /= 1024, u++)
        if (x < 1024)
            return *u;
}
