//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.
#ifndef __HUMAN_H__
#define __HUMAN_H__

// Main interface
char *human_string(long long size); // csprintf() based.
long long human_size(char *human);


// Low level interface usage:
// printf("%.2f %s", human_format(number));
#define human_format(x) human_number(x), human_units(x)
char *human_units(long long x);
float human_number(long long x);

#endif /* __HUMAN_H__ */

