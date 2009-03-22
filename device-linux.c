//  Copyright (c) 2008 David Caldwell and Jim Radford,  All Rights Reserved.
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

static unsigned int sector_size(int fd)
{
    uint32_t size;
    if (ioctl(fd, BLKSSZGET, &size) == -1)
        return 0;
    return size;
}

static unsigned long long byte_count(int fd)
{
    uint64_t count;
    if (ioctl(fd, BLKGETSIZE64, &count) == -1)
        return 0;
    return count;
}

struct device *open_disk_device(char *name)
{
    int fd = open(name, O_RDWR | O_EXCL);
    if (fd < 0) {
        if (errno == ENOENT) return NULL;
        if (errno == EBUSY)
            fd = open(name, O_RDWR);
    }
    if (fd < 0)
        return NULL;

    unsigned int ss = sector_size(fd);
    unsigned long long bc = byte_count(fd);

    struct device dev = {
        .fd = fd,
        .name = xstrdup(name),
        .sector_size = ss,
        .sector_count = ss ? bc/ss : 0,
    };
    return xmemdup(&dev, sizeof(dev));
}

char *device_help()
{
    return "  <device> is /dev/sd* style device (full path)\n";
}
