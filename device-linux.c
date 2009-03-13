//  Copyright (c) 2008 David Caldwell and Jim Radford,  All Rights Reserved.
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <err.h>
#include "device.h"

struct device_linux {
    struct device dev;
    int fd;
};
static unsigned int sector_size(int fd)
{
    uint32_t size;
    if (ioctl(fd, BLKSSZGET, &size) == -1)
        err(errno, "ioctl(BLKSSZGET)");
    return size;
}

static unsigned long long byte_count(int fd)
{
    uint64_t count;
    if (ioctl(fd, BLKGETSIZE64, &count) == -1)
        err(errno, "ioctl(BLKGETSIZE64)");
    return count;
}

struct device *open_device(char *name)
{
    int fd = open(name, O_RDWR | O_EXCL);
    if (fd < 0) {
        if (errno == ENOENT) return NULL;
        if (errno == EBUSY)
            fd = open(name, O_RDWR);
    }
    if (fd < 0)
        err(errno, "Error opening device %s", name);

    struct device_linux *dev = malloc(sizeof(*dev));
    if (!dev) {
        close(fd);
        err(ENOMEM, "No memory for device structure");
    }
    dev->fd = fd;
    dev->dev.name = strdup(name);
    dev->dev.sector_size = sector_size(fd);
    dev->dev.sector_count = byte_count(fd)/dev->dev.sector_size;
    return &dev->dev;
}

void close_device(struct device *dev)
{
    struct device_linux *dev_linux = (struct device_linux *)dev;
    close(dev_linux->fd);
    free(dev->name);
    free(dev_linux);
}

#include <stdio.h>
bool device_read(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector)
{
    struct device_linux *dev_linux = (struct device_linux *)dev;
    return pread(dev_linux->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
}

bool device_write(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector)
{
    struct device_linux *dev_linux = (struct device_linux *)dev;
    return pwrite(dev_linux->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
}

char *device_help()
{
    return "  <device> is /dev/sd* style device (full path)\n";
}
