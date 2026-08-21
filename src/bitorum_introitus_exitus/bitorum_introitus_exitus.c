// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

/**
 * @file bitio.c
 * @brief Bit writer. Bits accumulate from the low end and flush a byte at a time.
 *
 * The first bits put land in the low bits of the first output byte: acc |= bits << nbits, and
 * the flush takes acc & 0xFF.
 */

void mmgr_bitor_put(mmgr_bitor_writer *w, uint32_t bits, int n)
{
    if (w->overflow)
    {
        return;
    }

    uint32_t low = (n >= 32) ? bits : (bits & ((1u << n) - 1u));
    w->acc |= low << w->nbits;
    w->nbits += n;
    while (w->nbits >= 8)
    {
        if (w->cnt >= w->cap)
        {
            w->overflow = MMGR_TRUE;
            w->nbits = 0;
            w->acc = 0;
            return;
        }
        w->out[w->cnt] = (uint8_t)(w->acc & 0xFF);
        w->cnt++;
        w->acc >>= 8;
        w->nbits -= 8;
    }
}

void mmgr_bitor_align(mmgr_bitor_writer *w)
{
    if (w->nbits > 0)
    {
        if (w->cnt >= w->cap)
        {
            w->overflow = MMGR_TRUE;
            return;
        }
        w->out[w->cnt++] = (uint8_t)(w->acc & 0xFF);
        w->acc = 0;
        w->nbits = 0;
    }
}
