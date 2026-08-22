// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFINIUM_EXTERNUM_H
#define MMGR_CONFINIUM_EXTERNUM_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_EXTRAM

MMGR_INCIPE_DECLS

/**
 * @file confinium_externum.h
 * @brief Where a request should be placed when there is more than one kind of memory.
 *
 * Built only when MMGR_ENABLE_EXTRAM is set.
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

/**
 * @brief Two buffers, one filling and one draining.
 *
 * Public, and in the header, because the caller is what builds it and what carries it from one
 * change of ends to the next. It is the config the four pingpong entries take, and it is also all
 * the state there is: one index, and the other buffer is the one it is not. There is nothing for an
 * internal context to hold that is not already here, and copying a byte in to copy it back out is
 * work to arrive at what was passed in.
 */
typedef struct
{
    uint8_t fill_idx; /**< Which of the two is filling. The other is draining. */
} PingPong;

/**
 * @brief What a placement decision is given.
 *
 * Public, and in the header, because the caller is what builds it. Every member is const: nothing
 * writes to a config once the caller has built it, and the compound literal is gone before there is
 * code that could.
 *
 * Not the module's context. ExterCtx wears the same six numbers and is file local in the .c.
 */
typedef struct
{
    const size_t size;            /**< Bytes wanted. */
    const mmgr_bool dma_required; /**< The bytes must be reachable by DMA. */
    const size_t free_dram;       /**< Bytes free in DRAM. */
    const size_t free_psram;      /**< Bytes free in PSRAM. */
    const size_t psram_threshold; /**< At or above this, prefer PSRAM. */
    const size_t dram_reserve;    /**< Bytes of DRAM to leave alone. */
} ExternumCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_place (*place)(const ExternumCfg *c);
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
mmgr_place mmgr_exter_place(const ExternumCfg *c);
void mmgr_pingpong_init(PingPong *const pp);
uint8_t mmgr_pingpong_fill_index(PingPong *const pp);
uint8_t mmgr_pingpong_drain_index(PingPong *const pp);
uint8_t mmgr_pingpong_swap(PingPong *const pp);
/** @} */

/** @name The types this module takes, settled where the call is written.
 *  @{ */
#define MMGR_EXTER_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0, int: 0, unsigned: 0, long: 0, unsigned long: 0))
#define MMGR_EXTER_IS_BOOL(x_) ((void)_Generic((x_), mmgr_bool: 0, int: 0, unsigned: 0))
#define MMGR_EXTER_IS_PAIR(x_) ((void)_Generic((x_), PingPong: 0))
/** @} */

/**
 * @brief Where @p size_ bytes should come from.
 *
 * Positional in, so the struct and the designators never reach a call site, and the type of every
 * argument is settled where the call is written.
 */
#define mmgr_exter_place(size_, dma_, free_dram_, free_psram_, threshold_, reserve_)                                   \
    (MMGR_EXTER_IS_SIZE(size_), MMGR_EXTER_IS_BOOL(dma_), MMGR_EXTER_IS_SIZE(free_dram_),                              \
     MMGR_EXTER_IS_SIZE(free_psram_), MMGR_EXTER_IS_SIZE(threshold_), MMGR_EXTER_IS_SIZE(reserve_),                    \
     exter.place(&(ExternumCfg){.size = (size_),                                                                       \
                                .dma_required = (dma_),                                                                \
                                .free_dram = (free_dram_),                                                             \
                                .free_psram = (free_psram_),                                                           \
                                .psram_threshold = (threshold_),                                                       \
                                .dram_reserve = (reserve_)}))

/** @name The pair, addressed by the macro so a call site never writes the ampersand.
 *  @{ */
#define mmgr_pingpong_init(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_init(&(pp_)))
#define mmgr_pingpong_fill_index(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_fill(&(pp_)))
#define mmgr_pingpong_drain_index(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_drain(&(pp_)))
#define mmgr_pingpong_swap(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_swap(&(pp_)))
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
