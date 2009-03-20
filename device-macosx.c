//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/disk.h>
#include <err.h>
#include "xmem.h"
#include "device.h"

static unsigned int sector_size(int fd)
{
    uint32_t size;
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &size) == -1)
        return 0;
    return size;
}

static unsigned long long sector_count(int fd)
{
    uint64_t count;
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &count) == -1)
        return 0;
    return count;
}

struct device *open_disk_device(char *name)
{
    int fd = open(name, O_RDWR | O_EXLOCK);
    if (fd < 0) {
        if (errno == ENOENT) return NULL;
        if (errno == EBUSY)
            fd = open(name, O_RDWR | O_SHLOCK);
    }
    if (fd < 0)
        return NULL;

    struct device dev = {
        .fd = fd,
        .name = strdup(name),
        .sector_size = sector_size(fd),
        .sector_count = sector_count(fd),
    };
    return xmemdup(&dev, sizeof(dev));
}

char *device_help()
{
    return "  <device> is /dev/disk* style device (full path)\n";
}
