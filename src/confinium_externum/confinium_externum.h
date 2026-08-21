// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_EXTERNUM_H
#define MMGR_CONFINIUM_EXTERNUM_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_PSRAM_POOL

MMGR_INCIPE_DECLS

/**
 * @file confinium_externum.h
 * @brief Where a request should be placed when there is more than one kind of memory.
 *
 * Built only when MMGR_ENABLE_PSRAM_POOL is set.
 *
 * The table is the whole surface. There are no free functions to call.
 *
 * The placement decision and the ping-pong pair are one table because they are one module: a
 * request is placed and then filled, and the two questions belong to the same caller. Neither is
 * large enough to be its own namespace.
 */

/** @brief Where a request landed. */
typedef enum MMGR_ENUM_PACKED
{
    PLACE_DRAM = 0,
    PLACE_PSRAM = 1,
    PLACE_FAIL = 2
} mmgr_place;

/** @brief Two buffers, one filling and one draining. */
typedef struct
{
    uint8_t fill_idx; /**< Which of the two is filling. The other is draining. */
} PingPong;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_place (*place)(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                        size_t psram_threshold, size_t dram_reserve);
    void (*pingpong_init)(PingPong *const pp);
    uint8_t (*pingpong_fill)(PingPong *const pp);
    uint8_t (*pingpong_drain)(PingPong *const pp);
    uint8_t (*pingpong_swap)(PingPong *const pp);
} ConfiniumExternumNs;
MMGR_NS_LAYOUT(ConfiniumExternumNs, place, pingpong_init, pingpong_fill, pingpong_drain, pingpong_swap);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
mmgr_place mmgr_exter_place(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                            size_t psram_threshold, size_t dram_reserve);
void mmgr_pingpong_init(PingPong *const pp);
uint8_t mmgr_pingpong_fill_index(PingPong *const pp);
uint8_t mmgr_pingpong_drain_index(PingPong *const pp);
uint8_t mmgr_pingpong_swap(PingPong *const pp);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS ConfiniumExternumNs exter MMGR_UNUSED = {
    .place = mmgr_exter_place,
    .pingpong_init = mmgr_pingpong_init,
    .pingpong_fill = mmgr_pingpong_fill_index,
    .pingpong_drain = mmgr_pingpong_drain_index,
    .pingpong_swap = mmgr_pingpong_swap,
};

MMGR_FINIS_DECLS

#endif

#endif
