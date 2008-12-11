//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdbool.h>

struct device {
    char *name;
    unsigned long sector_size;
    unsigned long long sector_count;
};

void *alloc_sectors(struct device *dev, unsigned long sectors);
void *get_sectors(struct device *dev, unsigned long sector_num, unsigned long sectors);

// device specific:
struct device *open_device(char *name);
void close_device(struct device *dev);
bool device_read(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector);
bool device_write(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector);
char *device_help();

#endif /* __DEVICE_H__ */

