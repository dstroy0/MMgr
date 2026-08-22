// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The ring, through the table a consumer uses.
//
// The translation unit is compiled in rather than linked, which is what makes the reservation word
// and the state behind the handle visible to a case. Everything a case drives, it drives through
// the table - the point of looking inside is to check what the entries did, not to reach past them.
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.c"

#include "unity.h"

#define CAP 256u
#define SEGS 8u
#define SEGBYTES (CAP / SEGS)

static uint8_t buf[CAP];
static _Atomic mmgr_word held;
static mmgr_ring ring;
static const int owner = 0;

void setUp(void)
{
    for (unsigned i = 0; i < CAP; i++)
    {
        buf[i] = 0u;
    }
    (void)iteratio_infinita.init(&ring, &(RingCfg){buf, CAP, SEGS, &held});
}

void tearDown(void)
{
}

/** @brief The ordinary cursor, opened the way a consumer opens one. */
static struct MmgrCursor *cursor(void)
{
    return iteratio_infinita.open(&(InfinCfg){.r = &ring, .owner = &owner});
}

void test_infin_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_exclusivum_infinitas.h compiled with no header before it");
}

void test_init_takes_a_ring_the_consumer_owns(void)
{
    mmgr_ring r;
    TEST_ASSERT_TRUE(iteratio_infinita.init(&r, &(RingCfg){buf, CAP, SEGS, &held}));
}

void test_init_refuses_a_capacity_that_is_not_a_power_of_two(void)
{
    mmgr_ring r;
    TEST_ASSERT_FALSE(iteratio_infinita.init(&r, &(RingCfg){buf, 100u, SEGS, &held}));
    TEST_ASSERT_FALSE_MESSAGE(iteratio_infinita.init(&r, &(RingCfg){buf, 0u, SEGS, &held}),
                              "a ring of nothing is not a ring");
}

void test_init_refuses_more_segments_than_the_word_has_bits(void)
{
    mmgr_ring r;
    const size_t too_many = (size_t)MMGR_RING_LOCULI_MAX * 2u;

    TEST_ASSERT_FALSE_MESSAGE(iteratio_infinita.init(&r, &(RingCfg){buf, CAP, too_many, &held}),
                              "a segment with no bit cannot be reserved");
}

void test_init_refuses_more_segments_than_bytes(void)
{
    mmgr_ring r;
    TEST_ASSERT_FALSE(iteratio_infinita.init(&r, &(RingCfg){buf, 4u, 8u, &held}));
}

void test_a_fresh_ring_is_empty_and_holds_one_byte_back(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(CAP - 1u, iteratio_infinita.free_(&(InfinCfg){.r = &ring}),
                                     "one byte is held back so full and empty differ");
}

void test_open_hands_out_one_cursor(void)
{
    TEST_ASSERT_NOT_NULL(cursor());
    TEST_ASSERT_NULL_MESSAGE(cursor(), "one accessor: a second is refused");
}

void test_write_moves_what_available_reports(void)
{
    static const uint8_t src[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    struct MmgrCursor *const cur = cursor();

    TEST_ASSERT_EQUAL_size_t(8u, iteratio_infinita.write(
                                     &(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 8u}));
    TEST_ASSERT_EQUAL_size_t(8u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));
    TEST_ASSERT_EQUAL_size_t(CAP - 1u - 8u, iteratio_infinita.free_(&(InfinCfg){.r = &ring}));
}

void test_a_raw_read_names_the_bytes_and_consumes_nothing(void)
{
    static const uint8_t src[4] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    struct MmgrCursor *const cur = cursor();

    iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 4u});

    const uint8_t *const at = iteratio_infinita.read(&(InfinCfg){.r = &ring, .cur = cur, .n = 4u});
    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, at, 4u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "a raw read behaves like the other memory entries");
}

void test_a_raw_read_of_more_than_is_there_is_null(void)
{
    static const uint8_t src[4] = {1u, 2u, 3u, 4u};
    struct MmgrCursor *const cur = cursor();

    iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 4u});
    TEST_ASSERT_NULL(iteratio_infinita.read(&(InfinCfg){.r = &ring, .cur = cur, .n = 5u}));
}

void test_a_raw_read_of_an_empty_ring_is_null(void)
{
    struct MmgrCursor *const cur = cursor();
    TEST_ASSERT_NULL(iteratio_infinita.read(&(InfinCfg){.r = &ring, .cur = cur, .n = 1u}));
}

void test_consume_is_what_moves_the_tail(void)
{
    static const uint8_t src[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    struct MmgrCursor *const cur = cursor();

    iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 8u});
    iteratio_infinita.consume(&(InfinCfg){.r = &ring, .cur = cur, .n = 3u});
    TEST_ASSERT_EQUAL_size_t(5u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));
}

void test_read_byte_takes_one_and_refuses_an_empty_ring(void)
{
    static const uint8_t src[2] = {0x5Au, 0xA5u};
    struct MmgrCursor *const cur = cursor();
    uint8_t got = 0u;

    TEST_ASSERT_FALSE_MESSAGE(
        iteratio_infinita.read_byte(&(InfinCfg){.r = &ring, .cur = cur, .dst = &got}),
        "an empty ring has no byte to hand back");

    iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 2u});
    TEST_ASSERT_TRUE(iteratio_infinita.read_byte(&(InfinCfg){.r = &ring, .cur = cur, .dst = &got}));
    TEST_ASSERT_EQUAL_HEX8(0x5Au, got);
    TEST_ASSERT_EQUAL_size_t(1u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));
}

void test_peek_copies_without_consuming(void)
{
    static const uint8_t src[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    struct MmgrCursor *const cur = cursor();
    uint8_t dst[2] = {0u, 0u};

    iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 4u});
    iteratio_infinita.peek(&(InfinCfg){.r = &ring, .cur = cur, .dst = dst, .n = 2u, .off = 1u});

    TEST_ASSERT_EQUAL_HEX8(0x22u, dst[0]);
    TEST_ASSERT_EQUAL_HEX8(0x33u, dst[1]);
    TEST_ASSERT_EQUAL_size_t(4u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));
}

void test_a_write_that_wraps_comes_back_in_order(void)
{
    static const uint8_t src[16] = {0};
    struct MmgrCursor *const cur = cursor();

    /* Push the head most of the way round, then write across the end. */
    for (unsigned i = 0; i < 15u; i++)
    {
        iteratio_infinita.write(&(InfinCfg){.r = &ring, .cur = cur, .src = src, .n = 16u});
        iteratio_infinita.consume(&(InfinCfg){.r = &ring, .cur = cur, .n = 16u});
    }
    static const uint8_t run[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    TEST_ASSERT_EQUAL_size_t(8u, iteratio_infinita.write(
                                     &(InfinCfg){.r = &ring, .cur = cur, .src = run, .n = 8u}));
    TEST_ASSERT_EQUAL_size_t(8u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));

    uint8_t got[8] = {0};
    iteratio_infinita.peek(&(InfinCfg){.r = &ring, .cur = cur, .dst = got, .n = 8u, .off = 0u});
    TEST_ASSERT_EQUAL_HEX8_ARRAY(run, got, 8u);
}

void test_seek_moves_a_cursor_inside_its_frame(void)
{
    struct MmgrCursor *const cur = cursor();

    iteratio_infinita.seek(&(InfinCfg){.r = &ring, .cur = cur, .off = 32u});
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, cur->off, "the cursor is an offset, not a pointer");

    iteratio_infinita.seek(&(InfinCfg){.r = &ring, .cur = cur, .off = 0u});
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, cur->off, "offset zero is the frame's start");
}

void test_a_drain_is_refused_without_the_capability(void)
{
#if MMGR_ENABLE_KEEPOUT
    TEST_IGNORE_MESSAGE("keepouts are built, so the drain suite covers this");
#else
    size_t tess = 0u;
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.drain(&(InfinCfg){.r = &ring, .from = 0u,
                                                                 .to = SEGBYTES, .tessera = &tess}),
                             "no keepout without the capability, so no second cursor");
    TEST_ASSERT_EQUAL_size_t(0u, tess);
#endif
}
