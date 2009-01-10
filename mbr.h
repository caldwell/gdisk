//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#ifndef __MBR_H__
#define __MBR_H__

#include "device.h"

struct chs {
    int cylinder;
    int head;
    int sector;
};

struct mbr {
    char code[440];
    int disk_signature;
    int unused;
    struct mbr_partition {
        int status;
        struct chs first_sector;
        int partition_type;
        struct chs last_sector;
        int first_sector_lba;
        int sectors;
    } partition[4];
    int mbr_signature;
};

#define MBR_SIGNATURE         0xAA55
#define MBR_STATUS_BOOTABLE   0x80

struct mbr init_mbr(struct device *dev);
struct mbr read_mbr(struct device *dev);
bool write_mbr(struct device *dev, struct mbr mbr);
void dump_mbr(struct mbr mbr);
struct mbr mbr_from_sector(void *sector); // frees sector
void *sector_from_mbr(struct device *dev, struct mbr mbr); // returns malloced mem


#endif /* __MBR_H__ */

