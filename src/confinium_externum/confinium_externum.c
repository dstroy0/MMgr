// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "confinium_externum/confinium_externum.h"

/**
 * @file confinium_externum.c
 * @brief Placement across DRAM and PSRAM, and the ping-pong buffer pair.
 */

#if MMGR_ENABLE_PSRAM_POOL

/**
 * @brief Would this request still leave the DRAM reserve intact.
 * @param size Byte count.
 * @param free_dram Bytes free.
 * @param dram_reserve Bytes to keep free.
 * @return MMGR_TRUE if it fits.
 */
static mmgr_bool dram_fits(size_t size, size_t free_dram, size_t reserve)
{
    return size <= free_dram && (free_dram - size) >= reserve;
}

mmgr_place mmgr_exter_place(size_t size, mmgr_bool dma_required, size_t free_dram, size_t free_psram,
                            size_t psram_threshold, size_t dram_reserve)
{
    if (size == 0)
    {
        return PLACE_FAIL;
    }

    mmgr_bool d_fits = dram_fits(size, free_dram, dram_reserve);
    mmgr_bool p_fits = size <= free_psram;

    if (dma_required)
    {
        return d_fits ? PLACE_DRAM : PLACE_FAIL;
    }

    if (size >= psram_threshold)
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

void mmgr_pingpong_init(PingPong *pp)
{
    if (pp)
    {
        pp->fill_idx = 0;
    }
}

uint8_t mmgr_pingpong_fill_index(const PingPong *pp)
{
    return pp ? pp->fill_idx : 0;
}

uint8_t mmgr_pingpong_drain_index(const PingPong *pp)
{
    return pp ? (uint8_t)(pp->fill_idx ^ 1u) : 1;
}

uint8_t mmgr_pingpong_swap(PingPong *pp)
{
    if (!pp)
    {
        return 0;
    }
    pp->fill_idx ^= 1u;
    return pp->fill_idx;
}

#endif
