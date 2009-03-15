//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.
#ifndef __PARTITION_TYPE_H__
#define __PARTITION_TYPE_H__

#include "guid.h"

struct gpt_partition_type {
    char *name;
    GUID guid;
};

extern struct gpt_partition_type gpt_partition_type[];
extern GUID gpt_partition_type_empty;

#endif /* __PARTITION_TYPE_H__ */

