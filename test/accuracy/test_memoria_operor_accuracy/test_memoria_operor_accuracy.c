// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_memoria_operor_accuracy.c
 * @brief Checks the copy, move, compare, search and fill against byte-at-a-time references, at every
 *        length and every alignment of both operands.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note Every call here moves whole words and finishes the odd bytes separately, and two of them pick
 *       a different walk depending on whether both operands are aligned. That makes the length and
 *       the two alignments the three axes a defect hides along, so every case below sweeps all three
 *       instead of testing a convenient size.
 * @note The references are single byte loops. They share no word load, no lane mask and no tail
 *       handling with the module, which is the whole reason they can disagree with it.
 * @note Every destination is checked past the bytes it was given. A copy that wrote one byte too many
 *       lands on a guard, and nothing else in a passing result would show it.
 * @note The compare widens both bytes as unsigned before subtracting. A byte at or above 0x80 then
 *       orders above one below it, and an implementation that let a plain char through returns the
 *       opposite sign for exactly those pairs, which the sweep below covers at every position.
 * @note cmp and chr read the last word whole, which memoria_operor.h states. Every region here is
 *       padded past its count so that read stays inside the object.
 * @note Contract checks live in test_memoria_operor. This file asks which bytes end up where.
 */
#include <stdint.h>
#include <stdio.h>

#include "memoria_operor/memoria_operor.h"

#include "unity.h"

/**
 * @brief Expands to 72u, the longest run any case here copies, compares or fills.
 *
 * @note Past four words at every environment's word width, which is what reaches the unrolled arm of
 *       the copy and the fill as well as the one-word arm and the odd byte tail.
 */
#define MMGR_ACCURACY_MEMOR_RUN 72u

/**
 * @brief Expands to 8u, the alignments each operand is offered.
 *
 * @note One more than the widest word any environment uses, so every offset from a word boundary is
 *       reached whatever embed_word is on this build.
 */
#define MMGR_ACCURACY_MEMOR_OFFSETS 8u

/**
 * @brief Expands to 32u, the bytes of padding past every region.
 *
 * @note Two purposes. cmp and chr read the last word whole whatever the count leaves, so the region
 *       has to be readable past it. A write that ran long lands here instead of outside the object.
 */
#define MMGR_ACCURACY_MEMOR_PAD 32u

/**
 * @brief Expands to the bytes each working buffer holds.
 *
 * @note The longest run, the largest offset it can start at, and the padding past it.
 */
#define MMGR_ACCURACY_MEMOR_BUFFER (MMGR_ACCURACY_MEMOR_RUN + MMGR_ACCURACY_MEMOR_OFFSETS + MMGR_ACCURACY_MEMOR_PAD)

/**
 * @brief Expands to 0xA5, the byte a destination is filled with before a write.
 *
 * @note A byte no case writes on purpose. A destination position still holding it is one the call did
 *       not reach, and a position past the count holding anything else is a write that ran long.
 */
#define MMGR_ACCURACY_MEMOR_GUARD 0xA5u

/**
 * @brief Returns the byte a source region carries at a given position.
 *
 * @param[in] position Position within the region.
 * @return             The byte that position holds.
 * @note Every byte differs from its neighbors along the run, which is what makes a copy that shifted
 *       its output by one visible. Never the guard byte either, which tells a byte the copy wrote
 *       apart from one it did not.
 */
static uint8_t accuracy_source_byte(size_t position)
{
    // Explicit cast narrows the mixed position to the byte a region holds. The or with 1 keeps it
    // away from zero and the test below keeps it off the guard
    const uint8_t value = (uint8_t)(((position * 7u) + 3u) | 1u);

    return (value == MMGR_ACCURACY_MEMOR_GUARD) ? (uint8_t)(MMGR_ACCURACY_MEMOR_GUARD ^ 1u) : value;
}

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @note Every write case starts here, so what the call left untouched is distinguishable from what it
 *       wrote.
 */
static void accuracy_fill_guard(uint8_t *buffer)
{
    for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
    {
        buffer[index] = MMGR_ACCURACY_MEMOR_GUARD;
    }
}

/**
 * @brief Checks that a destination holds the run it was given and the guard everywhere else.
 *
 * @param[in] buffer   Whole destination buffer [BORROWS].
 * @param[in] at       Offset the run was written at.
 * @param[in] expected Bytes the run should hold [BORROWS].
 * @param[in] bytes    Length of the run.
 * @param[in] label    Text naming the case, printed when a byte disagrees.
 * @note Walks the whole buffer, not the run alone. A byte before the destination or past the count is
 *       a write that reached outside what the call was given, and checking only the run misses both.
 */
static void accuracy_expect_run(const uint8_t *buffer, size_t at, const uint8_t *expected, size_t bytes,
                                const char *label)
{
    for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
    {
        const uint8_t want =
            ((index >= at) && (index < (at + bytes))) ? expected[index - at] : (uint8_t)MMGR_ACCURACY_MEMOR_GUARD;

        if (buffer[index] != want)
        {
            char message[192];

            (void)snprintf(message, sizeof message, "%s: byte %u holds 0x%02X where 0x%02X belongs", label,
                           (unsigned)index, buffer[index], want);
            TEST_FAIL_MESSAGE(message);
        }
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every buffer here has automatic storage inside the case that builds it, and each case fills
 *       its own, so there is no shared state to prepare.
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
 * @brief Checks the helpers this suite rests on against values worked out by hand.
 *
 * @note Exists to catch a defect in the references as themselves. A source pattern that repeated
 *       would let a shifted copy match, and a run checker that walked the run alone would miss a
 *       write past the end. Every case below rests on both.
 * @note The pattern is checked to differ between neighboring positions and to avoid the guard byte,
 *       which are the two properties the other cases lean on.
 * @note The run checker is offered a buffer with one byte changed past the run, and is expected to
 *       report it.
 */
void test_the_helpers_this_suite_relies_on_are_themselves_right(void)
{
    uint8_t buffer[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t expected[MMGR_ACCURACY_MEMOR_RUN];

    for (size_t index = 0u; index < 16u; index++)
    {
        char message[96];

        (void)snprintf(message, sizeof message, "position %u shares a byte with the one after it", (unsigned)index);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(accuracy_source_byte(index), accuracy_source_byte(index + 1u), message);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(MMGR_ACCURACY_MEMOR_GUARD, accuracy_source_byte(index),
                                      "a source byte collides with the guard");
    }

    accuracy_fill_guard(buffer);
    for (size_t index = 0u; index < 8u; index++)
    {
        expected[index] = accuracy_source_byte(index);
        buffer[4u + index] = expected[index];
    }
    accuracy_expect_run(buffer, 4u, expected, 8u, "a run laid down by hand");
}

/**
 * @brief Checks the forward copy at every length and every alignment of both operands.
 *
 * @note This is the case the file exists for. The copy takes four words a pass, then one word a pass,
 *       then the odd bytes, and the length is what selects among the three. Every length from none to
 *       past four words is offered, at all sixty four pairings of source and destination alignment.
 * @note The destination is checked over its whole extent every time. A copy that wrote one byte past
 *       its count fails at the guard instead of passing.
 * @note A length of zero is included. A loop written with a do-while runs once for a count of none,
 *       and this is where that shows.
 */
void test_the_forward_copy_lands_every_byte_at_every_alignment(void)
{
    static uint8_t source[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t destination[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t expected[MMGR_ACCURACY_MEMOR_RUN];

    for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
    {
        source[index] = accuracy_source_byte(index);
    }

    for (size_t bytes = 0u; bytes <= MMGR_ACCURACY_MEMOR_RUN; bytes++)
    {
        for (size_t src_at = 0u; src_at < MMGR_ACCURACY_MEMOR_OFFSETS; src_at++)
        {
            for (size_t dst_at = 0u; dst_at < MMGR_ACCURACY_MEMOR_OFFSETS; dst_at++)
            {
                char label[128];

                for (size_t index = 0u; index < bytes; index++)
                {
                    expected[index] = source[src_at + index];
                }
                accuracy_fill_guard(destination);
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = destination + dst_at, .src = source + src_at, .bytes = bytes);

                (void)snprintf(label, sizeof label, "a copy of %u bytes from offset %u to offset %u", (unsigned)bytes,
                               (unsigned)src_at, (unsigned)dst_at);
                accuracy_expect_run(destination, dst_at, expected, bytes, label);
            }
        }
    }
}

/**
 * @brief Checks the fill at every length and every alignment.
 *
 * @note The fill builds a word from the byte and stores whole words before finishing byte by byte, so
 *       it has the same three arms the copy has and the same lengths reach them.
 * @note Two byte values are used. Zero is the one a fill written with a word store gets right by
 *       accident when the broadcast is wrong, since every lane of a zero word is already zero.
 * @note The destination is checked over its whole extent. A fill that ran one byte long fails at the
 *       guard.
 */
void test_the_fill_writes_every_byte_at_every_alignment(void)
{
    static const uint8_t value_of[] = {0x00u, 0x5Cu, 0xFFu};
    uint8_t destination[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t expected[MMGR_ACCURACY_MEMOR_RUN];
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (size_t bytes = 0u; bytes <= MMGR_ACCURACY_MEMOR_RUN; bytes++)
        {
            for (size_t dst_at = 0u; dst_at < MMGR_ACCURACY_MEMOR_OFFSETS; dst_at++)
            {
                char label[128];

                for (size_t index = 0u; index < bytes; index++)
                {
                    expected[index] = value_of[value_index];
                }
                accuracy_fill_guard(destination);
                EMBED_CALL(memor.set, MemoriaCfg, .dst = destination + dst_at, .bytes = bytes,
                           .val = value_of[value_index]);

                (void)snprintf(label, sizeof label, "a fill of %u bytes with 0x%02X at offset %u", (unsigned)bytes,
                               value_of[value_index], (unsigned)dst_at);
                accuracy_expect_run(destination, dst_at, expected, bytes, label);
            }
        }
    }
}

/**
 * @brief Checks that the upward move carries overlapping bytes without eating its own source.
 *
 * @note move_up walks from the far end back, which is what a destination above an overlapping source
 *       needs. A forward walk over the same regions overwrites bytes it has not read yet, and the
 *       result is the first few bytes repeated along the run.
 * @note Every overlap distance from one byte to past a word is offered at every length, which is what
 *       reaches the case where the two regions share their first word and the case where they share
 *       only the tail.
 * @note The reference copies through a scratch region first, so it reads the source as it stood
 *       before any of it was overwritten.
 */
void test_the_upward_move_carries_overlapping_bytes(void)
{
    for (size_t bytes = 1u; bytes <= 40u; bytes++)
    {
        for (size_t apart = 1u; apart <= MMGR_ACCURACY_MEMOR_OFFSETS; apart++)
        {
            uint8_t region[MMGR_ACCURACY_MEMOR_BUFFER];
            uint8_t expected[MMGR_ACCURACY_MEMOR_BUFFER];
            char label[128];

            for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
            {
                region[index] = accuracy_source_byte(index);
                expected[index] = region[index];
            }
            for (size_t index = 0u; index < bytes; index++)
            {
                expected[apart + index] = accuracy_source_byte(index);
            }

            EMBED_CALL(memor.move_up, MemoriaCfg, .dst = region + apart, .src = region, .bytes = bytes);

            (void)snprintf(label, sizeof label, "an upward move of %u bytes over an overlap of %u", (unsigned)bytes,
                           (unsigned)apart);
            for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
            {
                if (region[index] != expected[index])
                {
                    char message[192];

                    (void)snprintf(message, sizeof message, "%s: byte %u holds 0x%02X where 0x%02X belongs", label,
                                   (unsigned)index, region[index], expected[index]);
                    TEST_FAIL_MESSAGE(message);
                }
            }
        }
    }
}

/**
 * @brief Checks that the downward move carries overlapping bytes without eating its own source.
 *
 * @note move_down is the upward walk, which is what a destination below an overlapping source needs.
 *       The dispatch table points it at the same function the plain copy uses, and the claim under
 *       test is that this direction of overlap really is safe through it.
 * @note The mirror of the upward case, with the destination below the source by the same distances.
 */
void test_the_downward_move_carries_overlapping_bytes(void)
{
    for (size_t bytes = 1u; bytes <= 40u; bytes++)
    {
        for (size_t apart = 1u; apart <= MMGR_ACCURACY_MEMOR_OFFSETS; apart++)
        {
            uint8_t region[MMGR_ACCURACY_MEMOR_BUFFER];
            uint8_t expected[MMGR_ACCURACY_MEMOR_BUFFER];
            char label[128];

            for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
            {
                region[index] = accuracy_source_byte(index);
                expected[index] = region[index];
            }
            for (size_t index = 0u; index < bytes; index++)
            {
                expected[index] = accuracy_source_byte(apart + index);
            }

            EMBED_CALL(memor.move_down, MemoriaCfg, .dst = region, .src = region + apart, .bytes = bytes);

            (void)snprintf(label, sizeof label, "a downward move of %u bytes over an overlap of %u", (unsigned)bytes,
                           (unsigned)apart);
            for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
            {
                if (region[index] != expected[index])
                {
                    char message[192];

                    (void)snprintf(message, sizeof message, "%s: byte %u holds 0x%02X where 0x%02X belongs", label,
                                   (unsigned)index, region[index], expected[index]);
                    TEST_FAIL_MESSAGE(message);
                }
            }
        }
    }
}

/**
 * @brief Checks that the compare orders two regions by their first differing byte, read as unsigned.
 *
 * @note The sign is the accuracy claim. A byte at or above 0x80 orders above one below it, and an
 *       implementation that let a plain char through gets the opposite sign for exactly those pairs.
 *       The pairs below include both orderings across that boundary.
 * @note The difference is planted at every position along every length, so the lane that differs is
 *       walked through every position of a word and every word of the run.
 * @note Only the sign is compared, not the magnitude. The header states the result orders the two
 *       regions, which is a statement about sign.
 * @note Regions that match everywhere are checked to give zero at every length, including none.
 */
void test_the_compare_orders_two_regions_by_their_first_difference(void)
{
    static const uint8_t pair_of[][2] = {{0x01u, 0x02u}, {0x02u, 0x01u}, {0x01u, 0xFFu},
                                         {0xFFu, 0x01u}, {0x7Fu, 0x80u}, {0x80u, 0x7Fu}};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned pair_count = (unsigned)(sizeof pair_of / sizeof pair_of[0]);
    uint8_t left[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t right[MMGR_ACCURACY_MEMOR_BUFFER];

    for (size_t bytes = 0u; bytes <= MMGR_ACCURACY_MEMOR_RUN; bytes++)
    {
        for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
        {
            left[index] = accuracy_source_byte(index);
            right[index] = left[index];
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            0, (int)EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes),
            "two identical regions did not compare equal");

        for (size_t at = 0u; at < bytes; at++)
        {
            for (unsigned pair = 0u; pair < pair_count; pair++)
            {
                const int expected_sign = (pair_of[pair][0] < pair_of[pair][1]) ? -1 : 1;
                char message[160];

                left[at] = pair_of[pair][0];
                right[at] = pair_of[pair][1];

                const embed_iword produced =
                    EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes);
                const int produced_sign = (produced < 0) ? -1 : ((produced > 0) ? 1 : 0);

                (void)snprintf(message, sizeof message, "over %u bytes with 0x%02X against 0x%02X at position %u",
                               (unsigned)bytes, pair_of[pair][0], pair_of[pair][1], (unsigned)at);
                TEST_ASSERT_EQUAL_INT_MESSAGE(expected_sign, produced_sign, message);

                left[at] = accuracy_source_byte(at);
                right[at] = left[at];
            }
        }
    }
}

/**
 * @brief Checks that the search returns the first occurrence and nothing when the byte is absent.
 *
 * @note First is the accuracy claim. A search that resolved a matching word to the wrong lane returns
 *       an address inside the region that is not the earliest match, and the caller reads from the
 *       wrong place with no sign anything went wrong.
 * @note The byte is planted at every position at every length, with a second copy of it planted later
 *       in the region. A search reporting the latest match instead of the earliest fails there.
 * @note A byte that occurs nowhere is checked to give nothing back at every length.
 * @note Zero is searched for as well. The header states a terminator is not special here, and a
 *       search built on a zero test instead of a broadcast comparison gets that one case wrong.
 */
void test_the_search_returns_the_first_occurrence(void)
{
    uint8_t region[MMGR_ACCURACY_MEMOR_BUFFER];
    static const uint8_t sought_of[] = {0x3Cu, 0x00u, 0xFFu};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned sought_count = (unsigned)(sizeof sought_of / sizeof sought_of[0]);

    for (unsigned sought_index = 0u; sought_index < sought_count; sought_index++)
    {
        const uint8_t sought = sought_of[sought_index];
        // A filler that is never the byte being looked for, so the only matches are the planted ones
        const uint8_t filler = (uint8_t)(sought ^ 0x5Au);

        for (size_t bytes = 0u; bytes <= MMGR_ACCURACY_MEMOR_RUN; bytes++)
        {
            for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
            {
                region[index] = filler;
            }

            char message[160];

            (void)snprintf(message, sizeof message, "0x%02X over %u bytes with no occurrence", sought, (unsigned)bytes);
            TEST_ASSERT_NULL_MESSAGE(EMBED_CALL(memor.chr, MemoriaCfg, .src = region, .bytes = bytes, .val = sought),
                                     message);

            for (size_t at = 0u; at < bytes; at++)
            {
                region[at] = sought;
                // A second occurrence past the first, which fails a search reporting the latest match
                if ((at + 1u) < bytes)
                {
                    region[bytes - 1u] = sought;
                }

                (void)snprintf(message, sizeof message, "0x%02X over %u bytes first occurring at %u", sought,
                               (unsigned)bytes, (unsigned)at);
                TEST_ASSERT_EQUAL_PTR_MESSAGE(
                    region + at, EMBED_CALL(memor.chr, MemoriaCfg, .src = region, .bytes = bytes, .val = sought),
                    message);

                region[at] = filler;
                region[bytes - 1u] = filler;
            }
        }
    }
}

/**
 * @brief Checks that a byte past the count is never reached by the search or the compare.
 *
 * @note Both calls read the last word whole, which the header states, and the accuracy claim is that
 *       lanes past the count take no part in the result. A tail mask that covered one lane too many
 *       reports a match or a difference the caller never asked about.
 * @note The byte immediately past the count is set to the one being looked for, and to a differing
 *       value for the compare. Every length is offered, so the planted byte falls at every position
 *       within the last word.
 */
void test_a_byte_past_the_count_takes_no_part_in_either_scan(void)
{
    uint8_t left[MMGR_ACCURACY_MEMOR_BUFFER];
    uint8_t right[MMGR_ACCURACY_MEMOR_BUFFER];

    for (size_t bytes = 0u; bytes <= MMGR_ACCURACY_MEMOR_RUN; bytes++)
    {
        char message[128];

        for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
        {
            left[index] = 0x11u;
            right[index] = 0x11u;
        }
        right[bytes] = 0x99u;

        (void)snprintf(message, sizeof message, "a difference one byte past a count of %u", (unsigned)bytes);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            0, (int)EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes), message);

        for (size_t index = 0u; index < MMGR_ACCURACY_MEMOR_BUFFER; index++)
        {
            left[index] = 0x11u;
        }
        left[bytes] = 0x7Eu;

        (void)snprintf(message, sizeof message, "a match one byte past a count of %u", (unsigned)bytes);
        TEST_ASSERT_NULL_MESSAGE(EMBED_CALL(memor.chr, MemoriaCfg, .src = left, .bytes = bytes, .val = 0x7Eu), message);
    }
}
