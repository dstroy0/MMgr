// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ENDIAN_H
#define MMGR_ENDIAN_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

typedef struct
{
    size_t (*wr16le)(uint8_t *p, uint16_t v);
    size_t (*wr32le)(uint8_t *p, uint32_t v);
    size_t (*wr64le)(uint8_t *p, uint64_t v);
    uint16_t (*rd16le)(const uint8_t *p);
    uint32_t (*rd32le)(const uint8_t *p);
    uint64_t (*rd64le)(const uint8_t *p);
    size_t (*wr16be)(uint8_t *p, uint16_t v);
    size_t (*wr32be)(uint8_t *p, uint32_t v);
    size_t (*wr64be)(uint8_t *p, uint64_t v);
    uint16_t (*rd16be)(const uint8_t *p);
    uint32_t (*rd32be)(const uint8_t *p);
    uint64_t (*rd64be)(const uint8_t *p);
} EndianNs;

size_t mmgr_wr16le(uint8_t *p, uint16_t v);
size_t mmgr_wr32le(uint8_t *p, uint32_t v);
size_t mmgr_wr64le(uint8_t *p, uint64_t v);
uint16_t mmgr_rd16le(const uint8_t *p);
uint32_t mmgr_rd32le(const uint8_t *p);
uint64_t mmgr_rd64le(const uint8_t *p);
size_t mmgr_wr16be(uint8_t *p, uint16_t v);
size_t mmgr_wr32be(uint8_t *p, uint32_t v);
size_t mmgr_wr64be(uint8_t *p, uint64_t v);
uint16_t mmgr_rd16be(const uint8_t *p);
uint32_t mmgr_rd32be(const uint8_t *p);
uint64_t mmgr_rd64be(const uint8_t *p);

static const EndianNs endian __attribute__((unused)) = {.wr16le = mmgr_wr16le,
                                                        .wr32le = mmgr_wr32le,
                                                        .wr64le = mmgr_wr64le,
                                                        .rd16le = mmgr_rd16le,
                                                        .rd32le = mmgr_rd32le,
                                                        .rd64le = mmgr_rd64le,
                                                        .wr16be = mmgr_wr16be,
                                                        .wr32be = mmgr_wr32be,
                                                        .wr64be = mmgr_wr64be,
                                                        .rd16be = mmgr_rd16be,
                                                        .rd32be = mmgr_rd32be,
                                                        .rd64be = mmgr_rd64be};

MMGR_END_DECLS

#endif
