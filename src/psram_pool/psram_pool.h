// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_PSRAM_POOL_H
#define PROTOCORE_PSRAM_POOL_H

#include "protocore_config.h"

#if PROTOCORE_ENABLE_PSRAM_POOL

PROTOCORE_BEGIN_DECLS

typedef enum PROTO_ENUM_PACKED
{
    PLACE_DRAM = 0,
    PLACE_PSRAM = 1,
    PLACE_FAIL = 2
} protocore_place;

protocore_place protocore_psram_place(size_t size, proto_bool dma_required, size_t free_dram, size_t free_psram,
                                      size_t psram_threshold, size_t dram_reserve);

typedef struct
{
    uint8_t fill_idx;
} PingPong;

void protocore_pingpong_init(PingPong *pp);

uint8_t protocore_pingpong_fill_index(const PingPong *pp);

uint8_t protocore_pingpong_drain_index(const PingPong *pp);

uint8_t protocore_pingpong_swap(PingPong *pp);

PROTOCORE_END_DECLS

#endif

#endif
