// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "proximus_operor/proximus_operor.h"

/**
 * @file proximus_operor.c
 * @brief The one load and store entry that is not inline.
 *
 * Every entry below takes one parameter, a pointer to ProximCtx. The read is one job and its
 * arguments and its cursor are one context.
 */

/** @brief The read, in one place. */
typedef struct
{
    unsigned char *d;       /**< Destination. */
    const unsigned char *u; /**< Source. */
    size_t sz;              /**< Byte count. */
    size_t i;               /**< How far along. */
} ProximCtx;

/**
 * @brief Copy single bytes until the destination sits on a word boundary.
 * @param c In/out. The read.
 */
MMGR_INLINE void proxim_head(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);

    while ((c->i < c->sz) && (((uintptr_t)(c->d + c->i) & mask) != 0u))
    {
        c->d[c->i] = c->u[c->i];
        c->i++;
    }
}

/**
 * @brief Word at a time while the source is on a boundary too.
 * @param c In/out. The read.
 */
MMGR_INLINE void proxim_aligned(ProximCtx *c)
{
    while ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        mmgr_migro_put(c->d + c->i, mmgr_migro_load(c->u + c->i));
        c->i += MMGR_RAW_WORD;
    }
}

/**
 * @brief Word at a time when the source is not, carrying the overlap across two loads.
 * @param c In/out. The read.
 *
 * One load per word either way. The previous word is kept so the two halves either side of the
 * boundary can be put together with shifts rather than read twice.
 */
MMGR_INLINE void proxim_straddled(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);
    const size_t off = (size_t)((uintptr_t)(c->u + c->i) & mask);
    const unsigned char *sa = (c->u + c->i) - off;
    const unsigned lo = (unsigned)(off * 8u);
    const unsigned hi = (unsigned)(MMGR_MV_BITS - (off * 8u));
    mmgr_migro_word prev = mmgr_migro_load(sa);

    while ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        sa += MMGR_RAW_WORD;
        const mmgr_migro_word cur = mmgr_migro_load(sa);
#if MMGR_HW_BIG_ENDIAN
        mmgr_migro_put(c->d + c->i, (mmgr_migro_word)((prev << lo) | (cur >> hi)));
#else
        mmgr_migro_put(c->d + c->i, (mmgr_migro_word)((prev >> lo) | (cur << hi)));
#endif
        prev = cur;
        c->i += MMGR_RAW_WORD;
    }
}

/**
 * @brief Whatever is left after the last whole word.
 * @param c In/out. The read.
 */
MMGR_INLINE void proxim_tail(ProximCtx *c)
{
    while (c->i < c->sz)
    {
        c->d[c->i] = c->u[c->i];
        c->i++;
    }
}

/**
 * @brief The read.
 * @param c In/out. The read.
 */
MMGR_INLINE void proxim_read(ProximCtx *c)
{
    const uintptr_t mask = (uintptr_t)(MMGR_RAW_WORD - 1u);

    proxim_head(c);

    if (((uintptr_t)(c->u + c->i) & mask) == 0u)
    {
        proxim_aligned(c);
    }
    else if ((c->sz - c->i) >= MMGR_RAW_WORD)
    {
        proxim_straddled(c);
    }

    proxim_tail(c);
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so this is what it points at. It builds the context and hands it to the body above.

   It is nameable rather than file local because a static const table in the header has to be able
   to point at it, and a static const table is what gcc devirtualizes. Through an extern one every
   call from another translation unit is a load of the table, a load of the entry, and an indirect
   call it cannot see through. */

void mmgr_proxim_read(void *dst, const void *p, size_t sz)
{
    MMGR_CALL(proxim_read, ProximCtx, .d = (unsigned char *)dst, .u = (const unsigned char *)p, .sz = sz);
}
