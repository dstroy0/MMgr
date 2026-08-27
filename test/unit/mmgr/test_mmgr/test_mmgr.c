/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "mmgr.h"

void test_umbrella_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr.h compiled with no header before it");
}

void test_every_namespace_is_reachable(void)
{
        TEST_ASSERT_NOT_NULL(bitio.put);
    TEST_ASSERT_NOT_NULL(byteio.put);
    TEST_ASSERT_NOT_NULL(cellul.len);
    TEST_ASSERT_NOT_NULL(carcer.persist_capio);
    TEST_ASSERT_NOT_NULL(parva_extremitas.wr);
    TEST_ASSERT_NOT_NULL(magna_extremitas.rd);
    TEST_ASSERT_NOT_NULL(fract.sign);
    TEST_ASSERT_NOT_NULL(memor.cpy);
    TEST_ASSERT_NOT_NULL(numer.build);
    TEST_ASSERT_NOT_NULL(carcer.secura_reddo);
    TEST_ASSERT_NOT_NULL(spat.from);
    TEST_ASSERT_NOT_NULL(proxim.put16);
    TEST_ASSERT_NOT_NULL(verba.put_n);
    TEST_ASSERT_NOT_NULL(lane.has_zero);
}

void test_namespaces_are_their_own_types(void)
{
    TEST_ASSERT_EQUAL_size_t(sizeof(MemoriaOperorNs), sizeof memor);
    TEST_ASSERT_EQUAL_size_t(sizeof(ScrutLaneNs), sizeof lane);
}
