#include "mmgr.h"

// mmgr.h carries the embed headers and nothing else, so each namespace below comes from the module
// that declares it. The header stays first, which is what test_umbrella_header_is_self_contained
// still checks
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "endian/endian.h"
#include "fractio/fractio.h"
#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "proximus_operor/proximus_operor.h"
#include "spatium/spatium.h"
#include "verba_scribo/verba_scribo.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include "unity.h"

ParsMemoriaeInternae(minimum, 64);
ParsMemoriaeInternae(maximum, 64);

LocusCarcerum(umbrella, MMGR_MINIMUM_SECURITY(minimum), MMGR_MAXIMUM_SECURITY(maximum));

void test_umbrella_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr.h compiled with no header before it");
}

void test_every_namespace_is_reachable(void)
{
    TEST_ASSERT_NOT_NULL(bitio.put);
    TEST_ASSERT_NOT_NULL(byteio.put);
    TEST_ASSERT_NOT_NULL(cellul.len);
    TEST_ASSERT_NOT_NULL(umbrella.minimum.persistent_buf_alloc);
    TEST_ASSERT_NOT_NULL(parva_extremitas.wr);
    TEST_ASSERT_NOT_NULL(magna_extremitas.rd);
    TEST_ASSERT_NOT_NULL(fract.sign);
    TEST_ASSERT_NOT_NULL(memor.cpy);
    TEST_ASSERT_NOT_NULL(numer.build);
    TEST_ASSERT_NOT_NULL(umbrella.maximum.persistent_buf_release);
    TEST_ASSERT_NOT_NULL(spat.from);
    TEST_ASSERT_NOT_NULL(proxim.put16);
    TEST_ASSERT_NOT_NULL(verba_textus.put_n);
    TEST_ASSERT_NOT_NULL(verba_littera.ch);
    TEST_ASSERT_NOT_NULL(verba_numerus.uint);
    TEST_ASSERT_NOT_NULL(verba_fractio.g);
    TEST_ASSERT_NOT_NULL(verba_finis.finish);
    TEST_ASSERT_NOT_NULL(lane.has_zero);
}

void test_namespaces_are_their_own_types(void)
{
    TEST_ASSERT_EQUAL_size_t(sizeof(MemoriaOperorNs), sizeof memor);
    TEST_ASSERT_EQUAL_size_t(sizeof(ScrutLaneNs), sizeof lane);
    TEST_ASSERT_EQUAL_size_t(sizeof(MinimumSecurityGuard), sizeof umbrella.minimum);
    TEST_ASSERT_EQUAL_size_t(sizeof(MaximumSecurityGuard), sizeof umbrella.maximum);
}
