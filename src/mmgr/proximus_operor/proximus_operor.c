// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/proximus_operor/proximus_operor.h"

void mmgr_proxim_read(void *dst, const void *p, size_t sz)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *u = (const unsigned char *)p;
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);
    size_t i = 0;

    while (i < sz && ((uintptr_t)(d + i) & mask) != 0u)
    {
        d[i] = u[i];
        i++;
    }

    const size_t off = (size_t)((uintptr_t)(u + i) & mask);
    if (off == 0u)
    {

        while (sz - i >= MMGR_RAW_WORD)
        {
            mmgr_migro_put(d + i, mmgr_migro_load(u + i));
            i += MMGR_RAW_WORD;
        }
    }
    else if (sz - i >= MMGR_RAW_WORD)
    {

        const unsigned char *sa = (u + i) - off;
        const unsigned lo = (unsigned)(off * 8u);
        const unsigned hi = (unsigned)(MMGR_MV_BITS - (off * 8u));
        mmgr_migro_word prev = mmgr_migro_load(sa);
        while (sz - i >= MMGR_RAW_WORD)
        {
            sa += MMGR_RAW_WORD;
            mmgr_migro_word cur = mmgr_migro_load(sa);
#if MMGR_HW_BIG_ENDIAN
            mmgr_migro_put(d + i, (mmgr_migro_word)((prev << lo) | (cur >> hi)));
#else
            mmgr_migro_put(d + i, (mmgr_migro_word)((prev >> lo) | (cur << hi)));
#endif
            prev = cur;
            i += MMGR_RAW_WORD;
        }
    }

    while (i < sz)
    {
        d[i] = u[i];
        i++;
    }
}
