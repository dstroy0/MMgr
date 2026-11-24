/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_mmgr_string_shim_symbols.c
 * @brief memset, memcpy, memmove and memcmp of src/config/mmgr_string_shim.c, at each count that takes another path.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note The suite calls each definition by EMBED_WEAK_NAME. Under gcc and clang that is the name itself, which the
 *       shim's target gives the link before the C library. Under MSVC it is the name with _default after it, the body
 *       of the shim, which a C runtime in the link does not hide.
 * @note The counts run from 0 past two lanes, four words and the odd bytes, and so each loop of each backend runs,
 *       and the straight tail stores each count of odd bytes.
 * @note A guard byte on each side of each region holds its value, and so a write past either end fails a case.
 */
#include "mmgr.h"

#include "embed_lane.h"
#include "unity.h"

#define COUNT_MAX ((2u * EMBED_LANE_BYTES) + (4u * sizeof(embed_word)) + sizeof(embed_word) + 3u)
#define GUARD_BYTE 0xA5u
#define SHIFT_MAX 9u
#define ROOM (COUNT_MAX + (2u * EMBED_LANE_BYTES) + SHIFT_MAX)

void *EMBED_WEAK_NAME(memset)(void *dest, int value, size_t bytes);
void *EMBED_WEAK_NAME(memcpy)(void *dest, const void *source, size_t bytes);
void *EMBED_WEAK_NAME(memmove)(void *dest, const void *source, size_t bytes);
int EMBED_WEAK_NAME(memcmp)(const void *left, const void *right, size_t bytes);

static EMBED_ALIGN(EMBED_LANE_BYTES) uint8_t s_region[ROOM];
static EMBED_ALIGN(EMBED_LANE_BYTES) uint8_t s_source[ROOM];
static EMBED_ALIGN(EMBED_LANE_BYTES) uint8_t s_expected[ROOM];

// Each byte of a region at a value of its place, so a byte that lands at another place reads as another value. Each
// value is below 0x80, and so a byte one above it does not wrap
static void pattern(uint8_t *region, size_t bytes, uint8_t seed)
{
    for (size_t place = 0u; place < bytes; place++)
    {
        // Explicit cast keeps the low seven bits of the place and the seed
        region[place] = (uint8_t)(((place * 7u) + seed) & 0x7Fu);
    }
}

// The guard bytes around the bytes at the start of s_region plus EMBED_LANE_BYTES
static void guards_held(size_t count, const char *message)
{
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(GUARD_BYTE, s_region[EMBED_LANE_BYTES - 1u], message);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(GUARD_BYTE, s_region[EMBED_LANE_BYTES + count], message);
}

void setUp(void)
{
    for (size_t place = 0u; place < ROOM; place++)
    {
        s_region[place] = GUARD_BYTE;
    }
    pattern(s_source, ROOM, 3u);
}

void tearDown(void)
{
}

void test_memset_fills_each_count_and_writes_no_byte_past_it(void)
{
    for (size_t count = 0u; count <= COUNT_MAX; count++)
    {
        setUp();
        uint8_t *const dest = &s_region[EMBED_LANE_BYTES];
        TEST_ASSERT_EQUAL_PTR(dest, EMBED_WEAK_NAME(memset)(dest, 0x5C, count));
        if (count != 0u)
        {
            TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE(0x5Cu, dest, count, "each byte of the count takes the fill");
        }
        guards_held(count, "the fill writes no byte past either end");
    }
}

void test_memset_keeps_the_low_byte_of_the_value(void)
{
    uint8_t *const dest = &s_region[EMBED_LANE_BYTES];
    (void)EMBED_WEAK_NAME(memset)(dest, 0x1F3, 5u);
    TEST_ASSERT_EACH_EQUAL_HEX8(0xF3u, dest, 5u);
    guards_held(5u, "a value past one byte writes its low byte alone");
}

void test_memcpy_copies_each_count_and_writes_no_byte_past_it(void)
{
    for (size_t count = 0u; count <= COUNT_MAX; count++)
    {
        setUp();
        uint8_t *const dest = &s_region[EMBED_LANE_BYTES];
        TEST_ASSERT_EQUAL_PTR(dest, EMBED_WEAK_NAME(memcpy)(dest, s_source, count));
        if (count != 0u)
        {
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(s_source, dest, count, "each byte of the count is the source's");
        }
        guards_held(count, "the copy writes no byte past either end");
    }
}

// The bytes that a move of count bytes from from to to leaves, built from a copy of the region first
static void moved(size_t from, size_t to, size_t count)
{
    for (size_t place = 0u; place < ROOM; place++)
    {
        s_expected[place] = s_region[place];
    }
    for (size_t place = 0u; place < count; place++)
    {
        s_expected[to + place] = s_source[from + place];
    }
}

void test_memmove_moves_each_count_down_over_its_own_source(void)
{
    for (size_t shift = 1u; shift <= SHIFT_MAX; shift++)
    {
        for (size_t count = 0u; count <= COUNT_MAX; count++)
        {
            setUp();
            pattern(s_region, ROOM, 11u);
            pattern(s_source, ROOM, 11u);
            moved(EMBED_LANE_BYTES + shift, EMBED_LANE_BYTES, count);
            uint8_t *const dest = &s_region[EMBED_LANE_BYTES];
            TEST_ASSERT_EQUAL_PTR(dest, EMBED_WEAK_NAME(memmove)(dest, dest + shift, count));
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(s_expected, s_region, ROOM, "a move down over its source");
        }
    }
}

void test_memmove_moves_each_count_up_over_its_own_source(void)
{
    for (size_t shift = 1u; shift <= SHIFT_MAX; shift++)
    {
        for (size_t count = 0u; count <= COUNT_MAX; count++)
        {
            setUp();
            pattern(s_region, ROOM, 13u);
            pattern(s_source, ROOM, 13u);
            moved(EMBED_LANE_BYTES, EMBED_LANE_BYTES + shift, count);
            uint8_t *const source = &s_region[EMBED_LANE_BYTES];
            TEST_ASSERT_EQUAL_PTR(source + shift, EMBED_WEAK_NAME(memmove)(source + shift, source, count));
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(s_expected, s_region, ROOM, "a move up over its source");
        }
    }
}

void test_memcmp_answers_zero_for_equal_bytes_and_the_sign_of_the_first_difference(void)
{
    for (size_t count = 0u; count <= COUNT_MAX; count++)
    {
        pattern(s_region, ROOM, 3u);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, EMBED_WEAK_NAME(memcmp)(s_region, s_source, count), "equal bytes give 0");
        for (size_t differ = 0u; differ < count; differ++)
        {
            pattern(s_region, ROOM, 3u);
            // Explicit cast raises the byte by one in its type, so the region sorts after the source there
            s_region[differ] = (uint8_t)(s_source[differ] + 1u);
            TEST_ASSERT_TRUE_MESSAGE(EMBED_WEAK_NAME(memcmp)(s_region, s_source, count) > 0, "a larger byte first");
            TEST_ASSERT_TRUE_MESSAGE(EMBED_WEAK_NAME(memcmp)(s_source, s_region, count) < 0, "a smaller byte first");
        }
    }
}
