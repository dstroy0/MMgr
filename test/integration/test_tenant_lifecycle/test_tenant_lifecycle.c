// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// custodia_soluta / occultum_custodiae -> confinium -> spatium
//
// The allocator stack, exercised as a stack. A guardian hands out a tenant, the tenant hands out
// bytes from two ends, a span wraps them, and a mark release has to put everything back without
// disturbing what was taken before the mark.
#include "unity.h"

#include "custodia_soluta/custodia_soluta.h"
#include "carceribus/carceribus.h"
#include "memoria_operor/memoria_operor.h"
#include "custodia_secura/custodia_secura.h"
#include "spatium/spatium.h"

void setUp(void)
{
    soluta.reset();
    secura.reset();
}

void tearDown(void)
{
    soluta.reset();
    secura.reset();
}

void test_a_fresh_tenant_is_empty_and_has_room(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, soluta.used());
    TEST_ASSERT_GREATER_THAN_size_t(0u, soluta.capacity());
}

void test_bytes_come_back_owned_and_aligned(void)
{
    void *p = soluta.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE_MESSAGE(soluta.owns(p), "the pool must recognize what it just handed out");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p & 7u, "alignment was requested and must hold");
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(32u, soluta.used());
}

void test_a_mark_release_puts_back_exactly_what_came_after_it(void)
{
    void *before = soluta.alloc(16u, 8u);
    TEST_ASSERT_NOT_NULL(before);
    const size_t used_before = soluta.used();

    const size_t mark = soluta.mark();
    TEST_ASSERT_NOT_NULL(soluta.alloc(64u, 8u));
    TEST_ASSERT_GREATER_THAN_size_t(used_before, soluta.used());

    soluta.release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used_before, soluta.used(), "release must put back what came after the mark");
    TEST_ASSERT_TRUE_MESSAGE(soluta.owns(before), "and must not disturb what came before it");
}

void test_high_water_records_the_peak_and_never_falls(void)
{
    const size_t mark = soluta.mark();
    TEST_ASSERT_NOT_NULL(soluta.alloc(128u, 8u));
    const size_t peak = soluta.high_water();
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, peak);

    soluta.release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(peak, soluta.high_water(), "high water is a peak, not a level");
}

void test_a_span_over_pool_bytes_writes_inside_the_pool(void)
{
    mmgr_spat s = soluta.span(24u, 8u);
    TEST_ASSERT_TRUE((s.buf != NULL));
    TEST_ASSERT_EQUAL_size_t(0u, (s.pos));
    TEST_ASSERT_EQUAL_size_t(24u, (((s.pos < s.cap) ? (s.cap - s.pos) : 0u)));
}

void test_the_pool_refuses_rather_than_overruns(void)
{
    // one request larger than a whole tenant cannot be satisfied, and must fail rather than
    // wander into the neighboring loculus
    void *p = soluta.alloc(soluta.capacity() + 1u, 8u);
    TEST_ASSERT_NULL_MESSAGE(p, "a request past the tenant must fail, not overrun");
    TEST_ASSERT_EQUAL_size_t(0u, soluta.used());
}

void test_exhausting_a_tenant_fails_cleanly_and_recovers(void)
{
    const size_t cap = soluta.capacity();
    size_t taken = 0;
    while (soluta.alloc(64u, 8u) != NULL)
    {
        taken += 64u;
        if (taken > cap + 4096u)
        {
            TEST_FAIL_MESSAGE("the pool handed out more than its capacity");
        }
    }
    TEST_ASSERT_GREATER_THAN_size_t(0u, taken);
    soluta.reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, soluta.used(), "reset must recover a tenant that ran out");
    TEST_ASSERT_NOT_NULL_MESSAGE(soluta.alloc(64u, 8u), "and it must be usable again");
}

void test_the_two_pools_are_separate_tenants(void)
{
    void *plain = soluta.alloc(32u, 8u);
    void *secure = secura.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(plain);
    TEST_ASSERT_NOT_NULL(secure);
    TEST_ASSERT_NOT_EQUAL(plain, secure);
    TEST_ASSERT_FALSE_MESSAGE(soluta.owns(secure), "the plaintext pool must not claim secure bytes");
    TEST_ASSERT_FALSE_MESSAGE(secura.owns(plain), "and the secure pool must not claim plaintext bytes");
}

void test_secure_release_wipes_what_it_gives_back(void)
{
    const size_t mark = secura.mark();
    unsigned char *p = (unsigned char *)secura.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(p);

    memor.set(p, 0xA5u, 32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    secura.release(mark);

    // the bytes are back in the pool; taking them again must not hand back the old contents
    unsigned char *again = (unsigned char *)secura.alloc(32u, 8u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, again, "the same bytes should come back");
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, again[i], "released secure bytes must be wiped before reuse");
    }
}

void test_persist_survives_a_mark_release(void)
{
    mmgr_spat keep = soluta.persist(16u);
    TEST_ASSERT_TRUE((keep.buf != NULL));

    const size_t mark = soluta.mark();
    TEST_ASSERT_NOT_NULL(soluta.alloc(32u, 8u));
    soluta.release(mark);

    TEST_ASSERT_TRUE_MESSAGE(soluta.owns(keep.buf), "a persist take is not what a mark release reclaims");
}
