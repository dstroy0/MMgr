// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Nothing reads past its bound.
//
// A poison pattern proves nothing here. A read leaves no trace, so the only way to catch one is to
// put the buffer where the next byte does not exist: a page marked no-access, flush against the end
// of the buffer, so a load one byte too far traps. test/support/guard_page.c arms that and catches
// the trap, so every entry gets asked and the failures are reported rather than being a crash.
//
// There are two bounds here and they are not the same number.
//
// A word at a time scan loads whole words, so it reads to the word-rounded cap. That is not slack
// that crept in - MMGR_SCAN_MAX_WORDS is asserted in verbum_scrutor.h to cover the largest tenant
// and to be tight, so the round up is reserved on purpose and every tenant has it. len, chr, eq,
// starts, diff and copy are held to that.
//
// find and has are held to the raw cap, with nothing rounded. Their scan takes an anchor byte from
// somewhere inside the needle, which used to put the load that many bytes further out than the
// scan position - measured at up to MMGR_SWAR_BYTES - 1 past the cap, past what the assert
// reserves. The scan now runs only while every load it makes is inside the cap and finishes the
// last candidates a byte at a time, so it holds to the stricter bound. This is the case that says
// so.
#include "unity.h"

#include "byteio/byteio.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "spatium/spatium.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include "guard_page.h"
#include "oracle_divergence.h"

#include <stdio.h>
#include <string.h>

// Enough caps to cross several word boundaries at every width, at both ends of the round up.
#define CAPS 200u

void setUp(void)
{
}

void tearDown(void)
{
}

/** @brief Round a byte count up the way a word at a time scan does. */
static size_t word_rounded(size_t n)
{
    return mmgr_scrut_words(n) * MMGR_SWAR_BYTES;
}

/**
 * @brief Lay out a buffer whose reserved extent ends at the guard.
 * @param cap What the caller will declare.
 * @param reserved How many bytes are actually readable, cap or the word rounding of it.
 * @return The first byte.
 */
static unsigned char *place(size_t cap, size_t reserved)
{
    unsigned char *run = mmgr_guard_run();
    const size_t page = mmgr_guard_page_size();

    (void)cap;
    memset(run, 'a', page);
    return run + page - reserved;
}

typedef struct
{
    const char *s;
    size_t cap;
} Ask;

/**
 * @brief Somewhere for a probe's answer to go.
 *
 * A probe throws its result away, and a call whose result is unused is a call the optimizer may
 * delete - which would leave the guard page untouched and the test passing for the wrong reason.
 * Handing the value to a sink the compiler cannot see through keeps the call without asking for
 * volatile semantics, which describe hardware that changes underneath you and not this.
 */
static size_t mmgr_probe_sink;
static void keep(size_t v)
{
    mmgr_probe_sink += v;
}

static void ask_len(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.len(a->s, a->cap)));
}
static void ask_chr(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.chr(a->s, a->cap, 0x02u)));
}
static void ask_eq(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.eq(a->s, a->s, a->cap, MMGR_FALSE)));
}
static void ask_eq_ci(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.eq(a->s, a->s, a->cap, MMGR_TRUE)));
}
static void ask_starts(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.starts(a->s, "aaa", a->cap, MMGR_FALSE)));
}
static void ask_diff(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(cellul.diff(a->s, a->s, a->cap, MMGR_FALSE)));
}
static void ask_copy(void *v)
{
    const Ask *a = (const Ask *)v;
    static char dst[CAPS + 8u];
    keep((size_t)(cellul.copy(dst, a->s, a->cap < sizeof dst ? a->cap : sizeof dst)));
}
static void ask_take_be(void *v)
{
    const Ask *a = (const Ask *)v;
    size_t rd_off = 0u;
    const uint8_t *rd_buf = (const uint8_t *)a->s;
    uint64_t out = 0;
    /* Right up to the bound and not one byte further - the page after it has no access. */
    byteio.take_be(rd_buf, a->cap, &rd_off, &out, (a->cap < 8u) ? a->cap : 8u);
    keep((size_t)out);
}

typedef struct
{
    const char *s;
    size_t cap;
    const char *needle;
    size_t nlen;
    mmgr_bool ci;
} Hunt;

static void ask_find(void *v)
{
    const Hunt *h = (const Hunt *)v;
    keep((size_t)(cellul.find(h->s, h->cap, h->needle, h->nlen, h->ci)));
}
static void ask_has(void *v)
{
    const Hunt *h = (const Hunt *)v;
    keep((size_t)(cellul.has(h->s, h->cap, h->needle, h->nlen, h->ci)));
}

/** @brief Ask one entry at every cap, and say which cap it went past at. */
static void none_past(const char *what, void (*fn)(void *), int raw_bound)
{
    for (size_t cap = 1; cap <= CAPS; cap++)
    {
        const size_t reserved = raw_bound ? cap : word_rounded(cap);
        Ask a;
        a.s = (const char *)place(cap, reserved);
        a.cap = cap;

        if (mmgr_guard_run_thunk(fn, &a))
        {
            char msg[128];
            (void)snprintf(msg, sizeof msg, "%s read past its bound at cap %zu (reserved %zu)", what, cap, reserved);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

/**
 * @brief Every case here needs both an armed guard and this library's bounds.
 *
 * On the oracle run the entries are libc's, and libc's take no read cap - strstr and strchr read
 * until something terminates. Asking them to respect a bound they were never given is not a test
 * of anything.
 */
static void needs_our_bounds(void)
{
    MMGR_SKIP_ON_ORACLE("a read cap is this library's, and libc's equivalents do not take one");

    if (!mmgr_guard_available())
    {
        TEST_IGNORE_MESSAGE("no page protection on this platform, so there is no instrument to read a bound with");
    }
}

void test_the_guard_is_armed(void)
{
    needs_our_bounds();

    unsigned char *run = mmgr_guard_run();
    const size_t page = mmgr_guard_page_size();

    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run + page), "the tail guard did not trap, so nothing below means "
                                                              "anything");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run - 1), "the head guard did not trap");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_guard_traps_on(run), "the run itself must be readable");
}

/* ---------------------------------------------------------------------------------------------
 * held to the word-rounded cap, which MMGR_SCAN_MAX_WORDS reserves
 * ------------------------------------------------------------------------------------------- */

void test_len_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("len", ask_len, 0);
}

void test_chr_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("chr", ask_chr, 0);
}

void test_eq_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("eq", ask_eq, 0);
    none_past("eq ignoring case", ask_eq_ci, 0);
}

void test_starts_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("starts", ask_starts, 0);
}

void test_diff_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("diff", ask_diff, 0);
}

void test_copy_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("copy", ask_copy, 0);
}

void test_take_be_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("take_be", ask_take_be, 0);
}

/* ---------------------------------------------------------------------------------------------
 * held to the raw cap, with nothing rounded
 *
 * Every needle length up to two words and both foldings, because how far the scan reaches used to
 * depend on where in the needle the anchor landed, and the anchor is picked by rarity.
 * ------------------------------------------------------------------------------------------- */

static void find_none_past(const char *what, void (*fn)(void *))
{
    // The rare byte is walked through the needle so the anchor is picked at every offset in turn.
    for (size_t nlen = 1; nlen <= 2u * MMGR_SWAR_BYTES; nlen++)
    {
        for (size_t rare = 0; rare < nlen; rare++)
        {
            char needle[2u * MMGR_SWAR_BYTES + 1u];
            memset(needle, 'e', nlen);
            needle[rare] = 'q';
            needle[nlen] = '\0';

            for (size_t cap = nlen; cap <= CAPS; cap++)
            {
                for (int ci = 0; ci <= 1; ci++)
                {
                    Hunt h;
                    h.s = (const char *)place(cap, cap);
                    h.cap = cap;
                    h.needle = needle;
                    h.nlen = nlen;
                    h.ci = ci ? MMGR_TRUE : MMGR_FALSE;

                    if (mmgr_guard_run_thunk(fn, &h))
                    {
                        char msg[160];
                        (void)snprintf(msg, sizeof msg,
                                       "%s read past read_cap: cap %zu, needle %zu, rare byte at %zu, ci %d", what, cap,
                                       nlen, rare, ci);
                        TEST_FAIL_MESSAGE(msg);
                    }
                }
            }
        }
    }
}

void test_find_stays_inside_the_raw_cap(void)
{
    needs_our_bounds();
    find_none_past("find", ask_find);
}

void test_has_stays_inside_the_raw_cap(void)
{
    needs_our_bounds();
    find_none_past("has", ask_has);
}

void test_find_still_finds_things_with_the_buffer_flush_to_the_guard(void)
{
    // A bound is easy to hold by refusing to look. These are the same placements with a needle that
    // is actually there, so the scan has to reach the end of the buffer and come back with it.
    needs_our_bounds();

    for (size_t cap = 8u; cap <= CAPS; cap++)
    {
        unsigned char *p = place(cap, cap);
        memset(p, 'e', cap);
        memcpy(p + cap - 3u, "qzj", 3u);

        Hunt h;
        h.s = (const char *)p;
        h.cap = cap;
        h.needle = "qzj";
        h.nlen = 3u;
        h.ci = MMGR_FALSE;

        TEST_ASSERT_FALSE_MESSAGE(mmgr_guard_run_thunk(ask_find, &h), "find read past the cap looking for a match");
        TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + cap - 3u, cellul.find(h.s, cap, "qzj", 3u, MMGR_FALSE),
                                      "find missed a match flush with the end of the buffer");
    }
}
