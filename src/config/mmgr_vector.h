/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr_vector.h
 * @brief The lane, embedded_types' 256-bit unit beside the word, as the walks of memoria_operor and
 *        cellularum_laboro take it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note A walk steps a word at a time through verbum_scrutor's lanes. The same walk first takes
 *       whole lanes, MMGR_VECTOR_BYTES at a step, and leaves the word walk the rest. The questions a
 *       walk asks of a word are the ones asked here of a lane: which bytes hold zero, which bytes two
 *       lanes differ in, and the first byte set in a mask.
 * @note The lane is embed_lane, and it partitions at compile time to what the target has: one AVX2
 *       register, two SSE2 or NEON registers, the compiler's scalar lowering where the target has no
 *       vector unit, or a struct of eight words on the portable arm. MMGR_VECTOR_BYTES is
 *       EMBED_LANE_BYTES, and 0 where MMGR_VECTOR_OFF is 1, which leaves the word walks alone. Every
 *       arm of every gate is defined. A mask holds one bit a byte, the first byte lowest.
 * @warning mmgr_vector_load and mmgr_vector_put are unaligned, for a walk over bytes it was given a
 *          count of, which keeps every read within them. A walk over text ended by a zero, whose cap
 *          may reach far past the text, loads through mmgr_vector_load_aligned alone, at an address
 *          a multiple of MMGR_VECTOR_BYTES: an aligned lane never crosses a page, and a read past
 *          the text's end stays in the page the text ends in, as the word walk's does.
 */
#ifndef MMGR_VECTOR_H
#define MMGR_VECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "embed_lane.h"

#ifndef MMGR_VECTOR_OFF
#define MMGR_VECTOR_OFF 0
#endif
#if (MMGR_VECTOR_OFF != 0) && (MMGR_VECTOR_OFF != 1)
#error "MMGR_VECTOR_OFF must be 0 or 1"
#endif

#if MMGR_VECTOR_OFF
#define MMGR_VECTOR_BYTES 0u
#else
#define MMGR_VECTOR_BYTES EMBED_LANE_BYTES
#endif

/**
 * @brief The bytes of a lane as a mask, one bit a byte, the first byte lowest.
 */
typedef embed_lane_mask MmgrVectorMask;

/**
 * @brief The mask of the first `bytes_` of a lane.
 */
#define MMGR_VECTOR_MASK_OF(bytes_)                                                                                    \
    (((bytes_) >= EMBED_LANE_BYTES) ? ~(MmgrVectorMask)0 : (((MmgrVectorMask)1 << (bytes_)) - 1u))

/**
 * @brief The index of the first byte set in a mask.
 *
 * @param[in] mask A mask with at least one byte set.
 * @return         The byte, counted from the first.
 * @note Built two ways, as verbum_scrutor's lane.first is: the trailing zero count where the compiler
 *       has it, else a walk of the bytes.
 */
EMBED_INLINE size_t mmgr_vector_mask_first(MmgrVectorMask mask)
{
#if EMBED_HAS_BUILTIN(__builtin_ctz)
    // Explicit cast widens the count, which is below 32, to the index it is used as
    return (size_t)(unsigned)__builtin_ctz((unsigned)mask);
#else
    size_t byte = 0u;
    while ((mask & MMGR_VECTOR_MASK_OF(byte + 1u)) == 0u)
    {
        byte += 1u;
    }
    return byte;
#endif
}

/**
 * @brief The bytes before the first byte set in a mask, every byte where none is set.
 *
 * @param[in] mask Bytes that end a walk.
 * @return         The bytes a match counts in.
 */
EMBED_INLINE MmgrVectorMask mmgr_vector_mask_before(MmgrVectorMask mask)
{
    return (mask == 0u) ? ~(MmgrVectorMask)0 : ((mask & (0u - mask)) - 1u);
}

#if MMGR_VECTOR_BYTES != 0u

typedef embed_lane MmgrVector;

EMBED_INLINE MmgrVector mmgr_vector_load(const uint8_t *at)
{
    return embed_lane_load(at);
}

EMBED_INLINE MmgrVector mmgr_vector_load_aligned(const uint8_t *at)
{
    return embed_lane_load_aligned(at);
}

EMBED_INLINE void mmgr_vector_put(uint8_t *dst, MmgrVector vector)
{
    embed_lane_put(dst, vector);
}

EMBED_INLINE MmgrVector mmgr_vector_fill(uint8_t val)
{
    return embed_lane_fill_byte(val);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_equal_lanes(MmgrVector one, MmgrVector other)
{
    return embed_lane_bytes_equal(one, other);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_zero_lanes(MmgrVector vector)
{
    return embed_lane_bytes_zero(vector);
}

/**
 * @brief The bytes two lanes differ in.
 */
EMBED_INLINE MmgrVectorMask mmgr_vector_differ_lanes(MmgrVector one, MmgrVector other)
{
    return MMGR_VECTOR_MASK_OF(MMGR_VECTOR_BYTES) & ~mmgr_vector_equal_lanes(one, other);
}

#endif

#endif
