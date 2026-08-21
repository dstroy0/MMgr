// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

// The three tables are the whole surface, so the cases are grouped the way the header is: the byte
// ring, the queue of segments over one, and the bitmap of loculi.
//
// Capacities are powers of two because a wrap is a mask, and every case here picks a small one: a
// ring of eight says everything a ring of eight thousand would about a cursor that has to wrap, and
// says it in a fixture that can be read by eye.

#define RING_CAP 8u
#define NSEGS 4u
#define SEG_SIZE 16u

static uint8_t ring[RING_CAP];
static _Atomic size_t head;
static _Atomic size_t tail;

static uint8_t segstore[NSEGS * SEG_SIZE];
static _Atomic size_t claim;
static _Atomic size_t rel;

static _Atomic uint32_t held;
static _Atomic uint32_t ready_mask;
static mmgr_keepout keepout[MMGR_RING_LOCULI_MAX];

void setUp(void)
{
    for (unsigned i = 0; i < RING_CAP; i++)
    {
        ring[i] = 0u;
    }
    for (unsigned i = 0; i < sizeof segstore; i++)
    {
        segstore[i] = 0u;
    }
    for (unsigned i = 0; i < MMGR_RING_LOCULI_MAX; i++)
    {
        keepout[i].buf = NULL;
        keepout[i].len = 0u;
    }
    atomic_store(&head, 0u);
    atomic_store(&tail, 0u);
    atomic_store(&claim, 0u);
    atomic_store(&rel, 0u);
    atomic_store(&held, 0u);
    atomic_store(&ready_mask, 0u);
}

void tearDown(void)
{
}

/** @brief Put @p len bytes into the ring through the entry that is meant to put them there. */
static void fill(const uint8_t *src, size_t len)
{
    atomic_store(&head, infin.write_span(ring, RING_CAP, atomic_load(&head), src, len));
}

void test_infin_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_exclusivum_infinitas.h compiled with no header before it");
}

// ------------------------------------------------------------------------------------------------
// The byte ring
// ------------------------------------------------------------------------------------------------

void test_a_fresh_ring_is_empty_and_holds_one_byte_back(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, infin.available(&head, &tail, RING_CAP));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(RING_CAP - 1u, infin.free_(&head, &tail, RING_CAP),
                                     "one loculus is held back so full and empty differ");
}

void test_available_and_free_move_against_each_other(void)
{
    static const uint8_t src[3] = {1u, 2u, 3u};
    fill(src, sizeof src);
    TEST_ASSERT_EQUAL_size_t(3u, infin.available(&head, &tail, RING_CAP));
    TEST_ASSERT_EQUAL_size_t((RING_CAP - 1u) - 3u, infin.free_(&head, &tail, RING_CAP));
}

void test_available_counts_across_a_wrap(void)
{
    // Both cursors driven past the end, so head is numerically below tail and the count is only
    // right if it is taken modulo the capacity.
    atomic_store(&tail, RING_CAP - 2u);
    atomic_store(&head, 1u);
    TEST_ASSERT_EQUAL_size_t(3u, infin.available(&head, &tail, RING_CAP));
}

void test_read_byte_refuses_an_empty_ring(void)
{
    uint8_t got = 0xAAu;
    TEST_ASSERT_FALSE_MESSAGE(infin.read_byte(ring, RING_CAP, &head, &tail, &got),
                              "an empty ring has no byte to hand back");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, got, "and it does not write to the out parameter");
    TEST_ASSERT_EQUAL_size_t(0u, atomic_load(&tail));
}

void test_read_byte_takes_one_and_moves_the_tail(void)
{
    static const uint8_t src[2] = {0x5Au, 0xA5u};
    fill(src, sizeof src);

    uint8_t got = 0u;
    TEST_ASSERT_TRUE(infin.read_byte(ring, RING_CAP, &head, &tail, &got));
    TEST_ASSERT_EQUAL_HEX8(0x5Au, got);
    TEST_ASSERT_EQUAL_size_t(1u, atomic_load(&tail));
    TEST_ASSERT_EQUAL_size_t(1u, infin.available(&head, &tail, RING_CAP));
}

void test_read_byte_wraps_the_tail_at_the_end(void)
{
    static const uint8_t one = 0x7Fu;
    atomic_store(&tail, RING_CAP - 1u);
    atomic_store(&head, RING_CAP - 1u);
    fill(&one, 1u);

    uint8_t got = 0u;
    TEST_ASSERT_TRUE(infin.read_byte(ring, RING_CAP, &head, &tail, &got));
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, got);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, atomic_load(&tail), "the tail wrapped rather than running off the end");
}

void test_read_stops_at_the_head(void)
{
    static const uint8_t src[3] = {1u, 2u, 3u};
    uint8_t dst[8] = {0};
    fill(src, sizeof src);

    // Asks for more than is there: the loop ends because the cursors met, not because it filled.
    TEST_ASSERT_EQUAL_size_t(3u, infin.read(ring, RING_CAP, &head, &tail, dst, sizeof dst));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, dst, 3u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, infin.available(&head, &tail, RING_CAP), "the read consumed what it took");
}

void test_read_stops_at_maxn(void)
{
    static const uint8_t src[4] = {9u, 8u, 7u, 6u};
    uint8_t dst[2] = {0};
    fill(src, sizeof src);

    // The other way out of the same loop: it filled, and the ring still holds the rest.
    TEST_ASSERT_EQUAL_size_t(2u, infin.read(ring, RING_CAP, &head, &tail, dst, sizeof dst));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, dst, 2u);
    TEST_ASSERT_EQUAL_size_t(2u, infin.available(&head, &tail, RING_CAP));
}

void test_read_of_nothing_takes_nothing(void)
{
    static const uint8_t src[2] = {1u, 2u};
    uint8_t dst[1] = {0xEEu};
    fill(src, sizeof src);

    TEST_ASSERT_EQUAL_size_t(0u, infin.read(ring, RING_CAP, &head, &tail, dst, 0u));
    TEST_ASSERT_EQUAL_HEX8(0xEEu, dst[0]);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, infin.available(&head, &tail, RING_CAP), "a read of zero is not a consume");
}

void test_read_wraps_around_the_end(void)
{
    static const uint8_t src[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t dst[4] = {0};
    atomic_store(&tail, RING_CAP - 2u);
    atomic_store(&head, RING_CAP - 2u);
    fill(src, sizeof src);

    TEST_ASSERT_EQUAL_size_t(4u, infin.read(ring, RING_CAP, &head, &tail, dst, sizeof dst));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, dst, 4u);
    TEST_ASSERT_EQUAL_size_t(2u, atomic_load(&tail));
}

void test_peek_copies_without_consuming(void)
{
    static const uint8_t src[4] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    uint8_t dst[2] = {0};
    fill(src, sizeof src);

    infin.peek(ring, RING_CAP, &tail, 1u, dst, sizeof dst);
    TEST_ASSERT_EQUAL_HEX8(0xADu, dst[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, dst[1]);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, atomic_load(&tail), "peek leaves the tail where it was");
    TEST_ASSERT_EQUAL_size_t(4u, infin.available(&head, &tail, RING_CAP));
}

void test_peek_of_nothing_writes_nothing(void)
{
    static const uint8_t src[2] = {1u, 2u};
    uint8_t dst[1] = {0xC3u};
    fill(src, sizeof src);

    infin.peek(ring, RING_CAP, &tail, 0u, dst, 0u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xC3u, dst[0], "a peek of zero bytes does not touch the destination");
}

void test_peek_wraps_around_the_end(void)
{
    static const uint8_t src[3] = {0x01u, 0x02u, 0x03u};
    uint8_t dst[3] = {0};
    atomic_store(&tail, RING_CAP - 1u);
    atomic_store(&head, RING_CAP - 1u);
    fill(src, sizeof src);

    infin.peek(ring, RING_CAP, &tail, 0u, dst, sizeof dst);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, dst, 3u);
}

void test_consume_drops_bytes_and_wraps(void)
{
    static const uint8_t src[4] = {1u, 2u, 3u, 4u};
    fill(src, sizeof src);

    infin.consume(&tail, RING_CAP, 3u);
    TEST_ASSERT_EQUAL_size_t(3u, atomic_load(&tail));
    TEST_ASSERT_EQUAL_size_t(1u, infin.available(&head, &tail, RING_CAP));

    infin.consume(&tail, RING_CAP, RING_CAP - 2u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, atomic_load(&tail), "the cursor is taken modulo the capacity");
}

void test_write_span_of_nothing_leaves_the_head(void)
{
    static const uint8_t src[1] = {0xFFu};
    TEST_ASSERT_EQUAL_size_t(3u, infin.write_span(ring, RING_CAP, 3u, src, 0u));
}

void test_write_span_fits_without_wrapping(void)
{
    static const uint8_t src[3] = {0xA0u, 0xA1u, 0xA2u};
    TEST_ASSERT_EQUAL_size_t(3u, infin.write_span(ring, RING_CAP, 0u, src, sizeof src));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(src, ring, 3u);
}

void test_write_span_splits_at_the_end_and_wraps(void)
{
    // Starts two from the end with four bytes to place: the first chunk is what is left before the
    // end, the second is the remainder at the front. Both arms of the chunk clamp, in one call.
    static const uint8_t src[4] = {0xB0u, 0xB1u, 0xB2u, 0xB3u};
    TEST_ASSERT_EQUAL_size_t(2u, infin.write_span(ring, RING_CAP, RING_CAP - 2u, src, sizeof src));
    TEST_ASSERT_EQUAL_HEX8(0xB0u, ring[RING_CAP - 2u]);
    TEST_ASSERT_EQUAL_HEX8(0xB1u, ring[RING_CAP - 1u]);
    TEST_ASSERT_EQUAL_HEX8(0xB2u, ring[0]);
    TEST_ASSERT_EQUAL_HEX8(0xB3u, ring[1]);
}

// ------------------------------------------------------------------------------------------------
// The segment queue
// ------------------------------------------------------------------------------------------------

void test_a_fresh_queue_has_nothing_in_flight(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, seg.inflight(&claim, &rel));
}

void test_next_hands_out_a_segment_and_publish_makes_it_visible(void)
{
    size_t idx = 0xFFu;
    TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t(0u, idx);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, seg.inflight(&claim, &rel), "a claim is not in flight until it is published");

    seg.publish(&claim);
    TEST_ASSERT_EQUAL_size_t(1u, seg.inflight(&claim, &rel));
}

void test_next_refuses_when_every_segment_is_in_flight(void)
{
    size_t idx = 0u;
    for (unsigned i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
        seg.publish(&claim);
    }
    TEST_ASSERT_EQUAL_size_t(NSEGS, seg.inflight(&claim, &rel));

    idx = 0xFFu;
    TEST_ASSERT_FALSE_MESSAGE(seg.next(&claim, &rel, NSEGS, &idx), "the queue is full");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0xFFu, idx, "and it does not write to the out parameter");
}

void test_next_wraps_its_index_at_the_segment_count(void)
{
    size_t idx = 0u;
    for (unsigned i = 0; i < NSEGS; i++)
    {
        TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
        TEST_ASSERT_EQUAL_size_t(i, idx);
        seg.publish(&claim);
        seg.release(&rel);
    }
    TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, idx, "the index is the cursor masked, so it comes back round");
}

void test_front_is_empty_until_a_claim_is_published(void)
{
    size_t idx = 0xFFu;
    TEST_ASSERT_FALSE(seg.front(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0xFFu, idx, "nothing to front means nothing written back");

    TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_FALSE_MESSAGE(seg.front(&claim, &rel, NSEGS, &idx), "a claim alone is not visible to the consumer");

    seg.publish(&claim);
    TEST_ASSERT_TRUE(seg.front(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t(0u, idx);
}

void test_release_retires_the_oldest_segment(void)
{
    size_t idx = 0u;
    for (unsigned i = 0; i < 2u; i++)
    {
        TEST_ASSERT_TRUE(seg.next(&claim, &rel, NSEGS, &idx));
        seg.publish(&claim);
    }

    TEST_ASSERT_TRUE(seg.front(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t(0u, idx);
    seg.release(&rel);

    TEST_ASSERT_TRUE(seg.front(&claim, &rel, NSEGS, &idx));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, idx, "the next oldest moved up");
    TEST_ASSERT_EQUAL_size_t(1u, seg.inflight(&claim, &rel));

    seg.release(&rel);
    TEST_ASSERT_FALSE_MESSAGE(seg.front(&claim, &rel, NSEGS, &idx), "the queue drained");
}

void test_at_indexes_the_segment_store(void)
{
    TEST_ASSERT_EQUAL_PTR(&segstore[0], seg.at(segstore, SEG_SIZE, 0u));
    TEST_ASSERT_EQUAL_PTR(&segstore[2u * SEG_SIZE], seg.at(segstore, SEG_SIZE, 2u));
}

// ------------------------------------------------------------------------------------------------
// The loculus bitmap
// ------------------------------------------------------------------------------------------------

void test_bit_is_the_loculus_and_zero_past_the_end(void)
{
    TEST_ASSERT_EQUAL_HEX32(1u, loculus.bit(0u));
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, loculus.bit(MMGR_RING_LOCULI_MAX - 1u));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0u, loculus.bit(MMGR_RING_LOCULI_MAX),
                                    "an index the bitmap cannot hold has no bit");
}

void test_all_masks_the_low_loculi_and_saturates(void)
{
    TEST_ASSERT_EQUAL_HEX32(0u, loculus.all(0u));
    TEST_ASSERT_EQUAL_HEX32(0x0Fu, loculus.all(4u));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xFFFFFFFFu, loculus.all(MMGR_RING_LOCULI_MAX),
                                    "a shift by the full width is what the saturation exists to avoid");
}

void test_take_claims_once_and_refuses_a_second_holder(void)
{
    TEST_ASSERT_TRUE(loculus.take(&held, 2u));
    TEST_ASSERT_FALSE_MESSAGE(loculus.take(&held, 2u), "it is already held");
    TEST_ASSERT_TRUE_MESSAGE(loculus.take(&held, 3u), "a different loculus is unaffected");
}

void test_take_refuses_an_index_the_bitmap_cannot_hold(void)
{
    TEST_ASSERT_FALSE(loculus.take(&held, MMGR_RING_LOCULI_MAX));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0u, atomic_load(&held), "and it set no bit doing so");
}

void test_drop_lets_a_loculus_be_taken_again(void)
{
    TEST_ASSERT_TRUE(loculus.take(&held, 5u));
    loculus.drop(&held, 5u);
    TEST_ASSERT_EQUAL_HEX32(0u, atomic_load(&held));
    TEST_ASSERT_TRUE(loculus.take(&held, 5u));
}

void test_hold_binds_a_region_to_the_loculus(void)
{
    static const uint8_t data[4] = {1u, 2u, 3u, 4u};
    TEST_ASSERT_TRUE(loculus.hold(&held, keepout, 1u, data, sizeof data));

    const mmgr_keepout *k = loculus.keepout(keepout, 1u);
    TEST_ASSERT_EQUAL_PTR(data, k->buf);
    TEST_ASSERT_EQUAL_size_t(sizeof data, k->len);
}

void test_hold_refuses_a_loculus_already_held(void)
{
    static const uint8_t first[2] = {0xA1u, 0xA2u};
    static const uint8_t second[2] = {0xB1u, 0xB2u};
    TEST_ASSERT_TRUE(loculus.hold(&held, keepout, 1u, first, sizeof first));
    TEST_ASSERT_FALSE(loculus.hold(&held, keepout, 1u, second, sizeof second));

    const mmgr_keepout *k = loculus.keepout(keepout, 1u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(first, k->buf, "the refused hold did not rebind the region");
    TEST_ASSERT_EQUAL_size_t(sizeof first, k->len);
}

void test_mark_and_clear_move_one_loculus(void)
{
    loculus.mark(&ready_mask, 4u);
    TEST_ASSERT_EQUAL_HEX32(1u << 4u, atomic_load(&ready_mask));
    loculus.mark(&ready_mask, 6u);
    TEST_ASSERT_EQUAL_HEX32((1u << 4u) | (1u << 6u), atomic_load(&ready_mask));

    loculus.clear(&ready_mask, 4u);
    TEST_ASSERT_EQUAL_HEX32(1u << 6u, atomic_load(&ready_mask));
}

void test_ready_is_marked_and_not_held_and_in_range(void)
{
    loculus.mark(&ready_mask, 0u);
    loculus.mark(&ready_mask, 1u);
    loculus.mark(&ready_mask, 5u);
    TEST_ASSERT_TRUE(loculus.take(&held, 1u));

    // 0 is ready, 1 is ready but held, 5 is ready but outside the count.
    TEST_ASSERT_EQUAL_HEX32(1u, loculus.ready(&ready_mask, &held, 4u));
}

void test_ctz_is_the_index_of_the_lowest_set_bit(void)
{
    TEST_ASSERT_EQUAL_INT32(0, loculus.ctz(1u));
    TEST_ASSERT_EQUAL_INT32(1, loculus.ctz(2u));
    TEST_ASSERT_EQUAL_INT32(3, loculus.ctz(0xF8u));
    TEST_ASSERT_EQUAL_INT32(31, loculus.ctz(0x80000000u));
}

void test_next_is_the_lowest_set_loculus_or_none(void)
{
    TEST_ASSERT_EQUAL_INT32_MESSAGE(-1, loculus.next(0u), "an empty bitmap has no next");
    TEST_ASSERT_EQUAL_INT32(0, loculus.next(0xFFFFFFFFu));
    TEST_ASSERT_EQUAL_INT32(4, loculus.next(1u << 4u));
}
