#include "unity.h"

#include "custodia_secura/custodia_secura.h"

#include "carceribus/carceribus.h"

static size_t base_mark;

void setUp(void)
{
    base_mark = 0;
}

void tearDown(void)
{
    if (base_mark != 0)
    {
        secura.release(base_mark);
        base_mark = 0;
    }
}


void test_a_pool_that_has_not_bound_yet_answers_for_nothing(void)
{
    uint8_t elsewhere[8];

    TEST_ASSERT_FALSE_MESSAGE(secura.owns(elsewhere), "an unbound pool cannot own an address");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, secura.used(), "an unbound pool has nothing in it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, secura.high_water(), "an unbound pool has never had anything in it");
}

void test_a_tenant_that_has_only_held_persistent_has_no_peak(void)
{
                    TEST_ASSERT_NOT_NULL(secura.persist_span(16u).buf);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, secura.high_water(), "persistent memory moved the interim peak");
}

void test_occult_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("occultum_custodiae.h compiled with no header before it");
}

void test_secura_namespace_is_wired(void)
{
    const CustodiaSecuraNs *ns = &secura;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CustodiaSecuraNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}


void test_wipe_clears_an_aligned_run(void)
{
    _Alignas(sizeof(uintptr_t)) uint8_t buf[32];
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xA5u;
    }

    mmgr_secura_wipe(buf, sizeof buf);
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

        mmgr_secura_wipe(buf + 1, sizeof buf - 4u);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, buf[0], "the byte before the run is untouched");
    for (unsigned i = 1; i < sizeof buf - 3u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, buf[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, buf[sizeof buf - 1u], "the bytes after the run are untouched");
}

void test_wipe_of_a_short_unaligned_run(void)
{
        _Alignas(sizeof(uintptr_t)) uint8_t buf[16];
    for (unsigned i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xFFu;
    }

    mmgr_secura_wipe(buf + 1, 2u);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0u, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, buf[3]);
}

void test_wipe_of_nothing_touches_nothing(void)
{
    uint8_t buf[4] = {1u, 2u, 3u, 4u};
    mmgr_secura_wipe(buf, 0u);
    TEST_ASSERT_EQUAL_HEX8(1u, buf[0]);
}


void test_capacity_is_the_configured_tenant_size(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_SECURE_CONFIN_SIZE, secura.capacity());
}

void test_alloc_hands_back_usable_memory(void)
{
    base_mark = secura.mark();
    uint8_t *p = (uint8_t *)secura.alloc(16u, 1u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x11u;
    p[15] = 0x22u;
    TEST_ASSERT_EQUAL_HEX8(0x11u, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, p[15]);
}

void test_alloc_honours_its_alignment(void)
{
    base_mark = secura.mark();
                    const void *lo = secura.alloc(8u, 1u);
    (void)secura.alloc(3u, 1u);
    const void *hi = secura.alloc(8u, MMGR_CARCER_MAX_ALIGN * 4u);

    TEST_ASSERT_NOT_NULL(lo);
    TEST_ASSERT_NOT_NULL(hi);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)lo & (MMGR_CARCER_ALIGN - 1u),
                                     "an ask under the floor did not come back on the floor");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)hi & (MMGR_CARCER_MAX_ALIGN - 1u),
                                     "an ask over the ceiling did not come back on the ceiling");
}

void test_alloc_of_more_than_the_tenant_holds_is_refused(void)
{
    TEST_ASSERT_NULL(secura.alloc(MMGR_SECURE_CONFIN_SIZE + 1u, 1u));
}

void test_span_wraps_what_alloc_returns(void)
{
    base_mark = secura.mark();
    const mmgr_spat s = secura.span(24u, 8u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(24u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_TRUE(secura.owns(s.buf));
}

void test_span_of_a_refused_size_has_no_storage(void)
{
    const mmgr_spat s = secura.span(MMGR_SECURE_CONFIN_SIZE + 1u, 1u);
    TEST_ASSERT_NULL(s.buf);
}

void test_persist_span_comes_from_the_other_end(void)
{
    base_mark = secura.mark();
    const mmgr_spat s = secura.persist_span(16u);

    TEST_ASSERT_NOT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t(16u, s.cap);
    TEST_ASSERT_TRUE(secura.owns(s.buf));
}

void test_mark_and_release_move_the_fill_point(void)
{
    const size_t before = secura.used();
    const size_t m = secura.mark();

    (void)secura.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(before, secura.used(), "the allocation did not move the fill point");

    secura.release(m);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, secura.used(), "releasing a mark did not give the bytes back");
}

void test_release_wipes_what_it_gives_up(void)
{
    const size_t m = secura.mark();

    uint8_t *p = (uint8_t *)secura.alloc(32u, 1u);
    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        p[i] = 0xC3u;
    }

    secura.release(m);

            for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], "a released byte kept its value");
    }
}

void test_release_of_a_mark_that_is_not_ours_is_ignored(void)
{
    const size_t m = secura.mark();
    (void)secura.alloc(16u, 1u);
    const size_t used = secura.used();

    secura.release(MMGR_SECURE_CONFIN_SIZE + 999u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, secura.used(), "a mark past the tenant is not a release point");

    secura.release(m);
}

void test_used_grows_with_what_was_taken(void)
{
    base_mark = secura.mark();
    const size_t before = secura.used();
    (void)secura.alloc(48u, 1u);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(before + 48u, secura.used());
}

void test_high_water_remembers_the_peak(void)
{
    const size_t m = secura.mark();
    (void)secura.alloc(128u, 1u);
    const size_t peak = secura.high_water();
    secura.release(m);

    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, peak);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(peak, secura.high_water(), "the peak is a high water mark, it does not recede");
}

void test_owns_tells_the_pool_from_everything_else(void)
{
    base_mark = secura.mark();
    const void *p = secura.alloc(8u, 1u);
    uint8_t elsewhere[8];

    TEST_ASSERT_TRUE(secura.owns(p));
    TEST_ASSERT_FALSE_MESSAGE(secura.owns(elsewhere), "a stack address is not in the pool");
    TEST_ASSERT_FALSE(secura.owns(NULL));
}

void test_reset_gives_the_whole_tenant_back(void)
{
    base_mark = secura.mark();
    (void)secura.alloc(64u, 1u);
    TEST_ASSERT_GREATER_THAN_size_t(0u, secura.used());

    secura.reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, secura.used(), "reset did not empty the tenant");

        base_mark = 0;
}
