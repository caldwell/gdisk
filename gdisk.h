//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#ifndef __GDISK_H__
#define __GDISK_H__

struct partition_table {
    struct device *dev;
    struct gpt_header *header;
    struct gpt_header *alt_header;
    struct gpt_partition *partition;
    struct mbr mbr;
    struct options {
        int mbr_sync;
    } options;
    int alias[lengthof(((struct mbr*)0)->partition)];
};

// This is how you get to anything good.
extern struct partition_table g_table;

struct command_arg {
    char *name;
    int type;
    char *help;
};
struct command {
    char *name;
    int (*handler)(char **arg);
    char *help;
    struct command_arg *arg;
};

#define C_Optional  0x80
#define C_Type_Mask 0x7f
#define C_Type(x)   ((x) & C_Type_Mask)
#define C_Flag      0x01
#define C_Number    0x02
#define C_String    0x03
#define C_File      0x04
#define C_Partition_Type 0x05

#include "autolist.h"
#define command_add(name, handler, help, ...) \
    static struct command_arg Unique(__command_arg__)[] = { NULL, 0, NULL, ##__VA_ARGS__, NULL, 0, NULL }; \
    static struct command Unique(__command__) = { name, handler, help, &Unique(__command_arg__)[1] }; \
    autolist_add(command, &Unique(__command__))

autolist_declare(struct command *, command);

#endif /* __GDISK_H__ */

