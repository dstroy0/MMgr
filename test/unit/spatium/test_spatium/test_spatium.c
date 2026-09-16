#include "spatium/spatium.h"

#include "unity.h"

static uint8_t buf[64];

void setUp(void)
{
    for (size_t index = 0; index < sizeof buf; index++)
    {
        buf[index] = 0xA5u;
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
    const mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, span.buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf, span.cap);
    TEST_ASSERT_EQUAL_size_t(0u, span.pos);
    TEST_ASSERT_FALSE_MESSAGE(span.overflow, "a fresh span has recorded no overflow");
    TEST_ASSERT_TRUE(EMBED_CALL(spat.ok, SpatiumCfg, .span = span));
    TEST_ASSERT_TRUE(EMBED_CALL(spat.has_storage, SpatiumCfg, .span = span));
}

void test_a_read_span_covers_the_buffer_it_was_given(void)
{
    const mmgr_cspan cspan = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = buf, .cap = sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, cspan.buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf, cspan.len);
    TEST_ASSERT_EQUAL_size_t(0u, cspan.pos);
    TEST_ASSERT_TRUE(EMBED_CALL(spat.cok, SpatiumCfg, .cspan = cspan));
}

void test_a_span_with_no_storage_is_not_ok(void)
{
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    span.cap = 0u;
    TEST_ASSERT_FALSE(EMBED_CALL(spat.has_storage, SpatiumCfg, .span = span));
    TEST_ASSERT_FALSE(EMBED_CALL(spat.ok, SpatiumCfg, .span = span));

    span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    span.buf = NULL;
    TEST_ASSERT_FALSE(EMBED_CALL(spat.has_storage, SpatiumCfg, .span = span));
}

void test_a_narrowing_past_the_end_fails_rather_than_shortening(void)
{
    const mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    const mmgr_span at_end = EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = sizeof buf);
    const mmgr_span past_end = EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = sizeof buf + 1u);

    TEST_ASSERT_EQUAL_size_t(0u, at_end.cap);
    TEST_ASSERT_FALSE_MESSAGE(at_end.overflow, "reaching the end is not a failure");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(spat.has_storage, SpatiumCfg, .span = at_end),
                              "but there is nothing left to write into");
    TEST_ASSERT_TRUE_MESSAGE(past_end.overflow, "past the end is a failed span");
    TEST_ASSERT_FALSE(EMBED_CALL(spat.ok, SpatiumCfg, .span = past_end));
    TEST_ASSERT_FALSE_MESSAGE(
        EMBED_CALL(spat.ok, SpatiumCfg,
                   .span = EMBED_CALL(spat.first, SpatiumCfg, .span = span, .count = sizeof buf + 1u)),
        "past the end is a failed span");
}

void test_a_narrowing_keeps_the_bytes_it_names(void)
{
    const mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
    const mmgr_span half = EMBED_CALL(spat.first, SpatiumCfg, .span = span, .count = 32u);

    TEST_ASSERT_EQUAL_size_t(32u, half.cap);
    TEST_ASSERT_EQUAL_PTR(buf, half.buf);
    TEST_ASSERT_EQUAL_PTR(buf + 16u, EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = 16u).buf);
    TEST_ASSERT_EQUAL_size_t(sizeof buf - 16u, EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = 16u).cap);
}

void test_a_narrowing_carries_the_cursor(void)
{
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    span.pos = 20u;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = 16u).pos,
                                     "the cursor moves with the window");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = 24u).pos,
                                     "a window past the cursor starts empty");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(10u, EMBED_CALL(spat.first, SpatiumCfg, .span = span, .count = 10u).pos,
                                     "a window shorter than the cursor is full");
}

void test_reset_clears_the_sticky_overflow(void)
{
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    span.pos = 8u;
    span.overflow = EMBED_TRUE;
    TEST_ASSERT_FALSE(EMBED_CALL(spat.ok, SpatiumCfg, .span = span));

    EMBED_CALL(spat.reset, SpatiumCfg, .at = &span);

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(spat.ok, SpatiumCfg, .span = span),
                             "reset is the one call that clears an overflow");
    TEST_ASSERT_EQUAL_size_t(0u, span.pos);
}

void test_a_read_span_reports_what_was_written(void)
{
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    span.pos = 10u;

    const mmgr_cspan done = EMBED_CALL(spat.produced, SpatiumCfg, .span = span);

    TEST_ASSERT_EQUAL_size_t(10u, done.len);
    TEST_ASSERT_EQUAL_PTR(buf, done.buf);
    TEST_ASSERT_TRUE(EMBED_CALL(spat.cok, SpatiumCfg, .cspan = done));

    TEST_ASSERT_FALSE_MESSAGE(
        EMBED_CALL(spat.cok, SpatiumCfg, .cspan = EMBED_CALL(spat.read, SpatiumCfg, .span = span, .count = 11u)),
        "a read past what was written must be marked");
    TEST_ASSERT_TRUE(
        EMBED_CALL(spat.cok, SpatiumCfg, .cspan = EMBED_CALL(spat.read, SpatiumCfg, .span = span, .count = 4u)));
    TEST_ASSERT_EQUAL_size_t(4u, EMBED_CALL(spat.read, SpatiumCfg, .span = span, .count = 4u).len);
}

void test_a_read_span_carries_the_fill_spans_failure(void)
{
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

    span.pos = 8u;
    span.overflow = EMBED_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(
        EMBED_CALL(spat.cok, SpatiumCfg, .cspan = EMBED_CALL(spat.produced, SpatiumCfg, .span = span)),
        "a span that overflowed produced less than was asked of it");
}
