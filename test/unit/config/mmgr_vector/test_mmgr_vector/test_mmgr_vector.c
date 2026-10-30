/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_mmgr_vector.c
 * @brief Holds the vector layer to the lane it is built on and to the scalar answer, at the width
 *        and on the arm the build takes.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The walks of memoria_operor and cellularum_laboro ask three things of a vector: which bytes
 *       hold zero, which bytes two vectors differ in, and the first byte set in a mask. Each is
 *       checked here against a byte loop, over every byte position, at whatever width the lane
 *       derived.
 * @note Every case is registered on every build. Where MMGR_VECTOR_OFF is 1 the layer declares no
 *       vector, and each case that needs one is ignored and says why, never left out of the runner.
 */
#include "config/mmgr_vector.h"

#include "unity.h"

#if MMGR_VECTOR_BYTES != 0u

/** @brief Two vectors and a half of bytes, which the cases load vectors from at offsets. */
#define BYTES_HELD ((2u * MMGR_VECTOR_BYTES) + 16u)

/** @brief The bytes the cases read vectors from: the index times eleven plus one, never zero below 256. */
static uint8_t s_bytes[BYTES_HELD];

/** @brief The mask a byte loop answers for the bytes of `one` and `other` that are equal. */
static MmgrVectorMask equal_by_bytes(const uint8_t *one, const uint8_t *other)
{
    MmgrVectorMask mask = 0u;
    for (size_t at = 0u; at < MMGR_VECTOR_BYTES; at += 1u)
    {
        if (one[at] == other[at])
        {
            mask |= (MmgrVectorMask)1 << at;
        }
    }
    return mask;
}

#define VECTOR_OFF_IGNORED()

#else

#define VECTOR_OFF_IGNORED() TEST_IGNORE_MESSAGE("MMGR_VECTOR_OFF is 1: the layer declares no vector")

#endif

void setUp(void)
{
#if MMGR_VECTOR_BYTES != 0u
    for (size_t at = 0u; at < sizeof(s_bytes); at += 1u)
    {
        // Explicit cast keeps the low byte of the product: that byte is the pattern
        s_bytes[at] = (uint8_t)((at * 11u) + 1u);
    }
#endif
}

void tearDown(void)
{
}

void test_the_vector_width_is_the_lane_or_nothing(void)
{
#if MMGR_VECTOR_OFF
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_VECTOR_BYTES, "the layer off declares no width");
#else
    TEST_ASSERT_EQUAL_size_t_MESSAGE(EMBED_LANE_BYTES, MMGR_VECTOR_BYTES, "the vector is the lane's width");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_VECTOR_BYTES, sizeof(MmgrVector), "the vector type is the lane's size");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_VECTOR_BYTES % sizeof(embed_word), "a vector is whole words");
#endif
}

void test_the_mask_of_a_count_is_that_many_low_bits(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)EMBED_LANE_MASK_ALL, (embed_u64)MMGR_VECTOR_MASK_OF(EMBED_LANE_BYTES),
                                    "the whole vector's mask is every byte");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x7u, (embed_u64)MMGR_VECTOR_MASK_OF(3u), "three bytes are three bits");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (embed_u64)MMGR_VECTOR_MASK_OF(0u), "no bytes, no bits");
}

void test_a_vector_loaded_at_an_odd_address_is_put_back_whole(void)
{
    VECTOR_OFF_IGNORED();
#if MMGR_VECTOR_BYTES != 0u
    uint8_t out[MMGR_VECTOR_BYTES + 8u];
    for (size_t at = 0u; at < sizeof(out); at += 1u)
    {
        out[at] = 0xccu;
    }
    mmgr_vector_put(out + 5, mmgr_vector_load(s_bytes + 3));
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_bytes + 3, out + 5, MMGR_VECTOR_BYTES, "the bytes come back as loaded");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xccu, out[4], "the byte before the put stands");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xccu, out[5u + MMGR_VECTOR_BYTES], "the byte after the put stands");
#endif
}

void test_an_aligned_load_reads_the_aligned_bytes(void)
{
    VECTOR_OFF_IGNORED();
#if MMGR_VECTOR_BYTES != 0u
    static uint8_t aligned[MMGR_VECTOR_BYTES] EMBED_ALIGN(EMBED_LANE_BYTES);
    uint8_t out[MMGR_VECTOR_BYTES];
    for (size_t at = 0u; at < MMGR_VECTOR_BYTES; at += 1u)
    {
        aligned[at] = s_bytes[at];
    }
    mmgr_vector_put(out, mmgr_vector_load_aligned(aligned));
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(aligned, out, MMGR_VECTOR_BYTES, "the aligned bytes come back");
#endif
}

void test_a_fill_repeats_the_byte_into_every_lane(void)
{
    VECTOR_OFF_IGNORED();
#if MMGR_VECTOR_BYTES != 0u
    uint8_t out[MMGR_VECTOR_BYTES];
    mmgr_vector_put(out, mmgr_vector_fill(0x5au));
    for (size_t at = 0u; at < MMGR_VECTOR_BYTES; at += 1u)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5au, out[at], "every byte is the fill");
    }
#endif
}

void test_equal_lanes_answer_the_byte_loop_at_every_position(void)
{
    VECTOR_OFF_IGNORED();
#if MMGR_VECTOR_BYTES != 0u
    uint8_t changed[MMGR_VECTOR_BYTES];
    for (size_t flipped = 0u; flipped < MMGR_VECTOR_BYTES; flipped += 1u)
    {
        for (size_t at = 0u; at < MMGR_VECTOR_BYTES; at += 1u)
        {
            changed[at] = s_bytes[at];
        }
        changed[flipped] ^= 0x80u;
        const MmgrVectorMask got = mmgr_vector_equal_lanes(mmgr_vector_load(s_bytes), mmgr_vector_load(changed));
        TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)equal_by_bytes(s_bytes, changed), (embed_u64)got,
                                        "the equal bytes are the bits set");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)((MmgrVectorMask)1 << flipped),
                                        (embed_u64)mmgr_vector_differ_lanes(mmgr_vector_load(s_bytes), mmgr_vector_load(changed)),
                                        "the one byte changed is the one bit differing");
    }
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)EMBED_LANE_MASK_ALL,
                                    (embed_u64)mmgr_vector_equal_lanes(mmgr_vector_load(s_bytes), mmgr_vector_load(s_bytes)),
                                    "a vector equals itself in every byte");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (embed_u64)mmgr_vector_differ_lanes(mmgr_vector_load(s_bytes), mmgr_vector_load(s_bytes)),
                                    "a vector differs from itself nowhere");
#endif
}

void test_zero_lanes_find_each_zero_byte_and_the_first_is_the_lowest(void)
{
    VECTOR_OFF_IGNORED();
#if MMGR_VECTOR_BYTES != 0u
    uint8_t text[MMGR_VECTOR_BYTES];
    for (size_t ended = 0u; ended < MMGR_VECTOR_BYTES; ended += 1u)
    {
        for (size_t at = 0u; at < MMGR_VECTOR_BYTES; at += 1u)
        {
            text[at] = s_bytes[at];
        }
        text[ended] = 0u;
        const MmgrVectorMask zeros = mmgr_vector_zero_lanes(mmgr_vector_load(text));
        TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)((MmgrVectorMask)1 << ended), (embed_u64)zeros, "the zero byte is the bit set");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(ended, mmgr_vector_mask_first(zeros), "the first byte set is the zero");
    }
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (embed_u64)mmgr_vector_zero_lanes(mmgr_vector_load(s_bytes)), "no zero byte, no bit");
#endif
}

void test_the_bytes_before_a_terminator_are_the_bytes_a_match_counts_in(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((embed_u64)EMBED_LANE_MASK_ALL, (embed_u64)mmgr_vector_mask_before(0u),
                                    "no terminator, every byte counts");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (embed_u64)mmgr_vector_mask_before(1u), "a terminator first, nothing counts");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x1fu, (embed_u64)mmgr_vector_mask_before((MmgrVectorMask)0x20u | (MmgrVectorMask)0x80u),
                                    "the bytes before the first terminator count, the later one is behind it");
    // the settle a walk makes: a match behind the terminator is no match
    const MmgrVectorMask hit = (MmgrVectorMask)0x44u;
    const MmgrVectorMask end = (MmgrVectorMask)0x10u;
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x4u, (embed_u64)(hit & mmgr_vector_mask_before(end)), "only the match before the end lives");
}
