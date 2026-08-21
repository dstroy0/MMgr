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

/** @brief Bit writer state. */
typedef struct
{
    uint8_t *out;        /**< The buffer. */
    size_t cap;          /**< Its size. */
    size_t cnt;          /**< Bytes written so far. */
    uint32_t acc;        /**< Bits not yet a whole byte. */
    int nbits;           /**< How many of them. */
    mmgr_bool overflow;  /**< The buffer filled. Latches, and is never cleared. */
} mmgr_bitor_writer;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put)(mmgr_bitor_writer *w, uint32_t bits, int n);
    void (*align)(mmgr_bitor_writer *w);
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, put, align);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
void mmgr_bitor_put(mmgr_bitor_writer *w, uint32_t bits, int n);
void mmgr_bitor_align(mmgr_bitor_writer *w);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .put = mmgr_bitor_put,
    .align = mmgr_bitor_align,
};

MMGR_FINIS_DECLS

#endif
