// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_ENDIAN_H
#define PROTOCORE_ENDIAN_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

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

size_t protocore_wr16le(uint8_t *p, uint16_t v);
size_t protocore_wr32le(uint8_t *p, uint32_t v);
size_t protocore_wr64le(uint8_t *p, uint64_t v);
uint16_t protocore_rd16le(const uint8_t *p);
uint32_t protocore_rd32le(const uint8_t *p);
uint64_t protocore_rd64le(const uint8_t *p);
size_t protocore_wr16be(uint8_t *p, uint16_t v);
size_t protocore_wr32be(uint8_t *p, uint32_t v);
size_t protocore_wr64be(uint8_t *p, uint64_t v);
uint16_t protocore_rd16be(const uint8_t *p);
uint32_t protocore_rd32be(const uint8_t *p);
uint64_t protocore_rd64be(const uint8_t *p);

static const EndianNs endian __attribute__((unused)) = {.wr16le = protocore_wr16le,
                                                        .wr32le = protocore_wr32le,
                                                        .wr64le = protocore_wr64le,
                                                        .rd16le = protocore_rd16le,
                                                        .rd32le = protocore_rd32le,
                                                        .rd64le = protocore_rd64le,
                                                        .wr16be = protocore_wr16be,
                                                        .wr32be = protocore_wr32be,
                                                        .wr64be = protocore_wr64be,
                                                        .rd16be = protocore_rd16be,
                                                        .rd32be = protocore_rd32be,
                                                        .rd64be = protocore_rd64be};

PROTOCORE_END_DECLS

#endif
