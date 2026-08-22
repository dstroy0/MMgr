// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CLZ_H
#define MMGR_CLZ_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file clz.h
 * @brief Leading zero count, in one translation unit.
 *
 * Its own module because two callers want it and neither owns it: the conversion engine reads it to
 * normalise a fraction, and the renderer reads it to size a decimal exponent. Living in either one
 * would put a copy in the other, or make the other reach into a neighbour for a primitive.
 *
 * Branchless. Each step's shift is the test itself, scaled: ((x >> k) == 0) is zero or one, and
 * shifting that left by the step's width gives the amount to move by. Six shift and add pairs on
 * one dependency chain, with nothing for a predictor to get wrong.
 *
 * No builtin. __builtin_clzll is a call to libgcc on a baseline target, which is what this avoids.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/**
 * @brief What a count is given.
 *
 * Public, and in the header, because the caller is what builds it. The member is const: nothing
 * writes to a config once the caller has built it.
 *
 * Not the module's context. ClzCtx is what the body works with and it is file local in the .c.
 */
typedef struct
{
    const mmgr_u64 x; /**< The value. Must not be zero. */
} ClzCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    int (*lead)(const ClzCfg *c);
} ClzNs;
MMGR_NS_LAYOUT(ClzNs, lead);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
int mmgr_clz_lead(const ClzCfg *c);
/** @} */

/** @brief The value a count is asked about, and nothing that merely converts to one. */
#define MMGR_CLZ_IS_VALUE(x_) ((void)_Generic((x_), mmgr_u64: 0))

/**
 * @brief How many leading zero bits @p x_ has, 0 through 63. @p x_ must not be zero.
 *
 * Positional in, so the struct and the designator never reach a call site.
 */
#define mmgr_clz_lead(x_) (MMGR_CLZ_IS_VALUE(x_), clz.lead(&(ClzCfg){.x = (x_)}))

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS ClzNs clz MMGR_UNUSED = {
    .lead = mmgr_clz_lead,
};

MMGR_FINIS_DECLS

#endif
