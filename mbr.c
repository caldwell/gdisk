//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include "lengthof.h"
#include "mbr.h"

struct mbr init_mbr(struct device *dev)
{
    return (struct mbr) {
        .mbr_signature = MBR_SIGNATURE,
    };
}

#define le2(a, o) ((a)[o] << 0 | (a)[(o)+1] << 8)
#define le4(a, o) (le2(a,o) | (a)[(o)+2] << 16 | (a)[(o)+3] << 24)

struct mbr mbr_from_sector(void *sector)
{
    struct mbr mbr;
    unsigned char *mbr_buf = sector;
    memcpy(mbr.code, mbr_buf, sizeof(mbr.code));
    mbr.disk_signature = le4(mbr_buf, 440);
    mbr.unused         = le2(mbr_buf, 444);
    for (int i=0; i<lengthof(mbr.partition); i++) {
        int po = 446 + i*16;
        struct mbr_partition *p = &mbr.partition[i];
        p->status                = mbr_buf[po + 0];
        p->first_sector.head     = mbr_buf[po + 1];
        p->first_sector.sector   = mbr_buf[po + 2] & 0x3f;
        p->first_sector.cylinder = (mbr_buf[po + 2] & 0xc0) << 2 | mbr_buf[po + 3];
        p->partition_type        = mbr_buf[po + 4];
        p->last_sector.head      = mbr_buf[po + 5];
        p->last_sector.sector    = mbr_buf[po + 6] & 0x3f;
        p->last_sector.cylinder  = (mbr_buf[po + 6] & 0xc0) << 2 | mbr_buf[po + 7];
        p->first_sector_lba      = le4(mbr_buf, po + 8);
        p->sectors               = le4(mbr_buf, po + 12);
    }
    mbr.mbr_signature = le2(mbr_buf, 510);
    return mbr;
}

void *sector_from_mbr(struct device *dev, struct mbr mbr)
{
    unsigned char *mbr_buf = alloc_sectors(dev, 1);
    memcpy(mbr_buf, mbr.code, sizeof(mbr.code));
    mbr_buf[440] = mbr.disk_signature >>  0 & 0xff;
    mbr_buf[441] = mbr.disk_signature >>  8 & 0xff;
    mbr_buf[442] = mbr.disk_signature >> 16 & 0xff;
    mbr_buf[443] = mbr.disk_signature >> 24 & 0xff;
    mbr_buf[444] = mbr.unused >>  0 & 0xff;
    mbr_buf[445] = mbr.unused >>  8 & 0xff;
    for (int i=0; i<lengthof(mbr.partition); i++) {
        int po = 446 + i*16;
        struct mbr_partition *p = &mbr.partition[i];
        mbr_buf[po +  0] = p->status;
        mbr_buf[po +  1] = p->first_sector.head;
        mbr_buf[po +  2] = p->first_sector.sector        & 0x3f |
                           p->first_sector.cylinder >> 2 & 0xc0;
        mbr_buf[po +  3] = p->first_sector.cylinder & 0xff;
        mbr_buf[po +  4] = p->partition_type;
        mbr_buf[po +  5] = p->last_sector.head;
        mbr_buf[po +  6] = p->last_sector.sector        & 0x3f |
                           p->last_sector.cylinder >> 2 & 0xc0;
        mbr_buf[po +  7] = p->last_sector.cylinder & 0xff;
        mbr_buf[po +  8] = p->first_sector_lba >>  0 & 0xff;
        mbr_buf[po +  9] = p->first_sector_lba >>  8 & 0xff;
        mbr_buf[po + 10] = p->first_sector_lba >> 16 & 0xff;
        mbr_buf[po + 11] = p->first_sector_lba >> 24 & 0xff;
        mbr_buf[po + 12] = p->sectors >>  0 & 0xff;
        mbr_buf[po + 13] = p->sectors >>  8 & 0xff;
        mbr_buf[po + 14] = p->sectors >> 16 & 0xff;
        mbr_buf[po + 15] = p->sectors >> 24 & 0xff;
    }
    mbr_buf[510] = mbr.mbr_signature >> 0 & 0xff;
    mbr_buf[511] = mbr.mbr_signature >> 8 & 0xff;
    return mbr_buf;
}

struct mbr read_mbr(struct device *dev)
{
    void *sector = get_sectors(dev, 0, 1);
    struct mbr mbr = mbr_from_sector(sector);
    free(sector);
    return mbr;
}

bool write_mbr(struct device *dev, struct mbr mbr)
{
    void *sector = sector_from_mbr(dev, mbr);
    bool status = device_write(dev, sector, 0, 1);
    free(sector);
    return status;
}

#include <stdio.h>
void dump_mbr(struct mbr mbr)
{
    //char code[440];
    printf("disk_signature             = %08x\n", mbr.disk_signature);
    printf("unused                     = %02x\n", mbr.unused);
    for (int i=0; i<4; i++) {
        printf("[%d] status                = %02x\n", i, mbr.partition[i].status);
        printf("[%d] first_sector.cylinder = %d\n",   i, mbr.partition[i].first_sector.cylinder);
        printf("[%d] first_sector.head     = %d\n",   i, mbr.partition[i].first_sector.head);
        printf("[%d] first_sector.sector   = %d\n",   i, mbr.partition[i].first_sector.sector);
        printf("[%d] partition_type        = %02x\n", i, mbr.partition[i].partition_type);
        printf("[%d] last_sector.cylinder  = %d\n",   i, mbr.partition[i].last_sector.cylinder);
        printf("[%d] last_sector.head      = %d\n",   i, mbr.partition[i].last_sector.head);
        printf("[%d] last_sector.sector    = %d\n",   i, mbr.partition[i].last_sector.sector);
        printf("[%d] first_sector_lba      = %d\n",   i, mbr.partition[i].first_sector_lba);
        printf("[%d] sectors               = %d\n",   i, mbr.partition[i].sectors);
    }
    printf("signature                  = %02x\n",mbr.mbr_signature);
}
