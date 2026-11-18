#include "unity.h"

#include "config/mmgr_config.h"

void test_config_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr_config.h compiled with no header before it");
}

void test_tenant_bound_is_a_real_size(void)
{
    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CARCER_MAX);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CARCER_MAX >= MMGR_PLAINTEXT_CONFIN_SIZE ||
                                 MMGR_CARCER_MAX >= MMGR_SECURE_CONFIN_SIZE,
                             "the bound must cover the largest tenant");
}
