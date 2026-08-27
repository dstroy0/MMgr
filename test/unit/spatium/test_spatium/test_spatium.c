/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "spatium/spatium.h"

static uint8_t buf[64];

void setUp(void)
{
    for (size_t i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xA5u;
    }
}

void tearDown(void)
{
}

void test_spatium_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("spatium.h compiled with no header before it");
}

void test_the_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(SpatiumNs), sizeof spat, "the table is not its own type");
    TEST_ASSERT_EQUAL_PTR(mmgr_spat_from, spat.from);
    TEST_ASSERT_EQUAL_PTR(mmgr_spat_read, spat.read);
}

void test_a_span_covers_the_buffer_it_was_given(void)
{
    const mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_FALSE_MESSAGE(s.overflow, "a fresh span has recorded no overflow");
    TEST_ASSERT_TRUE(MMGR_CALL(spat.ok, SpatiumCfg, .s = s));
    TEST_ASSERT_TRUE(MMGR_CALL(spat.has_storage, SpatiumCfg, .s = s));
}

void test_a_read_span_covers_the_buffer_it_was_given(void)
{
    const mmgr_cspan s = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = buf, .cap = sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf, s.len);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_TRUE(MMGR_CALL(spat.cok, SpatiumCfg, .cs = s));
}

void test_a_span_with_no_storage_is_not_ok(void)
{
    mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    s.cap = 0u;
    TEST_ASSERT_FALSE(MMGR_CALL(spat.has_storage, SpatiumCfg, .s = s));
    TEST_ASSERT_FALSE(MMGR_CALL(spat.ok, SpatiumCfg, .s = s));

    s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    s.buf = NULL;
    TEST_ASSERT_FALSE(MMGR_CALL(spat.has_storage, SpatiumCfg, .s = s));
}

/**
 * @brief Reaching the end is an empty span; going past it is a failed one.
 *
 * @note The overflow flag is what tells the two apart. A narrowing past the end that came back as a
 *       shorter span would look whole, and the caller's bug would go unreported.
 */
void test_a_narrowing_past_the_end_fails_rather_than_shortening(void)
{
    const mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    const mmgr_span at_end = MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = sizeof buf);
    const mmgr_span past_end = MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = sizeof buf + 1u);

    TEST_ASSERT_EQUAL_size_t(0u, at_end.cap);
    TEST_ASSERT_FALSE_MESSAGE(at_end.overflow, "reaching the end is not a failure");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(spat.has_storage, SpatiumCfg, .s = at_end), "but there is nothing left to write into");
    TEST_ASSERT_TRUE_MESSAGE(past_end.overflow, "past the end is a failed span");
    TEST_ASSERT_FALSE(MMGR_CALL(spat.ok, SpatiumCfg, .s = past_end));
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(spat.ok, SpatiumCfg, .s = MMGR_CALL(spat.first, SpatiumCfg, .s = s, .n = sizeof buf + 1u)), "past the end is a failed span");
}

void test_a_narrowing_keeps_the_bytes_it_names(void)
{
    const mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    const mmgr_span half = MMGR_CALL(spat.first, SpatiumCfg, .s = s, .n = 32u);

    TEST_ASSERT_EQUAL_size_t(32u, half.cap);
    TEST_ASSERT_EQUAL_PTR(buf, half.buf);
    TEST_ASSERT_EQUAL_PTR(buf + 16u, MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = 16u).buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf - 16u, MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = 16u).cap);
}

/**
 * @brief A narrowing carries the cursor with it, so what was written stays accounted for.
 */
void test_a_narrowing_carries_the_cursor(void)
{
    mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    s.pos = 20u;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = 16u).pos, "the cursor moves with the window");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(spat.after, SpatiumCfg, .s = s, .n = 24u).pos, "a window past the cursor starts empty");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(10u, MMGR_CALL(spat.first, SpatiumCfg, .s = s, .n = 10u).pos, "a window shorter than the cursor is full");
}

void test_reset_clears_the_sticky_overflow(void)
{
    mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    s.pos = 8u;
    s.overflow = MMGR_TRUE;
    TEST_ASSERT_FALSE(MMGR_CALL(spat.ok, SpatiumCfg, .s = s));

    MMGR_CALL(spat.reset, SpatiumCfg, .at = &s);

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(spat.ok, SpatiumCfg, .s = s), "reset is the one call that clears an overflow");
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
}

void test_a_read_span_reports_what_was_written(void)
{
    mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    s.pos = 10u;

    const mmgr_cspan done = MMGR_CALL(spat.produced, SpatiumCfg, .s = s);

    TEST_ASSERT_EQUAL_size_t(10u, done.len);
    TEST_ASSERT_EQUAL_PTR(buf, done.buf);
    TEST_ASSERT_TRUE(MMGR_CALL(spat.cok, SpatiumCfg, .cs = done));

    // Asking for more than was written is an error, not a shorter span that looks whole
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(spat.cok, SpatiumCfg, .cs = MMGR_CALL(spat.read, SpatiumCfg, .s = s, .n = 11u)), "a read past what was written must be marked");
    TEST_ASSERT_TRUE(MMGR_CALL(spat.cok, SpatiumCfg, .cs = MMGR_CALL(spat.read, SpatiumCfg, .s = s, .n = 4u)));
    TEST_ASSERT_EQUAL_size_t(4u, MMGR_CALL(spat.read, SpatiumCfg, .s = s, .n = 4u).len);
}

void test_a_read_span_carries_the_fill_spans_failure(void)
{
    mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    s.pos = 8u;
    s.overflow = MMGR_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(spat.cok, SpatiumCfg, .cs = MMGR_CALL(spat.produced, SpatiumCfg, .s = s)),
                              "a span that overflowed produced less than was asked of it");
}
