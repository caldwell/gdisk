//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "csprintf.h"
#include "human.h"

float human_number(long long x)
{
    float n = x;
    while (n > 1024)
        n /= 1024;
    return n;
}

static char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", NULL };
char *human_units(long long x)
{
    for (char **u = units; ; x /= 1024, u++)
        if (x < 1024)
            return *u;
}

char *human_string(long long size)
{
    return csprintf("%.2f %2s", human_format(size));
}

long long human_units_multiplier(char *unit)
{
    while (isspace(*unit)) unit++;
    if (!*unit) return 1;
    long long x = 1;
    for (char **u = units; *u; x *= 1024, u++)
        if (strncasecmp(*u, unit, strlen(unit)) == 0)
            return x;
    fprintf(stderr, "Bad units: \"%s\". Ignoring...", unit);
    return 1;
}

long long human_size(char *human)
{
    if (strchr(human, '.')) {
        double num = strtod(human, &human);
        return num * human_units_multiplier(human);
    } else {
        long long num = strtoull(human, &human, 0);
        return num * human_units_multiplier(human);
    }
    return 0;
}

#ifdef TEST
// gcc -std=gnu99 -o human-test human.c csprintf.c -DTEST
#include <inttypes.h>
int main()
{
#define test(s) ({ long long x = human_size(s); printf("%10s => %16"PRId64" => %10s\n", s, x, human_string(x)); })
    test("0");
    test("1");
    test("10");
    test("100");
    test("1024");
    test("1025");
    test("1048575");
    test("1048576");
    test("1048577");
    test("1B");
    test("1KB");
    test("1MB");
    test("1GB");
    test("1TB");
    test("0x10B");
    test("0x10GB");
    test("3000");

    test("1.5M");
    test("2.0G");
    test("2G");
    test(".9T");
    test("3.K");
    test("3K");

    test("1 B");
    test("1 KB");
    test("1 MB");
    test("1 GB");
    test("1 TB");
    test("1 b");
    test("1 kb");
    test("1 mb");
    test("1 gb");
    test("1 tb");

    test("1 K");
    test("1 M");
    test("1 G");
    test("1 T");
    test("1 k");
    test("1 m");
    test("1 g");
    test("1 t");

    test("1K");
    test("1M");
    test("1G");
    test("1T");
    test("1k");
    test("1m");
    test("1g");
    test("1t");
    return 0;
}
#endif
