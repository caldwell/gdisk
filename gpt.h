//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.
#ifndef __GPT_H__
#define __GPT_H__

#include <stdint.h>

// [1] Extensible Firmware Interface Specification, Version 1.10
// [2] Extensible Firmware Interface Specification, Version 1.10 Specification Update
// Both can be downloaded from here: http://www.intel.com/technology/efi/main_specification.htm

struct gpt_header {
    char     signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    GUID     disk_guid;
    uint64_t partition_entry_lba;
    uint32_t partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_crc32;
} __attribute__((packed)); // odd number of 32bit items so 64bit machines might make this too big

#define PARTITION_REVISION 0x00010000 // [2] 11-9.1

struct gpt_partition {
    GUID partition_type;
    GUID partition_guid;
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];
};

// gpt_partition.attribute
#define PA_SYSTEM_PARTITION (1LL <<  0)
// GUID specific
#define PA_READ_ONLY        (1LL << 60)
#define PA_HIDDEN           (1LL << 62)
#define PA_NO_AUTOMOUNT     (1LL << 63)

#include "endian.h"
static inline void gpt_header_to_host(struct gpt_header *h) {
    h->revision             = from_le32(h->revision);
    h->header_size          = from_le32(h->header_size);
    h->header_crc32         = from_le32(h->header_crc32);
    h->reserved             = from_le32(h->reserved);
    h->my_lba               = from_le64(h->my_lba);
    h->alternate_lba        = from_le64(h->alternate_lba);
    h->first_usable_lba     = from_le64(h->first_usable_lba);
    h->last_usable_lba      = from_le64(h->last_usable_lba);
    h->partition_entry_lba  = from_le64(h->partition_entry_lba);
    h->partition_entries    = from_le32(h->partition_entries);
    h->partition_entry_size = from_le32(h->partition_entry_size);
    h->partition_crc32      = from_le32(h->partition_crc32);
}

static inline void gpt_partition_to_host(struct gpt_partition *partition, int entries) {
    for (int p=0; p<entries; p++) {
        partition[p].first_lba = from_le64(partition[p].first_lba);
        partition[p].last_lba = from_le64(partition[p].last_lba);
        partition[p].attributes = from_le64(partition[p].attributes);
        for (int i=0; i<lengthof(partition[p].name); i++)
            partition[p].name[i] = from_le16(partition[p].name[i]);
    }
}

// No use in repeating the whole thing when it's going to turn out exactly the same.
#define gpt_partition_from_host gpt_partition_to_host
#define gpt_header_from_host    gpt_header_to_host

#endif /* __GPT_H__ */

