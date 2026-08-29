/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#define CAP 64u
#define NSEGS 4u

static mmgr_ring g_ring;
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_buf[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_src[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_dst[CAP * 4u];

static void fresh(void)
{
    for (size_t i = 0; i < sizeof g_src; i++)
    {
        g_src[i] = (uint8_t)(i + 1u);
    }
    for (size_t i = 0; i < sizeof g_buf; i++)
    {
        g_buf[i] = 0u;
    }
    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                               .segment_count = NSEGS));
}

void test_init_refuses_bad_sizes(void)
{
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = 0u,
                                .segment_count = NSEGS));
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = 48u,
                                .segment_count = NSEGS));
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                                .segment_count = 0u));
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                                .segment_count = 3u));
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                                .segment_count = CAP * 2u));
    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_buf, .capacity = CAP,
                               .segment_count = NSEGS));
}

void test_init_starts_empty_with_every_loculus_free(void)
{
    fresh();

    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));
    TEST_ASSERT_EQUAL_size_t(CAP - 1u, MMGR_CALL(iteratio_infinita.vacant, InfinCfg, .ring = &g_ring));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(iteratio_infinita.seg_inflight, InfinCfg, .ring = &g_ring));

    const mmgr_word ready = MMGR_CALL(iteratio_infinita.loculus_ready, InfinCfg, .ring = &g_ring);

    for (size_t i = 0; i < (size_t)MMGR_RING_LOCULI; i++)
    {
        TEST_ASSERT_NOT_EQUAL(0u, (unsigned)(ready & (mmgr_word)((mmgr_word)1 << i)));
    }
}

void test_put_then_read_round_trips(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));
    TEST_ASSERT_EQUAL_size_t(16u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));

    const size_t got = MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 16u);

    TEST_ASSERT_EQUAL_size_t(16u, got);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 16u);
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));
}

void test_put_refuses_more_than_vacant(void)
{
    fresh();

    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = CAP));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));
    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = CAP - 1u));
}

void test_read_takes_only_what_arrived(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 4u));

    const size_t got = MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 32u);

    TEST_ASSERT_EQUAL_size_t(4u, got);
}

void test_move_wraps_in_order(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 56u));
    TEST_ASSERT_EQUAL_size_t(
        56u, MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 56u));

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 32u));
    TEST_ASSERT_EQUAL_size_t(32u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));

    const size_t got = MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 32u);

    TEST_ASSERT_EQUAL_size_t(32u, got);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 32u);
}

/**
 * @brief Every start offset and every odd length, so the wrap and the narrowing tail both run.
 *
 * @note A guard byte past the request catches a tail that carries more than it was given.
 */
void test_move_carries_every_length_at_every_offset(void)
{
    for (size_t start = 0u; start < CAP; start++)
    {
        for (size_t n = 1u; n < CAP; n++)
        {
            fresh();

            if (start != 0u)
            {
                TEST_ASSERT_TRUE(
                    MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = start));
                MMGR_CALL(iteratio_infinita.consume, InfinCfg, .ring = &g_ring, .bytes = start);
            }
            if (n > MMGR_CALL(iteratio_infinita.vacant, InfinCfg, .ring = &g_ring))
            {
                continue;
            }

            TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = n));

            g_dst[n] = 0xA5u;
            TEST_ASSERT_EQUAL_size_t(
                n, MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = n));
            TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, n);
            TEST_ASSERT_EQUAL_HEX8(0xA5u, g_dst[n]);
        }
    }
}

void test_peek_leaves_the_tail_and_consume_moves_it(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));

    MMGR_CALL(iteratio_infinita.peek, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 8u, .offset = 0u);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_src, g_dst, 8u);
    TEST_ASSERT_EQUAL_size_t(16u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));

    MMGR_CALL(iteratio_infinita.consume, InfinCfg, .ring = &g_ring, .bytes = 8u);
    TEST_ASSERT_EQUAL_size_t(8u, MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring));

    MMGR_CALL(iteratio_infinita.peek, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = 8u, .offset = 0u);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(&g_src[8], g_dst, 8u);
}

void test_peek_holds_a_request_above_capacity(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 16u));

    for (size_t i = 0; i < sizeof g_dst; i++)
    {
        g_dst[i] = 0xA5u;
    }

    MMGR_CALL(iteratio_infinita.peek, InfinCfg, .ring = &g_ring, .dst = g_dst, .bytes = CAP * 3u, .offset = 0u);

    for (size_t i = CAP; i < sizeof g_dst; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xA5u, g_dst[i]);
    }
}

void test_read_byte_empties_the_ring(void)
{
    fresh();

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_src, .bytes = 3u));

    uint8_t b = 0u;

    for (size_t i = 0; i < 3u; i++)
    {
        TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.read_byte, InfinCfg, .ring = &g_ring, .dst = &b));
        TEST_ASSERT_EQUAL_HEX8(g_src[i], b);
    }
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.read_byte, InfinCfg, .ring = &g_ring, .dst = &b));
}

void test_loculus_next_picks_the_lowest_and_reports_empty(void)
{
    fresh();

    TEST_ASSERT_EQUAL_INT(
        -1, (int)MMGR_CALL(iteratio_infinita.loculus_next, InfinCfg, .ring = &g_ring, .mask = (mmgr_word)0));
    TEST_ASSERT_EQUAL_INT(
        0, (int)MMGR_CALL(iteratio_infinita.loculus_next, InfinCfg, .ring = &g_ring, .mask = (mmgr_word)1));
    TEST_ASSERT_EQUAL_INT(
        3, (int)MMGR_CALL(iteratio_infinita.loculus_next, InfinCfg, .ring = &g_ring, .mask = (mmgr_word)0x18));
}

void test_hold_takes_a_loculus_and_drop_returns_it(void)
{
    fresh();

    const mmgr_word before = MMGR_CALL(iteratio_infinita.loculus_ready, InfinCfg, .ring = &g_ring);
    const mmgr_iword pick = MMGR_CALL(iteratio_infinita.loculus_next, InfinCfg, .ring = &g_ring, .mask = before);

    TEST_ASSERT_EQUAL_INT(0, (int)pick);

    TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.loculus_hold, InfinCfg, .ring = &g_ring, .index = 0u,
                               .src = g_src, .bytes = 12u));
    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.loculus_hold, InfinCfg, .ring = &g_ring, .index = 0u,
                                .src = g_src, .bytes = 12u));

    const mmgr_word held = MMGR_CALL(iteratio_infinita.loculus_ready, InfinCfg, .ring = &g_ring);

    TEST_ASSERT_EQUAL_UINT(0u, (unsigned)(held & (mmgr_word)1));

    const mmgr_ring_span *const k =
        MMGR_CALL(iteratio_infinita.loculus_keepout, InfinCfg, .ring = &g_ring, .index = 0u);

    TEST_ASSERT_NOT_NULL(k);
    TEST_ASSERT_EQUAL_PTR(g_src, k->buf);
    TEST_ASSERT_EQUAL_size_t(12u, k->bytes);
    TEST_ASSERT_EQUAL_size_t(0u, k->read_offset);

    MMGR_CALL(iteratio_infinita.loculus_drop, InfinCfg, .ring = &g_ring, .index = 0u);
    TEST_ASSERT_EQUAL_UINT((unsigned)before,
                           (unsigned)MMGR_CALL(iteratio_infinita.loculus_ready, InfinCfg, .ring = &g_ring));
}

void test_hold_refuses_a_loculus_that_does_not_exist(void)
{
    fresh();

    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.loculus_hold, InfinCfg, .ring = &g_ring,
                                .index = MMGR_RING_LOCULI, .src = g_src, .bytes = 4u));
    TEST_ASSERT_NULL(
        MMGR_CALL(iteratio_infinita.loculus_keepout, InfinCfg, .ring = &g_ring, .index = MMGR_RING_LOCULI));
}

void test_segments_publish_and_release_in_order(void)
{
    fresh();

    size_t idx = 99u;

    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.seg_front, InfinCfg, .ring = &g_ring, .out_index = &idx));

    for (size_t i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.seg_next, InfinCfg, .ring = &g_ring, .out_index = &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
        MMGR_CALL(iteratio_infinita.seg_publish, InfinCfg, .ring = &g_ring);
    }

    TEST_ASSERT_FALSE(MMGR_CALL(iteratio_infinita.seg_next, InfinCfg, .ring = &g_ring, .out_index = &idx));
    TEST_ASSERT_EQUAL_size_t(NSEGS, MMGR_CALL(iteratio_infinita.seg_inflight, InfinCfg, .ring = &g_ring));

    for (size_t i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(MMGR_CALL(iteratio_infinita.seg_front, InfinCfg, .ring = &g_ring, .out_index = &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
        MMGR_CALL(iteratio_infinita.seg_release, InfinCfg, .ring = &g_ring);
    }
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(iteratio_infinita.seg_inflight, InfinCfg, .ring = &g_ring));
}

void test_seg_at_walks_the_buffer(void)
{
    fresh();

    for (size_t i = 0; i < NSEGS; i++)
    {
        const uint8_t *const at = MMGR_CALL(iteratio_infinita.seg_at, InfinCfg, .ring = &g_ring, .index = i);

        TEST_ASSERT_EQUAL_PTR(&g_buf[i * (CAP / NSEGS)], at);
    }
}
