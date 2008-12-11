//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include "device.h"

void *alloc_sectors(struct device *dev, unsigned long sectors)
{
    char *data = malloc(dev->sector_size * sectors);
    if (!data)
        err(0, "Couldn't allocate memory for %ld sectors (%ld bytes)", sectors, dev->sector_size * sectors);
    return data;
}

void *get_sectors(struct device *dev, unsigned long sector_num, unsigned long sectors)
{
    void *data = alloc_sectors(dev, sectors);
    if (!device_read(dev, data, sectors, sector_num))
        err(errno, "Couldn't read sectors %ld through %ld", sector_num, sector_num+sectors);
    return data;
}


