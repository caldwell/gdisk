//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#define _GNU_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include "device.h"
#include "xmem.h"

void *alloc_sectors(struct device *dev, unsigned long sectors)
{
    return xcalloc(sectors, dev->sector_size);
}

void *get_sectors(struct device *dev, unsigned long long sector_num, unsigned long sectors)
{
    void *data = alloc_sectors(dev, sectors);
    if (!device_read(dev, data, sector_num, sectors))
        err(errno, "Couldn't read sectors %ld through %ld", sector_num, sector_num+sectors);
    return data;
}

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "xmem.h"
static struct device *open_file_device(char *name)
{
    unsigned long long sector_size=512, sector_count=0;
    char *meta = name;
    char *filename         = strsep(&meta, ",");
    char *sector_size_str  = meta;
    if (sector_size_str)  sector_size  = strtoull(sector_size_str, NULL, 0);

    int fd = open(filename, O_RDWR);
    if (fd < 0)
        err(errno, "Error opening %s", filename);
    if (!sector_count) {
        struct stat st;
        if (fstat(fd, &st) == -1)
            err(errno, "can't find file size");
        sector_count = st.st_size/sector_size;
    }
    struct device dev = {
        .fd = fd,
        .name = xstrdup(filename),
        .sector_size = sector_size,
        .sector_count = sector_count,
    };
    return xmemdup(&dev, sizeof(dev));
}

struct device *open_device(char *name)
{
    struct device *dev = open_disk_device(name);
    if (!dev || dev->sector_size == 0 || dev->sector_count == 0) {
        close_device(dev);
        dev = open_file_device(name);
    }
    return dev;
}

void close_device(struct device *dev)
{
    if (!dev) return;
    close(dev->fd);
}

#include <stdio.h>
bool device_read(struct device *dev, void *buffer, unsigned long long sector, unsigned long sectors)
{
    return pread(dev->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
}

bool device_write(struct device *dev, void *buffer, unsigned long long sector, unsigned long sectors)
{
    return pwrite(dev->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
}
