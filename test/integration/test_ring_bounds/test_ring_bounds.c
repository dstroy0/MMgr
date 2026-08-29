// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Every region the ring touches, held to exactly the extent its caller was asked for.
//
// The mover steps whole words and then carries the odd tail. A tail carried as one masked word
// reaches up to a word past the request - and because such a blend writes the destination's own
// lanes back with the values they already held, it is invisible to a poison byte: the bytes past
// the request come out unchanged either way. A page that faults on any access is the only
// instrument that sees it, which is why this suite exists rather than a memcmp against a canary.
//
// Three regions can each be the one that overruns, so each gets its own case with the guarded page
// under it in turn: the ring's own buffer, the destination a drain writes, and the source a fill
// reads. In every case the object is placed so its last byte is the last readable byte before the
// guard, and it is exactly the size the header asks a caller for - no slack.
#include "unity.h"

#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include "guard_page.h"

#include <stdio.h>
#include <string.h>

#define RING_CAP 64u
#define NSEGS 4u
#define BIG_CAP 512u
#define SPANS 200u

static mmgr_ring g_ring;

// The unguarded side of a move, big enough to be whatever the guarded side is not
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_side[BIG_CAP * 2u];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_big[BIG_CAP];

void setUp(void)
{
}

void tearDown(void)
{
}

static void needs_the_guard(void)
{
    if (!mmgr_guard_available())
    {
        TEST_IGNORE_MESSAGE("no page protection on this platform, so there is no instrument to read a bound with");
    }
}

/**
 * @brief Places n bytes so their last byte is the last readable one before the tail guard.
 */
static uint8_t *place(size_t n)
{
    return mmgr_guard_run() + mmgr_guard_page_size() - n;
}

static size_t g_sink;

static void keep(size_t v)
{
    g_sink += v;
}

typedef struct
{
    uint8_t *buf;      /**< Ring bytes for a case that re-inits [BORROWS]. */
    size_t cap;        /**< Ring size for that init. */
    size_t start;      /**< Head offset to bring the ring to first. */
    uint8_t *dst;      /**< Destination a drain writes [BORROWS]. */
    const uint8_t *src;/**< Source a fill reads [BORROWS]. */
    size_t bytes;      /**< Byte count the call moves. */
} Move;

/**
 * @brief Lays the ring over m->buf and walks the head to m->start, leaving nothing readable behind.
 *
 * @note Priming is itself a fill, so it runs under the guard rather than outside it - otherwise a
 *       mover that overran would take the process down here instead of failing a case.
 */
static void do_prime(void *v)
{
    const Move *const m = (const Move *)v;

    (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = m->buf, .capacity = m->cap,
                    .segment_count = NSEGS);
    if (m->start != 0u)
    {
        (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_side, .bytes = m->start);
        MMGR_CALL(iteratio_infinita.consume, InfinCfg, .ring = &g_ring, .bytes = m->start);
    }
}

static void do_put(void *v)
{
    const Move *const m = (const Move *)v;

    keep((size_t)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = m->src, .bytes = m->bytes));
}

static void do_peek(void *v)
{
    const Move *const m = (const Move *)v;

    MMGR_CALL(iteratio_infinita.peek, InfinCfg, .ring = &g_ring, .dst = m->dst, .bytes = m->bytes, .offset = 0u);
}

static void do_read(void *v)
{
    const Move *const m = (const Move *)v;

    keep(MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = m->dst, .bytes = m->bytes));
}

static void none_past(const char *what, void (*fn)(void *), const Move *m, size_t at)
{
    if (mmgr_guard_run_thunk(fn, (void *)(uintptr_t)m))
    {
        char msg[160];

        (void)snprintf(msg, sizeof msg, "%s left its region: %zu bytes at %zu", what, m->bytes, at);
        TEST_FAIL_MESSAGE(msg);
    }
}

/**
 * @brief The instrument itself, so a clean run below cannot be a guard that never armed.
 */
void test_the_guard_is_armed(void)
{
    needs_the_guard();

    uint8_t *const run = mmgr_guard_run();
    const size_t page = mmgr_guard_page_size();

    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run + page),
                             "the tail guard did not trap, so nothing below means anything");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run - 1), "the head guard did not trap");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_guard_traps_on(run), "the run itself must be readable");
}

/**
 * @brief The ring's own buffer, at every head the producer can be sitting on.
 *
 * @note A head that is not a whole number of words is what puts a partial word at the buffer's end,
 *       so the offsets that matter are the ones the caller never chose deliberately.
 */
void test_the_ring_stays_inside_its_buffer_at_every_head(void)
{
    needs_the_guard();

    for (size_t start = 0u; start < RING_CAP; start++)
    {
        Move m;

        memset(&m, 0, sizeof m);
        m.buf = place(RING_CAP);
        m.cap = RING_CAP;
        m.start = start;
        m.src = g_side;
        m.dst = g_side;

        none_past("priming the head", do_prime, &m, start);

        m.bytes = MMGR_CALL(iteratio_infinita.vacant, InfinCfg, .ring = &g_ring);

        none_past("put", do_put, &m, start);
        none_past("peek", do_peek, &m, start);
        none_past("read", do_read, &m, start);
    }
}

/**
 * @brief The destination a drain writes, sized to exactly what the caller asked for.
 */
void test_a_drain_stays_inside_the_destination_it_was_given(void)
{
    needs_the_guard();

    (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                    .segment_count = NSEGS);
    (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_side, .bytes = BIG_CAP / 2u);

    for (size_t n = 1u; n <= SPANS; n++)
    {
        Move m;

        memset(&m, 0, sizeof m);
        m.dst = place(n);
        m.bytes = n;

        none_past("peek", do_peek, &m, n);

        // read advances the tail, so the ring is refilled rather than drained across the sweep
        (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                        .segment_count = NSEGS);
        (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_side, .bytes = BIG_CAP / 2u);

        none_past("read", do_read, &m, n);
    }
}

/**
 * @brief The source a fill reads, sized to exactly what the caller offered.
 */
void test_a_fill_stays_inside_the_source_it_was_given(void)
{
    needs_the_guard();

    for (size_t n = 1u; n <= SPANS; n++)
    {
        Move m;

        memset(&m, 0, sizeof m);
        m.src = place(n);
        m.bytes = n;

        (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                        .segment_count = NSEGS);

        none_past("put", do_put, &m, n);
    }
}
