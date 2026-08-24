#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.c"

#include "unity.h"

#define CAP 256u
#define SEGS 8u
#define SEGBYTES (CAP / SEGS)

static uint8_t buf[CAP];
static uint8_t arrived[CAP];
static _Atomic mmgr_word held;
static mmgr_ring ring;
static const int owner = 0;
static const SingularitasCfg bytewise = {&owner, 1u};

void setUp(void)
{
    for (unsigned i = 0; i < CAP; i++)
    {
        arrived[i] = (uint8_t)(i + 1u);
    }
    (void)iteratio_infinita.init(&(RingCfg){&ring, buf, CAP, SEGS, &held});

    struct MmgrCursor *const cur = iteratio_infinita.open(&(InfinCfg){.ring = &ring, .owner = &owner});
    (void)iteratio_infinita.singularitas(&(InfinCfg){.ring = &ring, .src = arrived, .bytes = CAP - 1u, .sing = &bytewise});
}

void tearDown(void)
{
}

static const uint8_t *ask(size_t from, size_t to, size_t *tess)
{
    *tess = 0u;
    return iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .from = from, .to = to, .tessera = tess});
}

static size_t walk(const uint8_t *at, size_t *tess)
{
    size_t n = 0;

    while (at != NULL)
    {
        n++;
        at = iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .tessera = tess});
    }
    return n;
}

void test_keepout_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_exclusivum_infinitas.h compiled with no header before it");
}

void test_a_drain_grants_the_segments_it_claimed(void)
{
    size_t t = 0u;
    const uint8_t *const at = ask(0u, 3u * SEGBYTES, &t);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(buf, at, "the first grant is the first segment of the range");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0u, t, "a grant comes with a tessera");
    TEST_ASSERT_EQUAL_size_t(3u, walk(at, &t));
    TEST_ASSERT_EQUAL_MESSAGE(0u, MMGR_ATOMIC_LOAD(&held), "finishing the frame dropped the reservation");
}

void test_an_unaligned_range_covers_the_segments_it_touches(void)
{
    size_t t = 0u;
    const uint8_t *const at = ask(1u, SEGBYTES + 1u, &t);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(buf, at, "an unaligned start grants from the segment it falls in");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, walk(at, &t), "one byte into the next segment covers two");
}

void test_an_overlapping_claim_is_refused_and_leaves_the_holder_alone(void)
{
    size_t t1 = 0u;
    size_t t2 = 0u;
    const uint8_t *const first = ask(0u, 4u * SEGBYTES, &t1);
    TEST_ASSERT_NOT_NULL(first);

    const mmgr_word after_first = MMGR_ATOMIC_LOAD(&held);
    TEST_ASSERT_NULL(ask(2u * SEGBYTES, 6u * SEGBYTES, &t2));
    TEST_ASSERT_EQUAL_MESSAGE(after_first, MMGR_ATOMIC_LOAD(&held),
                              "a refused claim put back only the bits it set, not the holder's");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, walk(first, &t1), "the holder still walks its whole range");
}

void test_disjoint_claims_coexist(void)
{
    size_t t1 = 0u;
    size_t t2 = 0u;
    const uint8_t *const a = ask(0u, 2u * SEGBYTES, &t1);
    const uint8_t *const b = ask(4u * SEGBYTES, 6u * SEGBYTES, &t2);

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_TRUE_MESSAGE(a != b, "two claims are two different segments");

    TEST_ASSERT_EQUAL_size_t(2u, walk(a, &t1));
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0u, MMGR_ATOMIC_LOAD(&held), "the second is still held");
    TEST_ASSERT_EQUAL_size_t(2u, walk(b, &t2));
    TEST_ASSERT_EQUAL(0u, MMGR_ATOMIC_LOAD(&held));
}

void test_no_more_drains_than_there_are_records(void)
{
    size_t t = 0u;
    size_t granted = 0;

    for (size_t i = 0; i < SEGS; i++)
    {
        if (ask(i * SEGBYTES, (i + 1u) * SEGBYTES, &t) != NULL)
        {
            granted++;
        }
    }
    TEST_ASSERT_EQUAL_size_t((size_t)MMGR_RING_DRAINS, granted);
}

void test_a_range_the_ring_does_not_hold_is_refused(void)
{
    size_t t = 0u;

    TEST_ASSERT_NULL_MESSAGE(ask(0u, CAP + 1u, &t), "a range past the ring is refused");
    TEST_ASSERT_NULL_MESSAGE(ask(CAP - 1u, CAP, &t),
                             "a range past what has arrived is refused: a drain takes what is there");
    TEST_ASSERT_NULL_MESSAGE(ask(8u, 8u, &t), "an empty range is refused");
    TEST_ASSERT_EQUAL_MESSAGE(0u, MMGR_ATOMIC_LOAD(&held), "a refused claim reserved nothing");
}

void test_a_spent_tessera_stops_working(void)
{
    size_t t = 0u;
    const uint8_t *const at = ask(0u, 2u * SEGBYTES, &t);
    const size_t spent = t;

    TEST_ASSERT_EQUAL_size_t(2u, walk(at, &t));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, t, "walking to the end clears the caller's token");

    size_t stale = spent;
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .tessera = &stale}),
                             "a token from a finished drain entitles the holder to nothing");
}

void test_a_reused_record_does_not_honour_the_old_tessera(void)
{
    size_t t1 = 0u;
    const uint8_t *const a = ask(0u, SEGBYTES, &t1);
    const size_t spent = t1;
    TEST_ASSERT_EQUAL_size_t(1u, walk(a, &t1));

        size_t t2 = 0u;
    TEST_ASSERT_NOT_NULL(ask(2u * SEGBYTES, 3u * SEGBYTES, &t2));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(MMGR_TESSERA_IDX(spent), MMGR_TESSERA_IDX(t2),
                                     "the same record was reused, which is the case that matters");

    size_t stale = spent;
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .tessera = &stale}),
                             "the old token names a use of that record that is over");
}

void test_a_tessera_nobody_issued_is_refused(void)
{
    size_t made_up = MMGR_TESSERA(0u, 999u);

    TEST_ASSERT_NULL(iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .tessera = &made_up}));

    size_t zero = 0u;
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.drain(&(InfinCfg){.ring = &ring, .tessera = &zero}),
                             "no token and no range is not a request for anything");
}
