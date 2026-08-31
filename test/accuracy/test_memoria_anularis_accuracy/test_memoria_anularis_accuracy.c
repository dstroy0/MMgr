// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_memoria_anularis_accuracy.c
 * @brief Checks that the byte sequence coming out of the ring is the sequence that went in, across
 *        thousands of wraps, against a queue that holds its bytes in a plain array and wraps nothing.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note A ring is a mask, two cursors and a move divided at the end of the buffer. Every one of those
 *       is arithmetic that can be off by one and still return plausible counts, and a caller only
 *       finds out when a byte arrives in the wrong place.
 * @note The reference queue appends at the end of an array and removes from the front by shifting the
 *       rest down. It has no capacity mask, no wrap and no two-run move, so it and the module have no
 *       arithmetic in common and cannot be wrong the same way.
 * @note The capacity here is deliberately small. A large ring wraps once in a long run of operations,
 *       and the wrap is where the two-run move either divides correctly or does not. A small ring
 *       reaches that path thousands of times in the same run.
 * @note Every byte written is drawn from a counter that never repeats within a case, which is what
 *       makes a byte delivered out of order visible as itself instead of matching by luck.
 * @note Contract checks on a capacity that is not a power of two, a segment count above the capacity
 *       and a null buffer live in test_memoria_anularis. This file asks where the bytes go.
 */
#include <stdint.h>
#include <stdio.h>

#include "memoria_anularis/memoria_anularis.h"

#include "unity.h"

/**
 * @brief Expands to 64u, the ring capacity every case here uses.
 *
 * @note A power of two, which mmgr_anular_init requires. Small enough that a run of a few thousand
 *       operations wraps the buffer many times over.
 */
#define MMGR_ACCURACY_RING_CAPACITY 64u

/**
 * @brief Expands to 8u, the segments the ring is divided into.
 *
 * @note A power of two no larger than the capacity, which mmgr_anular_init requires. Eight segments
 *       of eight bytes divide the buffer exactly.
 */
#define MMGR_ACCURACY_RING_SEGMENTS 8u

/**
 * @brief Expands to the bytes one segment covers, worked out from the two numbers init was given.
 *
 * @note Computed here from what this file passed to mmgr_anular_init, not read back from the ring.
 *       The ring's own segment_bytes is exactly what the segment cases are checking.
 */
#define MMGR_ACCURACY_RING_SEGMENT_BYTES (MMGR_ACCURACY_RING_CAPACITY / MMGR_ACCURACY_RING_SEGMENTS)

/**
 * @brief Expands to 0xA5, the byte a destination is filled with before a read.
 *
 * @note A byte the module never writes on its own. A destination position left holding it is one the
 *       read did not reach, which is how a short move is told from a wrong one.
 */
#define MMGR_ACCURACY_RING_GUARD 0xA5u

/**
 * @brief A byte queue that holds its contents in order, with no wrap of any kind.
 *
 * @note The whole reference. Bytes go on at the end and come off the front, and removing shifts the
 *       rest down. That shift is what a ring exists to avoid, which is exactly why it is the right
 *       construction to check a ring against.
 */
typedef struct
{
    uint8_t byte[MMGR_ACCURACY_RING_CAPACITY]; /**< Bytes held, oldest first. */
    size_t count;                              /**< How many of them are held. */
} AccuracyQueue;

/**
 * @brief Appends bytes to the back of the queue.
 *
 * @param[in,out] queue Queue to append to [BORROWS].
 * @param[in]     src   Bytes to append [BORROWS].
 * @param[in]     bytes How many to append.
 * @warning The caller keeps the total at or below one less than the capacity, which is the most the
 *          ring holds. Nothing here bounds it.
 */
static void accuracy_queue_push(AccuracyQueue *queue, const uint8_t *src, size_t bytes)
{
    for (size_t index = 0u; index < bytes; index++)
    {
        queue->byte[queue->count] = src[index];
        queue->count++;
    }
}

/**
 * @brief Removes bytes from the front of the queue, copying them out.
 *
 * @param[in,out] queue Queue to remove from [BORROWS].
 * @param[out]    dst   Where the removed bytes go, or NULL to discard them [BORROWS].
 * @param[in]     bytes How many to remove.
 * @note Shifts the rest down one position at a time. The cost is beside the point here; what matters
 *       is that the order the bytes come off in is a property of the array and of nothing else.
 * @warning The caller keeps bytes at or below the count held. Nothing here bounds it.
 */
static void accuracy_queue_pop(AccuracyQueue *queue, uint8_t *dst, size_t bytes)
{
    for (size_t index = 0u; index < bytes; index++)
    {
        if (dst != NULL)
        {
            dst[index] = queue->byte[index];
        }
    }
    for (size_t index = bytes; index < queue->count; index++)
    {
        queue->byte[index - bytes] = queue->byte[index];
    }
    queue->count -= bytes;
}

/**
 * @brief Copies bytes out of the queue without removing them.
 *
 * @param[in]  queue  Queue to read [BORROWS].
 * @param[out] dst    Where the bytes go [BORROWS].
 * @param[in]  offset How far past the front to start.
 * @param[in]  bytes  How many to copy.
 * @note The mirror of what peek is documented to do, built as a plain array read.
 * @warning The caller keeps offset plus bytes at or below the count held. Nothing here bounds it.
 */
static void accuracy_queue_peek(const AccuracyQueue *queue, uint8_t *dst, size_t offset, size_t bytes)
{
    for (size_t index = 0u; index < bytes; index++)
    {
        dst[index] = queue->byte[offset + index];
    }
}

/**
 * @brief Advances a deterministic pattern and returns the next value.
 *
 * @param[in,out] state Generator state [BORROWS].
 * @return              The advanced state.
 * @note A fixed recurrence. A failure reproduces on the next run with the same values in the same
 *       order, and nothing here is seeded from the clock.
 * @note The multiplier and increment are the ones Knuth gives for a 64-bit linear congruential
 *       sequence. The values only have to be varied and repeatable, and no statistical property of
 *       them is relied on.
 */
static uint64_t accuracy_next_pattern(uint64_t *state)
{
    *state = (*state * UINT64_C(6364136223846793005)) + UINT64_C(1442695040888963407);
    return *state;
}

/**
 * @brief Returns the index of the lowest set bit of a mask, or -1 when it holds none.
 *
 * @param[in] mask Mask to scan.
 * @return         The index, or -1.
 * @note A bit at a time from the bottom up. The module folds a population count instead, so the two
 *       reach the same number by different arithmetic.
 */
static int accuracy_lowest_set_bit(embed_word mask)
{
    for (unsigned position = 0u; position < EMBED_WORD_BITS; position++)
    {
        // Explicit cast builds the probe bit at the width the mask is carried in
        if ((mask & (embed_word)((embed_word)1 << position)) != 0u)
        {
            return (int)position;
        }
    }
    return -1;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every case lays a fresh ring down with mmgr_anular_init, which is what puts the cursors and
 *       the loculus masks back to a known state.
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
 * @brief Checks the reference queue this suite rests on against sequences worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. A queue that dropped a byte on the shift
 *       would report the ring as wrong at the first wrap, and the ring would be blamed for it.
 * @note Push, pop and peek are each checked to move the bytes the operation names and to leave the
 *       count where it belongs.
 * @note The shift is checked with a pop from the middle of a filled queue, which is the step that
 *       reorders bytes when it is written wrongly.
 */
void test_the_reference_queue_this_suite_relies_on_is_itself_right(void)
{
    static const uint8_t source[6] = {10u, 20u, 30u, 40u, 50u, 60u};
    AccuracyQueue queue = {{0u}, 0u};
    uint8_t taken[6] = {0u};

    accuracy_queue_push(&queue, source, 6u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(6u, queue.count, "six bytes were pushed");

    accuracy_queue_peek(&queue, taken, 2u, 3u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(30u, taken[0], "a peek at offset two did not start at the third byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(50u, taken[2], "a peek of three did not end at the fifth byte");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(6u, queue.count, "a peek removed bytes");

    accuracy_queue_pop(&queue, taken, 2u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(10u, taken[0], "a pop did not take the oldest byte first");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(20u, taken[1], "a pop took the second byte out of order");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, queue.count, "a pop of two did not leave four");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(30u, queue.byte[0], "the shift did not bring the third byte to the front");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(60u, queue.byte[3], "the shift lost the last byte");
}

/**
 * @brief Checks that a long run of writes and reads delivers every byte in the order it went in.
 *
 * @note This is the case the file exists for. The counts and the byte order are both checked after
 *       every operation, which catches a wrap that divided a move wrongly at the operation that did
 *       it and not thousands of bytes later.
 * @note The sizes are drawn from a repeatable pattern and are bounded only by the room the ring
 *       reports, which is what walks the head and the tail into every alignment against the end of
 *       the buffer.
 * @note The destination is filled with a guard byte before every read. A read that reported more
 *       bytes than it moved leaves the guard standing, which the comparison against the queue then
 *       reports.
 */
void test_a_long_run_of_writes_and_reads_delivers_every_byte_in_order(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;
    AccuracyQueue queue = {{0u}, 0u};
    uint64_t state = UINT64_C(0x0123456789ABCDEF);
    uint8_t next_value = 1u;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two capacity");

    for (unsigned step = 0u; step < 4000u; step++)
    {
        const size_t vacant = EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &ring);
        const size_t available = EMBED_CALL(anularis.available, AnularisCfg, .ring = &ring);
        char label[96];

        (void)snprintf(label, sizeof label, "at step %u", step);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(queue.count, available, label);
        TEST_ASSERT_EQUAL_size_t_MESSAGE((MMGR_ACCURACY_RING_CAPACITY - 1u) - queue.count, vacant, label);

        if (((accuracy_next_pattern(&state) >> 33) & 1u) != 0u)
        {
            uint8_t written[MMGR_ACCURACY_RING_CAPACITY];
            // Explicit cast narrows the pattern to the count a write moves. The modulus holds it at
            // the vacant bytes, which is what the ring is documented to accept
            const size_t bytes = (vacant == 0u) ? 0u : (size_t)(accuracy_next_pattern(&state) % (vacant + 1u));

            for (size_t index = 0u; index < bytes; index++)
            {
                written[index] = next_value;
                next_value = (uint8_t)((next_value % 251u) + 1u);
            }
            TEST_ASSERT_TRUE_MESSAGE(
                EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = written, .bytes = bytes),
                "the ring refused a span that fits in its vacant bytes");
            accuracy_queue_push(&queue, written, bytes);
        }
        else
        {
            uint8_t taken[MMGR_ACCURACY_RING_CAPACITY];
            uint8_t expected[MMGR_ACCURACY_RING_CAPACITY];
            // Explicit cast narrows the pattern to the count a read asks for. It may exceed what is
            // available, which is the case the read is documented to shorten
            const size_t asked = (size_t)(accuracy_next_pattern(&state) % (MMGR_ACCURACY_RING_CAPACITY + 1u));
            const size_t should_take = (asked < queue.count) ? asked : queue.count;

            for (size_t index = 0u; index < MMGR_ACCURACY_RING_CAPACITY; index++)
            {
                taken[index] = MMGR_ACCURACY_RING_GUARD;
            }
            accuracy_queue_peek(&queue, expected, 0u, should_take);

            const size_t took = EMBED_CALL(anularis.read, AnularisCfg, .ring = &ring, .dst = taken, .bytes = asked);

            TEST_ASSERT_EQUAL_size_t_MESSAGE(should_take, took, label);

            // Unity refuses a comparison over no elements, and an empty ring legitimately hands back
            // none. The count above is what covers that case
            if (should_take != 0u)
            {
                TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(expected, taken, should_take, label);
            }
            accuracy_queue_pop(&queue, NULL, should_take);
        }
    }

    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0u, (unsigned)next_value, "the run wrote no bytes at all");
}

/**
 * @brief Checks that a byte at a time comes out in the same order a whole span does.
 *
 * @note read_byte takes its byte from the tail and advances one position, which is the ring's
 *       arithmetic without the two-run move over it. A ring whose move divided wrongly at the wrap
 *       still passes this, and a ring whose cursors are wrong fails both.
 * @note The run is longer than the capacity many times over, so the single-byte path crosses the end
 *       of the buffer repeatedly.
 */
void test_a_byte_at_a_time_comes_out_in_the_order_it_went_in(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;
    uint8_t next_value = 1u;
    uint8_t expected_value = 1u;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two capacity");

    for (unsigned round = 0u; round < 200u; round++)
    {
        const unsigned span = (round % 13u) + 1u;

        for (unsigned index = 0u; index < span; index++)
        {
            const uint8_t value = next_value;

            TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = &value, .bytes = 1u),
                                     "a single byte would not fit in an almost empty ring");
            next_value = (uint8_t)((next_value % 251u) + 1u);
        }
        for (unsigned index = 0u; index < span; index++)
        {
            uint8_t taken = MMGR_ACCURACY_RING_GUARD;
            char message[96];

            TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.read_byte, AnularisCfg, .ring = &ring, .dst = &taken),
                                     "the ring reported empty while bytes were waiting");
            (void)snprintf(message, sizeof message, "round %u byte %u came out of order", round, index);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected_value, taken, message);
            expected_value = (uint8_t)((expected_value % 251u) + 1u);
        }
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, EMBED_CALL(anularis.available, AnularisCfg, .ring = &ring),
                                         "the ring still reported bytes after every one was taken");
    }
}

/**
 * @brief Checks that a peek returns what a read would and leaves the tail where it was.
 *
 * @note peek is the one read that moves no cursor. The property under test is that it is a window on
 *       the same bytes and not a second reader with a position of its own.
 * @note The peek is taken at every offset into the held bytes and at every length that fits, which
 *       walks the window across the end of the buffer at both of its ends.
 * @note The bytes are read again with a real read afterwards, and they are the same ones, which is
 *       what shows the peek moved nothing.
 */
void test_a_peek_returns_what_a_read_would_and_moves_nothing(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;
    uint8_t written[MMGR_ACCURACY_RING_CAPACITY];

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two capacity");

    for (unsigned start = 0u; start < MMGR_ACCURACY_RING_CAPACITY; start++)
    {
        AccuracyQueue queue = {{0u}, 0u};
        const size_t held = MMGR_ACCURACY_RING_CAPACITY - 1u;

        // Walk the cursors to a fresh position before each pass, so the peek below starts at a
        // different distance from the end of the buffer every time
        for (unsigned index = 0u; index < start; index++)
        {
            const uint8_t filler = 0u;

            (void)EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = &filler, .bytes = 1u);
            EMBED_CALL(anularis.consume, AnularisCfg, .ring = &ring, .bytes = 1u);
        }

        for (size_t index = 0u; index < held; index++)
        {
            // Explicit cast narrows the mixed index to the byte the ring carries. The values differ
            // along the span, which is what makes a byte delivered out of place visible
            written[index] = (uint8_t)((index * 7u) + start + 1u);
        }
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = written, .bytes = held),
                                 "a full span would not fit in an empty ring");
        accuracy_queue_push(&queue, written, held);

        for (size_t offset = 0u; offset < held; offset += 5u)
        {
            const size_t bytes = held - offset;
            uint8_t peeked[MMGR_ACCURACY_RING_CAPACITY];
            uint8_t expected[MMGR_ACCURACY_RING_CAPACITY];
            char message[96];

            accuracy_queue_peek(&queue, expected, offset, bytes);
            EMBED_CALL(anularis.peek, AnularisCfg, .ring = &ring, .dst = peeked, .bytes = bytes, .offset = offset);

            (void)snprintf(message, sizeof message, "a peek at offset %u returned the wrong bytes", (unsigned)offset);
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(expected, peeked, bytes, message);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(held, EMBED_CALL(anularis.available, AnularisCfg, .ring = &ring),
                                             "a peek moved the tail");
        }

        EMBED_CALL(anularis.consume, AnularisCfg, .ring = &ring, .bytes = held);
    }
}

/**
 * @brief Checks that a span too large for the vacant bytes is refused whole.
 *
 * @note A partial write is worse than a refusal, since the consumer then reads a span that was never
 *       complete and nothing reports it. The claim is that the head does not move at all.
 * @note The ring is filled to every level, and at each one a span one byte too large is offered. The
 *       readable count is checked before and after, and the bytes already held are read back to show
 *       none of them was overwritten.
 * @note One byte is held back always, so the largest span an empty ring takes is the capacity less
 *       one. The refusal is offered at exactly one byte past what is vacant.
 */
void test_a_span_too_large_is_refused_whole(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;
    uint8_t oversized[MMGR_ACCURACY_RING_CAPACITY + 1u];

    for (size_t index = 0u; index < sizeof oversized; index++)
    {
        // Explicit cast narrows the index to the byte the span carries. Any value distinct from what
        // is already held would do; this one is distinct along the span as well
        oversized[index] = (uint8_t)(index + 200u);
    }

    for (unsigned held = 0u; held < MMGR_ACCURACY_RING_CAPACITY; held++)
    {
        AccuracyQueue queue = {{0u}, 0u};
        uint8_t written[MMGR_ACCURACY_RING_CAPACITY];
        uint8_t taken[MMGR_ACCURACY_RING_CAPACITY];
        char message[96];

        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                            .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                            .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                                 "the ring refused a power of two capacity");

        for (size_t index = 0u; index < held; index++)
        {
            // Explicit cast narrows the mixed index to the byte the ring carries
            written[index] = (uint8_t)((index * 3u) + 1u);
        }
        if (held != 0u)
        {
            TEST_ASSERT_TRUE_MESSAGE(
                EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = written, .bytes = held),
                "the ring refused a span inside its capacity");
            accuracy_queue_push(&queue, written, held);
        }

        const size_t vacant = EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &ring);

        (void)snprintf(message, sizeof message, "with %u bytes held", held);
        TEST_ASSERT_EQUAL_size_t_MESSAGE((MMGR_ACCURACY_RING_CAPACITY - 1u) - held, vacant, message);
        TEST_ASSERT_FALSE_MESSAGE(
            EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = oversized, .bytes = vacant + 1u),
            "the ring took a span one byte larger than it had room for");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(held, EMBED_CALL(anularis.available, AnularisCfg, .ring = &ring), message);

        if (held != 0u)
        {
            TEST_ASSERT_EQUAL_size_t_MESSAGE(
                held, EMBED_CALL(anularis.read, AnularisCfg, .ring = &ring, .dst = taken, .bytes = held), message);
            TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(written, taken, held, "a refused span overwrote bytes already held");
        }
    }
}

/**
 * @brief Checks that segments come back in the order they were claimed and cover the buffer exactly.
 *
 * @note The segment view is a second reading of the same bytes, and its accuracy claim is that the
 *       spans tile the buffer: each one starts where the last ended, none overlaps another, and
 *       together they reach the whole capacity and no further.
 * @note The indices are checked to cycle through every segment in turn and to come back to the first,
 *       which is what a counter wrapped by mask is expected to do and what an off-by-one in that mask
 *       breaks at the wrap and nowhere else.
 * @note Publishes and releases run in step for more laps than there are segments, so the counters
 *       pass their own wrap several times.
 */
void test_segments_come_back_in_order_and_tile_the_buffer(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two segment count");

    for (unsigned index = 0u; index < MMGR_ACCURACY_RING_SEGMENTS; index++)
    {
        uint8_t *const at = EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = index);
        char message[96];

        (void)snprintf(message, sizeof message, "segment %u does not start where the one below it ended", index);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(&buffer[index * MMGR_ACCURACY_RING_SEGMENT_BYTES], at, message);
    }

    // The last segment's own span is what reaches the end. seg_at is documented not to bound its
    // index, so the byte past the last segment is worked out here instead of asked for
    TEST_ASSERT_EQUAL_PTR_MESSAGE(
        &buffer[MMGR_ACCURACY_RING_CAPACITY],
        EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = MMGR_ACCURACY_RING_SEGMENTS - 1u) +
            MMGR_ACCURACY_RING_SEGMENT_BYTES,
        "the segments do not reach the end of the buffer exactly");

    for (unsigned lap = 0u; lap < 5u; lap++)
    {
        for (unsigned index = 0u; index < MMGR_ACCURACY_RING_SEGMENTS; index++)
        {
            size_t claimed_index = MMGR_ACCURACY_RING_SEGMENTS;
            size_t front_index = MMGR_ACCURACY_RING_SEGMENTS;
            char message[96];

            TEST_ASSERT_TRUE_MESSAGE(
                EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &claimed_index),
                "the producer was refused a segment while none was in flight");
            (void)snprintf(message, sizeof message, "lap %u segment %u was claimed out of order", lap, index);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(index, claimed_index, message);

            EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &ring);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &ring),
                                             "one published segment did not report as one in flight");

            TEST_ASSERT_TRUE_MESSAGE(
                EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &ring, .out_index = &front_index),
                "the consumer saw no segment after one was published");
            (void)snprintf(message, sizeof message, "lap %u segment %u came back out of order", lap, index);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(index, front_index, message);

            EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &ring);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &ring),
                                             "a released segment still reported as in flight");
        }
    }
}

/**
 * @brief Checks that the producer is refused once every segment is in flight.
 *
 * @note The bound is the segment count, and a producer that could claim past it would hand out a
 *       segment the consumer has not released. The refusal is what makes the view safe to fill.
 * @note Every segment is claimed and published without a release, and the claim after the last one is
 *       expected to fail. A single release then lets exactly one more through.
 */
void test_the_producer_is_refused_once_every_segment_is_in_flight(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    mmgr_ring ring;
    size_t index_out = 0u;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two segment count");

    for (unsigned index = 0u; index < MMGR_ACCURACY_RING_SEGMENTS; index++)
    {
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &index_out),
                                 "the producer was refused a segment before the ring was full");
        EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &ring);
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_ACCURACY_RING_SEGMENTS,
                                     EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &ring),
                                     "every segment was published but not every one is in flight");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &index_out),
                              "the producer claimed a segment while every one was in flight");

    EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &ring);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &index_out),
                             "one release did not let one claim through");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, index_out, "the segment freed first was not the one handed out next");
}

/**
 * @brief Checks the loculus masks against a bitset this file keeps for itself.
 *
 * @note A loculus is takeable only while it is free and not held, and the ready mask is that pair of
 *       conditions as one word. The reference keeps the two conditions as separate arrays and builds
 *       the mask a bit at a time, so it shares no masking with the module.
 * @note Every loculus is held and dropped in turn, and the mask is compared after each step. A hold
 *       that set the wrong bit shows up as two loculi disagreeing at once.
 * @note A second hold of a loculus already held is expected to be refused, which is what keeps two
 *       callers from recording keepouts over each other.
 */
void test_the_loculus_masks_match_a_bitset_kept_alongside(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    uint8_t region[MMGR_RING_LOCULI > 0u ? MMGR_RING_LOCULI : 1u];
    mmgr_ring ring;
    embed_bool held_by_us[MMGR_RING_LOCULI > 0u ? MMGR_RING_LOCULI : 1u];

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two capacity");

    for (unsigned index = 0u; index < MMGR_RING_LOCULI; index++)
    {
        held_by_us[index] = EMBED_FALSE;
    }

    for (unsigned index = 0u; index < MMGR_RING_LOCULI; index++)
    {
        embed_word expected = 0u;
        char message[96];

        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &ring, .index = index,
                                            .src = &region[index], .bytes = 1u),
                                 "a free loculus was refused");
        held_by_us[index] = EMBED_TRUE;
        TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &ring, .index = index,
                                             .src = &region[index], .bytes = 1u),
                                  "a loculus already held was handed out again");

        for (unsigned probe = 0u; probe < MMGR_RING_LOCULI; probe++)
        {
            if (!held_by_us[probe])
            {
                // Explicit cast builds the reference bit at the width the mask is carried in
                expected |= (embed_word)((embed_word)1 << probe);
            }
        }

        (void)snprintf(message, sizeof message, "the ready mask is wrong after holding loculus %u", index);
        TEST_ASSERT_EQUAL_MESSAGE(expected, EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &ring), message);
    }

    for (unsigned index = 0u; index < MMGR_RING_LOCULI; index++)
    {
        embed_word expected = 0u;
        char message[96];

        EMBED_CALL(anularis.loculus_drop, AnularisCfg, .ring = &ring, .index = index);
        held_by_us[index] = EMBED_FALSE;

        for (unsigned probe = 0u; probe < MMGR_RING_LOCULI; probe++)
        {
            if (!held_by_us[probe])
            {
                // Explicit cast builds the reference bit at the width the mask is carried in
                expected |= (embed_word)((embed_word)1 << probe);
            }
        }

        (void)snprintf(message, sizeof message, "the ready mask is wrong after dropping loculus %u", index);
        TEST_ASSERT_EQUAL_MESSAGE(expected, EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &ring), message);
    }
}

/**
 * @brief Checks the lowest set bit against a bit-at-a-time scan, over every single bit and many masks.
 *
 * @note The module folds a population count to reach the index. The reference tests one bit at a time
 *       from the bottom, so the two arrive at the same number by arithmetic with nothing in common.
 * @note Every single-bit mask is covered, which pins the index at each position. A fold that lost a
 *       step is off by that step's width at half the positions.
 * @note An empty mask is checked to give -1, which is the one input the fold is documented not to
 *       handle on its own.
 */
void test_the_lowest_set_bit_matches_a_bit_at_a_time_scan(void)
{
    uint64_t state = UINT64_C(0xFEDCBA9876543210);

    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .mask = (embed_word)0),
                                  "an empty mask did not report a miss");

    for (unsigned position = 0u; position < EMBED_WORD_BITS; position++)
    {
        // Explicit cast builds the probe bit at the width the entry takes
        const embed_word mask = (embed_word)((embed_word)1 << position);
        char message[96];

        (void)snprintf(message, sizeof message, "a single bit at %u was not found there", position);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_lowest_set_bit(mask),
                                      (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .mask = mask), message);
    }

    for (unsigned step = 0u; step < 4096u; step++)
    {
        // Explicit cast narrows the pattern to the width the mask is carried in
        const embed_word mask = (embed_word)accuracy_next_pattern(&state);
        char message[96];

        (void)snprintf(message, sizeof message, "the lowest set bit is wrong at step %u", step);
        TEST_ASSERT_EQUAL_INT_MESSAGE(accuracy_lowest_set_bit(mask),
                                      (int)EMBED_CALL(anularis.loculus_next, AnularisCfg, .mask = mask), message);
    }
}

/**
 * @brief Checks that a held loculus hands back the region it was given.
 *
 * @note The keepout is what a reader walks in place, and the claim is that the span comes back naming
 *       the same bytes the hold recorded. A span that came back short leaves a reader walking past
 *       the region it was told to keep out of.
 * @note The read offset is checked to start at zero, since a fresh hold has had no reader walk it.
 * @note The record is checked again after the drop, which leaves it standing for a restream.
 */
void test_a_held_loculus_hands_back_the_region_it_was_given(void)
{
    uint8_t buffer[MMGR_ACCURACY_RING_CAPACITY];
    uint8_t region[32];
    mmgr_ring ring;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = buffer,
                                        .capacity = MMGR_ACCURACY_RING_CAPACITY,
                                        .segment_count = MMGR_ACCURACY_RING_SEGMENTS),
                             "the ring refused a power of two capacity");

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &ring, .index = 0u, .src = region,
                                        .bytes = sizeof region),
                             "a free loculus was refused");

    const mmgr_ring_span *const span = EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &ring, .index = 0u);

    TEST_ASSERT_NOT_NULL_MESSAGE(span, "a loculus in range handed back no span");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(region, span->buf, "the span names a different region than the hold recorded");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof region, span->bytes, "the span is a different length than was recorded");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, span->read_offset, "a fresh hold started with a reader part way through it");

    EMBED_CALL(anularis.loculus_drop, AnularisCfg, .ring = &ring, .index = 0u);

    const mmgr_ring_span *const after = EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &ring, .index = 0u);

    TEST_ASSERT_NOT_NULL_MESSAGE(after, "the span went away when the loculus was dropped");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(region, after->buf, "a drop moved the recorded region");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof region, after->bytes, "a drop changed the recorded length");
}
