// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BITORUM_INTROITUS_EXITUS_H
#define MMGR_BITORUM_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file bitorum_introitus_exitus.h
 * @brief Write bits into a byte buffer.
 *
 * Bits accumulate from the low end and flush a byte at a time. The first bits put land in the low
 * bits of the first output byte.
 *
 * The writer latches once the buffer is full and is never cleared, so one check at the end covers
 * the whole run rather than a test after every put.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/**
 * @brief What a put is given, and what it leaves behind.
 *
 * Public, and in the header, because the caller is what builds it and what carries it from one put
 * to the next. Not the module's context: BitorCtx is what the body works with, it holds these by
 * value so the arithmetic runs in registers, and it is file local in the .c.
 */
typedef struct
{
    uint8_t *out;        /**< The buffer. */
    size_t cap;          /**< Its size. */
    size_t cnt;          /**< Whole bytes written. A trailing fragment sits at out[cnt]. */
    uint32_t acc;        /**< The bits, low end first. What a byte does not take stays. */
    int nbits;           /**< How many of them. What a byte does not take stays. */
    mmgr_bool overflow;  /**< The buffer filled. Latches, and is never cleared. */
} BitorumCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put)(BitorumCfg *c);
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, put);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
void mmgr_bitor_put(BitorumCfg *c);
/** @} */

/** @name The types a put takes, settled where the call is written.
 *  @{ */
#define MMGR_BITOR_IS_WRITER(x_) ((void)_Generic((x_), BitorumCfg: 0))
#define MMGR_BITOR_IS_BITS(x_) ((void)_Generic((x_), uint32_t: 0, int: 0))
#define MMGR_BITOR_IS_COUNT(x_) ((void)_Generic((x_), int: 0))
/** @} */

/**
 * @brief Put @p n_ bits of @p bits_ into @p c_.
 *
 * The bits go in at nbits and the count goes up; the entry drains whatever whole bytes that makes.
 * Splitting it there is what keeps the packing inside the module - a caller that had to write
 * acc |= bits << nbits itself would be a caller that has to know which end the bits go in at.
 *
 * n_ at or above 32 takes the value whole, because the mask arm would shift by the width.
 */
#define mmgr_bitor_put(c_, bits_, n_)                                                                                  \
    (MMGR_BITOR_IS_WRITER(c_), MMGR_BITOR_IS_BITS(bits_), MMGR_BITOR_IS_COUNT(n_),                                     \
     (c_).acc |= (uint32_t)(((n_) >= 32) ? (bits_) : ((bits_) & ((1u << (n_)) - 1u))) << (c_).nbits,                  \
     (c_).nbits += (n_), bitio.put(&(c_)))

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .put = mmgr_bitor_put,
};

MMGR_FINIS_DECLS

#endif
