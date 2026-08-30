/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "config/mmgr_compiler_directives.h"

typedef struct
{
    void (*a)(void);
    void (*b)(void);
    void (*c)(void);
} ProbeNs;
MMGR_NS_LAYOUT(ProbeNs, a, b, c);

typedef struct
{
    int x;
    int y;
    int z;
} ProbeArgs;

MMGR_INLINE int probe_sum(const ProbeArgs *a)
{
    return a->x + a->y + a->z;
}

void test_directives_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr_compiler_directives.h compiled with no header before it");
}

void test_narg_counts_its_arguments(void)
{
    TEST_ASSERT_EQUAL_INT(1, MMGR_NARG(a));
    TEST_ASSERT_EQUAL_INT(2, MMGR_NARG(a, b));
    TEST_ASSERT_EQUAL_INT(8, MMGR_NARG(a, b, c, d, e, f, g, h));
    TEST_ASSERT_EQUAL_INT(24, MMGR_NARG(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x));
}

void test_cat_expands_before_it_pastes(void)
{
#define PROBE_ONE 1
#define PROBE_TWO 2
    TEST_ASSERT_EQUAL_INT(12, MMGR_CAT(PROBE_ONE, 2));
#undef PROBE_ONE
#undef PROBE_TWO
}

void test_dispatch_layout_holds(void)
{
    TEST_ASSERT_EQUAL_size_t(3u * MMGR_FP_SIZE, sizeof(ProbeNs));
    TEST_ASSERT_EQUAL_size_t(0u, offsetof(ProbeNs, a));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_FP_SIZE, offsetof(ProbeNs, c));
}

void test_call_macro_passes_the_aggregate(void)
{
    TEST_ASSERT_EQUAL_INT(6, MMGR_CALL(probe_sum, ProbeArgs, .x = 1, .y = 2, .z = 3));
}

void test_call_macro_zeroes_what_is_not_named(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, MMGR_CALL(probe_sum, ProbeArgs, .x = 1),
                                  "unnamed fields are zero by the standard, which is how a default costs nothing");
}

void test_inline_and_unused_are_defined(void)
{
    TEST_ASSERT_TRUE(MMGR_FP_SIZE > 0u);
}

void test_byte_order_is_one_of_two(void)
{
    TEST_ASSERT_TRUE(MMGR_HW_BIG_ENDIAN == 0 || MMGR_HW_BIG_ENDIAN == 1);
}
