#include "unity.h"

#include "confinium_externum/confinium_externum.h"

void test_exter_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_externum.h compiled with no header before it");
}
