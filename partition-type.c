//  Copyright (c) 2009 David Caldwell,  All Rights Reserved.

#include "guid.h"
#include "partition-type.h"

// This list came from <http://en.wikipedia.org/wiki/GUID_Partition_Table>. Hardly definitive, but convenient.
// If you know where these are originally defined, please document them.
// [1] http://developer.apple.com/technotes/tn2006/tn2166.html

GUID gpt_partition_type_empty = STATIC_GUID(00000000,0000,0000,0000,000000000000);

struct gpt_partition_type gpt_partition_type[] = {
    { "Unused entry",                             STATIC_GUID(00000000,0000,0000,0000,000000000000) },
    { "MBR partition scheme",                     STATIC_GUID(024DEE41,33E7,11D3,9D69,0008C781F39F) },
    { "EFI System Partition",                     STATIC_GUID(C12A7328,F81F,11D2,BA4B,00A0C93EC93B), { 0xef } },
    { "BIOS Boot Partition",                      STATIC_GUID(21686148,6449,6E6F,744E,656564454649) },

    { "Windows/Reserved",                         STATIC_GUID(E3C9E316,0B5C,4DB8,817D,F92DF00215AE) },
    { "Windows/Basic Data",                       STATIC_GUID(EBD0A0A2,B9E5,4433,87C0,68B6B72699C7), { 0x0c, 0x07 } },
    { "Windows/Logical Disk Manager metadata",    STATIC_GUID(5808C8AA,7E8F,42E0,85D2,E1E90434CFB3) },
    { "Windows/Logical Disk Manager data",        STATIC_GUID(AF9B60A0,1431,4F62,BC68,3311714A69AD) },

    { "HP-UX/Data",                               STATIC_GUID(75894C1E,3AEB,11D3,B7C1,7B03A0000000) },
    { "HP-UX/Service",                            STATIC_GUID(E2A1E728,32E3,11D6,A682,7B03A0000000) },

    { "Linux/Data",                               STATIC_GUID(EBD0A0A2,B9E5,4433,87C0,68B6B72699C7), { 0x83 } },
    { "Linux/RAID",                               STATIC_GUID(A19D880F,05FC,4D3B,A006,743F0F84911E), { 0xfd } },
    { "Linux/Swap",                               STATIC_GUID(0657FD6D,A4AB,43C4,84E5,0933C84B4F4F), { 0x82 } },
    { "Linux/LVM",                                STATIC_GUID(E6D6D379,F507,44C2,A23C,238F2A3DF928), { 0x8e } },
    { "Linux/Reserved",                           STATIC_GUID(8DA63339,0007,60C0,C436,083AC8230908) },

    { "FreeBSD/Boot",                             STATIC_GUID(83BD6B9D,7F41,11DC,BE0B,001560B84F0F) },
    { "FreeBSD/Data",                             STATIC_GUID(516E7CB4,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/Swap",                             STATIC_GUID(516E7CB5,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/UFS",                              STATIC_GUID(516E7CB6,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/Vinum volume manager",             STATIC_GUID(516E7CB8,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/ZFS",                              STATIC_GUID(516E7CBA,6ECF,11D6,8FF8,00022D09712B) },

    { "Mac OS X/HFS+",                            STATIC_GUID(48465300,0000,11AA,AA11,00306543ECAC), { 0xaf } }, // [1]
    { "Mac OS X/Apple UFS",                       STATIC_GUID(55465300,0000,11AA,AA11,00306543ECAC), { 0xa8 } }, // [1]
    { "Mac OS X/ZFS",                             STATIC_GUID(6A898CC3,1DD2,11B2,99A6,080020736631) },
    { "Mac OS X/RAID",                            STATIC_GUID(52414944,0000,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Offline RAID",                    STATIC_GUID(52414944,5F4F,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Boot",                            STATIC_GUID(426F6F74,0000,11AA,AA11,00306543ECAC), { 0xab } }, // [1]
    { "Mac OS X/Label",                           STATIC_GUID(4C616265,6C00,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Apple TV Recovery",               STATIC_GUID(5265636F,7665,11AA,AA11,00306543ECAC) },

    { "Solaris/Boot",                             STATIC_GUID(6A82CB45,1DD2,11B2,99A6,080020736631), { 0xbe } },
    { "Solaris/Root",                             STATIC_GUID(6A85CF4D,1DD2,11B2,99A6,080020736631), { 0xbf } },
    { "Solaris/Swap",                             STATIC_GUID(6A87C46F,1DD2,11B2,99A6,080020736631), { 0x82 } },
    { "Solaris/Backup",                           STATIC_GUID(6A8B642B,1DD2,11B2,99A6,080020736631) },
    { "Solaris/usr",                              STATIC_GUID(6A898CC3,1DD2,11B2,99A6,080020736631), { 0xbf }  },
    { "Solaris/var",                              STATIC_GUID(6A8EF2E9,1DD2,11B2,99A6,080020736631), { 0xbf }  },
    { "Solaris/home",                             STATIC_GUID(6A90BA39,1DD2,11B2,99A6,080020736631), { 0xbf }  },
    { "Solaris/EFI_ALTSCTR",                      STATIC_GUID(6A9283A5,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Reserved",                         STATIC_GUID(6A945A3B,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Reserved",                         STATIC_GUID(6A9630D1,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Reserved",                         STATIC_GUID(6A980767,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Reserved",                         STATIC_GUID(6A96237F,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Reserved",                         STATIC_GUID(6A8D2AC7,1DD2,11B2,99A6,080020736631) },

    { "NetBSD/Swap",                              STATIC_GUID(49F48D32,B10E,11DC,B99B,0019D1879648) },
    { "NetBSD/FFS",                               STATIC_GUID(49F48D5A,B10E,11DC,B99B,0019D1879648) },
    { "NetBSD/LFS",                               STATIC_GUID(49F48D82,B10E,11DC,B99B,0019D1879648) },
    { "NetBSD/RAID",                              STATIC_GUID(49F48DAA,B10E,11DC,B99B,0019D1879648) },
    { "NetBSD/concatenated",                      STATIC_GUID(2DB519C4,B10F,11DC,B99B,0019D1879648) },
    { "NetBSD/encrypted",                         STATIC_GUID(2DB519EC,B10F,11DC,B99B,0019D1879648) },
    {}
};

// This was brazenly stolen from util-linux's fdisk/i386_sys_types.c:
char *mbr_partition_type[256] = {
    [0x00] = "Empty",
    [0x01] = "FAT12",
    [0x02] = "XENIX root",
    [0x03] = "XENIX usr",
    [0x04] = "FAT16 <32M",
    [0x05] = "Extended",              /* DOS 3.3+ extended partition */
    [0x06] = "FAT16",                 /* DOS 16-bit >=32M */
    [0x07] = "HPFS/NTFS",             /* OS/2 IFS, eg, HPFS or NTFS or QNX */
    [0x08] = "AIX",                   /* AIX boot (AIX -- PS/2 port) or SplitDrive */
    [0x09] = "AIX bootable",          /* AIX data or Coherent */
    [0x0a] = "OS/2 Boot Manager",     /* OS/2 Boot Manager */
    [0x0b] = "W95 FAT32",
    [0x0c] = "W95 FAT32 (LBA)",       /* LBA really is `Extended Int 13h' */
    [0x0e] = "W95 FAT16 (LBA)",
    [0x0f] = "W95 Ext'd (LBA)",
    [0x10] = "OPUS",
    [0x11] = "Hidden FAT12",
    [0x12] = "Compaq diagnostics",
    [0x14] = "Hidden FAT16 <32M",
    [0x16] = "Hidden FAT16",
    [0x17] = "Hidden HPFS/NTFS",
    [0x18] = "AST SmartSleep",
    [0x1b] = "Hidden W95 FAT32",
    [0x1c] = "Hidden W95 FAT32 (LBA)",
    [0x1e] = "Hidden W95 FAT16 (LBA)",
    [0x24] = "NEC DOS",
    [0x39] = "Plan 9",
    [0x3c] = "PartitionMagic recovery",
    [0x40] = "Venix 80286",
    [0x41] = "PPC PReP Boot",
    [0x42] = "SFS",
    [0x4d] = "QNX4.x",
    [0x4e] = "QNX4.x 2nd part",
    [0x4f] = "QNX4.x 3rd part",
    [0x50] = "OnTrack DM",
    [0x51] = "OnTrack DM6 Aux1",      /* (or Novell) */
    [0x52] = "CP/M",                  /* CP/M or Microport SysV/AT */
    [0x53] = "OnTrack DM6 Aux3",
    [0x54] = "OnTrackDM6",
    [0x55] = "EZ-Drive",
    [0x56] = "Golden Bow",
    [0x5c] = "Priam Edisk",
    [0x61] = "SpeedStor",
    [0x63] = "GNU HURD or SysV",      /* GNU HURD or Mach or Sys V/386 (such as ISC UNIX) */
    [0x64] = "Novell Netware 286",
    [0x65] = "Novell Netware 386",
    [0x70] = "DiskSecure Multi-Boot",
    [0x75] = "PC/IX",
    [0x80] = "Old Minix",             /* Minix 1.4a and earlier */
    [0x81] = "Minix / old Linux",     /* Minix 1.4b and later */
    [0x82] = "Linux swap / Solaris",
    [0x83] = "Linux",
    [0x84] = "OS/2 hidden C: drive",
    [0x85] = "Linux extended",
    [0x86] = "NTFS volume set",
    [0x87] = "NTFS volume set",
    [0x88] = "Linux plaintext",
    [0x8e] = "Linux LVM",
    [0x93] = "Amoeba",
    [0x94] = "Amoeba BBT",            /* (bad block table) */
    [0x9f] = "BSD/OS",                /* BSDI */
    [0xa0] = "IBM Thinkpad hibernation",
    [0xa5] = "FreeBSD",               /* various BSD flavours */
    [0xa6] = "OpenBSD",
    [0xa7] = "NeXTSTEP",
    [0xa8] = "Darwin UFS",
    [0xa9] = "NetBSD",
    [0xab] = "Darwin boot",
    [0xaf] = "Mac HFS/HFS+",
    [0xb7] = "BSDI fs",
    [0xb8] = "BSDI swap",
    [0xbb] = "Boot Wizard hidden",
    [0xbe] = "Solaris boot",
    [0xbf] = "Solaris",
    [0xc1] = "DRDOS/sec (FAT-12)",
    [0xc4] = "DRDOS/sec (FAT-16 < 32M)",
    [0xc6] = "DRDOS/sec (FAT-16)",
    [0xc7] = "Syrinx",
    [0xda] = "Non-FS data",
    [0xdb] = "CP/M / CTOS / ...",     /* CP/M or Concurrent CP/M or Concurrent DOS or CTOS */
    [0xde] = "Dell Utility",          /* Dell PowerEdge Server utilities */
    [0xdf] = "BootIt",                /* BootIt EMBRM */
    [0xe1] = "DOS access",            /* DOS access or SpeedStor 12-bit FAT extended partition */
    [0xe3] = "DOS R/O",               /* DOS R/O or SpeedStor */
    [0xe4] = "SpeedStor",             /* SpeedStor 16-bit FAT extended partition < 1024 cyl. */
    [0xeb] = "BeOS fs",
    [0xee] = "EFI GPT",               /* Intel EFI GUID Partition Table */
    [0xef] = "EFI (FAT-12/16/32)",    /* Intel EFI System Partition */
    [0xf0] = "Linux/PA-RISC boot",    /* Linux/PA-RISC boot loader */
    [0xf1] = "SpeedStor",
    [0xf4] = "SpeedStor",             /* SpeedStor large partition */
    [0xf2] = "DOS secondary",         /* DOS 3.3+ secondary */
    [0xfd] = "Linux raid autodetect", /* New (2.2.x) raid partition with autodetect using persistent superblock */
    [0xfe] = "LANstep",               /* SpeedStor >1024 cyl. or LANstep */
    [0xff] = "BBT",                   /* Xenix Bad Block Table */
};

int find_mbr_equivalent(GUID g)
{
    for (int i=0; gpt_partition_type[i].name; i++)
        if (guid_eq(g, gpt_partition_type[i].guid))
            return gpt_partition_type[i].mbr_equivalent[0];
    return 0;
}
