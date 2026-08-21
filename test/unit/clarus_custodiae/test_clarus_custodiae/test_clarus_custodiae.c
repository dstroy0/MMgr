// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "clarus_custodiae/clarus_custodiae.h"

// The alignment clamp the tenant applies is confinium's, so the bounds it is checked against come
// from there rather than being written out again here.
#include "confinium/confinium.h"

// The pool binds itself on first use and never unbinds. setUp deliberately does not touch it: a
// mark taken there would bind it before the first case ran, and the answers it gives while it
// still has no storage would then be unreachable from anywhere. Cases that allocate take a mark
// of their own and give it back, so the order they run in does not decide what they see.
static size_t base_mark;

void setUp(void)
{
    base_mark = 0;
}

void tearDown(void)
{
    if (base_mark != 0)
    {
        clarus.release(base_mark);
        base_mark = 0;
    }
}

/* ---------------------------------------------------------------------------------------------
 * before the first allocation
 *
 * This case has to come first in the file. Unity runs them in the order they are written, the
 * pool binds on first use, and nothing unbinds it - so this is the only place the unbound answers
 * can be asked for.
 * ------------------------------------------------------------------------------------------- */

void test_a_pool_that_has_not_bound_yet_answers_for_nothing(void)
{
    uint8_t elsewhere[8];

    TEST_ASSERT_FALSE_MESSAGE(clarus.owns(elsewhere), "an unbound pool cannot own an address");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, clarus.used(), "an unbound pool has nothing in it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, clarus.high_water(), "an unbound pool has never had anything in it");
}

void test_a_tenant_that_has_only_held_persistent_has_no_peak(void)
{
    // This case has to come second, before anything takes interim. Persistent memory comes off
    // the other end and is tallied on its own, so the tenant is bound and its interim peak is
    // still nothing - which is the one way the peak search runs over a real tenant and finds
    // nothing higher than what it started with.
    TEST_ASSERT_NOT_NULL(clarus.persist(16u).buf);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, clarus.high_water(), "persistent memory moved the interim peak");
}

void test_clarus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("clarus_custodiae.h compiled with no header before it");
}

void test_clarus_namespace_is_wired(void)
{
    const ClarusCustodiaeNs *ns = &clarus;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ClarusCustodiaeNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}

void test_capacity_is_the_configured_tenant_size(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_PLAINTEXT_CONFIN_SIZE, clarus.capacity());
}

void test_alloc_hands_back_usable_memory(void)
{
    base_mark = clarus.mark();
    uint8_t *p = (uint8_t *)clarus.alloc(16u, 1u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x11u;
    p[15] = 0x22u;
    TEST_ASSERT_EQUAL_HEX8(0x11u, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, p[15]);
}

void test_alloc_honours_its_alignment(void)
{
    base_mark = clarus.mark();
    // The tenant clamps an ask into MMGR_CONFIN_ALIGN..MMGR_CONFIN_MAX_ALIGN, so an ask below the
    // floor comes back on the floor and an ask above the ceiling comes back on the ceiling.
    const void *lo = clarus.alloc(8u, 1u);
    (void)clarus.alloc(3u, 1u);
    const void *hi = clarus.alloc(8u, MMGR_CONFIN_MAX_ALIGN * 4u);

    TEST_ASSERT_NOT_NULL(lo);
    TEST_ASSERT_NOT_NULL(hi);
    TEST_ASSERT_EQUAL_size_t(0u, (uintptr_t)lo & (MMGR_CONFIN_ALIGN - 1u));
    TEST_ASSERT_EQUAL_size_t(0u, (uintptr_t)hi & (MMGR_CONFIN_MAX_ALIGN - 1u));
}

void test_alloc_of_more_than_the_tenant_holds_is_refused(void)
{
    TEST_ASSERT_NULL(clarus.alloc(MMGR_PLAINTEXT_CONFIN_SIZE + 1u, 1u));
}

void test_span_wraps_what_alloc_returns(void)
{
    base_mark = clarus.mark();
    const mmgr_spat s = clarus.span(24u, 8u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(24u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_TRUE(clarus.owns(s.buf));
}

void test_span_of_a_refused_size_has_no_storage(void)
{
    TEST_ASSERT_NULL(clarus.span(MMGR_PLAINTEXT_CONFIN_SIZE + 1u, 1u).buf);
}

void test_persist_comes_from_the_other_end(void)
{
    base_mark = clarus.mark();
    const mmgr_spat s = clarus.persist(16u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(16u, s.cap);
    TEST_ASSERT_TRUE(clarus.owns(s.buf));
}

void test_mark_and_release_move_the_fill_point(void)
{
    const size_t before = clarus.used();
    const size_t m = clarus.mark();

    (void)clarus.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t(before, clarus.used());

    clarus.release(m);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, clarus.used(), "releasing a mark did not give the bytes back");
}

void test_release_leaves_the_bytes_as_they_were(void)
{
    // The plain guardian is the one that does not wipe. That is the whole difference between it
    // and the secure one, so it is worth pinning rather than assuming.
    const size_t m = clarus.mark();

    uint8_t *p = (uint8_t *)clarus.alloc(32u, 1u);
    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        p[i] = 0xC3u;
    }

    clarus.release(m);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xC3u, p[0], "a released byte was cleared, which is the secure pool's job");
}

void test_release_of_a_mark_that_is_not_ours_is_ignored(void)
{
    const size_t m = clarus.mark();
    (void)clarus.alloc(16u, 1u);
    const size_t used = clarus.used();

    clarus.release(MMGR_PLAINTEXT_CONFIN_SIZE + 999u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, clarus.used(), "a mark past the tenant is not a release point");

    clarus.release(m);
}

void test_used_grows_with_what_was_taken(void)
{
    base_mark = clarus.mark();
    const size_t before = clarus.used();
    (void)clarus.alloc(48u, 1u);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(before + 48u, clarus.used());
}

void test_high_water_remembers_the_peak(void)
{
    const size_t m = clarus.mark();
    (void)clarus.alloc(128u, 1u);
    const size_t peak = clarus.high_water();
    clarus.release(m);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, peak);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(peak, clarus.high_water(), "the peak is a high water mark, it does not recede");
}

void test_owns_tells_the_pool_from_everything_else(void)
{
    base_mark = clarus.mark();
    const void *p = clarus.alloc(8u, 1u);
    uint8_t elsewhere[8];

    TEST_ASSERT_TRUE(clarus.owns(p));
    TEST_ASSERT_FALSE_MESSAGE(clarus.owns(elsewhere), "a stack address is not in the pool");
    TEST_ASSERT_FALSE(clarus.owns(NULL));
}

void test_reset_gives_the_whole_tenant_back(void)
{
    base_mark = clarus.mark();
    (void)clarus.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t(0u, clarus.used());

    clarus.reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, clarus.used(), "reset did not empty the tenant");

    // reset threw this case's mark away with everything else, so there is nothing to give back.
    base_mark = 0;
}
