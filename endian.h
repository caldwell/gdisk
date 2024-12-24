// Copyright © 2009-2024 David Caldwell <david@porkrind.org>
#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#if defined(__APPLE__)
# include <machine/endian.h>
#elif defined(__linux__)
# include <endian.h>
#else
# warning "Unknown compilation host os"
#endif

#ifndef BYTE_ORDER
# error "Couldn't determine machine/os endianess!"
#endif

#if BYTE_ORDER == BIG_ENDIAN
#  define to_le16(x)    __swap16(x)
#  define to_le32(x)    __swap32(x)
#  define to_le64(x)    __swap64(x)
#  define from_le16(x)  __swap16(x)
#  define from_le32(x)  __swap32(x)
#  define from_le64(x)  __swap64(x)
#  define to_be16(x)    (x)
#  define to_be32(x)    (x)
#  define to_be64(x)    (x)
#  define from_be16(x)  (x)
#  define from_be32(x)  (x)
#  define from_be64(x)  (x)
#else
#  define to_le16(x)    (x)
#  define to_le32(x)    (x)
#  define to_le64(x)    (x)
#  define from_le16(x)  (x)
#  define from_le32(x)  (x)
#  define from_le64(x)  (x)
#  define to_be16(x)    __swap16(x)
#  define to_be32(x)    __swap32(x)
#  define to_be64(x)    __swap64(x)
#  define from_be16(x)  __swap16(x)
#  define from_be32(x)  __swap32(x)
#  define from_be64(x)  __swap64(x)
#endif

// Don't use these directly. The above macros are better for self-documentation
#define __swap16(x) ((x) <<  8 & 0xff00 | \
                     (x) >>  8 & 0x00ff)
#define __swap32(x) ((x) << 24 & 0xff000000 | \
                     (x) <<  8 & 0x00ff0000 | \
                     (x) >>  8 & 0x0000ff00 | \
                     (x) >> 24 & 0x000000ff)

#define __swap64(x) ((x) << 56 & 0xff00000000000000LL | \
                     (x) << 40 & 0x00ff000000000000LL | \
                     (x) << 24 & 0x0000ff0000000000LL | \
                     (x) <<  8 & 0x000000ff00000000LL | \
                     (x) >>  8 & 0x00000000ff000000LL | \
                     (x) >> 24 & 0x0000000000ff0000LL | \
                     (x) >> 40 & 0x000000000000ff00LL | \
                     (x) >> 56 & 0x00000000000000ffLL)

#endif /* __ENDIAN_H__ */

