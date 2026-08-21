// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_EXTERNUM_H
#define MMGR_CONFINIUM_EXTERNUM_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_PSRAM_POOL

MMGR_BEGIN_DECLS

/**
 * @file confinium_externum.h
 * @brief Where a request should be placed when there is more than one kind of memory.
 *
 * Built only when MMGR_ENABLE_PSRAM_POOL is set.
 */

/** @brief Where a request landed. */
typedef enum MMGR_ENUM_PACKED
{
    PLACE_DRAM = 0,
    PLACE_PSRAM = 1,
    PLACE_FAIL = 2
} mmgr_place;

/**
 * @brief Decide where a request goes.
 * @param size Byte count.
 * @param dma_required Whether it must be DMA capable, which rules out PSRAM.
 * @param free_dram Bytes free in DRAM.
 * @param free_psram Bytes free in PSRAM.
 * @param psram_threshold At or above this, prefer PSRAM.
 * @param dram_reserve Keep this much DRAM free.
 * @return PLACE_DRAM, PLACE_PSRAM or PLACE_FAIL.
 */
mmgr_place mmgr_exter_place(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                            size_t psram_threshold, size_t dram_reserve);

/** @brief Two buffers, one filling and one draining. */
typedef struct
{
    uint8_t fill_idx;
} PingPong;

/**
 * @brief Start with buffer 0 filling.
 * @param pp State.
 */
void mmgr_pingpong_init(PingPong *pp);

/**
 * @brief Which buffer is filling.
 * @param pp State.
 * @return 0 or 1.
 */
uint8_t mmgr_pingpong_fill_index(const PingPong *pp);

/**
 * @brief Which buffer is draining.
 * @param pp State.
 * @return 0 or 1.
 */
uint8_t mmgr_pingpong_drain_index(const PingPong *pp);

/**
 * @brief Swap the two.
 * @param pp State.
 * @return The new fill index.
 */
uint8_t mmgr_pingpong_swap(PingPong *pp);

MMGR_END_DECLS

#endif

#endif
