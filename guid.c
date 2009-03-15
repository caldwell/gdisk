//  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

#include "guid.h"

// This list came from <http://en.wikipedia.org/wiki/GUID_Partition_Table>. Hardly definitive, but convenient.
// If you know where these are originally defined, please document them.
// [1] http://developer.apple.com/technotes/tn2006/tn2166.html

GUID gpt_partition_type_empty = STATIC_GUID(00000000,0000,0000,0000,000000000000);

struct gpt_partition_type gpt_partition_type[] = {
    { "Unused entry",                             STATIC_GUID(00000000,0000,0000,0000,000000000000) },
    { "MBR partition scheme",                     STATIC_GUID(024DEE41,33E7,11D3,9D69,0008C781F39F) },
    { "EFI System Partition",                     STATIC_GUID(C12A7328,F81F,11D2,BA4B,00A0C93EC93B) },
    { "BIOS Boot Partition",                      STATIC_GUID(21686148,6449,6E6F,744E,656564454649) },

    { "Windows/Reserved",                         STATIC_GUID(E3C9E316,0B5C,4DB8,817D,F92DF00215AE) },
    { "Windows/Basic Data",                       STATIC_GUID(EBD0A0A2,B9E5,4433,87C0,68B6B72699C7) },
    { "Windows/Logical Disk Manager metadata",    STATIC_GUID(5808C8AA,7E8F,42E0,85D2,E1E90434CFB3) },
    { "Windows/Logical Disk Manager data",        STATIC_GUID(AF9B60A0,1431,4F62,BC68,3311714A69AD) },

    { "HP-UX/Data",                               STATIC_GUID(75894C1E,3AEB,11D3,B7C1,7B03A0000000) },
    { "HP-UX/Service",                            STATIC_GUID(E2A1E728,32E3,11D6,A682,7B03A0000000) },

    { "Linux/Data",                               STATIC_GUID(EBD0A0A2,B9E5,4433,87C0,68B6B72699C7) },
    { "Linux/RAID",                               STATIC_GUID(A19D880F,05FC,4D3B,A006,743F0F84911E) },
    { "Linux/Swap",                               STATIC_GUID(0657FD6D,A4AB,43C4,84E5,0933C84B4F4F) },
    { "Linux/LVM",                                STATIC_GUID(E6D6D379,F507,44C2,A23C,238F2A3DF928) },
    { "Linux/Reserved",                           STATIC_GUID(8DA63339,0007,60C0,C436,083AC8230908) },

    { "FreeBSD/Boot",                             STATIC_GUID(83BD6B9D,7F41,11DC,BE0B,001560B84F0F) },
    { "FreeBSD/Data",                             STATIC_GUID(516E7CB4,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/Swap",                             STATIC_GUID(516E7CB5,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/UFS",                              STATIC_GUID(516E7CB6,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/Vinum volume manager",             STATIC_GUID(516E7CB8,6ECF,11D6,8FF8,00022D09712B) },
    { "FreeBSD/ZFS",                              STATIC_GUID(516E7CBA,6ECF,11D6,8FF8,00022D09712B) },

    { "Mac OS X/HFS+",                            STATIC_GUID(48465300,0000,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Apple UFS",                       STATIC_GUID(55465300,0000,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/ZFS",                             STATIC_GUID(6A898CC3,1DD2,11B2,99A6,080020736631) },
    { "Mac OS X/RAID",                            STATIC_GUID(52414944,0000,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Offline RAID",                    STATIC_GUID(52414944,5F4F,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Boot",                            STATIC_GUID(426F6F74,0000,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Label",                           STATIC_GUID(4C616265,6C00,11AA,AA11,00306543ECAC) }, // [1]
    { "Mac OS X/Apple TV Recovery",               STATIC_GUID(5265636F,7665,11AA,AA11,00306543ECAC) },

    { "Solaris/Boot",                             STATIC_GUID(6A82CB45,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Root",                             STATIC_GUID(6A85CF4D,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Swap",                             STATIC_GUID(6A87C46F,1DD2,11B2,99A6,080020736631) },
    { "Solaris/Backup",                           STATIC_GUID(6A8B642B,1DD2,11B2,99A6,080020736631) },
    { "Solaris/usr",                              STATIC_GUID(6A898CC3,1DD2,11B2,99A6,080020736631) },
    { "Solaris/var",                              STATIC_GUID(6A8EF2E9,1DD2,11B2,99A6,080020736631) },
    { "Solaris/home",                             STATIC_GUID(6A90BA39,1DD2,11B2,99A6,080020736631) },
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

#include <stdio.h>
char *guid_str(GUID g)
{
    // Obviously not thread safe, but convenient. Don't use 2 in the same print. :-)
    static char str[sizeof(GUID)*2+4+1]; // four '-'s and a null
    snprintf(str, sizeof(str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             g.byte[3], g.byte[2], g.byte[1], g.byte[0],
             g.byte[5], g.byte[4],
             g.byte[7], g.byte[6],
             g.byte[8], g.byte[9],
             g.byte[10], g.byte[11], g.byte[12], g.byte[13], g.byte[14], g.byte[15]);
    return str;
}
