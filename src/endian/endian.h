#ifndef MMGR_ENDIAN_H
#define MMGR_ENDIAN_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENDIAN_16 = 2,
    MMGR_ENDIAN_32 = 4,
    MMGR_ENDIAN_64 = 8,
} mmgr_endian_width;

typedef struct
{
    uint8_t *const dst;
    const uint8_t *const src;
    const uint64_t val;
    const mmgr_endian_width width;
} EndianCfg;

typedef struct
{
    size_t (*wr)(const EndianCfg *c);
    uint64_t (*rd)(const EndianCfg *c);
    uint64_t (*rev)(const EndianCfg *c);
} EndianNs;
MMGR_NS_LAYOUT(EndianNs, wr, rd, rev);

size_t mmgr_wr_le(const EndianCfg *c);
uint64_t mmgr_rd_le(const EndianCfg *c);
size_t mmgr_wr_be(const EndianCfg *c);
uint64_t mmgr_rd_be(const EndianCfg *c);
uint64_t mmgr_endian_rev(const EndianCfg *c);

MMGR_NS EndianNs parva_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_le,
    .rd = mmgr_rd_le,
    .rev = mmgr_endian_rev,
};

MMGR_NS EndianNs magna_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_be,
    .rd = mmgr_rd_be,
    .rev = mmgr_endian_rev,
};

MMGR_FINIS_DECLS

#endif
