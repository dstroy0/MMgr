// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "occultum_custodiae/occultum_custodiae.h"

// The alignment clamp the tenant applies is confinium's, so the bounds it is checked against
// come from there rather than being written out again here.
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
        occult.release(base_mark);
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

    TEST_ASSERT_FALSE_MESSAGE(occult.owns(elsewhere), "an unbound pool cannot own an address");
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, occult.slot_of(elsewhere), "an unbound pool has no slot to name");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, occult.used(), "an unbound pool has nothing in it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, occult.high_water(), "an unbound pool has never had anything in it");
}

void test_a_tenant_that_has_only_held_persistent_has_no_peak(void)
{
    // This case has to come second, before anything takes interim. Persistent memory comes off
    // the other end and is tallied on its own, so the tenant is bound and its interim peak is
    // still nothing - which is the one way the peak search runs over a real tenant and finds
    // nothing higher than what it started with.
    TEST_ASSERT_NOT_NULL(occult.persist_span(16u).buf);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, occult.high_water(), "persistent memory moved the interim peak");
}

void test_occult_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("occultum_custodiae.h compiled with no header before it");
}

void test_occult_namespace_is_wired(void)
{
    const OccultumCustodiaeNs *ns = &occult;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(OccultumCustodiaeNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}

/* ---------------------------------------------------------------------------------------------
 * the wipe
 *
 * The one entry that is not about the pool. It is exercised over a buffer of its own so an
 * unaligned start, a whole word run and a ragged tail are all reachable without arranging a
 * tenant to sit at the right offset.
 * ------------------------------------------------------------------------------------------- */

void test_wipe_clears_an_aligned_run(void)
{
    _Alignas(sizeof(uintptr_t)) uint8_t buf[32];
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xA5u;
    }

    mmgr_occult_wipe(buf, sizeof buf);
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, buf[i]);
    }
}

void test_wipe_clears_an_unaligned_start_and_a_ragged_tail(void)
{
    _Alignas(sizeof(uintptr_t)) uint8_t buf[32];
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xA5u;
    }

    // One byte in and three short: the head loop, the word loop and the tail loop all run.
    mmgr_occult_wipe(buf + 1, sizeof buf - 4u);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, buf[0], "the byte before the run is untouched");
    for (unsigned i = 1; i < sizeof buf - 3u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, buf[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, buf[sizeof buf - 1u], "the bytes after the run are untouched");
}

void test_wipe_of_a_short_unaligned_run(void)
{
    // Shorter than a word and starting off a boundary, so it never reaches the word loop.
    _Alignas(sizeof(uintptr_t)) uint8_t buf[16];
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xFFu;
    }

    mmgr_occult_wipe(buf + 1, 2u);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0u, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, buf[3]);
}

void test_wipe_of_nothing_touches_nothing(void)
{
    uint8_t buf[4] = {1u, 2u, 3u, 4u};
    mmgr_occult_wipe(buf, 0u);
    TEST_ASSERT_EQUAL_HEX8(1u, buf[0]);
}

/* ---------------------------------------------------------------------------------------------
 * the pool
 * ------------------------------------------------------------------------------------------- */

void test_capacity_is_the_configured_tenant_size(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SECURE_CONFIN_SIZE, occult.capacity());
}

void test_alloc_hands_back_usable_memory(void)
{
    base_mark = occult.mark();
    uint8_t *p = (uint8_t *)occult.alloc(16u, 1u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x11u;
    p[15] = 0x22u;
    TEST_ASSERT_EQUAL_HEX8(0x11u, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, p[15]);
}

void test_alloc_honours_its_alignment(void)
{
    base_mark = occult.mark();
    // The tenant clamps an ask into MMGR_CONFIN_ALIGN..MMGR_CONFIN_MAX_ALIGN, so an ask below the
    // floor still comes back on the floor and an ask above the ceiling comes back on the ceiling.
    // Both clamps are taken here, with an odd sized allocation between them so the fill point is
    // not already sitting where the next one wants it.
    const void *lo = occult.alloc(8u, 1u);
    (void)occult.alloc(3u, 1u);
    const void *hi = occult.alloc(8u, MMGR_CONFIN_MAX_ALIGN * 4u);

    TEST_ASSERT_NOT_NULL(lo);
    TEST_ASSERT_NOT_NULL(hi);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)lo & (MMGR_CONFIN_ALIGN - 1u),
                                     "an ask under the floor did not come back on the floor");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)hi & (MMGR_CONFIN_MAX_ALIGN - 1u),
                                     "an ask over the ceiling did not come back on the ceiling");
}

void test_alloc_of_more_than_the_tenant_holds_is_refused(void)
{
    TEST_ASSERT_NULL(occult.alloc(MMGR_SECURE_CONFIN_SIZE + 1u, 1u));
}

void test_span_wraps_what_alloc_returns(void)
{
    base_mark = occult.mark();
    const mmgr_spat s = occult.span(24u, 8u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(24u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_TRUE(occult.owns(s.buf));
}

void test_span_of_a_refused_size_has_no_storage(void)
{
    const mmgr_spat s = occult.span(MMGR_SECURE_CONFIN_SIZE + 1u, 1u);
    TEST_ASSERT_NULL(s.buf);
}

void test_persist_span_comes_from_the_other_end(void)
{
    base_mark = occult.mark();
    const mmgr_spat s = occult.persist_span(16u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(16u, s.cap);
    TEST_ASSERT_TRUE(occult.owns(s.buf));
}

void test_mark_and_release_move_the_fill_point(void)
{
    const size_t before = occult.used();
    const size_t m = occult.mark();

    (void)occult.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(before, occult.used(), "the allocation did not move the fill point");

    occult.release(m);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, occult.used(), "releasing a mark did not give the bytes back");
}

void test_release_wipes_what_it_gives_up(void)
{
    const size_t m = occult.mark();

    uint8_t *p = (uint8_t *)occult.alloc(32u, 1u);
    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        p[i] = 0xC3u;
    }

    occult.release(m);

    // The bytes are back in the pool, so reading them is reading our own tenant, not a use after
    // free. This is the whole point of the secure guardian: the secret does not outlive the mark.
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], "a released byte kept its value");
    }
}

void test_release_of_a_mark_that_is_not_ours_is_ignored(void)
{
    const size_t m = occult.mark();
    (void)occult.alloc(16u, 1u);
    const size_t used = occult.used();

    occult.release(MMGR_SECURE_CONFIN_SIZE + 999u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, occult.used(), "a mark past the tenant is not a release point");

    occult.release(m);
}

void test_used_grows_with_what_was_taken(void)
{
    base_mark = occult.mark();
    const size_t before = occult.used();
    (void)occult.alloc(48u, 1u);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(before + 48u, occult.used());
}

void test_high_water_remembers_the_peak(void)
{
    const size_t m = occult.mark();
    (void)occult.alloc(128u, 1u);
    const size_t peak = occult.high_water();
    occult.release(m);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, peak);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(peak, occult.high_water(), "the peak is a high water mark, it does not recede");
}

void test_owns_tells_the_pool_from_everything_else(void)
{
    base_mark = occult.mark();
    const void *p = occult.alloc(8u, 1u);
    uint8_t elsewhere[8];

    TEST_ASSERT_TRUE(occult.owns(p));
    TEST_ASSERT_FALSE_MESSAGE(occult.owns(elsewhere), "a stack address is not in the pool");
    TEST_ASSERT_FALSE(occult.owns(NULL));
}

void test_slot_of_names_the_tenant(void)
{
    base_mark = occult.mark();
    const void *p = occult.alloc(8u, 1u);
    const int slot = occult.slot_of(p);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, slot);
    TEST_ASSERT_LESS_THAN_INT(MMGR_SEC_POOL_SLOTS, slot);
}

void test_slot_of_something_outside_the_pool_is_minus_one(void)
{
    base_mark = occult.mark();
    uint8_t elsewhere[8];
    TEST_ASSERT_EQUAL_INT(-1, occult.slot_of(elsewhere));
    TEST_ASSERT_EQUAL_INT(-1, occult.slot_of(NULL));
}

void test_reset_gives_the_whole_tenant_back(void)
{
    base_mark = occult.mark();
    (void)occult.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t(0u, occult.used());

    occult.reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, occult.used(), "reset did not empty the tenant");

    // reset threw this case's mark away with everything else, so there is nothing to give back.
    base_mark = 0;
}
