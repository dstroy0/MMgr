// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The SWAR core. Every case here is written against MMGR_SWAR_BYTES rather than against eight, so
// the same file asserts the same claims at 16, 32 and 64 bits. A case that has to name a lane
// names lane 0, which exists at every width.
//
// Where a lane operation has a plain scalar equivalent, the scalar loop is the oracle: it is
// obvious by inspection in a way the bit trick is not, and that is the whole reason for checking
// one against the other.
#include "unity.h"

#include "verbum_scrutor/verbum_scrutor.h"

/** @brief Broadcast one byte into every lane. */
static mmgr_scrut_word all(uint8_t c)
{
    return (mmgr_scrut_word)(MMGR_SWAR_ONES * (mmgr_scrut_word)c);
}

/** @brief Build a word from a byte per lane, lane 0 first in address order. */
static mmgr_scrut_word lanes_of(const uint8_t *b)
{
    char tmp[MMGR_SWAR_BYTES];
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        tmp[i] = (char)b[i];
    }
    return mmgr_scrut_load(tmp);
}

/** @brief The high bit of lane @p i, counted in address order. */
static mmgr_scrut_word lane_bit(size_t i)
{
    uint8_t b[MMGR_SWAR_BYTES];
    for (size_t k = 0; k < MMGR_SWAR_BYTES; k++)
    {
        b[k] = (k == i) ? 0x80u : 0x00u;
    }
    return lanes_of(b);
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
    const VerbumScrutorNs *ns = &scrut;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(VerbumScrutorNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}

void test_the_word_constants_agree_with_each_other(void)
{
    TEST_ASSERT_EQUAL_size_t(sizeof(mmgr_scrut_word), MMGR_SWAR_BYTES);
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES * 8u, MMGR_SWAR_LANE_BITS);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)(MMGR_SWAR_ONES * 0x80u), (uint64_t)MMGR_VERBUM_SCRUTOR_HIGH,
                                    "the high mask is not one bit per lane");
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_scrut_word) ~(mmgr_scrut_word)0,
                            (uint64_t)(mmgr_scrut_word)(MMGR_VERBUM_SCRUTOR_HIGH | MMGR_SWAR_LOW7));
}

/* ---------------------------------------------------------------------------------------------
 * comparison
 * ------------------------------------------------------------------------------------------- */

void test_ge_and_le_against_a_scalar_loop(void)
{
    // Both take the threshold as a plain byte and broadcast it themselves, so the caller hands
    // over a value and not a word.
    //
    // The range stops below 0x80. These are borrow tricks: the compare sets the lane's high bit
    // and then reads it back, so a lane that already had its high bit set has nothing left to
    // carry the answer. Every caller in the library is comparing text, which is why the limit has
    // never cost anything.
    for (unsigned t = 0; t < 0x80u; t += 7u)
    {
        for (unsigned v = 0; v < 0x80u; v++)
        {
            const mmgr_scrut_word w = all((uint8_t)v);

            TEST_ASSERT_EQUAL_INT_MESSAGE(v >= t, scrut.ge(w, (mmgr_scrut_word)t) != 0, "ge disagrees with >=");
            TEST_ASSERT_EQUAL_INT_MESSAGE(v <= t, scrut.le(w, (mmgr_scrut_word)t) != 0, "le disagrees with <=");
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
    const mmgr_scrut_word w = lanes_of(b);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES - 1u, mmgr_scrut_lanes(scrut.ge(w, 0x20u)),
                                     "one lane is below the threshold and the rest are not");
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_lanes(scrut.le(w, 0x20u)));
}

void test_sub7_is_a_per_lane_difference(void)
{
    for (unsigned v = 0; v < 256u; v++)
    {
        for (unsigned lo = 0; lo < 256u; lo += 17u)
        {
            const mmgr_scrut_word got = scrut.sub7(all((uint8_t)v), all((uint8_t)lo));
            const unsigned want = (v - lo) & 0x7Fu;
            TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)want, (uint8_t)(got & 0xFFu), "sub7 disagrees with a scalar -");
        }
    }
}

void test_spread_fills_a_lane_from_its_high_bit(void)
{
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_scrut_word) ~(mmgr_scrut_word)0,
                            (uint64_t)scrut.spread(MMGR_VERBUM_SCRUTOR_HIGH));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)scrut.spread(0u));

    const mmgr_scrut_word one = scrut.spread(lane_bit(0u));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, (uint8_t)(mmgr_scrut_load("\0\0\0\0\0\0\0\0") | (one & 0xFFu)),
                                   "the set lane did not fill");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_scrut_lanes(one & MMGR_VERBUM_SCRUTOR_HIGH),
                                     "spread lit a lane that was not set");
}

/* ---------------------------------------------------------------------------------------------
 * search
 * ------------------------------------------------------------------------------------------- */

void test_has_zero_finds_a_zero_lane_and_only_a_zero_lane(void)
{
    TEST_ASSERT_TRUE(scrut.has_zero(0u) != 0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)scrut.has_zero(all(1u)), "a word with no zero lane reported one");

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        uint8_t b[MMGR_SWAR_BYTES];
        for (size_t k = 0; k < MMGR_SWAR_BYTES; k++)
        {
            b[k] = (k == i) ? 0x00u : 0x41u;
        }
        const mmgr_scrut_word m = scrut.has_zero(lanes_of(b));
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_scrut_lanes(m), "exactly one lane was zero");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(i, scrut.zero_lane(m), "the wrong lane came back");
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
        const mmgr_scrut_word m = scrut.eq(lanes_of(b), (uint8_t)'x', MMGR_FALSE);
        TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_lanes(m));
        TEST_ASSERT_EQUAL_size_t(i, scrut.zero_lane(m));
    }
}

void test_eq_ignoring_case_matches_either_case(void)
{
    const mmgr_scrut_word upper = all((uint8_t)'A');
    const mmgr_scrut_word lower = all((uint8_t)'a');

    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, mmgr_scrut_lanes(scrut.eq(upper, (uint8_t)'a', MMGR_TRUE)));
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, mmgr_scrut_lanes(scrut.eq(lower, (uint8_t)'A', MMGR_TRUE)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_scrut_lanes(scrut.eq(upper, (uint8_t)'a', MMGR_FALSE)),
                                     "matching case must not fold");

    // Folding must not reach past the letters: '{' is 'z' + 1 and must not answer to '['.
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_scrut_lanes(scrut.eq(all((uint8_t)'{'), (uint8_t)'[', MMGR_TRUE)),
                                     "the fold leaked past the letters");
}

void test_xor_reports_the_lanes_that_differ(void)
{
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)scrut.xor_(all(0x41u), all(0x41u), MMGR_FALSE));
    TEST_ASSERT_TRUE(scrut.xor_(all(0x41u), all(0x42u), MMGR_FALSE) != 0u);

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)scrut.xor_(all(0x41u), all(0x61u), MMGR_TRUE),
                                    "a case difference is no difference once case stops counting");
    TEST_ASSERT_TRUE(scrut.xor_(all(0x41u), all(0x61u), MMGR_FALSE) != 0u);
    TEST_ASSERT_TRUE_MESSAGE(scrut.xor_(all((uint8_t)'1'), all((uint8_t)'2'), MMGR_TRUE) != 0u,
                             "digits do not fold into each other");
}

/* ---------------------------------------------------------------------------------------------
 * lane arithmetic
 * ------------------------------------------------------------------------------------------- */

void test_lanes_counts_set_lanes(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_lanes(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, mmgr_scrut_lanes(MMGR_VERBUM_SCRUTOR_HIGH));

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_scrut_lanes(lane_bit(i)), "one lane set should count one");
    }
}

void test_lane_lo_and_lane_hi_find_the_ends(void)
{
    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        const mmgr_scrut_word one = lane_bit(i);
        TEST_ASSERT_EQUAL_size_t(mmgr_scrut_lane_lo(one), mmgr_scrut_lane_hi(one));
    }

    // Two lanes set at once is where a borrow that is not masked stops at the wrong bit.
    const mmgr_scrut_word two = (mmgr_scrut_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));
    TEST_ASSERT_NOT_EQUAL_size_t_MESSAGE(mmgr_scrut_lane_lo(two), mmgr_scrut_lane_hi(two),
                                         "the two ends of a two lane mask are the same lane");
}

void test_lane_first_and_last_follow_address_order(void)
{
    const mmgr_scrut_word two = (mmgr_scrut_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_scrut_lane_first(two), "the first lane is the one at p+0");
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES - 1u, mmgr_scrut_lane_last(two));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, scrut.zero_lane(two), "zero_lane and lane_first are the same answer");
}

void test_drop_lo_and_drop_hi_clear_one_lane_each(void)
{
    const mmgr_scrut_word two = (mmgr_scrut_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_lanes(mmgr_scrut_drop_lo(two)));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_lanes(mmgr_scrut_drop_hi(two)));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_drop_lo(lane_bit(0u)));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_drop_hi(lane_bit(0u)));
}

void test_drop_first_and_last_follow_address_order(void)
{
    const mmgr_scrut_word two = (mmgr_scrut_word)(lane_bit(0u) | lane_bit(MMGR_SWAR_BYTES - 1u));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES - 1u, mmgr_scrut_lane_first(mmgr_scrut_drop_first(two)),
                                     "dropping the first lane leaves the last");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_lane_first(mmgr_scrut_drop_last(two)));
}

void test_walking_a_mask_visits_every_lane_once(void)
{
    mmgr_scrut_word m = MMGR_VERBUM_SCRUTOR_HIGH;
    size_t seen = 0;

    while (m != 0u)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(seen, scrut.zero_lane(m), "the walk skipped a lane");
        m = mmgr_scrut_drop_first(m);
        seen++;
    }
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, seen);
}

/* ---------------------------------------------------------------------------------------------
 * bounds
 * ------------------------------------------------------------------------------------------- */

void test_words_is_the_load_count(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_words(0u));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_words(1u));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_words(MMGR_SWAR_BYTES));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, mmgr_scrut_words(MMGR_SWAR_BYTES + 1u), "one byte over needs a second load");
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_scrut_words(MMGR_SWAR_BYTES * 2u));
}

void test_the_worst_case_word_count_covers_the_largest_tenant(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SCAN_MAX_WORDS, mmgr_scrut_words(MMGR_CONFIN_MAX));
}

void test_bytes_below_keeps_the_first_n_lanes(void)
{
    // The count starts at one. Zero would shift by the full word width, which C leaves undefined,
    // and no caller can reach it: find returns early on an empty needle, tail_mask's last word
    // always has at least one byte in it, and run_edge's argument is capped at the word size.
    TEST_ASSERT_EQUAL_HEX64((uint64_t)(mmgr_scrut_word) ~(mmgr_scrut_word)0,
                            (uint64_t)mmgr_scrut_bytes_below(MMGR_SWAR_BYTES));
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)(mmgr_scrut_word) ~(mmgr_scrut_word)0,
                                    (uint64_t)mmgr_scrut_bytes_below(MMGR_SWAR_BYTES + 99u),
                                    "asking for more lanes than there are keeps them all");

    // The kept lanes are the ones at the low addresses, whichever end of the word those sit at.
    const mmgr_scrut_word one = mmgr_scrut_bytes_below(1u);
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_scrut_lanes(one & MMGR_VERBUM_SCRUTOR_HIGH));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_lane_first(one & MMGR_VERBUM_SCRUTOR_HIGH));
}

void test_lanes_below_is_the_high_bit_of_bytes_below(void)
{
    // From one, for the reason bytes_below's case gives.
    for (size_t n = 1; n <= MMGR_SWAR_BYTES; n++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(n, mmgr_scrut_lanes(mmgr_scrut_lanes_below(n)), "the wrong number of lanes");
    }
}

void test_tail_mask_keeps_everything_but_the_last_word(void)
{
    const size_t cap = (MMGR_SWAR_BYTES * 2u) + 1u;

    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, mmgr_scrut_lanes(mmgr_scrut_tail_mask(cap, 0u)));
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES, mmgr_scrut_lanes(mmgr_scrut_tail_mask(cap, 1u)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_scrut_lanes(mmgr_scrut_tail_mask(cap, 2u)),
                                     "the last word of a ragged cap keeps only the bytes that are there");
}

void test_lanes_before_drops_everything_from_the_first_hit_on(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES, mmgr_scrut_lanes(mmgr_scrut_lanes_before(0u)),
                                     "no hit means nothing is dropped");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_scrut_lanes(mmgr_scrut_lanes_before(lane_bit(0u))),
                                     "a hit in the first lane leaves nothing before it");

    for (size_t i = 0; i < MMGR_SWAR_BYTES; i++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(i, mmgr_scrut_lanes(mmgr_scrut_lanes_before(lane_bit(i))),
                                         "the count of lanes before a hit is its address order index");
    }
}

/* ---------------------------------------------------------------------------------------------
 * families
 * ------------------------------------------------------------------------------------------- */

void test_fam_eq_selects_a_block(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES,
                             mmgr_scrut_lanes(mmgr_scrut_fam_eq(all((uint8_t)'A'), MMGR_FAM_CS, 0x40u)));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_lanes(mmgr_scrut_fam_eq(all((uint8_t)'a'), MMGR_FAM_CS, 0x40u)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES,
                                     mmgr_scrut_lanes(mmgr_scrut_fam_eq(all((uint8_t)'a'), MMGR_FAM_CI, 0x40u)),
                                     "the case insensitive mask ignores the bit that tells the two blocks apart");
}

void test_any_upper_is_a_gate_and_not_a_test(void)
{
    TEST_ASSERT_TRUE(mmgr_scrut_any_upper(all((uint8_t)'A')) != 0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)mmgr_scrut_any_upper(all((uint8_t)'a')),
                                    "a word of lower case has no upper case in it");
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_any_upper(all((uint8_t)'0')));

    // Documented as over-broad in the safe direction: the whole 0x40 block answers yes.
    TEST_ASSERT_TRUE_MESSAGE(mmgr_scrut_any_upper(all((uint8_t)'_')) != 0u, "the gate is meant to be a superset");
}

void test_any_digit_is_a_gate_and_not_a_test(void)
{
    TEST_ASSERT_TRUE(mmgr_scrut_any_digit(all((uint8_t)'5')) != 0u);
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_any_digit(all((uint8_t)'a')));
    TEST_ASSERT_TRUE_MESSAGE(mmgr_scrut_any_digit(all((uint8_t)';')) != 0u, "the gate is meant to be a superset");
}

void test_alpha_is_exact_where_the_gates_are_not(void)
{
    for (unsigned c = 0; c < 128u; c++)
    {
        const int want = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const int got = mmgr_scrut_alpha(all((uint8_t)c)) != 0u;
        TEST_ASSERT_EQUAL_INT_MESSAGE(want, got, "alpha disagrees with a scalar range check");
    }

    // The characters the gates are wrong about are the point of alpha existing.
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_alpha(all((uint8_t)'_')));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_alpha(all((uint8_t)'{')));
    TEST_ASSERT_EQUAL_HEX64(0u, (uint64_t)mmgr_scrut_alpha(all((uint8_t)'@')));
}

void test_fold_lower_touches_letters_and_nothing_else(void)
{
    for (unsigned c = 0; c < 256u; c++)
    {
        const unsigned want = (c >= 'A' && c <= 'Z') ? (c | 0x20u) : c;
        const mmgr_scrut_word got = mmgr_scrut_fold_lower(all((uint8_t)c));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)want, (uint8_t)(got & 0xFFu),
                                       "the fold changed a byte it had no business changing");
    }
}

/* ---------------------------------------------------------------------------------------------
 * runs
 * ------------------------------------------------------------------------------------------- */

void test_run_of_one_is_the_mask_itself(void)
{
    TEST_ASSERT_EQUAL_HEX64((uint64_t)MMGR_VERBUM_SCRUTOR_HIGH, (uint64_t)mmgr_scrut_run(MMGR_VERBUM_SCRUTOR_HIGH, 1u));
}

void test_run_keeps_only_the_lanes_a_run_starts_at(void)
{
    const mmgr_scrut_word full = MMGR_VERBUM_SCRUTOR_HIGH;

    for (size_t n = 1u; n <= MMGR_SWAR_BYTES; n++)
    {
        const mmgr_scrut_word m = mmgr_scrut_run(full, n);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_SWAR_BYTES - n + 1u, mmgr_scrut_lanes(m),
                                         "a full word holds one run of n at each lane that has n lanes left");
    }
}

void test_run_of_a_broken_mask(void)
{
    // Every lane but the last, so a run of two cannot reach the end of the word.
    const mmgr_scrut_word broken = (mmgr_scrut_word)(MMGR_VERBUM_SCRUTOR_HIGH & ~lane_bit(MMGR_SWAR_BYTES - 1u));
    const mmgr_scrut_word m = mmgr_scrut_run(broken, 2u);

    TEST_ASSERT_EQUAL_size_t(MMGR_SWAR_BYTES > 2u ? MMGR_SWAR_BYTES - 2u : 0u, mmgr_scrut_lanes(m));
}

void test_run_edge_names_the_lanes_a_run_cannot_be_tested_at(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)mmgr_scrut_run_edge(1u), "a run of one always fits");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)mmgr_scrut_run_edge(0u), "a run of nothing always fits");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, (uint64_t)mmgr_scrut_run_edge(MMGR_SWAR_BYTES + 1u),
                                    "a run wider than the word is not this function's business");

    for (size_t n = 2u; n <= MMGR_SWAR_BYTES; n++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(n - 1u, mmgr_scrut_lanes(mmgr_scrut_run_edge(n)),
                                         "the wrong number of lanes are too near the end");
    }
}

/* ---------------------------------------------------------------------------------------------
 * loads
 * ------------------------------------------------------------------------------------------- */

void test_load_reads_a_word_at_any_alignment(void)
{
    static const char text[] = "0123456789abcdefghijklmnop";

    for (size_t off = 0; off < 9u; off++)
    {
        const mmgr_scrut_word w = scrut.load(text + off);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)text[off], (uint8_t)(w & 0xFFu),
                                       "lane 0 does not hold the byte at p+0");
    }
}

void test_load_al_reads_a_word_from_an_aligned_address(void)
{
    _Alignas(8) static const char text[16] = "0123456789abcde";

    TEST_ASSERT_EQUAL_HEX64((uint64_t)scrut.load(text), (uint64_t)scrut.load_al(text));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)scrut.load(text + 8), (uint64_t)scrut.load_al(text + 8));
}

void test_a_load_puts_the_first_byte_in_the_first_lane(void)
{
    static const char text[] = "abcdefghij";
    const mmgr_scrut_word w = scrut.load(text);

    // Address order, not bit order: this is the fact every endian derivation in the header rests
    // on, so it is asserted directly rather than assumed.
    const mmgr_scrut_word hit = scrut.eq(w, (uint8_t)'a', MMGR_FALSE);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_scrut_lane_first(hit));
    TEST_ASSERT_EQUAL_size_t(0u, scrut.zero_lane(hit));
}
