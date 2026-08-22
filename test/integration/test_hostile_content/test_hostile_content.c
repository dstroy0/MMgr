// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Adversarial content, inside legitimate bounds.
//
// Every size this library takes is the caller's own and is bound at compile time - a tenant is
// MMGR_CARCER_SIZE, a scan is MMGR_CARCER_MAX, and a request that does not fit is a static assert
// and not a runtime case. So there is nothing to learn from feeding a nonsense size to an entry
// that was never going to be handed one. What does arrive from outside is the bytes, and this file
// is about the bytes.
//
// Two things are asserted after every operation. That the answer is right, checked against libc
// wherever libc has the same question. And that nothing outside the buffer moved: every buffer here
// sits in the middle of a larger array with a poison pattern on both sides, and the poison is
// checked afterwards. A one byte overrun writes into the poison rather than into whatever the
// linker put next, so it is reported rather than being a crash somewhere else later.
//
// The content is chosen to sit on the seams of the word at a time scan: a word with no terminator,
// a terminator in every lane in turn, bytes with the high bit set going through a compare that is
// only valid below it, caps a byte either side of a word boundary, and every start alignment.
#include "oracle_divergence.h"
#include "unity.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "numeros_scribo/numeros_scribo.h"
#include "spatium/spatium.h"
#include "verba_scribo/verba_scribo.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Wide enough to hold several words at any alignment, with room either side.
enum
{
    FENCE = 32,
    BODY = 96,
    ROOM = FENCE + BODY + FENCE,
    POISON = 0xDB
};

static unsigned char arena[ROOM];

/** @brief Put the buffer back, poison and all. */
static unsigned char *fresh(void)
{
    memset(arena, POISON, sizeof arena);
    return arena + FENCE;
}

/** @brief Nothing outside the body moved. */
static void fences_intact(const char *what)
{
    for (size_t i = 0; i < ROOM; i++)
    {
        if (i >= FENCE && i < FENCE + BODY)
        {
            continue;
        }
        if (arena[i] != POISON)
        {
            char msg[128];
            (void)snprintf(msg, sizeof msg, "%s: wrote %zu bytes %s the buffer", what,
                           i < FENCE ? FENCE - i : i - (FENCE + BODY) + 1u, i < FENCE ? "before" : "past");
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

void setUp(void)
{
    (void)fresh();
}

void tearDown(void)
{
}

/* ---------------------------------------------------------------------------------------------
 * len and chr, against content that never terminates
 *
 * A bounded scan is allowed to read to its cap and no further. Content with no terminator in it is
 * the only way to make it go the whole distance, and a cap a byte either side of a word boundary
 * is where a word at a time loop gets it wrong.
 * ------------------------------------------------------------------------------------------- */

void test_len_of_a_run_that_never_terminates(void)
{
    for (size_t cap = 1; cap <= BODY; cap++)
    {
        unsigned char *p = fresh();
        memset(p, 'a', BODY);

        const size_t got = cellul.len((const char *)p, cap);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(cap, got, "an unterminated run must measure exactly its cap");
        fences_intact("len, unterminated");
    }
}

void test_len_finds_a_terminator_in_every_lane(void)
{
    for (size_t at = 0; at < BODY; at++)
    {
        unsigned char *p = fresh();
        memset(p, 'a', BODY);
        p[at] = 0u;

        TEST_ASSERT_EQUAL_size_t(at, cellul.len((const char *)p, BODY));
        TEST_ASSERT_EQUAL_size_t_MESSAGE(strlen((const char *)p), cellul.len((const char *)p, BODY),
                                         "len disagrees with strlen");
        fences_intact("len, terminator walk");
    }
}

void test_len_at_every_start_alignment(void)
{
    // The load is unaligned by design, so every offset into the word has to give the same answer.
    for (size_t off = 0; off < 16u; off++)
    {
        unsigned char *p = fresh();
        memset(p, 'a', BODY);
        p[off + 20u] = 0u;

        TEST_ASSERT_EQUAL_size_t(20u, cellul.len((const char *)(p + off), BODY - off));
        fences_intact("len, alignment walk");
    }
}

void test_chr_of_a_byte_that_is_not_there_in_an_unterminated_run(void)
{
    MMGR_SKIP_ON_ORACLE("strchr has no cap and reads until something terminates, and nothing here does");
    unsigned char *p = fresh();
    memset(p, 0xFFu, BODY);

    TEST_ASSERT_NULL_MESSAGE(cellul.chr((const char *)p, BODY, 0x01u), "found a byte that is not in the run");
    fences_intact("chr, absent");
}

void test_chr_finds_a_byte_in_every_lane(void)
{
    // The terminator sits in the last byte, so the marker walks every lane before it. Putting the
    // marker in that byte too would just be writing it and then writing over it.
    for (size_t at = 0; at + 1u < BODY; at++)
    {
        unsigned char *p = fresh();
        memset(p, 'a', BODY);
        p[BODY - 1u] = 0u;
        p[at] = 'Z';

        const char *got = cellul.chr((const char *)p, BODY, (uint8_t)'Z');
        TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + at, got, "the wrong lane came back");
        fences_intact("chr, lane walk");
    }
}

void test_chr_of_the_terminator_itself(void)
{
    unsigned char *p = fresh();
    memset(p, 'a', BODY);
    p[10] = 0u;

    // strchr finds the terminator, and so should this.
    TEST_ASSERT_EQUAL_PTR(strchr((const char *)p, 0), cellul.chr((const char *)p, BODY, 0u));
    fences_intact("chr, terminator");
}

/* ---------------------------------------------------------------------------------------------
 * the high half of the byte range
 *
 * The lane compares are borrow tricks and are only exact below 0x80. Nothing in the library feeds
 * them anything else, and this is the case that proves the entries above them do not either: text
 * with the high bit set has to come back the same as it does out of libc.
 * ------------------------------------------------------------------------------------------- */

void test_a_run_of_high_bytes_measures_and_searches_like_libc(void)
{
    unsigned char *p = fresh();
    for (size_t i = 0; i < BODY - 1u; i++)
    {
        p[i] = (unsigned char)(0x80u + (i % 0x7Fu));
    }
    p[BODY - 1u] = 0u;

    TEST_ASSERT_EQUAL_size_t(strlen((const char *)p), cellul.len((const char *)p, BODY));
    TEST_ASSERT_EQUAL_PTR(strchr((const char *)p, 0xC3), cellul.chr((const char *)p, BODY, 0xC3u));
    fences_intact("high bytes");
}

void test_folding_never_touches_a_byte_outside_the_letters(void)
{
    // A case insensitive compare folds both sides. If the fold reached past the letters it would
    // make two different bytes look alike, so every byte is checked against itself and against the
    // one it must not be confused with.
    for (unsigned c = 0; c < 256u; c++)
    {
        const char a[2] = {(char)c, '\0'};
        const char b2[2] = {(char)(c ^ 0x20u), '\0'};

        const int is_letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const mmgr_bool same = cellul.eq(a, b2, 2u, MMGR_TRUE);

        if (is_letter)
        {
            TEST_ASSERT_TRUE_MESSAGE(same, "two cases of a letter did not fold together");
        }
        else
        {
            TEST_ASSERT_FALSE_MESSAGE(same, "the fold reached a byte that is not a letter");
        }
    }
}

void test_a_case_insensitive_search_through_high_bytes(void)
{
    unsigned char *p = fresh();
    memset(p, 0xE9u, BODY);
    memcpy(p + 40, "NeEdLe", 6u);
    p[BODY - 1u] = 0u;

    const char *got = cellul.find((const char *)p, BODY - 1u, "needle", 6u, MMGR_TRUE);
    TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + 40, got, "the needle was lost among the high bytes");
    fences_intact("ci search, high bytes");
}

/* ---------------------------------------------------------------------------------------------
 * find, against content built to defeat it
 *
 * The scan picks one rare byte of the needle as its anchor and checks that lane first. Content
 * made entirely of the anchor makes every lane a candidate, which is the worst case it has.
 * ------------------------------------------------------------------------------------------- */

void test_find_where_every_lane_is_a_candidate(void)
{
    unsigned char *p = fresh();
    memset(p, 'a', BODY);
    p[BODY - 1u] = 0u;

    // "aaaa" against a run of nothing but 'a': every start matches the anchor and every one has to
    // be verified.
    const char *got = cellul.find((const char *)p, BODY - 1u, "aaaa", 4u, MMGR_FALSE);
    TEST_ASSERT_EQUAL_PTR(strstr((const char *)p, "aaaa"), got);
    fences_intact("find, all anchors");
}

void test_find_a_needle_that_is_only_the_last_bytes(void)
{
    for (size_t nlen = 1; nlen <= 8u; nlen++)
    {
        unsigned char *p = fresh();
        memset(p, '.', BODY);
        memset(p + (BODY - 1u - nlen), 'q', nlen);
        p[BODY - 1u] = 0u;

        char needle[16];
        memset(needle, 'q', nlen);
        needle[nlen] = '\0';

        const char *got = cellul.find((const char *)p, BODY - 1u, needle, nlen, MMGR_FALSE);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(strstr((const char *)p, needle), got, "a needle flush with the end was missed");
        fences_intact("find, flush with the end");
    }
}

void test_find_a_needle_one_byte_longer_than_the_hay(void)
{
    unsigned char *p = fresh();
    memcpy(p, "abcdefgh", 8u);
    p[8] = 0u;

    TEST_ASSERT_NULL_MESSAGE(cellul.find((const char *)p, 8u, "abcdefghi", 9u, MMGR_FALSE),
                             "a needle longer than the hay cannot be in it");
    fences_intact("find, needle too long");
}

void test_find_the_hay_in_itself(void)
{
    unsigned char *p = fresh();
    memcpy(p, "the whole thing", 15u);
    p[15] = 0u;

    TEST_ASSERT_EQUAL_PTR((const char *)p, cellul.find((const char *)p, 15u, "the whole thing", 15u, MMGR_FALSE));
    fences_intact("find, self");
}

void test_find_across_every_word_boundary(void)
{
    // The match is walked one byte at a time through the buffer so it straddles every boundary a
    // word at a time scan has, and lands flush against each one in turn.
    for (size_t at = 0; at + 5u < BODY - 1u; at++)
    {
        unsigned char *p = fresh();
        memset(p, '.', BODY);
        memcpy(p + at, "xyzzy", 5u);
        p[BODY - 1u] = 0u;

        const char *got = cellul.find((const char *)p, BODY - 1u, "xyzzy", 5u, MMGR_FALSE);
        TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + at, got, "a match straddling a word boundary was missed");
        fences_intact("find, boundary walk");
    }
}

void test_find_with_the_terminator_before_the_match(void)
{
    unsigned char *p = fresh();
    memcpy(p, "abc", 3u);
    p[3] = 0u;
    memcpy(p + 4, "needle", 6u);
    p[10] = 0u;

    // The scan stops at the first terminator, so what is behind it is not there as far as it is
    // concerned. strstr says the same.
    TEST_ASSERT_EQUAL_PTR(strstr((const char *)p, "needle"),
                          cellul.find((const char *)p, BODY, "needle", 6u, MMGR_FALSE));
    fences_intact("find, past the terminator");
}

void test_find_of_an_empty_needle(void)
{
    unsigned char *p = fresh();
    memcpy(p, "anything", 8u);
    p[8] = 0u;

    TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p, cellul.find((const char *)p, 8u, "", 0u, MMGR_FALSE),
                                  "an empty needle is at the start, the way strstr has it");
    TEST_ASSERT_EQUAL_PTR(strstr((const char *)p, ""), cellul.find((const char *)p, 8u, "", 0u, MMGR_FALSE));
}

/* ---------------------------------------------------------------------------------------------
 * copy, at the seam of its destination
 * ------------------------------------------------------------------------------------------- */

void test_copy_never_writes_past_its_destination(void)
{
    static const char src[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    for (size_t cap = 1; cap <= 40u; cap++)
    {
        unsigned char *p = fresh();
        const size_t got = cellul.copy((char *)p, src, cap);

        TEST_ASSERT_TRUE_MESSAGE(got < cap, "copy reported a length that leaves no room for a terminator");
        TEST_ASSERT_EQUAL_CHAR_MESSAGE('\0', (char)p[got], "copy did not terminate what it wrote");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[cap], "copy wrote at its cap");
        fences_intact("copy");
    }
}

void test_copy_of_a_source_that_never_terminates(void)
{
    unsigned char *big = fresh();
    memset(big, 'x', BODY);

    unsigned char out[16];
    memset(out, POISON, sizeof out);
    const size_t got = cellul.copy((char *)out, (const char *)big, 8u);

    TEST_ASSERT_EQUAL_size_t(7u, got);
    TEST_ASSERT_EQUAL_CHAR('\0', (char)out[7]);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, out[8], "copy wrote past the cap it was given");
}

/* ---------------------------------------------------------------------------------------------
 * the builder, at every capacity
 * ------------------------------------------------------------------------------------------- */

void test_a_builder_at_every_capacity_stays_inside_it(void)
{
    for (size_t cap = 0; cap <= 40u; cap++)
    {
        unsigned char *p = fresh();
        mmgr_verba b = {(char *)p, cap, 0, MMGR_TRUE};

        verba.put(&b, "the quick brown fox");
        verba.u64(&b, 18446744073709551615ull);
        verba.hex(&b, 0xDEADBEEFCAFEBABEull, 16u);
        verba.g(&b, 1.0 / 3.0, MMGR_G_MAX_SIG);
        verba.fixed(&b, 2.5, MMGR_FIXED_MAX_DECIMALS);
        verba.json(&b, "\"\\\n\x01");
        verba.xml(&b, "<&>\"'");
        const size_t n = verba.finish(&b);

        TEST_ASSERT_TRUE_MESSAGE(n < cap || n == 0u, "finish reported a length outside the buffer");
        if (cap != 0u)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[cap], "the builder wrote at its cap");
        }
        fences_intact("builder");
    }
}

void test_a_builder_with_no_room_for_a_terminator(void)
{
    unsigned char *p = fresh();
    mmgr_verba b = {(char *)p, 1u, 0, MMGR_TRUE};

    verba.put(&b, "x");
    TEST_ASSERT_FALSE_MESSAGE(b.ok, "one byte holds a terminator and nothing else");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[1], "the builder wrote at its cap");
    fences_intact("builder, cap one");
}

void test_a_write_of_every_length_into_a_fixed_buffer(void)
{
    // Walks the boundary: one short, exact, one over.
    for (size_t len = 1; len <= 32u; len++)
    {
        char src[40];
        memset(src, 'z', len);
        src[len] = '\0';

        for (int delta = -1; delta <= 1; delta++)
        {
            const size_t cap = (size_t)((long)len + 1 + delta);
            if (cap == 0u)
            {
                continue;
            }
            unsigned char *p = fresh();
            mmgr_verba b = {(char *)p, cap, 0, MMGR_TRUE};

            verba.put(&b, src);
            const size_t n = verba.finish(&b);

            if (delta >= 0)
            {
                TEST_ASSERT_EQUAL_size_t_MESSAGE(len, n, "a write that fits was refused");
            }
            else
            {
                TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, n, "a write one byte too long was accepted");
            }
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[cap], "the builder wrote at its cap");
            fences_intact("builder, boundary walk");
        }
    }
}

/* ---------------------------------------------------------------------------------------------
 * the record builder, at every capacity
 * ------------------------------------------------------------------------------------------- */

void test_a_record_at_every_capacity_stays_inside_it(void)
{
    for (size_t cap = 1; cap <= 48u; cap++)
    {
        unsigned char *p = fresh();
        const size_t n =
            mmgr_write((char *)p, cap, MMGR_VSTR("id="), MMGR_VU64(18446744073709551615ull), MMGR_VSTR(" x="),
                       MMGR_VHEXW(0xDEADBEEFu, 8), MMGR_VSTR(" f="), MMGR_VFIXW(-2.5, 4));

        TEST_ASSERT_TRUE_MESSAGE(n < cap, "the record reported a length outside the buffer");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[cap], "the record wrote at its cap");
        fences_intact("record");
    }
}

void test_appending_to_a_record_until_it_stops_fitting(void)
{
    unsigned char *p = fresh();
    const size_t cap = 32u;

    size_t n = mmgr_write((char *)p, cap, MMGR_VSTR("start"));
    TEST_ASSERT_EQUAL_size_t(5u, n);

    for (unsigned i = 0; i < 20u; i++)
    {
        (void)mmgr_write_append((char *)p, cap, MMGR_VSTR(":more"));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(POISON, p[cap], "an append wrote at the cap");
    }
    TEST_ASSERT_TRUE_MESSAGE(cellul.len((const char *)p, cap) < cap, "the record lost its terminator");
    fences_intact("record, append until full");
}

/* ---------------------------------------------------------------------------------------------
 * the parsers, against content that is not a number
 * ------------------------------------------------------------------------------------------- */

void test_the_parsers_against_content_that_never_terminates(void)
{
    // No terminator and nothing numeric: the parse has to stop on the first byte it cannot use
    // rather than reading on.
    unsigned char *p = fresh();
    memset(p, '9', BODY);

    const char *end = NULL;
    (void)cellul.to_ulong((const char *)p, &end);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_TRUE_MESSAGE((const unsigned char *)end <= p + BODY, "the parse ran past the buffer");
    fences_intact("to_ulong, all digits");
}

void test_the_parsers_agree_with_libc_on_rubbish(void)
{
    static const char *cases[] = {"",
                                  " ",
                                  "+",
                                  "-",
                                  "  -",
                                  "..",
                                  ".5",
                                  "5.",
                                  "--5",
                                  "++5",
                                  "\t\n 42",
                                  "9999999999999999999999",
                                  "-9999999999999999999999",
                                  "0000000000000000005"};

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *mine_end = NULL;
        char *ref_end = NULL;

        const double mine = cellul.to_double(cases[i], &mine_end);
        const double ref = strtod(cases[i], &ref_end);

        char msg[128];
        (void)snprintf(msg, sizeof msg, "to_double stopped somewhere else than strtod on \"%s\"", cases[i]);
        TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)ref_end, mine_end, msg);

        if (mine == mine && ref == ref)
        {
            (void)snprintf(msg, sizeof msg, "to_double disagrees with strtod on \"%s\"", cases[i]);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE((ref < 0 ? -ref : ref) * 1e-12 + 1e-300, ref, mine, msg);
        }
    }
}

void test_the_parser_takes_decimal_and_stops_at_anything_else(void)
{
    MMGR_SKIP_ON_ORACLE("C99 gives strtod a hex float form, which this parser deliberately does not take");
    // C99 gives strtod a hex float form. This one is decimal, so the x ends the number and the
    // zero before it is the whole of it. Pinned here rather than left to disagree with whichever
    // library the host ships, because it is this parser's own answer and not a difference of
    // opinion with somebody else's.
    static const char *cases[] = {"0x10", "0X1p4", "0b101", "1_000"};

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *end = NULL;
        const double v = cellul.to_double(cases[i], &end);

        char msg[96];
        (void)snprintf(msg, sizeof msg, "\"%s\" should have stopped after its first digit", cases[i]);
        TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(cases[i][0] == '1' ? 1.0 : 0.0, v, msg);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(cases[i] + 1, end, msg);
    }
}

void test_an_exponent_with_no_digits_after_it(void)
{
    // C says the number is the longest leading run that is actually of the expected form, so "1e"
    // is the number 1 and the e is the byte that ended it. The parser takes the e, and a sign
    // after it, before it looks for a digit, and puts the cursor back on the e when there is not
    // one - so a caller reading the cursor to find out whether the whole string was a number is
    // told no, which it is.
    static const struct
    {
        const char *text;
        double want;
        size_t stops_at;
    } cases[] = {
        {"1e", 1.0, 1u}, {"1e+", 1.0, 1u},   {"1e-", 1.0, 1u},      {"2.5E", 2.5, 3u},      {"2.5E-", 2.5, 3u},
        {"7e", 7.0, 1u}, {"0.5e+", 0.5, 3u}, {"1e5", 100000.0, 3u}, {"1e+5", 100000.0, 4u},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        const char *end = NULL;
        const double v = cellul.to_double(cases[i].text, &end);

        char msg[96];
        (void)snprintf(msg, sizeof msg, "\"%s\"", cases[i].text);
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-12, cases[i].want, v, msg);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(cases[i].text + cases[i].stops_at, end, msg);
    }
}

void test_an_exponent_that_is_real_is_still_taken(void)
{
    // The other half: the early return must not have eaten the ordinary path.
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 25000000000.0, cellul.to_double("2.5e10", NULL));
    TEST_ASSERT_DOUBLE_WITHIN(1e-18, 0.00125, cellul.to_double("1.25e-3", NULL));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 602200.0, cellul.to_double("6.022E5", NULL));
}

void test_a_number_made_entirely_of_leading_zeros(void)
{
    unsigned char *p = fresh();
    memset(p, '0', BODY - 1u);
    p[BODY - 1u] = 0u;

    const char *end = NULL;
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)cellul.to_ulong((const char *)p, &end));
    TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + BODY - 1u, end, "every zero should have been consumed");
    fences_intact("to_ulong, all zeros");
}
