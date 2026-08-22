// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "clz/clz.h"

/**
 * @file clz.c
 * @brief Leading zero count. One parameter, a pointer to ClzCtx.
 */

/** @brief One count. */
typedef struct
{
    mmgr_u64 x; /**< The value. Must not be zero. */
} ClzCtx;

/**
 * @brief How many leading zero bits @c x has.
 * @param c The count.
 * @return 0 through 63.
 *
 * The test is the shift. ((x >> k) == 0) is zero or one, and shifting that left by the step's width
 * gives the amount to move by, so each step is a shift and an add with no branch to take.
 */
MMGR_INLINE int clz_lead(const ClzCtx *c)
{
    mmgr_u64 x = c->x;
    mmgr_u64 shift;
    int n = 0;

    shift = (mmgr_u64)((x >> 32) == 0u) << 5;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 48) == 0u) << 4;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 56) == 0u) << 3;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 60) == 0u) << 2;
    x <<= shift;
    n += (int)shift;
    shift = (mmgr_u64)((x >> 62) == 0u) << 1;
    x <<= shift;
    n += (int)shift;
    n += (int)((x >> 63) == 0u);
    return n;
}

/* The namespace is a table of function pointers with the caller's config in their types, so this is
   what it points at. It builds the context and hands it to the body above.

   Parenthesised because the header defines a macro of this name for the call site, and the
   identifier here would otherwise sit immediately before a parenthesis and be taken for an
   invocation of it. */

int (mmgr_clz_lead)(const ClzCfg *c)
{
    return MMGR_CALL(clz_lead, ClzCtx, .x = c->x);
}
