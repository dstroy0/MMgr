// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Singularitas: the one ingestion path, and what it is like to run a stream through it while the
// ring is doing everything else it does.
//
// The cases below are in two halves. The first half pins the contract one property at a time -
// who may hold the path, what a unit means, what a grant is and is not, what a token stops being.
// The second half is the one that matters: a producer claiming, filling and committing against a
// consumer taking bytes out and a drain reserving priority slices out of the middle, round after
// round, checking that every byte that goes in comes out exactly once and in order.
//
// That interleave is the case single stepping cannot reach. A grant is ground the ring has promised
// to someone who has not written it yet, and the failure it exists to stop - a drain being handed a
// run the producer is still filling - only appears when a claim and a drain are live over the same
// ring at the same time. It is checked here by the drain reading what it was given and comparing it
// against the stream: ground that is still being filled does not match, and unmatched is a failure.
//
// The translation unit is compiled in rather than linked, so a case can read the reservation word
// and the path's own record and see what the entries actually did.
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.c"

#include "unity.h"

#define CAP 256u
#define SEGS 8u
#define SEGBYTES (CAP / SEGS)

/** @brief The wide stream's unit: whatever one store is on the machine being modelled. */
#define WORDBYTES MMGR_SING_GRANULE_MAX

static uint8_t buf[CAP];
static _Atomic mmgr_word held;
static mmgr_ring ring;
static const int reader = 0;
static const int auctor = 0;
static const int stranger = 0;

static const SingularitasCfg bytewise = {&auctor, 1u};
static const SingularitasCfg wordwise = {&auctor, WORDBYTES};
static const SingularitasCfg poacher = {&stranger, 1u};

void setUp(void)
{
    for (unsigned i = 0; i < CAP; i++)
    {
        buf[i] = 0u;
    }
    (void)iteratio_infinita.init(&ring, &(RingCfg){buf, CAP, SEGS, &held});
}

void tearDown(void)
{
}

/** @brief What the ring is keeping to itself, for a case that wants to check the entries' work. */
static RingState *state(void)
{
    return ring_of(&ring);
}

/** @brief The byte this stream carries at position @p i. Never zero, so a hole reads as one. */
static uint8_t seq(size_t i)
{
    return (uint8_t)((i * 7u) + 1u);
}

/* ------------------------------------------------------------------ the path is one, and owned */

void test_stream_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_exclusivum_infinitas.h compiled with no header before it");
}

void test_the_path_opens_to_one_auctor_and_stays_there(void)
{
    static const uint8_t src[4] = {1u, 2u, 3u, 4u};
    mmgr_u16 st = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 4u, .sing = &bytewise}));

    TEST_ASSERT_NULL_MESSAGE(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 4u, .sing = &poacher, .status = &st}),
        "a second stream does not get the path by asking for it");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_ALIEN, "and is told why");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "the refusal put nothing in the ring");
}

void test_a_unit_that_is_not_one_store_is_refused(void)
{
    static const uint8_t src[8] = {0};
    static const SingularitasCfg odd = {&auctor, 3u};
    static const SingularitasCfg none = {&auctor, 0u};
    static const SingularitasCfg huge = {&auctor, MMGR_SING_GRANULE_MAX * 2u};

    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 1u, .sing = &odd}));
    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 1u, .sing = &none}));
    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 1u, .sing = &huge}));
    TEST_ASSERT_EQUAL_MESSAGE(0, state()->sing.open, "none of them opened the path");
}

void test_a_status_can_be_read_without_asking_for_anything(void)
{
    mmgr_u16 st = 0u;

    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .status = &st}));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_READY, "an unheld path is ready");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_REFUSED, "reading a status is not an ask");
    TEST_ASSERT_EQUAL_MESSAGE(0, state()->sing.open, "and it opened nothing");
}

void test_a_cfg_and_no_ask_attaches_the_path(void)
{
    mmgr_u16 st = 0u;

    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .sing = &wordwise, .status = &st}));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_ATTACHED, "presenting a cfg is the attach");
    TEST_ASSERT_FALSE(MMGR_SING_FLAGS(st) & MMGR_SING_READY);
    TEST_ASSERT_FALSE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_REFUSED, "nothing was asked, so nothing was refused");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(WORDBYTES, state()->sing.gran, "and the unit came with it");
}

/* --------------------------------------------------------------------------- units, not bytes */

void test_every_count_on_the_path_is_in_units(void)
{
    size_t t = 0u;
    size_t got = 0u;

    uint8_t *const at = iteratio_infinita.singularitas(
        &(InfinCfg){.r = &ring, .n = 3u, .tessera = &t, .sing = &wordwise, .units = &got});

    TEST_ASSERT_EQUAL_PTR(buf, at);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, got, "three units asked for is three units given");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u * WORDBYTES, state()->sing.span, "three units is three units of bytes");

    for (size_t i = 0; i < (3u * WORDBYTES); i++)
    {
        at[i] = seq(i);
    }
    TEST_ASSERT_NULL(iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 3u, .tessera = &t}));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u * WORDBYTES, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "committing three units published three units of bytes");
}

void test_the_head_only_ever_moves_by_whole_units(void)
{
    size_t t = 0u;

    /* Round after round, whatever is asked and whatever is committed, the head stays on the unit.
       That is what keeps a granted address one a channel can be handed without a fixup. */
    for (unsigned round = 0; round < 40u; round++)
    {
        size_t got = 0u;
        uint8_t *const at = iteratio_infinita.singularitas(
            &(InfinCfg){.r = &ring, .n = 0u, .tessera = &t, .sing = &wordwise, .units = &got});
        if (at == NULL)
        {
            break;
        }
        TEST_ASSERT_EQUAL_PTR_MESSAGE(&buf[MMGR_ATOMIC_LOAD(&state()->head)], at, "a grant starts where the path is");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)(at - buf) % WORDBYTES, "and is aligned to the unit");

        /* Commit fewer units than were granted, which is the short transfer that would otherwise
           knock every later grant off the unit. */
        const size_t part = (got > 1u) ? (got - 1u) : got;
        (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = part, .tessera = &t});
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_ATOMIC_LOAD(&state()->head) % WORDBYTES,
                                         "a short commit leaves the head on the unit");
        iteratio_infinita.consume(&(InfinCfg){.r = &ring, .n = part * WORDBYTES});
    }
}

/* ------------------------------------------------------------ a grant is not a publication */

void test_a_grant_publishes_nothing_until_it_is_committed(void)
{
    size_t t = 0u;
    size_t got = 0u;
    uint8_t *const at = iteratio_infinita.singularitas(
        &(InfinCfg){.r = &ring, .n = 8u, .tessera = &t, .sing = &bytewise, .units = &got});

    TEST_ASSERT_NOT_NULL(at);
    for (unsigned i = 0; i < 8u; i++)
    {
        at[i] = seq(i);
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "the bytes are written and the reader cannot see one of them");
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.read(&(InfinCfg){.r = &ring, .n = 1u}), "nor name one");

    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 8u, .tessera = &t});
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "the commit is the publication");
}

void test_a_grant_never_wraps(void)
{
    static const uint8_t src[16] = {0};
    size_t t = 0u;
    size_t got = 0u;

    /* Walk the head to eight bytes short of the end. */
    for (unsigned i = 0; i < 15u; i++)
    {
        (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 16u, .sing = &bytewise});
        iteratio_infinita.consume(&(InfinCfg){.r = &ring, .n = 16u});
    }
    TEST_ASSERT_EQUAL_size_t(CAP - 16u, MMGR_ATOMIC_LOAD(&state()->head));

    TEST_ASSERT_NULL_MESSAGE(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 32u, .tessera = &t, .sing = &bytewise}),
        "a run that would cross the end is not a run a channel can be given");

    uint8_t *const at = iteratio_infinita.singularitas(
        &(InfinCfg){.r = &ring, .n = 0u, .tessera = &t, .sing = &bytewise, .units = &got});
    TEST_ASSERT_NOT_NULL_MESSAGE(at, "asking for whatever fits is the ask that gets an answer here");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, got, "and what fits is what is left before the end");
    TEST_ASSERT_EQUAL_PTR(&buf[CAP - 16u], at);
}

void test_asking_for_whatever_fits_needs_somewhere_to_be_told(void)
{
    size_t t = 0u;
    mmgr_u16 st = 0u;

    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.singularitas(
                                 &(InfinCfg){.r = &ring, .n = 0u, .tessera = &t, .sing = &bytewise, .status = &st}),
                             "a length the caller did not name has to be handed back somewhere");
    TEST_ASSERT_TRUE(MMGR_SING_FLAGS(st) & MMGR_SING_REFUSED);
}

void test_one_grant_at_a_time(void)
{
    size_t t1 = 0u;
    size_t t2 = 0u;
    mmgr_u16 st = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 8u, .tessera = &t1, .sing = &bytewise}));
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.singularitas(
                                 &(InfinCfg){.r = &ring, .n = 8u, .tessera = &t2, .sing = &bytewise, .status = &st}),
                             "the head cannot move under two grants");
    TEST_ASSERT_TRUE(MMGR_SING_FLAGS(st) & MMGR_SING_GRANTED);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, t2, "and no second token was issued");
}

/* ------------------------------------------------------------------------------- the tesserae */

void test_a_spent_tessera_publishes_nothing(void)
{
    size_t t = 0u;
    mmgr_u16 st = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 8u, .tessera = &t, .sing = &bytewise}));
    const size_t spent = t;
    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 8u, .tessera = &t});
    TEST_ASSERT_EQUAL_size_t(8u, iteratio_infinita.available(&(InfinCfg){.r = &ring}));

    /* The completion that arrives after the channel was torn down. */
    size_t late = spent;
    TEST_ASSERT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 8u, .tessera = &late, .status = &st}));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_STALE, "the ring knows what it is");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "and it published nothing");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, late, "the token is cleared, so it cannot be tried again");
}

void test_a_drain_tessera_will_not_publish_an_ingest(void)
{
    static const uint8_t src[64] = {0};
    size_t dt = 0u;
    mmgr_u16 st = 0u;

    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 64u, .sing = &bytewise});
    TEST_ASSERT_NOT_NULL(iteratio_infinita.drain(&(InfinCfg){.r = &ring, .from = 0u, .to = SEGBYTES, .tessera = &dt}));

    const size_t before = iteratio_infinita.available(&(InfinCfg){.r = &ring});
    size_t borrowed = dt;
    TEST_ASSERT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 8u, .tessera = &borrowed, .status = &st}));
    TEST_ASSERT_TRUE(MMGR_SING_FLAGS(st) & MMGR_SING_STALE);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, iteratio_infinita.available(&(InfinCfg){.r = &ring}),
                                     "a drain's token moved the head by nothing");
}

void test_an_ingest_tessera_will_not_open_a_drain(void)
{
    size_t t = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 8u, .tessera = &t, .sing = &bytewise}));

    size_t borrowed = t;
    TEST_ASSERT_NULL_MESSAGE(iteratio_infinita.drain(&(InfinCfg){.r = &ring, .tessera = &borrowed}),
                             "the ingestion path is not a drain and its token does not name one");
}

/* ------------------------------------------------------------------- the two halves, together */

void test_a_grant_and_a_drain_are_denied_by_the_same_word(void)
{
    static const uint8_t src[128] = {0};
    size_t dt = 0u;
    size_t t = 0u;
    mmgr_u16 st = 0u;

    /* Four segments of arrived bytes, and a drain holding the first two of them. */
    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 4u * SEGBYTES, .sing = &bytewise});
    iteratio_infinita.consume(&(InfinCfg){.r = &ring, .n = 4u * SEGBYTES});
    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 4u * SEGBYTES, .sing = &bytewise});

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.drain(&(InfinCfg){.r = &ring, .from = 4u * SEGBYTES, .to = 6u * SEGBYTES, .tessera = &dt}));
    const mmgr_word after_drain = MMGR_ATOMIC_LOAD(&held);

    /* The head is now at segment 0 again, having wrapped. Consume enough that a claim would like to
       run straight through the ground the drain is holding. */
    iteratio_infinita.consume(&(InfinCfg){.r = &ring, .n = 4u * SEGBYTES});

    size_t got = 0u;
    uint8_t *const at = iteratio_infinita.singularitas(
        &(InfinCfg){.r = &ring, .n = 0u, .tessera = &t, .sing = &bytewise, .units = &got, .status = &st});

    TEST_ASSERT_NOT_NULL(at);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u * SEGBYTES, got,
                                     "the claim stopped at the first segment the drain was holding");
    TEST_ASSERT_EQUAL_MESSAGE(after_drain | seg_mask(0u, 4u), MMGR_ATOMIC_LOAD(&held),
                              "and took only the segments in front of it, leaving the drain's alone");
}

void test_a_drain_cannot_have_ground_a_grant_is_holding(void)
{
    static const uint8_t src[64] = {0};
    size_t t = 0u;
    size_t dt = 0u;

    /* Two segments arrived, then a grant over the two in front of them. */
    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 2u * SEGBYTES, .sing = &bytewise});
    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 2u * SEGBYTES, .tessera = &t, .sing = &bytewise}));

    /* A drain over the arrived bytes is fine - the grant is not there. */
    TEST_ASSERT_NOT_NULL_MESSAGE(
        iteratio_infinita.drain(&(InfinCfg){.r = &ring, .from = 0u, .to = 2u * SEGBYTES, .tessera = &dt}),
        "what has arrived is still drainable while a grant is out in front of it");

    /* One that reaches into the grant is not, and the mask is the whole of the reason. */
    size_t dt2 = 0u;
    TEST_ASSERT_NULL_MESSAGE(
        iteratio_infinita.drain(&(InfinCfg){.r = &ring, .from = 0u, .to = 3u * SEGBYTES, .tessera = &dt2}),
        "ground the producer has been promised is not ground a drain may be given");
}

/* ------------------------------------------------------------------------------ switching over */

void test_only_the_auctor_may_detach(void)
{
    static const uint8_t src[4] = {0};
    mmgr_u16 st = 0u;

    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .src = src, .n = 4u, .sing = &bytewise});

    TEST_ASSERT_FALSE_MESSAGE(iteratio_infinita.detach(&(InfinCfg){.r = &ring, .sing = &poacher, .status = &st}),
                              "a stream that does not hold the path cannot put it down");
    TEST_ASSERT_TRUE(MMGR_SING_FLAGS(st) & MMGR_SING_ALIEN);
    TEST_ASSERT_TRUE_MESSAGE(iteratio_infinita.detach(&(InfinCfg){.r = &ring, .sing = &bytewise, .status = &st}),
                             "the auctor may");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_READY, "and the ring says so");
}

void test_a_grant_still_out_refuses_the_detach(void)
{
    size_t t = 0u;
    mmgr_u16 st = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 8u, .tessera = &t, .sing = &bytewise}));

    TEST_ASSERT_FALSE_MESSAGE(iteratio_infinita.detach(&(InfinCfg){.r = &ring, .sing = &bytewise, .status = &st}),
                              "the ground is promised, and saying you are finished is not the hardware being finished");
    TEST_ASSERT_TRUE(MMGR_SING_FLAGS(st) & MMGR_SING_GRANTED);

    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 0u, .tessera = &t});
    TEST_ASSERT_TRUE_MESSAGE(iteratio_infinita.detach(&(InfinCfg){.r = &ring, .sing = &bytewise}),
                             "committing nothing gives the ground back, and then it may be let go");
}

void test_a_new_stream_takes_the_path_and_the_old_tokens_stop(void)
{
    size_t t = 0u;
    mmgr_u16 st = 0u;

    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 8u, .tessera = &t, .sing = &bytewise}));
    const size_t orphan = t;
    (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 0u, .tessera = &t});
    TEST_ASSERT_TRUE(iteratio_infinita.detach(&(InfinCfg){.r = &ring, .sing = &bytewise}));

    /* The path is ready, and a different stream in a different unit takes it. */
    TEST_ASSERT_NOT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .n = 2u, .tessera = &t, .sing = &wordwise}));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u * WORDBYTES, state()->sing.span, "two units of the new stream, not the old");

    size_t late = orphan;
    TEST_ASSERT_NULL(
        iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = 2u, .tessera = &late, .status = &st}));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SING_FLAGS(st) & MMGR_SING_STALE,
                             "a token from the stream that let go does not land in the one that replaced it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, iteratio_infinita.available(&(InfinCfg){.r = &ring}), "and published nothing");
}

/* ------------------------------------------------------------------------------- the stream */

/**
 * @brief Run a stream in through the granted path while the ring is drained from both ends.
 *
 * The producer claims, fills and commits; the consumer takes bytes out one at a time and checks
 * each against the stream; a drain reserves a priority slice out of the arrived middle every few
 * rounds and reads it, checking the same thing. Every byte that goes in has to come out exactly
 * once and in order, and nothing a drain is handed may be ground the producer has not finished.
 *
 * @param cfg    The unit to run the stream in.
 * @param total  How many bytes to push through.
 * @param drains Whether to work a keepout drain into the interleave.
 */
static void stream(const SingularitasCfg *cfg, size_t total, int drains)
{
    const size_t gran = cfg->gran;
    size_t made = 0u;  /* bytes committed */
    size_t taken = 0u; /* bytes read back out */
    size_t t = 0u;
    size_t round = 0u;

    while (taken < total)
    {
        /* The producer. Ask for a fixed run, and take whatever fits when the end of the buffer is
           closer than that - which is the ask a driver falls back to and the only one that gets an
           answer at the wrap. */
        if (made < total)
        {
            size_t got = 0u;
            uint8_t *at = iteratio_infinita.singularitas(
                &(InfinCfg){.r = &ring, .n = 6u, .tessera = &t, .sing = cfg, .units = &got});
            if (at == NULL)
            {
                at = iteratio_infinita.singularitas(
                    &(InfinCfg){.r = &ring, .n = 0u, .tessera = &t, .sing = cfg, .units = &got});
            }
            if (at != NULL)
            {
                size_t give = got * gran;
                if (give > (total - made))
                {
                    give = ((total - made) / gran) * gran;
                }
                for (size_t i = 0; i < give; i++)
                {
                    at[i] = seq(made + i);
                }
                TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)(at - buf) % gran, "a grant is aligned to the unit");
                (void)iteratio_infinita.singularitas(&(InfinCfg){.r = &ring, .off = give / gran, .tessera = &t});
                TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_ATOMIC_LOAD(&state()->head) % gran,
                                                 "and the head stays on it");
                made += give;
            }
        }

        /* The drain. A priority slice out of what has arrived and not yet been read out. Whatever it
           is handed has to be the stream, because the alternative is ground still being filled. */
        if (drains && ((round % 3u) == 0u))
        {
            const size_t live = iteratio_infinita.available(&(InfinCfg){.r = &ring});
            if (live >= SEGBYTES)
            {
                const size_t from = MMGR_ATOMIC_LOAD(&state()->tail);
                size_t dt = 0u;
                const uint8_t *at = iteratio_infinita.drain(
                    &(InfinCfg){.r = &ring, .from = from, .to = from + SEGBYTES, .tessera = &dt});
                size_t pos = taken;
                while (at != NULL)
                {
                    const size_t seg = (size_t)(at - buf) / SEGBYTES;
                    for (size_t i = 0; i < SEGBYTES; i++)
                    {
                        const size_t where = (seg * SEGBYTES) + i;
                        if ((where >= from) && ((where - from) < SEGBYTES) && (pos + (where - from)) < made)
                        {
                            TEST_ASSERT_EQUAL_HEX8_MESSAGE(seq(pos + (where - from)), at[i],
                                                           "a drain was handed ground the producer had not finished");
                        }
                    }
                    at = iteratio_infinita.drain(&(InfinCfg){.r = &ring, .tessera = &dt});
                }
                TEST_ASSERT_EQUAL_MESSAGE(0u,
                                          MMGR_ATOMIC_LOAD(&held) & seg_mask(from / SEGBYTES, (from / SEGBYTES) + 1u),
                                          "the drain gave its reservation back when it ran out of frame");
            }
        }

        /* The consumer. */
        for (size_t k = 0; (k < 11u) && (taken < made); k++)
        {
            uint8_t got = 0u;
            TEST_ASSERT_TRUE_MESSAGE(iteratio_infinita.read_byte(&(InfinCfg){.r = &ring, .dst = &got}),
                                     "available said there was a byte");
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(seq(taken), got, "the stream came out of order, short, or twice");
            taken++;
        }

        round++;
        TEST_ASSERT_TRUE_MESSAGE(round < (total * 4u), "the interleave stopped making progress");
    }

    TEST_ASSERT_EQUAL_size_t_MESSAGE(total, taken, "everything that went in came out");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, iteratio_infinita.available(&(InfinCfg){.r = &ring}), "and nothing was left");
    TEST_ASSERT_EQUAL_MESSAGE(0u, MMGR_ATOMIC_LOAD(&held), "no reservation outlived the run");
}

void test_a_bytewise_stream_survives_the_wrap(void)
{
    stream(&bytewise, CAP * 9u, 0);
}

void test_a_wordwise_stream_survives_the_wrap(void)
{
    stream(&wordwise, CAP * 9u, 0);
}

void test_a_stream_runs_while_drains_are_worked(void)
{
    stream(&bytewise, CAP * 9u, 1);
}

void test_a_wordwise_stream_runs_while_drains_are_worked(void)
{
    stream(&wordwise, CAP * 9u, 1);
}
