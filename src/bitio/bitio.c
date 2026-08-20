// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/bitio/bitio.h"

void protocore_bitw_put(protocore_bit_writer *w, uint32_t bits, int n)
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
            w->overflow = PROTO_TRUE;
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

void protocore_bitw_align(protocore_bit_writer *w)
{
    if (w->nbits > 0)
    {
        if (w->cnt >= w->cap)
        {
            w->overflow = PROTO_TRUE;
            return;
        }
        w->out[w->cnt++] = (uint8_t)(w->acc & 0xFF);
        w->acc = 0;
        w->nbits = 0;
    }
}
