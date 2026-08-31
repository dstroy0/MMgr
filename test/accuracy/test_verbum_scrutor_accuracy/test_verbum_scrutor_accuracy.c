// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_verbum_scrutor_accuracy.c
 * @brief Checks each lane predicate against a byte-at-a-time reference computed here, over words
 *        whose lanes hold different values, across every byte value and every threshold.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note test_verbum_scrutor already compares ge, le and sub7 against scalar loops. Every word it
 *       builds holds one byte value repeated into all its lanes, and a word like that cannot show a
 *       borrow crossing a lane boundary: the neighbor it would leak into holds the same value and
 *       gives the same answer either way. Every word here holds lanes that differ.
 * @note The neighboring lanes are 0x00 and 0xFF alternating, which is the pair a borrow or a carry
 *       has the furthest to travel between. A lane arithmetic defect shows up against those before
 *       it shows up against anything else.
 * @note The reference is a plain loop over the bytes, written here. It shares no shift, no mask and
 *       no constant with the word arithmetic under test.
 * @note Words and lane masks are assembled through a union of an embed_word and its bytes. A lane
 *       index then means the same position here as it does to a load. Nothing is taken from
 *       MMGR_VERBUM_SCRUTOR_HIGH or the other lane constants, which are part of what is being
 *       checked.
 * @note Contract checks on empty masks, on run and run_edge, and on what the tables are wired to
 *       live in test_verbum_scrutor. This file asks whether each lane gets the right answer.
 */
#include <stdint.h>

#include "verbum_scrutor/verbum_scrutor.h"

#include "unity.h"

/**
 * @brief Expands to 0x80, the bit a lane mask sets in a lane that matched.
 *
 * @note Written from what a lane mask is, and not taken from MMGR_VERBUM_SCRUTOR_HIGH. That constant
 *       is built by the module and would supply both sides of every comparison below.
 */
#define MMGR_ACCURACY_LANE_HIGH_BIT 0x80u

/**
 * @brief A word and its bytes over the same storage.
 *
 * @note Writing the bytes and reading the word puts a lane at the position a load would put it,
 *       whichever end of the word the target starts from. That keeps every expectation here in
 *       memory order without naming an endianness.
 */
typedef union {
    embed_word word;                  /**< The assembled word. */
    uint8_t lane[sizeof(embed_word)]; /**< Its bytes, lane 0 first in memory. */
} AccuracyLanes;

/**
 * @brief Builds a word from one byte per lane.
 *
 * @param[in] lane_byte_of Byte for each lane, lane 0 first in memory [BORROWS].
 * @return                 The word those bytes make.
 * @note The bytes go in through the union, so this places a lane exactly where a load reads one.
 */
static embed_word accuracy_word_from(const uint8_t *lane_byte_of)
{
    AccuracyLanes builder;

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        builder.lane[lane_index] = lane_byte_of[lane_index];
    }
    return builder.word;
}

/**
 * @brief Builds the lane mask a set of per-lane verdicts calls for.
 *
 * @param[in] lane_matched_of Non-zero for each lane that matched, lane 0 first in memory [BORROWS].
 * @return                    A word holding the high bit in each matching lane and zero elsewhere.
 * @note This is the shape every lane predicate returns. It is assembled from the byte positions.
 *       The module's own mask constants take no part in building it.
 */
static embed_word accuracy_mask_from(const uint8_t *lane_matched_of)
{
    AccuracyLanes builder;

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        // Explicit cast narrows the verdict to the byte one lane holds. Every caller passes 0 or 1,
        // and the shift places the high bit of that lane
        builder.lane[lane_index] = (uint8_t)((lane_matched_of[lane_index] != 0u) ? MMGR_ACCURACY_LANE_HIGH_BIT : 0u);
    }
    return builder.word;
}

/**
 * @brief Fills a lane byte array with a pattern whose neighbors are as far apart as bytes get.
 *
 * @param[out] lane_byte_of Bytes to fill, lane 0 first in memory [BORROWS].
 * @param[in]  under_test   Byte placed in the lane the sweep is examining.
 * @param[in]  at_lane      Lane the byte under test goes in.
 * @note Every other lane alternates 0x00 and 0xFF. A borrow leaving the lane under test lands in a
 *       neighbor whose own result is already settled, which is what makes the leak visible.
 */
static void accuracy_fill_contrasting(uint8_t *lane_byte_of, uint8_t under_test, size_t at_lane)
{
    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        // Explicit cast narrows the alternating selector to the byte one lane holds
        lane_byte_of[lane_index] = (uint8_t)(((lane_index % 2u) == 0u) ? 0x00u : 0xFFu);
    }
    lane_byte_of[at_lane] = under_test;
}

/**
 * @brief Candidate at-or-above: the shipping subtraction with the word's own high bits or-ed in.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against, under 0x80.
 * @return         A lane mask holding the lanes at or above byte.
 * @note A lane at or above 0x80 is above any threshold under 0x80 whatever its low bits say, and its
 *       own high bit already carries that. Or-ing it into the result covers those lanes without a
 *       second comparison.
 * @note No lane can borrow out here. A lane under 0x80 has the or supply its reserve, and one at or
 *       above 0x80 is already larger than the threshold, so the subtraction stays non-negative in
 *       both cases and the neighbor is untouched.
 * @warning The threshold must be under 0x80. Above it the minuend's reserve is gone and this is no
 *          better than what it replaces. Every call inside the library passes a literal under it.
 */
static embed_word accuracy_candidate_ge_or(embed_word word, uint8_t byte)
{
    const embed_word high = MMGR_SWAR_ONES * MMGR_ACCURACY_LANE_HIGH_BIT;
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)byte;

    return (word & high) | (((word | high) - broadcast) & high);
}

/**
 * @brief Candidate at-or-below: the shipping subtraction with the word narrowed and its high lanes
 *        removed.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against, under 0x80.
 * @return         A lane mask holding the lanes at or below byte.
 * @note The mirror of accuracy_candidate_ge_or and not symmetric with it. A lane at or above 0x80 is
 *       never at or below a threshold under 0x80, so those lanes are taken out of the result instead
 *       of put into it.
 * @note Narrowing the subtrahend is what stops the underflow. The lanes it changes are exactly the
 *       ones the final and-not removes, so nothing else is disturbed.
 * @warning The threshold must be under 0x80, for the reason given on accuracy_candidate_ge_or.
 */
static embed_word accuracy_candidate_le_mask(embed_word word, uint8_t byte)
{
    const embed_word high = MMGR_SWAR_ONES * MMGR_ACCURACY_LANE_HIGH_BIT;
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)byte;

    return ~word & (((broadcast | high) - (word & ~high)) & high);
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every value this suite uses has automatic storage inside the case that builds it, and there
 *       is no shared state to prepare here.
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
 * @brief Checks the word and mask assembly this suite rests on against values worked out by hand.
 *
 * @note Exists to catch a defect in the helpers as itself. Without this case a broken
 *       accuracy_mask_from would surface as a predicate mismatch, and verbum_scrutor would be blamed
 *       for it.
 * @note A word of all zero bytes is zero whichever end the target starts from, and a word whose
 *       every lane matched is the high bit repeated. Neither depends on byte order.
 */
void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        lane_byte_of[lane_index] = 0x00u;
        lane_matched_of[lane_index] = 0u;
    }
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)accuracy_word_from(lane_byte_of), "all zero bytes are a zero word");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)accuracy_mask_from(lane_matched_of),
                                    "no lane matched is an empty mask");

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        lane_byte_of[lane_index] = 0xFFu;
        lane_matched_of[lane_index] = 1u;
    }
    // Explicit cast widens the assembled word to the uint64_t the assertion compares in, which holds
    // every environment's word whatever its width
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)(embed_word) ~(embed_word)0, (uint64_t)accuracy_word_from(lane_byte_of),
                                    "all set bytes are a full word");

    const embed_word every_lane_matched = accuracy_mask_from(lane_matched_of);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(embed_word),
                                     EMBED_CALL(lane.count, ScrutLaneCfg, .mask = every_lane_matched),
                                     "a mask with every lane set does not count as every lane");

    accuracy_fill_contrasting(lane_byte_of, 0x41u, 0u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x41u, lane_byte_of[0], "the byte under test is not in the lane it was given");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, lane_byte_of[1], "the neighboring lane is not the contrasting byte");
}

/**
 * @brief Checks ge and le for every byte value against every threshold, in a word whose other lanes
 *        hold the opposite extremes.
 *
 * @note The lane under test and the threshold are swept over the seven bit range, and the neighbors
 *       are 0x00 and 0xFF. verbum_scrutor.h states both entries compare bytes as unsigned with 0x80
 *       and above the largest values, and that is what this asserts.
 * @note le fails here today. The borrow that keeps lanes apart is the lane's own high bit, and a
 *       neighbor at 0x80 or above has that bit spoken for as data, so it takes a borrow out of the
 *       lane under test even when that lane is itself inside the seven bit range. Narrowing the
 *       neighbors to 0x7F makes this pass and measures nothing, which is why they are not narrowed.
 * @note The reference compares the two bytes with the C operators.
 */
void test_ge_and_le_answer_per_lane_with_contrasting_neighbors(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (unsigned byte_value = 0u; byte_value < 0x80u; byte_value++)
    {
        for (unsigned threshold = 0u; threshold < 0x80u; threshold++)
        {
            // Explicit cast narrows the loop counter to the byte the lane under test holds
            accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, 0u);

            const embed_word assembled = accuracy_word_from(lane_byte_of);

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] >= (uint8_t)threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(
                (uint64_t)accuracy_mask_from(lane_matched_of),
                (uint64_t)EMBED_CALL(lane.ge, ScrutLaneCfg, .word = assembled, .byte = (uint8_t)threshold),
                "ge answered for a lane other than the one holding the byte");

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] <= (uint8_t)threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(
                (uint64_t)accuracy_mask_from(lane_matched_of),
                (uint64_t)EMBED_CALL(lane.le, ScrutLaneCfg, .word = assembled, .byte = (uint8_t)threshold),
                "le answered for a lane other than the one holding the byte");
        }
    }
}

/**
 * @brief Checks both candidates over every lane value against every threshold under 0x80.
 *
 * @note The lane values run the whole 0 to 255 range with 0x00 and 0xFF neighbors, which is where
 *       the shipping pair fails. The thresholds run 0 to 0x7F, which is the bound both candidates
 *       state and which every call inside the library satisfies.
 * @note A candidate that has not passed this has no business being benched.
 */
void test_both_candidates_answer_for_every_lane_under_a_low_threshold(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (unsigned byte_value = 0u; byte_value < 256u; byte_value++)
    {
        for (unsigned threshold = 0u; threshold < 0x80u; threshold++)
        {
            // Explicit casts narrow the loop counters to the bytes a lane holds, as in the cases
            // above
            accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, 0u);

            const embed_word assembled = accuracy_word_from(lane_byte_of);

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] >= (uint8_t)threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)accuracy_candidate_ge_or(assembled, (uint8_t)threshold),
                                            "the at-or-above candidate disagrees with a byte-at-a-time compare");

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] <= (uint8_t)threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)accuracy_candidate_le_mask(assembled, (uint8_t)threshold),
                                            "the at-or-below candidate disagrees with a byte-at-a-time compare");
        }
    }
}

/**
 * @brief Expands to 4, the boundary values one lane takes in the exhaustive word sweep.
 *
 * @note 0x00, 0x7F, 0x80 and 0xFF: the two ends of each half of the byte. A borrow either escapes at
 *       one of those or it does not escape at all, and four values keep the whole-word sweep to
 *       4 raised to the lane count, which is 65536 on an eight lane word and 16 on a two lane one.
 */
#define MMGR_ACCURACY_EDGE_VALUES 4u

/**
 * @brief The four boundary bytes the exhaustive word sweep draws its lanes from.
 *
 * @note Ordered low to high, so an index into this is also an ordering of the values.
 */
static const uint8_t accuracy_edge_of[MMGR_ACCURACY_EDGE_VALUES] = {0x00u, 0x7Fu, 0x80u, 0xFFu};

/**
 * @brief Checks both candidates with the byte under test in every lane, not only the first.
 *
 * @note The earlier sweep only ever planted the byte at lane 0, which is the one lane with no
 *       neighbor below it. A defect at the top lane, or one that depends on which side the borrow
 *       would travel, passes that sweep and fails this one.
 * @note Thresholds run to 0x7F, the bound both candidates state.
 */
void test_both_candidates_answer_with_the_byte_in_every_lane(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (size_t at_lane = 0u; at_lane < sizeof(embed_word); at_lane++)
    {
        for (unsigned byte_value = 0u; byte_value < 256u; byte_value++)
        {
            for (unsigned threshold = 0u; threshold < 0x80u; threshold++)
            {
                // Explicit casts narrow the loop counters to the bytes a lane holds, as above
                accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, at_lane);

                const embed_word assembled = accuracy_word_from(lane_byte_of);

                for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
                {
                    lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] >= (uint8_t)threshold);
                }
                TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                                (uint64_t)accuracy_candidate_ge_or(assembled, (uint8_t)threshold),
                                                "the at-or-above candidate disagrees in some lane");

                for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
                {
                    lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] <= (uint8_t)threshold);
                }
                TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                                (uint64_t)accuracy_candidate_le_mask(assembled, (uint8_t)threshold),
                                                "the at-or-below candidate disagrees in some lane");
            }
        }
    }
}

/**
 * @brief Checks both candidates over every whole-word arrangement of the four boundary bytes.
 *
 * @note The per-lane sweeps hold every other lane at one of two values. This one lets every lane
 *       take any of the four boundaries independently, so two high lanes side by side, high lanes at
 *       both ends and a word of nothing but high lanes all appear. A borrow chain crossing more than
 *       one boundary is only reachable here.
 * @note 4 raised to the lane count: 65536 arrangements on an eight lane word, 256 on a four lane one
 *       and 16 on a two lane one. Every environment sweeps its own width in full.
 * @note The thresholds are the four boundaries themselves. All 128 of them would keep the pass well
 *       past a second, and the four cover each end of the range the candidates state.
 */
void test_both_candidates_answer_over_every_boundary_word(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];
    unsigned long arrangements = 1ul;

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        arrangements *= MMGR_ACCURACY_EDGE_VALUES;
    }

    for (unsigned long arrangement = 0ul; arrangement < arrangements; arrangement++)
    {
        unsigned long remaining = arrangement;

        for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
        {
            lane_byte_of[lane_index] = accuracy_edge_of[remaining % MMGR_ACCURACY_EDGE_VALUES];
            remaining /= MMGR_ACCURACY_EDGE_VALUES;
        }

        const embed_word assembled = accuracy_word_from(lane_byte_of);

        for (unsigned edge = 0u; edge < MMGR_ACCURACY_EDGE_VALUES; edge++)
        {
            const uint8_t threshold = accuracy_edge_of[edge];

            // 0x80 and 0xFF are past the bound both candidates state, so they are not asked for
            if (threshold >= 0x80u)
            {
                continue;
            }
            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] >= threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)accuracy_candidate_ge_or(assembled, threshold),
                                            "the at-or-above candidate disagrees on a boundary word");

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] <= threshold);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)accuracy_candidate_le_mask(assembled, threshold),
                                            "the at-or-below candidate disagrees on a boundary word");
        }
    }
}

/**
 * @brief Checks eq for every byte value against every sought byte, with contrasting neighbors.
 *
 * @note The neighbors are 0x00 and 0xFF. A sought byte of either one makes several lanes match at
 *       once, which is the case where a mask built from a subtraction can set a lane its own byte
 *       never earned.
 */
void test_eq_answers_per_lane_with_contrasting_neighbors(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (unsigned byte_value = 0u; byte_value < 256u; byte_value++)
    {
        for (unsigned sought = 0u; sought < 256u; sought++)
        {
            // Explicit casts narrow the loop counters to the bytes a lane holds, as in the case above
            accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, 0u);

            const embed_word assembled = accuracy_word_from(lane_byte_of);

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)(lane_byte_of[lane_index] == (uint8_t)sought);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)EMBED_CALL(lane.eq, ScrutLaneCfg, .word = assembled,
                                                                 .byte = (uint8_t)sought, .ci = EMBED_FALSE),
                                            "eq answered for a lane other than the one holding the byte");
        }
    }
}

/**
 * @brief Checks has_zero for a zero placed in each lane in turn, with the rest holding 0xFF.
 *
 * @note A zero lane beside a full one is the hardest case for a test built on a subtraction, since
 *       the neighbor is the value most able to lend a borrow.
 * @note Also covers the word with no zero lane at all, where the answer is an empty mask.
 */
void test_has_zero_finds_the_zero_lane_and_no_other(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (size_t zero_at = 0u; zero_at < sizeof(embed_word); zero_at++)
    {
        for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
        {
            lane_byte_of[lane_index] = 0xFFu;
            lane_matched_of[lane_index] = 0u;
        }
        lane_byte_of[zero_at] = 0x00u;
        lane_matched_of[zero_at] = 1u;

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(
            (uint64_t)accuracy_mask_from(lane_matched_of),
            (uint64_t)EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = accuracy_word_from(lane_byte_of)),
            "has_zero named a lane that was not the zero one");
    }

    for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
    {
        lane_byte_of[lane_index] = 0xFFu;
    }
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0u, (uint64_t)EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = accuracy_word_from(lane_byte_of)),
        "a word with no zero lane reported one");
}

/**
 * @brief Checks alpha, any_upper and any_digit for every byte value, in every lane position.
 *
 * @note The reference is the byte range each entry documents. alpha is the two letter runs, any_upper
 *       is the 0x40 block with bit seven disregarded, and any_digit is the 0x30 block.
 * @note Sweeping the lane position as well as the value is what separates a predicate that is right
 *       from one that is right only in the lane the constants happen to favor.
 */
void test_the_character_predicates_answer_per_lane_for_every_byte(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_matched_of[sizeof(embed_word)];

    for (size_t at_lane = 0u; at_lane < sizeof(embed_word); at_lane++)
    {
        for (unsigned byte_value = 0u; byte_value < 256u; byte_value++)
        {
            // Explicit cast narrows the loop counter to the byte a lane holds, as in the cases above
            accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, at_lane);

            const embed_word assembled = accuracy_word_from(lane_byte_of);

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                const uint8_t held = lane_byte_of[lane_index];

                lane_matched_of[lane_index] =
                    (uint8_t)(((held >= 0x41u) && (held <= 0x5Au)) || ((held >= 0x61u) && (held <= 0x7Au)));
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)EMBED_CALL(lane.alpha, ScrutLaneCfg, .word = assembled),
                                            "alpha disagrees with the two letter ranges");

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                lane_matched_of[lane_index] = (uint8_t)((lane_byte_of[lane_index] & MMGR_FAM_CS) == MMGR_FAM_CI);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)EMBED_CALL(lane.any_upper, ScrutLaneCfg, .word = assembled),
                                            "any_upper disagrees with the block it gates on");

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                const uint8_t held = lane_byte_of[lane_index];

                lane_matched_of[lane_index] = (uint8_t)((held >= 0x30u) && (held <= 0x3Fu));
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)accuracy_mask_from(lane_matched_of),
                                            (uint64_t)EMBED_CALL(lane.any_digit, ScrutLaneCfg, .word = assembled),
                                            "any_digit disagrees with the block it gates on");
        }
    }
}

/**
 * @brief Checks fold_lower for every byte value, in every lane position.
 *
 * @note The reference lowers a byte in the capital range and leaves every other byte as it stands,
 *       which is what the entry documents. A fold that reached past the letters changes a byte here
 *       that the reference does not.
 * @note The result is a whole word and not a lane mask, so the comparison is against a word built
 *       from the folded bytes.
 */
void test_fold_lower_changes_the_letters_and_leaves_every_other_byte(void)
{
    uint8_t lane_byte_of[sizeof(embed_word)];
    uint8_t lane_folded_of[sizeof(embed_word)];

    for (size_t at_lane = 0u; at_lane < sizeof(embed_word); at_lane++)
    {
        for (unsigned byte_value = 0u; byte_value < 256u; byte_value++)
        {
            // Explicit cast narrows the loop counter to the byte a lane holds, as in the cases above
            accuracy_fill_contrasting(lane_byte_of, (uint8_t)byte_value, at_lane);

            for (size_t lane_index = 0u; lane_index < sizeof(embed_word); lane_index++)
            {
                const uint8_t held = lane_byte_of[lane_index];

                // Explicit cast narrows the lowered value to the byte a lane holds. Setting bit five
                // moves a capital to its lowercase and is applied to nothing else
                lane_folded_of[lane_index] =
                    (uint8_t)(((held >= 0x41u) && (held <= 0x5Au)) ? (uint8_t)(held | 0x20u) : held);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(
                (uint64_t)accuracy_word_from(lane_folded_of),
                (uint64_t)EMBED_CALL(word.fold_lower, ScrutWordCfg, .word = accuracy_word_from(lane_byte_of)),
                "fold_lower changed a byte the letter ranges do not cover");
        }
    }
}
