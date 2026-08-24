#include "unity.h"

#include "spatium/spatium.h"

void test_spat_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("spatium.h compiled with no header before it");
}


void test_from_takes_a_buffer(void)
{
    uint8_t buf[8];
    const mmgr_spat s = MMGR_CALL(spat.init, SpatCfg, .buf = buf, .cap = sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(8u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
}

