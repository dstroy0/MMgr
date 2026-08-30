// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "memoria_anularis/memoria_anularis.h"

#include "guard_page.h"

#include <stdio.h>
#include <string.h>

#define RING_CAP 64u
#define NSEGS 4u
#define BIG_CAP 512u
#define SPANS 200u

static mmgr_ring g_ring;

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
    uint8_t *buf;
    size_t cap;
    size_t start;
    uint8_t *dst;
    const uint8_t *src;
    size_t bytes;
} Move;

static void do_prime(void *v)
{
    const Move *const m = (const Move *)v;

    (void)MMGR_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = m->buf, .capacity = m->cap,
                    .segment_count = NSEGS);
    if (m->start != 0u)
    {
        (void)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_side, .bytes = m->start);
        MMGR_CALL(anularis.consume, AnularisCfg, .ring = &g_ring, .bytes = m->start);
    }
}

static void do_put(void *v)
{
    const Move *const m = (const Move *)v;

    keep((size_t)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = m->src, .bytes = m->bytes));
}

static void do_peek(void *v)
{
    const Move *const m = (const Move *)v;

    MMGR_CALL(anularis.peek, AnularisCfg, .ring = &g_ring, .dst = m->dst, .bytes = m->bytes, .offset = 0u);
}

static void do_read(void *v)
{
    const Move *const m = (const Move *)v;

    keep(MMGR_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = m->dst, .bytes = m->bytes));
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

        m.bytes = MMGR_CALL(anularis.vacant, AnularisCfg, .ring = &g_ring);

        none_past("put", do_put, &m, start);
        none_past("peek", do_peek, &m, start);
        none_past("read", do_read, &m, start);
    }
}

void test_a_drain_stays_inside_the_destination_it_was_given(void)
{
    needs_the_guard();

    (void)MMGR_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                    .segment_count = NSEGS);
    (void)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_side, .bytes = BIG_CAP / 2u);

    for (size_t n = 1u; n <= SPANS; n++)
    {
        Move m;

        memset(&m, 0, sizeof m);
        m.dst = place(n);
        m.bytes = n;

        none_past("peek", do_peek, &m, n);

        (void)MMGR_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                        .segment_count = NSEGS);
        (void)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_side, .bytes = BIG_CAP / 2u);

        none_past("read", do_read, &m, n);
    }
}

void test_a_fill_stays_inside_the_source_it_was_given(void)
{
    needs_the_guard();

    for (size_t n = 1u; n <= SPANS; n++)
    {
        Move m;

        memset(&m, 0, sizeof m);
        m.src = place(n);
        m.bytes = n;

        (void)MMGR_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_big, .capacity = BIG_CAP,
                        .segment_count = NSEGS);

        none_past("put", do_put, &m, n);
    }
}
