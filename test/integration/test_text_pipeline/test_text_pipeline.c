// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// numeros_scribo -> verba_scribo -> cellularum_laboro -> verbum_scrutor
//
// A unit suite proves one module keeps its own contract. This proves the stack still agrees when
// text goes in one end and is searched out the other: the builder writes, the scanner reads back
// what the builder claimed to write, and the length everyone believes is the same length.
#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "numeros_scribo/numeros_scribo.h"
#include "verba_scribo/verba_scribo.h"

void test_built_text_reads_back_at_the_length_it_reported(void)
{
    char buf[128];
    const size_t n = mmgr_write(buf, sizeof buf, MMGR_VSTR("id="), MMGR_VU32(4242u), MMGR_VSTR(" ok"));

    TEST_ASSERT_GREATER_THAN_size_t(0u, n);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(n, cellul.len(buf, sizeof buf),
                                     "the builder's length and the scanner's length must agree");
}

void test_scanner_finds_what_the_builder_wrote(void)
{
    char buf[128];
    mmgr_write(buf, sizeof buf, MMGR_VSTR("user="), MMGR_VSTR("dstroy0"), MMGR_VSTR(" id="), MMGR_VU32(7u));

    TEST_ASSERT_NOT_NULL(cellul.find(buf, sizeof buf, "dstroy0", 8u, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "id=7", 5u, MMGR_FALSE));
    TEST_ASSERT_FALSE(cellul.has(buf, sizeof buf, "id=8", 5u, MMGR_FALSE));
}

void test_case_folding_agrees_across_builder_and_scanner(void)
{
    char buf[64];
    mmgr_write(buf, sizeof buf, MMGR_VSTR("Content-Type"));

    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "content-type", 13u, MMGR_TRUE));
    TEST_ASSERT_FALSE(cellul.has(buf, sizeof buf, "content-type", 13u, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.eq(buf, "CONTENT-TYPE", sizeof buf, MMGR_TRUE));
}

void test_appending_keeps_every_earlier_field_findable(void)
{
    char buf[160];
    mmgr_write(buf, sizeof buf, MMGR_VSTR("a=1"));
    mmgr_write_append(buf, sizeof buf, MMGR_VSTR(";b=2"));
    mmgr_write_append(buf, sizeof buf, MMGR_VSTR(";c=3"));

    TEST_ASSERT_EQUAL_STRING("a=1;b=2;c=3", buf);
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "a=1", 4u, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "b=2", 4u, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "c=3", 4u, MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(11u, cellul.len(buf, sizeof buf));
}

void test_a_builder_overflow_leaves_nothing_for_the_scanner(void)
{
    char buf[8];
    // the builder reports failure and terminates; the scanner must not then find a fragment
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_write(buf, sizeof buf, MMGR_VSTR("far too long to fit")));
    TEST_ASSERT_EQUAL_size_t(0u, cellul.len(buf, sizeof buf));
    TEST_ASSERT_FALSE(cellul.has(buf, sizeof buf, "far", 4u, MMGR_FALSE));
}

void test_every_rendered_number_is_found_by_the_scanner(void)
{
    // drives the builder's integer paths and the scanner's word-boundary handling together
    static const uint32_t vals[] = {0u, 1u, 9u, 10u, 99u, 100u, 65535u, 1000000u, 4294967295u};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        char buf[64];
        char want[32];
        mmgr_write(buf, sizeof buf, MMGR_VSTR("<"), MMGR_VU32(vals[i]), MMGR_VSTR(">"));
        mmgr_write(want, sizeof want, MMGR_VU32(vals[i]));

        TEST_ASSERT_NOT_NULL_MESSAGE(cellul.find(buf, sizeof buf, want, sizeof want, MMGR_FALSE),
                                     "a number the builder rendered must be findable in its own output");
    }
}

void test_escaped_output_is_still_scannable(void)
{
    char buf[96];
    mmgr_write(buf, sizeof buf, MMGR_VJSON("a\"b"));

    // the escape is text like any other, and the scanner reads it back as text
    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\"", buf);
    TEST_ASSERT_EQUAL_size_t(6u, cellul.len(buf, sizeof buf));
    TEST_ASSERT_TRUE_MESSAGE(cellul.has(buf, sizeof buf, "\\\"", 3u, MMGR_FALSE), "the escape survives into the text");
    TEST_ASSERT_NOT_NULL(cellul.find(buf, sizeof buf, "a", 2u, MMGR_FALSE));

    mmgr_write(buf, sizeof buf, MMGR_VXML("a<b&c"));
    TEST_ASSERT_EQUAL_STRING("a&lt;b&amp;c", buf);
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "&lt;", 5u, MMGR_FALSE));
    TEST_ASSERT_TRUE(cellul.has(buf, sizeof buf, "&amp;", 6u, MMGR_FALSE));
}

void test_an_empty_render_reports_nothing_written(void)
{
    // 0 means "nothing usable came back" across this whole family - build, append, emit - and an
    // empty render is indistinguishable from a failed one by the return alone. The buffer state is
    // the same either way, so a caller that checks the buffer is never misled.
    char buf[32];
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_write(buf, sizeof buf, MMGR_VSTR("")));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_EQUAL_size_t(0u, cellul.len(buf, sizeof buf));
}
