// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_impensa_ancorae_acus_accuracy.c
 * @brief Checks the properties a cost table has to hold for the sieve offset it feeds to be worth
 *        picking, over all 256 byte values and whichever of the five tables this build linked.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The costs are weights gathered from a corpus to make parsing faster. There is no arithmetic
 *       that reproduces them and no reference outside the corpus they came from, so nothing here
 *       predicts a value. What is checkable is whether the table can still do its job.
 * @note A second copy of the table written out in this file would be the same data twice and would
 *       agree with the module until one of the two was edited. It would prove nothing on the day it
 *       was written and would be wrong on the day the table changed.
 * @note cellul_pick_rows keeps the lowest cost it finds. Every case below follows from that: a byte
 *       that costs nothing wins every comparison, a table with one value ranks nothing, and a NUL
 *       that dropped below the ceiling starts being chosen out of text it terminates.
 * @note Which of the five tables is linked is a build option, and no macro carries the choice into
 *       this file. Every case here is written to hold for all five, which is why none of them names
 *       a byte other than the NUL.
 * @note Contract checks on the argument itself live in test_impensa_ancorae_acus. This file asks
 *       whether the numbers that come back can be ranked.
 */
#include <stdint.h>
#include <stdio.h>

#include "impensa_ancorae_acus/impensa_ancorae_acus.h"

#include "unity.h"

/**
 * @brief Expands to 256u, the byte values the table is indexed by and the width of every sweep here.
 *
 * @note The table holds one entry per byte value. Every case walks all of them, since a table is
 *       exactly 256 independent numbers and a sample covers none of the others.
 */
#define MMGR_ACCURACY_ANCORAE_BYTES 256u

/**
 * @brief Expands to 255u, the cost every table gives the NUL.
 *
 * @note All five tables document the NUL at the ceiling. It terminates the strings the sieve walks,
 *       and a NUL that could be chosen as the offset would anchor the search on the one byte that is
 *       present in every string exactly once and at the end.
 */
#define MMGR_ACCURACY_ANCORAE_CEILING 255u

/**
 * @brief Returns what the linked table charges for a byte.
 *
 * @param[in] byte Byte value to look up.
 * @return         The cost the table holds for it.
 * @note Wraps the call so every case below reads as a lookup. The entry takes its argument through a
 *       compound literal, and repeating that at every site would bury what the cases are checking.
 */
static uint8_t accuracy_cost_of(unsigned byte)
{
    // Explicit cast narrows the sweep counter to the byte the entry takes. Every caller holds it
    // below MMGR_ACCURACY_ANCORAE_BYTES
    return EMBED_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)byte);
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note The table is initialized data and no case writes anything, so there is no state to prepare.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Required alongside setUp, since the generated runner calls both around every case.
 * @note Nothing here allocates, so there is nothing to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks that no byte costs nothing.
 *
 * @note This is the case the file exists for. A cost of 0 wins every comparison cellul_pick_rows
 *       makes, so that byte becomes the sieve offset for every string it appears in, whatever the
 *       corpus said about it.
 * @note The defect this catches is an initializer one entry short. C fills the rest of the array
 *       with zeros and the compiler reports nothing, and the tail of a hand-written 256 entry table
 *       is exactly where a miscount lands.
 * @note Walks every byte value, since a zero-filled tail starts wherever the count ran out.
 */
void test_no_byte_costs_nothing(void)
{
    for (unsigned byte = 0u; byte < MMGR_ACCURACY_ANCORAE_BYTES; byte++)
    {
        char message[96];

        (void)snprintf(message, sizeof message, "byte %u costs nothing and would win every comparison", byte);
        TEST_ASSERT_GREATER_THAN_UINT8_MESSAGE(0u, accuracy_cost_of(byte), message);
    }
}

/**
 * @brief Checks that the NUL sits at the ceiling.
 *
 * @note All five tables document the NUL at 255, which is the one byte-specific fact they agree on.
 *       It terminates the strings the sieve walks, and one that could be chosen anchors the search
 *       on a byte that is present exactly once and at the end of every string.
 * @note Checked against the ceiling and not merely against a high value, since the ceiling is what
 *       keeps it from ever being the lowest cost in a set that contains anything else.
 */
void test_the_terminator_sits_at_the_ceiling(void)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(MMGR_ACCURACY_ANCORAE_CEILING, accuracy_cost_of(0u),
                                    "the NUL is not at the ceiling and could be chosen as a sieve offset");
}

/**
 * @brief Checks that the table ranks bytes instead of scoring them all the same.
 *
 * @note A table holding one value everywhere passes every case above and ranks nothing. The offset
 *       the sieve picks out of it is whichever byte the walk reached first, and the weighting buys
 *       nothing at all.
 * @note Two counts are taken. The distinct value count states the table has a range, and the count
 *       of bytes below the ceiling states there is something for the ceiling to be above.
 * @note The threshold is deliberately low. This case is looking for a table that is degenerate, not
 *       one that is coarse, and a table with a handful of tiers is a legitimate weighting.
 */
void test_the_table_ranks_bytes_instead_of_scoring_them_alike(void)
{
    uint8_t seen[MMGR_ACCURACY_ANCORAE_BYTES] = {0u};
    unsigned distinct = 0u;
    unsigned below_ceiling = 0u;

    for (unsigned byte = 0u; byte < MMGR_ACCURACY_ANCORAE_BYTES; byte++)
    {
        const uint8_t cost = accuracy_cost_of(byte);

        if (seen[cost] == 0u)
        {
            seen[cost] = 1u;
            distinct++;
        }
        if (cost < MMGR_ACCURACY_ANCORAE_CEILING)
        {
            below_ceiling++;
        }
    }

    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(1u, distinct, "the table holds one cost everywhere and ranks nothing");
    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0u, below_ceiling, "every byte sits at the ceiling and none can be chosen");
}

/**
 * @brief Checks that a lookup depends on its own byte and nothing else.
 *
 * @note The sieve looks a byte up once per candidate row, in whatever order the walk reaches them.
 *       A lookup that carried state between calls would rank the same string differently depending
 *       on what was parsed before it.
 * @note Every byte is read three times: once in a forward walk, once in a backward walk, and once
 *       interleaved with a lookup of a different byte. A cost that changed between the three would
 *       show up as a mismatch against the first reading.
 */
void test_a_lookup_depends_on_its_own_byte_and_nothing_else(void)
{
    uint8_t first_reading[MMGR_ACCURACY_ANCORAE_BYTES];

    for (unsigned byte = 0u; byte < MMGR_ACCURACY_ANCORAE_BYTES; byte++)
    {
        first_reading[byte] = accuracy_cost_of(byte);
    }

    for (unsigned byte = MMGR_ACCURACY_ANCORAE_BYTES; byte-- > 0u;)
    {
        char message[96];

        (void)snprintf(message, sizeof message, "byte %u cost a different amount walking backward", byte);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(first_reading[byte], accuracy_cost_of(byte), message);
    }

    for (unsigned byte = 0u; byte < MMGR_ACCURACY_ANCORAE_BYTES; byte++)
    {
        const unsigned other = (byte + 128u) % MMGR_ACCURACY_ANCORAE_BYTES;
        char message[96];

        (void)accuracy_cost_of(other);
        (void)snprintf(message, sizeof message, "byte %u cost a different amount after a lookup of %u", byte, other);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(first_reading[byte], accuracy_cost_of(byte), message);
    }
}

/**
 * @brief Checks that the byte the table is indexed by is the byte that was asked for.
 *
 * @note An index that dropped a bit or took the byte as signed returns a real cost from a real entry
 *       and looks correct at every case above. What separates the two is whether two different bytes
 *       can be made to return the same reading in a pattern that a wrong index would produce.
 * @note Bytes 0 through 127 are read against 128 through 255. A lookup that masked the high bit off
 *       returns the low half's costs for both halves, and this fails as soon as one pair differs.
 * @note A table where a pair legitimately holds the same cost is fine. The case is looking for every
 *       pair matching, which is what a dropped bit produces and what a real weighting does not.
 */
void test_the_high_half_is_not_the_low_half_read_twice(void)
{
    unsigned matching_pairs = 0u;

    for (unsigned byte = 0u; byte < 128u; byte++)
    {
        if (accuracy_cost_of(byte) == accuracy_cost_of(byte + 128u))
        {
            matching_pairs++;
        }
    }
    TEST_ASSERT_LESS_THAN_UINT_MESSAGE(128u, matching_pairs,
                                       "every high byte cost what its low counterpart cost, as a masked index would");
}
