// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "confinium_externum/confinium_externum.h"

/**
 * @file confinium_externum.c
 * @brief Placement across DRAM and PSRAM, and the ping-pong buffer pair.
 *
 * The placement decision takes one parameter, a pointer to ExterCtx: it is one question about
 * six numbers, so the six are one context. The ping pong entries take the pair and nothing
 * else, so they take the pair.
 */

#if MMGR_ENABLE_EXTRAM

/** @brief A placement decision. */
typedef struct
{
    size_t size;             /**< Bytes wanted. */
    mmgr_bool dma_required;  /**< The bytes must be reachable by DMA. */
    size_t free_dram;        /**< Bytes free in DRAM. */
    size_t free_psram;       /**< Bytes free in PSRAM. */
    size_t psram_threshold;  /**< At or above this, prefer PSRAM. */
    size_t dram_reserve;     /**< Bytes of DRAM to leave alone. */
} ExterCtx;

/**
 * @brief Would this request still leave the DRAM reserve intact.
 * @param c The decision.
 * @return MMGR_TRUE if it fits.
 */
MMGR_INLINE mmgr_bool exter_dram_fits(const ExterCtx *c)
{
    return (c->size <= c->free_dram) && ((c->free_dram - c->size) >= c->dram_reserve);
}

/**
 * @brief Does it fit in PSRAM at all.
 * @param c The decision.
 * @return MMGR_TRUE if it fits.
 */
MMGR_INLINE mmgr_bool exter_psram_fits(const ExterCtx *c)
{
    return c->size <= c->free_psram;
}

/**
 * @brief Where the bytes should come from.
 * @param c The decision.
 * @return The placement.
 */
MMGR_INLINE mmgr_place exter_place(const ExterCtx *c)
{
    if (c->size == 0)
    {
        return PLACE_FAIL;
    }

    const mmgr_bool d_fits = exter_dram_fits(c);
    const mmgr_bool p_fits = exter_psram_fits(c);

    if (c->dma_required)
    {
        return d_fits ? PLACE_DRAM : PLACE_FAIL;
    }

    if (c->size >= c->psram_threshold)
    {
        if (p_fits)
        {
            return PLACE_PSRAM;
        }
        if (d_fits)
        {
            return PLACE_DRAM;
        }
        return PLACE_FAIL;
    }

    if (d_fits)
    {
        return PLACE_DRAM;
    }
    if (p_fits)
    {
        return PLACE_PSRAM;
    }
    return PLACE_FAIL;
}

/**
 * @brief Start the pair on its first buffer.
 * @param c In/out. The pair.
 */
MMGR_INLINE void exter_pingpong_init(PingPong *const pp)
{
    if (pp != NULL)
    {
        pp->fill_idx = 0;
    }
}

/**
 * @brief Which buffer is being filled.
 * @param c The pair.
 * @return Its index.
 */
MMGR_INLINE uint8_t exter_pingpong_fill(PingPong *const pp)
{
    return (pp != NULL) ? pp->fill_idx : 0u;
}

/**
 * @brief Which buffer is being drained.
 * @param c The pair.
 * @return Its index, which is always the other one.
 */
MMGR_INLINE uint8_t exter_pingpong_drain(PingPong *const pp)
{
    return (pp != NULL) ? (uint8_t)(pp->fill_idx ^ 1u) : 1u;
}

/**
 * @brief Change ends.
 * @param c In/out. The pair.
 * @return The index now being filled.
 */
MMGR_INLINE uint8_t exter_pingpong_swap(PingPong *const pp)
{
    if (pp == NULL)
    {
        return 0u;
    }
    pp->fill_idx ^= 1u;
    return pp->fill_idx;
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each builds the context, where there is one, and hands it to the
   body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

mmgr_place mmgr_exter_place(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                            size_t psram_threshold, size_t dram_reserve)
{
    return MMGR_CALL(exter_place, ExterCtx, .size = size, .dma_required = dma_required, .free_dram = free_dram,
                     .free_psram = free_psram, .psram_threshold = psram_threshold, .dram_reserve = dram_reserve);
}

void mmgr_pingpong_init(PingPong *const pp)
{
    exter_pingpong_init(pp);
}

uint8_t mmgr_pingpong_fill_index(PingPong *const pp)
{
    return exter_pingpong_fill(pp);
}

uint8_t mmgr_pingpong_drain_index(PingPong *const pp)
{
    return exter_pingpong_drain(pp);
}

uint8_t mmgr_pingpong_swap(PingPong *const pp)
{
    return exter_pingpong_swap(pp);
}

#endif
