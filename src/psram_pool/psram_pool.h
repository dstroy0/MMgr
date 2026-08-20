// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_PSRAM_POOL_H
#define MMGR_PSRAM_POOL_H

#include "mmgr_config.h"

#if MMGR_ENABLE_PSRAM_POOL

MMGR_BEGIN_DECLS

typedef enum MMGR_ENUM_PACKED
{
    PLACE_DRAM = 0,
    PLACE_PSRAM = 1,
    PLACE_FAIL = 2
} mmgr_place;

mmgr_place mmgr_psram_place(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                            size_t psram_threshold, size_t dram_reserve);

typedef struct
{
    uint8_t fill_idx;
} PingPong;

void mmgr_pingpong_init(PingPong *pp);

uint8_t mmgr_pingpong_fill_index(const PingPong *pp);

uint8_t mmgr_pingpong_drain_index(const PingPong *pp);

uint8_t mmgr_pingpong_swap(PingPong *pp);

MMGR_END_DECLS

#endif

#endif
