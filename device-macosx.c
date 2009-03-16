//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/disk.h>
#include <err.h>
#include "device.h"

#warning "TODO: Pull as much of this as possible into device-fd.c so that linux and the mac can share more code."
struct device_macosx {
    struct device dev;
    int fd;
};

static unsigned int sector_size(int fd)
{
    uint32_t size;
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &size) == -1)
        err(errno, "ioctl(DKIOCGETBLOCKSIZE)");
    return size;
}

static unsigned long long sector_count(int fd)
{
    uint64_t count;
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &count) == -1)
        err(errno, "ioctl(DKIOCGETBLOCKCOUNT)");
    return count;
}

struct device *open_device(char *name)
{
    int fd = open(name, O_RDWR | O_EXLOCK);
    if (fd < 0) {
        if (errno == ENOENT) return NULL;
        if (errno == EBUSY)
            fd = open(name, O_RDWR | O_SHLOCK);
    }
    if (fd < 0)
        err(errno, "Error opening device %s", name);

    struct device_macosx *dev = malloc(sizeof(struct device_macosx));
    if (!dev) {
        close(fd);
        err(ENOMEM, "No memory for device structure");
    }
    dev->fd = fd;
    dev->dev.name = strdup(name);
    dev->dev.sector_count = sector_count(fd);
    dev->dev.sector_size = sector_size(fd);
    return &dev->dev;
}

void close_device(struct device *dev)
{
    struct device_macosx *macosx = (struct device_macosx *)dev;
    close(macosx->fd);
    free(dev->name);
    free(macosx);
}

#include <stdio.h>
bool device_read(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector)
{
    struct device_macosx *macosx = (struct device_macosx *)dev;
    return pread(macosx->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
//     int red = pread(macosx->fd, buffer, dev->sector_size, dev->sector_size * sector);
//     if (red != dev->sector_size)
//         printf("Read %d bytes instead of %d\n", red, dev->sector_size);
//     return red == dev->sector_size;
}

bool device_write(struct device *dev, void *buffer, unsigned int sectors, unsigned long long sector)
{
    struct device_macosx *macosx = (struct device_macosx *)dev;
    return pwrite(macosx->fd, buffer, dev->sector_size * sectors, dev->sector_size * sector) == dev->sector_size * sectors;
}

char *device_help()
{
    return "  <device> is /dev/disk* style device (full path)\n";
}
