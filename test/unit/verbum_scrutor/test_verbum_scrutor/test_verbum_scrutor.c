/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "verbum_scrutor/verbum_scrutor.h"

static mmgr_word all(uint8_t c)
{
    return (mmgr_word)(MMGR_SWAR_ONES * (mmgr_word)c);
}

static mmgr_word lanes_of(const uint8_t *b)
{
    char tmp[MMGR_SWAR_BYTES];
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        tmp[i] = (char)b[i];
    }
    return MMGR_CALL(word.load, ScrutWordCfg, .at = tmp);
}

static mmgr_word lane_bit(size_t i)
{
    uint8_t b[MMGR_SWAR_BYTES];
    for (size_t k = 0; k < MMGR_SWAR_BYTES; k++)
    {
        b[k] = (k == i) ? 0x80u : 0x00u;
    }
    return lanes_of(b);
}

static size_t lanes(mmgr_word m)
{
    return MMGR_CALL(lane.count, ScrutLaneCfg, .mask = m);
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_scrut_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("verbum_scrutor.h compiled with no header before it");
}

void test_scrut_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ScrutLaneNs), sizeof lane, "the lane table is not its own type");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ScrutMaskNs), sizeof mask, "the mask table is not its own type");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ScrutWordNs), sizeof word, "the word table is not its own type");
}

void test_the_word_constants_agree_with_each_other(void)
{
    TEST_ASSERT_EQUAL_size_t(sizeof(mmgr_word), MMGR_SWAR_BYTES);
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES * 8u, MMGR_SWAR_LANE_BITS);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)(MMGR_SWAR_ONES * 0x80u), (uint64_t)MMGR_VERBUM_SCRUTOR_HIGH,
                                    "the high mask is not one bit per lane");
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_word) ~(mmgr_word)0,
                            (uint64_t)(mmgr_word)(MMGR_VERBUM_SCRUTOR_HIGH | MMGR_SWAR_LOW7));
}


void test_ge_and_le_against_a_scalar_loop(void)
{
    for (uint32_t t = 0; t < 0x80u; t += 7u)
    {
        for (uint32_t v = 0; v < 0x80u; v++)
        {
            const mmgr_word w = all((uint8_t)v);

            TEST_ASSERT_EQUAL_INT_MESSAGE(v >= t, MMGR_CALL(lane.ge, ScrutLaneCfg, .word = w, .byte = (uint8_t)t) != 0,
                                          "ge disagrees with >=");
            TEST_ASSERT_EQUAL_INT_MESSAGE(v <= t, MMGR_CALL(lane.le, ScrutLaneCfg, .word = w, .byte = (uint8_t)t) != 0,
                                          "le disagrees with <=");
        }
    }
}

void test_ge_and_le_set_only_the_lanes_that_pass(void)
{
    uint8_t b[MMGR_SWAR_BYTES];
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        b[i] = (i == 0u) ? 0x10u : 0x40u;
    }
    const mmgr_word w = lanes_of(b);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES - 1u,
                                     lanes(MMGR_CALL(lane.ge, ScrutLaneCfg, .word = w, .byte = 0x20u)),
                                     "one lane is below the threshold and the rest are not");
    TEST_ASSERT_EQUAL_size_t(1u, lanes(MMGR_CALL(lane.le, ScrutLaneCfg, .word = w, .byte = 0x20u)));
}

void test_sub7_is_a_per_lane_difference(void)
{
    for (uint32_t v = 0; v < 256u; v++)
    {
        for (uint32_t lo = 0; lo < 256u; lo += 17u)
        {
            const mmgr_word got =
                MMGR_CALL(lane.sub7, ScrutLaneCfg, .word = all((uint8_t)v), .byte = (uint8_t)lo);
            const uint32_t want = (v - lo) & 0x7Fu;
            TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)want, (uint8_t)(got & 0xFFu), "sub7 disagrees with a scalar -");
        }
    }
}

void test_spread_fills_a_lane_from_its_high_bit(void)
{
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_word) ~(mmgr_word)0,
                            (uint64_t)MMGR_CALL(mask.spread, ScrutMaskCfg, .mask = MMGR_VERBUM_SCRUTOR_HIGH));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(mask.spread, ScrutMaskCfg, .mask = 0u));

    const mmgr_word one = MMGR_CALL(mask.spread, ScrutMaskCfg, .mask = lane_bit(0u));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(
        0xFFu,
        (uint8_t)(MMGR_CALL(word.load, ScrutWordCfg, .at = "\0\0\0\0\0\0\0\0") | (one & 0xFFu)),
        "the set lane did not fill");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, lanes(one & MMGR_VERBUM_SCRUTOR_HIGH), "spread lit a lane that was not set");
}


void test_has_zero_finds_a_zero_lane_and_only_a_zero_lane(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = 0u) != 0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = all(1u)),
                                    "a word with no zero lane reported one");

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        uint8_t b[MMGR_SWAR_BYTES];
        for (size_t k = 0; k < MMGR_SWAR_BYTES; k++)
        {
            b[k] = (k == i) ? 0x00u : 0x41u;
        }
        const mmgr_word m = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = lanes_of(b));
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, lanes(m), "exactly one lane was zero");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(i, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m), "the wrong lane came back");
    }
}

void test_eq_finds_a_byte_at_every_lane(void)
{
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        uint8_t b[MMGR_SWAR_BYTES];
        for (size_t k = 0; k < MMGR_SWAR_BYTES; k++)
        {
            b[k] = (k == i) ? 'x' : 'a';
        }
        const mmgr_word m =
            MMGR_CALL(lane.eq, ScrutLaneCfg, .word = lanes_of(b), .byte = (uint8_t)'x', .ci = MMGR_FALSE);
        TEST_ASSERT_EQUAL_size_t(1u, lanes(m));
        TEST_ASSERT_EQUAL_size_t(i, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m));
    }
}

void test_eq_ignoring_case_matches_either_case(void)
{
    const mmgr_word upper = all((uint8_t)'A');
    const mmgr_word lower = all((uint8_t)'a');

    TEST_ASSERT_EQUAL_size_t(
        MMGR_SWAR_BYTES,
        lanes(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = upper, .byte = (uint8_t)'a', .ci = MMGR_TRUE)));
    TEST_ASSERT_EQUAL_size_t(
        MMGR_SWAR_BYTES,
        lanes(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = lower, .byte = (uint8_t)'A', .ci = MMGR_TRUE)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        0u, lanes(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = upper, .byte = (uint8_t)'a', .ci = MMGR_FALSE)),
        "matching case must not fold");

    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        0u,
        lanes(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = all((uint8_t)'{'), .byte = (uint8_t)'[', .ci = MMGR_TRUE)),
        "the fold leaked past the letters");
}

void test_xor_reports_the_lanes_that_differ(void)
{
    TEST_ASSERT_EQUAL_HEX64(
        0u,
        (uint64_t)MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = all(0x41u), .val = all(0x41u), .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = all(0x41u), .val = all(0x42u), .ci = MMGR_FALSE) != 0u);

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0u, (uint64_t)MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = all(0x41u), .val = all(0x61u), .ci = MMGR_TRUE),
        "a case difference is no difference once case stops counting");
    TEST_ASSERT_TRUE(MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = all(0x41u), .val = all(0x61u), .ci = MMGR_FALSE) != 0u);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = all((uint8_t)'1'), .val = all((uint8_t)'2'),
                                       .ci = MMGR_TRUE) != 0u,
                             "digits do not fold into each other");
}


void test_lanes_counts_set_lanes(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, lanes(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, lanes(MMGR_VERBUM_SCRUTOR_HIGH));

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, lanes(lane_bit(i)), "one lane set should count one");
    }
}

void test_lane_lo_and_lane_hi_find_the_ends(void)
{
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        const mmgr_word one = lane_bit(i);
        TEST_ASSERT_EQUAL_size_t(MMGR_CALL(mmgr_scrut_lane_lo, ScrutLaneCfg, .mask = one),
                                 MMGR_CALL(mmgr_scrut_lane_hi, ScrutLaneCfg, .mask = one));
    }

    const mmgr_word two = (mmgr_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));
    TEST_ASSERT_NOT_EQUAL_size_t_MESSAGE(MMGR_CALL(mmgr_scrut_lane_lo, ScrutLaneCfg, .mask = two),
                                         MMGR_CALL(mmgr_scrut_lane_hi, ScrutLaneCfg, .mask = two),
                                         "the two ends of a two lane mask are the same lane");
}

void test_lane_first_and_last_follow_address_order(void)
{
    const mmgr_word two = (mmgr_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = two),
                                     "the first lane is the one at p+0");
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES - 1u, MMGR_CALL(lane.last, ScrutLaneCfg, .mask = two));
}

void test_first_and_last_agree_on_an_empty_mask(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = 0u),
                                     "an empty mask has no first lane, so the answer is out of range");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES, MMGR_CALL(lane.last, ScrutLaneCfg, .mask = 0u),
                                     "and last must say the same thing first does");
}

void test_drop_lo_and_drop_hi_clear_one_lane_each(void)
{
    const mmgr_word two = (mmgr_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t(1u, lanes(MMGR_CALL(mmgr_scrut_drop_lo, ScrutMaskCfg, .mask = two)));
    TEST_ASSERT_EQUAL_size_t(1u, lanes(MMGR_CALL(mmgr_scrut_drop_hi, ScrutMaskCfg, .mask = two)));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(mmgr_scrut_drop_lo, ScrutMaskCfg, .mask = lane_bit(0u)));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(mmgr_scrut_drop_hi, ScrutMaskCfg, .mask = lane_bit(0u)));
}

void test_drop_first_and_last_follow_address_order(void)
{
    const mmgr_word two = (mmgr_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        MMGR_SWAR_BYTES - 1u,
        MMGR_CALL(lane.first, ScrutLaneCfg, .mask = MMGR_CALL(mask.drop_first, ScrutMaskCfg, .mask = two)),
        "dropping the first lane leaves the last");
    TEST_ASSERT_EQUAL_size_t(
        0u, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = MMGR_CALL(mask.drop_last, ScrutMaskCfg, .mask = two)));
}

void test_walking_a_mask_visits_every_lane_once(void)
{
    mmgr_word m = MMGR_VERBUM_SCRUTOR_HIGH;
    size_t seen = 0;

    while (m != 0u)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(seen, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m), "the walk skipped a lane");
        m = MMGR_CALL(mask.drop_first, ScrutMaskCfg, .mask = m);
        seen++;
    }
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, seen);
}


void test_words_is_the_load_count(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(word.count, ScrutWordCfg, .bytes = 0u));
    TEST_ASSERT_EQUAL_size_t(1u, MMGR_CALL(word.count, ScrutWordCfg, .bytes = 1u));
    TEST_ASSERT_EQUAL_size_t(1u, MMGR_CALL(word.count, ScrutWordCfg, .bytes = MMGR_SWAR_BYTES));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, MMGR_CALL(word.count, ScrutWordCfg, .bytes = MMGR_SWAR_BYTES + 1u),
                                     "one byte over needs a second load");
    TEST_ASSERT_EQUAL_size_t(2u, MMGR_CALL(word.count, ScrutWordCfg, .bytes = MMGR_SWAR_BYTES * 2u));
}

void test_the_worst_case_word_count_covers_the_largest_tenant(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SCAN_MAX_WORDS, MMGR_CALL(word.count, ScrutWordCfg, .bytes = MMGR_CARCER_MAX));
}

void test_bytes_below_keeps_the_first_n_lanes(void)
{
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_word) ~(mmgr_word)0,
                            (uint64_t)MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = MMGR_SWAR_BYTES));
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)(mmgr_word) ~(mmgr_word)0,
                                    (uint64_t)MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = MMGR_SWAR_BYTES + 99u),
                                    "asking for more lanes than there are keeps them all");

    const mmgr_word one = MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = 1u);
    TEST_ASSERT_EQUAL_size_t(1u, lanes(one & MMGR_VERBUM_SCRUTOR_HIGH));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = one & MMGR_VERBUM_SCRUTOR_HIGH));
}

void test_bytes_below_nothing_is_an_empty_mask(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = 0u),
                                    "there are no bytes below index zero");
}

void test_lanes_below_is_the_high_bit_of_bytes_below(void)
{
    for (size_t n = 1; n <= MMGR_SWAR_BYTES; n++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(n, lanes(MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = n)),
                                         "the wrong number of lanes");
    }
}

void test_tail_mask_keeps_everything_but_the_last_word(void)
{
    const size_t cap = (MMGR_SWAR_BYTES * 2u) + 1u;

    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, lanes(MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = cap, .wi = 0u)));
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, lanes(MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = cap, .wi = 1u)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, lanes(MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = cap, .wi = 2u)),
                                     "the last word of a ragged cap keeps only the bytes that are there");
}

void test_tail_mask_past_the_end_is_empty(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = 20u, .wi = 20u),
                                    "a word index past the span has no bytes left in it to keep");
}

void test_lanes_before_drops_everything_from_the_first_hit_on(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES, lanes(MMGR_CALL(mask.before, ScrutMaskCfg, .mask = 0u)),
                                     "no hit means nothing is dropped");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, lanes(MMGR_CALL(mask.before, ScrutMaskCfg, .mask = lane_bit(0u))),
                                     "a hit in the first lane leaves nothing before it");

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(i, lanes(MMGR_CALL(mask.before, ScrutMaskCfg, .mask = lane_bit(i))),
                                         "the count of lanes before a hit is its address order index");
    }
}


void test_fam_eq_selects_a_block(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, lanes(MMGR_CALL(lane.fam_eq, ScrutLaneCfg, .word = all((uint8_t)'A'),
                                                              .fam = MMGR_FAM_CS, .byte = 0x40u)));
    TEST_ASSERT_EQUAL_size_t(0u, lanes(MMGR_CALL(lane.fam_eq, ScrutLaneCfg, .word = all((uint8_t)'a'),
                                                 .fam = MMGR_FAM_CS, .byte = 0x40u)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES,
                                     lanes(MMGR_CALL(lane.fam_eq, ScrutLaneCfg, .word = all((uint8_t)'a'),
                                                     .fam = MMGR_FAM_CI, .byte = 0x40u)),
                                     "the case insensitive mask ignores the bit that tells the two blocks apart");
}

void test_any_upper_is_a_gate_and_not_a_test(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = all((uint8_t)'A')) != 0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = all((uint8_t)'a')),
                                    "a word of lower case has no upper case in it");
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = all((uint8_t)'0')));

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = all((uint8_t)'_')) != 0u,
                             "the gate is meant to be a superset");
}

void test_any_digit_is_a_gate_and_not_a_test(void)
{
    TEST_ASSERT_TRUE(MMGR_CALL(lane.any_digit, ScrutLaneCfg, .word = all((uint8_t)'5')) != 0u);
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(lane.any_digit, ScrutLaneCfg, .word = all((uint8_t)'a')));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(lane.any_digit, ScrutLaneCfg, .word = all((uint8_t)';')) != 0u,
                             "the gate is meant to be a superset");
}

void test_alpha_is_exact_where_the_gates_are_not(void)
{
    for (uint32_t c = 0; c < 128u; c++)
    {
        const int want = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const int got = MMGR_CALL(lane.alpha, ScrutLaneCfg, .word = all((uint8_t)c)) != 0u;
        TEST_ASSERT_EQUAL_INT_MESSAGE(want, got, "alpha disagrees with a scalar range check");
    }

    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(lane.alpha, ScrutLaneCfg, .word = all((uint8_t)'_')));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(lane.alpha, ScrutLaneCfg, .word = all((uint8_t)'{')));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)MMGR_CALL(lane.alpha, ScrutLaneCfg, .word = all((uint8_t)'@')));
}

void test_fold_lower_touches_letters_and_nothing_else(void)
{
    for (uint32_t c = 0; c < 256u; c++)
    {
        const uint32_t want = (c >= 'A' && c <= 'Z') ? (c | 0x20u) : c;
        const mmgr_word got = MMGR_CALL(word.fold_lower, ScrutWordCfg, .word = all((uint8_t)c));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)want, (uint8_t)(got & 0xFFu),
                                       "the fold changed a byte it had no business changing");
    }
}


void test_run_of_one_is_the_mask_itself(void)
{
    TEST_ASSERT_EQUAL_HEX64(
        (uint64_t)MMGR_VERBUM_SCRUTOR_HIGH,
        (uint64_t)MMGR_CALL(mask.run, ScrutMaskCfg, .mask = MMGR_VERBUM_SCRUTOR_HIGH, .bytes = 1u));
}

void test_run_keeps_only_the_lanes_a_run_starts_at(void)
{
    const mmgr_word full = MMGR_VERBUM_SCRUTOR_HIGH;

    for (size_t n = 1u; n <= MMGR_SWAR_BYTES; n++)
    {
        const mmgr_word m = MMGR_CALL(mask.run, ScrutMaskCfg, .mask = full, .bytes = n);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES - n + 1u, lanes(m),
                                         "a full word holds one run of n at each lane that has n lanes left");
    }
}

void test_run_of_a_broken_mask(void)
{
    const mmgr_word broken = (mmgr_word)(MMGR_VERBUM_SCRUTOR_HIGH & ~lane_bit(MMGR_SWAR_BYTES - 1u));
    const mmgr_word m = MMGR_CALL(mask.run, ScrutMaskCfg, .mask = broken, .bytes = 2u);

    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES > 2u ? MMGR_SWAR_BYTES - 2u : 0u, lanes(m));
}

void test_run_wider_than_the_word_is_refused(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0u,
        (uint64_t)MMGR_CALL(mask.run, ScrutMaskCfg, .mask = ~(mmgr_word)0, .bytes = MMGR_SWAR_BYTES + 1u),
        "a run that can't fit in one word has nowhere to start");
}

void test_run_edge_names_the_lanes_a_run_cannot_be_tested_at(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(mask.run_edge, ScrutMaskCfg, .bytes = 1u),
                                    "a run of one always fits");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(mask.run_edge, ScrutMaskCfg, .bytes = 0u),
                                    "a run of nothing always fits");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)MMGR_CALL(mask.run_edge, ScrutMaskCfg, .bytes = MMGR_SWAR_BYTES + 1u),
                                    "a run wider than the word is not this function's business");

    for (size_t n = 2u; n <= MMGR_SWAR_BYTES; n++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(n - 1u, lanes(MMGR_CALL(mask.run_edge, ScrutMaskCfg, .bytes = n)),
                                         "the wrong number of lanes are too near the end");
    }
}


void test_load_reads_a_word_at_any_alignment(void)
{
    static const char text[] = "0123456789abcdefghijklmnop";

    for (size_t off = 0; off < 9u; off++)
    {
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = text + off);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)text[off], (uint8_t)(w & 0xFFu),
                                       "lane 0 does not hold the byte at p+0");
    }
}

void test_load_al_reads_a_word_from_an_aligned_address(void)
{
    _Alignas(8) static const char text[16] = "0123456789abcde";

    TEST_ASSERT_EQUAL_HEX64((uint64_t)MMGR_CALL(word.load, ScrutWordCfg, .at = text),
                            (uint64_t)MMGR_CALL(word.load_al, ScrutWordCfg, .at = text));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)MMGR_CALL(word.load, ScrutWordCfg, .at = text + 8),
                            (uint64_t)MMGR_CALL(word.load_al, ScrutWordCfg, .at = text + 8));
}

void test_a_load_puts_the_first_byte_in_the_first_lane(void)
{
    static const char text[] = "abcdefghij";
    const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = text);

    const mmgr_word hit = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = (uint8_t)'a', .ci = MMGR_FALSE);
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(lane.first, ScrutLaneCfg, .mask = hit));
}
