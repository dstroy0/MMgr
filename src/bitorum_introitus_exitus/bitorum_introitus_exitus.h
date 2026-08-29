/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file bitorum_introitus_exitus.h
 * @brief Bit writer state, its arguments, and the bitio dispatch table.
 *
 * @warning put writes whole bytes only, so bits that do not fill one stay in the writer's residue
 *          and align is what puts that last partial byte out. A stream whose length is not a
 *          multiple of eight and that never calls align ends one byte short, with no flag raised
 *          and nothing to notice at the call.
 */
#ifndef MMGR_BITORUM_INTROITUS_EXITUS_H
#define MMGR_BITORUM_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Bit writer state: the buffer, how much is written, and the partial byte.
 *
 * @note Built by mmgr_bitor_init, advanced by mmgr_bitor_put, and finished by mmgr_bitor_align.
 */
typedef struct
{
    uint8_t *out;       /**< Destination buffer [BORROWS]. */
    size_t cap;         /**< Bytes available in out. */
    size_t cnt;         /**< Whole bytes written so far. */
    uint8_t residue;    /**< Bits not yet written, in the low nbits positions. */
    mmgr_word nbits;    /**< Bits held in residue, always under 8. */
    mmgr_bool overflow; /**< Set once a request would pass cap; blocks later writes. */
} mmgr_bitor;

/**
 * @brief Arguments for the bitor calls; each reads only what it needs.
 *
 * @note mmgr_bitor_init reads out and cap; mmgr_bitor_put reads writer, val and nbits; and
 *       mmgr_bitor_align reads writer alone.
 */
typedef struct
{
    mmgr_bitor *const writer; /**< Writer for mmgr_bitor_put and mmgr_bitor_align [BORROWS]. */
    uint8_t *const out;       /**< Buffer for mmgr_bitor_init [BORROWS]. */
    const size_t cap;         /**< Bytes available in out. */
    const uint64_t val;       /**< Bits for mmgr_bitor_put, taken from the low end. */
    const mmgr_word nbits;    /**< Bits of val to write; must not exceed 64. */
} BitorumCfg;

/**
 * @brief Type of the bitio dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the three members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_bitor (*init)(const BitorumCfg *args); /**< Builds a writer over a buffer. */
    void (*put)(const BitorumCfg *args);        /**< Appends bits, writing whole bytes only. */
    void (*align)(const BitorumCfg *args);      /**< Writes the partial byte still held. */
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, init, put, align);

/**
 * @brief Builds a bit writer over args->out with capacity args->cap.
 *
 * @param[in] args Buffer and capacity [BORROWS].
 * @return      A writer with no bytes written and no residue.
 * @note The returned writer keeps args->out, which must outlive it [BORROWS].
 * @warning args->out must not be null and args->cap must not be zero. Neither is held to outside a
 *          MMGR_DEBUG_CHECKS build, and a null out is not noticed here: mmgr_bitor_put writes
 *          through it on the first whole byte.
 */
mmgr_bitor mmgr_bitor_init(const BitorumCfg *args);

/**
 * @brief Appends the low args->nbits bits of args->val to args->writer.
 *
 * @param[in,out] args Writer, value and bit count [BORROWS].
 * @note Writes whole bytes only; leftover bits stay in the writer's residue.
 * @note Does nothing when the writer's overflow is already set.
 * @note Sets the writer's overflow and clears its residue when the bytes would pass its cap.
 * @warning args->nbits must not exceed 64, and nothing holds it there outside a MMGR_DEBUG_CHECKS
 *          build. A larger count writes zeros past the sixty-fourth bit and advances the writer as
 *          though they were data.
 */
void mmgr_bitor_put(const BitorumCfg *args);

/**
 * @brief Writes the partial byte the writer still holds, padded with zeros above its bits.
 *
 * @param[in,out] args Writer to finish [BORROWS].
 * @note mmgr_bitor_put writes whole bytes only; without this call the residue is never written.
 * @note Does nothing when the residue is empty, so a second call writes nothing.
 * @note Does nothing when the writer's overflow is already set.
 * @note Only args->writer is read.
 * @warning Sets the writer's overflow when the byte would pass its cap.
 */
void mmgr_bitor_align(const BitorumCfg *args);

/**
 * @brief Dispatch table instance named bitio; each member calls the matching mmgr_bitor_ function.
 */
MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .init = mmgr_bitor_init,
    .put = mmgr_bitor_put,
    .align = mmgr_bitor_align,
};

MMGR_FINIS_DECLS

#endif
