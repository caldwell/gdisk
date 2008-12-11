//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <err.h>
#include "lengthof.h"
#include "round.h"
#include "guid.h"
#include "device.h"
#include "gpt.h"
#include "mbr.h"

void dump_dev(struct device *dev);
void dump_header(struct gpt_header *header);
void dump_partition(struct gpt_partition *p);

static void usage(char *me, int exit_code)
{
    fprintf(exit_code ? stderr : stdout,
            "Usage: \n"
            "   %s <device>\n"
            "%s", me, device_help());
    exit(exit_code);
}

int main(int c, char **v)
{
    char *device_name = v[1];
    if (!device_name)
        usage(v[1], 1);

    struct device *dev = open_device(device_name);
    if (!dev)
        err(0, "Couldn't find device %s", v[1]);


    struct gpt_header *header = get_sectors(dev,1,1);
    struct gpt_header *alt_header = get_sectors(dev,header->alternate_lba,1);

    if (sizeof(struct gpt_partition) < header->partition_entry_size)
        err(0, "Size of partition entries are %d instead of %d", header->partition_entry_size, sizeof(struct gpt_partition));

    union {
        struct gpt_partition part;
        char pad[header->partition_entry_size];
    } *table = get_sectors(dev, 2, divide_round_up(header->partition_entry_size * header->partition_entries,dev->sector_size));

    struct mbr mbr = read_mbr(dev);

    dump_mbr(mbr);
    dump_dev(dev);
    dump_header(header);
    dump_header(alt_header);
    for (int i=0; i<header->partition_entries; i++) {
        if (memcmp(gpt_partition_type_empty, table[i].part.partition_type, sizeof(gpt_partition_type_empty)) == 0)
            continue;
        printf("Partition %d of %d\n", i, header->partition_entries);
        dump_partition(&table[i].part);
    }
    free(header);
    free(table);

    //if (!write_mbr(dev, mbr))
    //    warn("Couldn't write MBR sector");
}

void dump_dev(struct device *dev)
{
    printf("dev.sector_size: %ld\n", dev->sector_size);
    printf("dev.sector_count: %lld\n", dev->sector_count);
}

void dump_header(struct gpt_header *header)
{
    printf("signature[8]         = %.8s\n", header->signature);
    printf("revision             = %08x\n", header->revision);
    printf("header_size          = %d\n",   header->header_size);
    printf("header_crc32         = %08x\n", header->header_crc32);
    printf("reserved             = %08x\n", header->reserved);
    printf("my_lba               = %lld\n", header->my_lba);
    printf("alternate_lba        = %lld\n", header->alternate_lba);
    printf("first_usable_lba     = %lld\n", header->first_usable_lba);
    printf("last_usable_lba      = %lld\n", header->last_usable_lba);
    printf("disk_guid            = %s\n",   guid_str(header->disk_guid));
    printf("partition_entry_lba  = %lld\n", header->partition_entry_lba);
    printf("partition_entries    = %d\n",   header->partition_entries);
    printf("partition_entry_size = %d\n",   header->partition_entry_size);
    printf("partition_crc        = %08x\n", header->partition_crc);
}

void dump_partition(struct gpt_partition *p)
{
    printf("partition_type = %s\n",   guid_str(p->partition_type));
    for (int i=0; gpt_partition_type[i].name; i++)
        if (memcmp(gpt_partition_type[i].guid, p->partition_type, sizeof(p->partition_type)) == 0)
            printf("      * %s\n", gpt_partition_type[i].name);
    printf("partition_guid = %s\n",   guid_str(p->partition_guid));
    printf("first_lba      = %lld\n", p->first_lba);
    printf("last_lba       = %lld\n",  p->last_lba);
    printf("attributes     = %016llx\n", p->attributes);
    wchar_t name[36];
    for (int i=0; i<lengthof(name); i++)
        name[i] = p->name[i];
    printf("name[36]       = %.36ls\n",   name);
}
