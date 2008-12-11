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
    uint32_t partition_crc;
};

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

#endif /* __GPT_H__ */

