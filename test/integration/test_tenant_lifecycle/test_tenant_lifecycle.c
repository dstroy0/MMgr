// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// clarus_custodiae / occultum_custodiae -> confinium -> spatium
//
// The allocator stack, exercised as a stack. A guardian hands out a tenant, the tenant hands out
// bytes from two ends, a span wraps them, and a mark release has to put everything back without
// disturbing what was taken before the mark.
#include "unity.h"

#include "clarus_custodiae/clarus_custodiae.h"
#include "confinium/confinium.h"
#include "memoria_operor/memoria_operor.h"
#include "occultum_custodiae/occultum_custodiae.h"
#include "spatium/spatium.h"

void setUp(void)
{
    clarus.reset();
    occult.reset();
}

void tearDown(void)
{
    clarus.reset();
    occult.reset();
}

void test_a_fresh_tenant_is_empty_and_has_room(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, clarus.used());
    TEST_ASSERT_GREATER_THAN_size_t(0u, clarus.capacity());
}

void test_bytes_come_back_owned_and_aligned(void)
{
    void *p = clarus.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE_MESSAGE(clarus.owns(p), "the pool must recognize what it just handed out");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p & 7u, "alignment was requested and must hold");
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(32u, clarus.used());
}

void test_a_mark_release_puts_back_exactly_what_came_after_it(void)
{
    void *before = clarus.alloc(16u, 8u);
    TEST_ASSERT_NOT_NULL(before);
    const size_t used_before = clarus.used();

    const size_t mark = clarus.mark();
    TEST_ASSERT_NOT_NULL(clarus.alloc(64u, 8u));
    TEST_ASSERT_GREATER_THAN_size_t(used_before, clarus.used());

    clarus.release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used_before, clarus.used(), "release must put back what came after the mark");
    TEST_ASSERT_TRUE_MESSAGE(clarus.owns(before), "and must not disturb what came before it");
}

void test_high_water_records_the_peak_and_never_falls(void)
{
    const size_t mark = clarus.mark();
    TEST_ASSERT_NOT_NULL(clarus.alloc(128u, 8u));
    const size_t peak = clarus.high_water();
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(128u, peak);

    clarus.release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(peak, clarus.high_water(), "high water is a peak, not a level");
}

void test_a_span_over_pool_bytes_writes_inside_the_pool(void)
{
    mmgr_spat s = clarus.span(24u, 8u);
    TEST_ASSERT_TRUE(spat.has_storage(s));
    TEST_ASSERT_TRUE(spat.ok(s));
    TEST_ASSERT_EQUAL_size_t(0u, spat.len(s));
    TEST_ASSERT_EQUAL_size_t(24u, spat.room(s));
}

void test_the_pool_refuses_rather_than_overruns(void)
{
    // one request larger than a whole tenant cannot be satisfied, and must fail rather than
    // wander into the neighboring slot
    void *p = clarus.alloc(clarus.capacity() + 1u, 8u);
    TEST_ASSERT_NULL_MESSAGE(p, "a request past the tenant must fail, not overrun");
    TEST_ASSERT_EQUAL_size_t(0u, clarus.used());
}

void test_exhausting_a_tenant_fails_cleanly_and_recovers(void)
{
    const size_t cap = clarus.capacity();
    size_t taken = 0;
    while (clarus.alloc(64u, 8u) != NULL)
    {
        taken += 64u;
        if (taken > cap + 4096u)
        {
            TEST_FAIL_MESSAGE("the pool handed out more than its capacity");
        }
    }
    TEST_ASSERT_GREATER_THAN_size_t(0u, taken);
    clarus.reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, clarus.used(), "reset must recover a tenant that ran out");
    TEST_ASSERT_NOT_NULL_MESSAGE(clarus.alloc(64u, 8u), "and it must be usable again");
}

void test_the_two_pools_are_separate_tenants(void)
{
    void *plain = clarus.alloc(32u, 8u);
    void *secure = occult.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(plain);
    TEST_ASSERT_NOT_NULL(secure);
    TEST_ASSERT_NOT_EQUAL(plain, secure);
    TEST_ASSERT_FALSE_MESSAGE(clarus.owns(secure), "the plaintext pool must not claim secure bytes");
    TEST_ASSERT_FALSE_MESSAGE(occult.owns(plain), "and the secure pool must not claim plaintext bytes");
}

void test_secure_release_wipes_what_it_gives_back(void)
{
    const size_t mark = occult.mark();
    unsigned char *p = (unsigned char *)occult.alloc(32u, 8u);
    TEST_ASSERT_NOT_NULL(p);

    memor.set(p, 0xA5u, 32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    occult.release(mark);

    // the bytes are back in the pool; taking them again must not hand back the old contents
    unsigned char *again = (unsigned char *)occult.alloc(32u, 8u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, again, "the same bytes should come back");
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, again[i], "released secure bytes must be wiped before reuse");
    }
}

void test_persist_survives_a_mark_release(void)
{
    mmgr_spat keep = clarus.persist(16u);
    TEST_ASSERT_TRUE(spat.has_storage(keep));

    const size_t mark = clarus.mark();
    TEST_ASSERT_NOT_NULL(clarus.alloc(32u, 8u));
    clarus.release(mark);

    TEST_ASSERT_TRUE_MESSAGE(clarus.owns(keep.buf), "a persist take is not what a mark release reclaims");
}
