// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/rawmemcpy/rawmemcpy.h"

void proto_raw_read(void *dst, const void *p, size_t sz)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *u = (const unsigned char *)p;
    const uintptr_t mask = (uintptr_t)(PROTO_RAW_WORD - 1u);
    size_t i = 0;

    while (i < sz && ((uintptr_t)(d + i) & mask) != 0u)
    {
        d[i] = u[i];
        i++;
    }

    const size_t off = (size_t)((uintptr_t)(u + i) & mask);
    if (off == 0u)
    {

        while (sz - i >= PROTO_RAW_WORD)
        {
            proto_mv_put(d + i, proto_mv_load(u + i));
            i += PROTO_RAW_WORD;
        }
    }
    else if (sz - i >= PROTO_RAW_WORD)
    {

        const unsigned char *sa = (u + i) - off;
        const unsigned lo = (unsigned)(off * 8u);
        const unsigned hi = (unsigned)(PROTO_MV_BITS - (off * 8u));
        proto_mv_word prev = proto_mv_load(sa);
        while (sz - i >= PROTO_RAW_WORD)
        {
            sa += PROTO_RAW_WORD;
            proto_mv_word cur = proto_mv_load(sa);
#if PROTOCORE_HW_BIG_ENDIAN
            proto_mv_put(d + i, (proto_mv_word)((prev << lo) | (cur >> hi)));
#else
            proto_mv_put(d + i, (proto_mv_word)((prev >> lo) | (cur << hi)));
#endif
            prev = cur;
            i += PROTO_RAW_WORD;
        }
    }

    while (i < sz)
    {
        d[i] = u[i];
        i++;
    }
}
