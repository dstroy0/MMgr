/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr_vector.h
 * @brief The machine's vector unit, where it has one, as the walks of memoria_operor and
 *        cellularum_laboro take it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-21
 *
 * @note A walk steps a word at a time through verbum_scrutor's lanes. Where the target has a vector
 *       unit, the same walk first takes whole vectors, MMGR_VECTOR_BYTES at a step, and leaves the
 *       word walk the rest. The questions a walk asks of a word are the ones asked here of a vector:
 *       which lanes hold zero, which lanes two vectors differ in, and the first lane set in a mask.
 * @note MMGR_VECTOR_BYTES is 32 under AVX2, 16 under SSE2 or NEON, and 0 where the target has none
 *       or MMGR_VECTOR_OFF is 1, so every arm of every gate is defined. A mask holds one bit a lane
 *       under SSE2 and AVX2 and four a lane under NEON, MMGR_VECTOR_MASK_LANE_BITS either way, and
 *       the helpers below read it the same on both.
 * @warning mmgr_vector_load and mmgr_vector_put are unaligned, for a walk over bytes it was given a
 *          count of, which keeps every read within them. A walk over text ended by a zero, whose cap
 *          may reach far past the text, loads through mmgr_vector_load_aligned alone, at an address
 *          a multiple of MMGR_VECTOR_BYTES: an aligned vector never crosses a page, so a read past
 *          the text's end stays in the page the text ends in, as the word walk's does.
 */
#ifndef MMGR_VECTOR_H
#define MMGR_VECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

#ifndef MMGR_VECTOR_OFF
#define MMGR_VECTOR_OFF 0
#endif
#if (MMGR_VECTOR_OFF != 0) && (MMGR_VECTOR_OFF != 1)
#error "MMGR_VECTOR_OFF must be 0 or 1"
#endif

#if MMGR_VECTOR_OFF
#define MMGR_VECTOR_BYTES 0u
#define MMGR_VECTOR_MASK_LANE_BITS 1u
#elif defined(__AVX2__)
#include <immintrin.h>
#define MMGR_VECTOR_BYTES 32u
#define MMGR_VECTOR_MASK_LANE_BITS 1u
#define MMGR_VECTOR_AVX2 1
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#include <emmintrin.h>
#define MMGR_VECTOR_BYTES 16u
#define MMGR_VECTOR_MASK_LANE_BITS 1u
#define MMGR_VECTOR_SSE2 1
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define MMGR_VECTOR_BYTES 16u
#define MMGR_VECTOR_MASK_LANE_BITS 4u
#define MMGR_VECTOR_NEON 1
#else
#define MMGR_VECTOR_BYTES 0u
#define MMGR_VECTOR_MASK_LANE_BITS 1u
#endif

#ifndef MMGR_VECTOR_AVX2
#define MMGR_VECTOR_AVX2 0
#endif
#ifndef MMGR_VECTOR_SSE2
#define MMGR_VECTOR_SSE2 0
#endif
#ifndef MMGR_VECTOR_NEON
#define MMGR_VECTOR_NEON 0
#endif

/**
 * @brief The lanes of a vector as a mask, MMGR_VECTOR_MASK_LANE_BITS a lane, the first lane lowest.
 */
typedef uint64_t MmgrVectorMask;

/**
 * @brief The lanes a mask holds for the first `lanes_` of a vector.
 */
#define MMGR_VECTOR_MASK_OF(lanes_)                                                                                    \
    (((lanes_) * MMGR_VECTOR_MASK_LANE_BITS >= 64u)                                                                    \
         ? ~(MmgrVectorMask)0                                                                                          \
         : (((MmgrVectorMask)1 << ((lanes_) * MMGR_VECTOR_MASK_LANE_BITS)) - 1u))

/**
 * @brief The index of the first lane set in a mask.
 *
 * @param[in] mask A mask with at least one lane set.
 * @return         The lane, counted from the first.
 * @note Built two ways, as verbum_scrutor's lane.first is: the trailing zero count where the compiler
 *       has it, else a walk of the lanes.
 */
EMBED_INLINE size_t mmgr_vector_mask_first(MmgrVectorMask mask)
{
#if EMBED_HAS_BUILTIN(__builtin_ctzll)
    // Explicit cast narrows the count, which is below 64, before the lane width divides it
    return (size_t)((unsigned)__builtin_ctzll((unsigned long long)mask) / MMGR_VECTOR_MASK_LANE_BITS);
#else
    size_t lane = 0u;
    while ((mask & MMGR_VECTOR_MASK_OF(lane + 1u)) == 0u)
    {
        lane += 1u;
    }
    return lane;
#endif
}

/**
 * @brief The lanes before the first lane set in a mask, every lane where none is set.
 *
 * @param[in] mask Lanes that end a walk.
 * @return         The lanes a match counts in.
 */
EMBED_INLINE MmgrVectorMask mmgr_vector_mask_before(MmgrVectorMask mask)
{
    return (mask == 0u) ? ~(MmgrVectorMask)0 : ((mask & (0u - mask)) - 1u);
}

#if MMGR_VECTOR_AVX2

typedef __m256i MmgrVector;

EMBED_INLINE MmgrVector mmgr_vector_load(const uint8_t *at)
{
    return _mm256_loadu_si256((const __m256i *)(const void *)at);
}

EMBED_INLINE MmgrVector mmgr_vector_load_aligned(const uint8_t *at)
{
    return _mm256_load_si256((const __m256i *)(const void *)at);
}

EMBED_INLINE void mmgr_vector_put(uint8_t *dst, MmgrVector vector)
{
    _mm256_storeu_si256((__m256i *)(void *)dst, vector);
}

EMBED_INLINE MmgrVector mmgr_vector_fill(uint8_t val)
{
    // Explicit cast: the intrinsic takes the byte as a char
    return _mm256_set1_epi8((char)val);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_equal_lanes(MmgrVector one, MmgrVector other)
{
    // Explicit cast reads the sign bits as the unsigned mask they are
    return (MmgrVectorMask)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(one, other));
}

EMBED_INLINE MmgrVectorMask mmgr_vector_zero_lanes(MmgrVector vector)
{
    return mmgr_vector_equal_lanes(vector, _mm256_setzero_si256());
}

#elif MMGR_VECTOR_SSE2

typedef __m128i MmgrVector;

EMBED_INLINE MmgrVector mmgr_vector_load(const uint8_t *at)
{
    return _mm_loadu_si128((const __m128i *)(const void *)at);
}

EMBED_INLINE MmgrVector mmgr_vector_load_aligned(const uint8_t *at)
{
    return _mm_load_si128((const __m128i *)(const void *)at);
}

EMBED_INLINE void mmgr_vector_put(uint8_t *dst, MmgrVector vector)
{
    _mm_storeu_si128((__m128i *)(void *)dst, vector);
}

EMBED_INLINE MmgrVector mmgr_vector_fill(uint8_t val)
{
    // Explicit cast: the intrinsic takes the byte as a char
    return _mm_set1_epi8((char)val);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_equal_lanes(MmgrVector one, MmgrVector other)
{
    // Explicit cast reads the sign bits as the unsigned mask they are
    return (MmgrVectorMask)(uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(one, other));
}

EMBED_INLINE MmgrVectorMask mmgr_vector_zero_lanes(MmgrVector vector)
{
    return mmgr_vector_equal_lanes(vector, _mm_setzero_si128());
}

#elif MMGR_VECTOR_NEON

typedef uint8x16_t MmgrVector;

EMBED_INLINE MmgrVector mmgr_vector_load(const uint8_t *at)
{
    return vld1q_u8(at);
}

EMBED_INLINE MmgrVector mmgr_vector_load_aligned(const uint8_t *at)
{
    return vld1q_u8(at);
}

EMBED_INLINE void mmgr_vector_put(uint8_t *dst, MmgrVector vector)
{
    vst1q_u8(dst, vector);
}

EMBED_INLINE MmgrVector mmgr_vector_fill(uint8_t val)
{
    return vdupq_n_u8(val);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_equal_lanes(MmgrVector one, MmgrVector other)
{
    // Each lane of the comparison is all ones or all zeros; narrowing each 16-bit pair by four bits
    // keeps four bits of each lane, so the 64-bit result holds the sixteen lanes four bits apiece
    const uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(vceqq_u8(one, other)), 4);
    return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
}

EMBED_INLINE MmgrVectorMask mmgr_vector_zero_lanes(MmgrVector vector)
{
    return mmgr_vector_equal_lanes(vector, vdupq_n_u8(0u));
}

#endif

#if MMGR_VECTOR_BYTES != 0u

/**
 * @brief The lanes two vectors differ in.
 */
EMBED_INLINE MmgrVectorMask mmgr_vector_differ_lanes(MmgrVector one, MmgrVector other)
{
    return MMGR_VECTOR_MASK_OF(MMGR_VECTOR_BYTES) & ~mmgr_vector_equal_lanes(one, other);
}

#endif

#endif
