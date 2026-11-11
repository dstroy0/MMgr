// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_praet_correctness.c
 * @brief Runs every scripted DMA scenario through the praet entries, on both engine arms, and
 *        checks what came back out against the outcome the generator computed.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note The engine under praet_engine.c is a real port. It defines the four
 *       mmgr_praet_hw_ hooks strongly, and so what these cases exercise is the contract a board support
 *       file implements.
 * @note The counts and the order that decide a case are taken at this file's own completion
 *       callback. The engine's tally says what it emitted; these say what reached the application
 *       through the library, and a dropped completion is exactly that gap.
 * @note The bytes a transfer names belong to this case. The library owns no storage and the engine
 *       owns none either, and so the buffer is declared here and its extent travels with the pointer.
 */
#include "unity.h"

#include "praet_scenarios.h"

// The engine compiles in instead of a link. mmgr_add_suite builds the one suite source and the
// shared files under test/support. These hooks must not reach any other suite:
// test_memoriam_praetereo exists to prove the weak defaults refuse
#include "praet_engine.c"

// This file sets no knob. The schedule compiles into the library, and a context declared here has
// to be the size the library walks. So every knob arrives on the build line through
// MMGR_PRAET_KNOBS and reaches both. The harness builds two arms: build-dma with recovery on and a
// settle window, build-dma-lean with both off. The cases gate on the knobs and hold on either

// The examination arm's counters. Every entry compiles out where PRAET_PROCURATOR is 0. A build that
// did not ask for it carries none of this
#include "praet_procurator.c"

/**
 * @brief The schedule context every schedule case runs against.
 *
 * @note File scope and never copied. The context's address is its identity, and every write into it
 *       is an offset from that address.
 * @note The token is the declaration that answers the boundary word question, and it reports whichever
 *       way it goes. Every build of this suite carries that line, as the token is for.
 * @note build-dma-lean sets PRAET_SUITE_CRC_CHOICE to the DISABLE token, because a context answers
 *       this once and the other arm needs its own compile.
 */
#ifndef PRAET_SUITE_CRC_CHOICE
#define PRAET_SUITE_CRC_CHOICE AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE
#endif

PraetOrdoContext(s_schedule, PRAET_SUITE_CRC_CHOICE);

/**
 * @brief Region descriptor the schedule cases attach with.
 *
 * @note Any byte will do. What the cases check is that it survives every state change, since nothing
 *       but an attach has business with it.
 */
#define PRAET_CASE_REGION PRAET_REGION_EXTERNAL

/**
 * @brief Completions one case may receive before the recorder loses their order.
 *
 * @note A case that overruns this records as a mismatch on the count, and so nothing goes silently
 *       lost.
 */
#define PRAET_ORDER_MAX 32u

/**
 * @brief Storage a case submits from, aligned the way every pool declaration aligns its bytes.
 *
 * @note The caller's, not the library's. Nothing under src sizes this and no knob describes it.
 */
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t s_case_storage[256];

/**
 * @brief Completion callbacks this case received.
 */
static uint16_t s_received_completions;

/**
 * @brief Bytes those completions reported, summed.
 */
static uint32_t s_received_moved;

/**
 * @brief Channels those completions arrived on, in the order they arrived.
 */
static uint8_t s_received_order[PRAET_ORDER_MAX];

/**
 * @brief Records one completion as the application would see it.
 *
 * @param[in] event Completion the port layer reports [BORROWS].
 * @param[in] user  Pointer registered alongside this callback [BORROWS].
 * @note This is the observation point that matters. Everything else in the run is the fixture.
 */
static void praet_record_completion(const mmgr_praet_event *event, void *user)
{
    (void)user;

    if (s_received_completions < PRAET_ORDER_MAX)
    {
        s_received_order[s_received_completions] = event->channel;
    }
    s_received_completions++;
    s_received_moved += event->bytes;
}

/**
 * @brief Callback and user pointer every case registers when it opens a channel.
 *
 * @note File scope, and so it outlives every call a case makes, as mmgr_praet_open requires of
 *       the pointer it forwards [BORROWS].
 */
static const PraetCallbackCfg s_completion_binding = {
    .callback = praet_record_completion,
    .user = NULL,
};

/**
 * @brief Records one completion and tells the schedule about it.
 *
 * @param[in] event Completion the port layer reports [BORROWS].
 * @param[in] user  Pointer registered alongside this callback [BORROWS].
 * @note The join. A port reports a finished transfer through this callback, and the schedule learns a
 *       channel is no longer busy from the same event. Nothing else can tell it: praet_ordo.c
 *       predicts no duration. A completion arrives here or it does not arrive.
 * @note This is the application's half. The library holds the schedule and the port layer, and joins
 *       neither to the other. So what wires the port's events to the schedule is the code that
 *       registered the callback, as a real integration writes.
 * @note An event carries no failure flag, and so every completion the engine reports is a clean one. A
 *       port that distinguishes them has somewhere to say so and this does not invent one.
 */
static void praet_joined_completion(const mmgr_praet_event *event, void *user)
{
    praet_record_completion(event, user);
    praet_ordo_completed(&s_schedule, (embed_word)event->channel, EMBED_FALSE);
}

/**
 * @brief Callback and user pointer the joined cases register.
 */
static const PraetCallbackCfg s_joined_binding = {
    .callback = praet_joined_completion,
    .user = NULL,
};

/**
 * @brief Prepares the fixture Unity runs before each case in this suite.
 */
void setUp(void)
{
    s_received_completions = 0u;
    s_received_moved = 0u;
}

/**
 * @brief Releases the fixture Unity runs after each case in this suite.
 *
 * @note Empty because each scenario arms the engine itself and leaves nothing a later case reads.
 */
void tearDown(void)
{
}

/**
 * @brief Makes one call from a scenario's program.
 *
 * @param[in]  made    Call to make [BORROWS].
 * @param[out] opens   Raised where an open came back EMBED_TRUE [BORROWS].
 * @param[out] submits Raised where a submit came back EMBED_TRUE [BORROWS].
 * @note The program names the entry, the channel and the byte count, and so nothing about a case changes
 *       per scenario.
 */
static void praet_make_call(const PraetProgramStep *made, uint16_t *opens, uint16_t *submits)
{
    if (made->call == (uint8_t)PRAET_HOOK_OPEN)
    {
        if (EMBED_CALL(praet.open, PraetCfg, .channel = made->channel, .peripheral = 0u, .loopback = EMBED_FALSE,
                       .on_complete = &s_completion_binding))
        {
            (*opens)++;
        }
        return;
    }

    if (made->call == (uint8_t)PRAET_HOOK_SUBMIT)
    {
        if (EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = made->channel, .buf = s_case_storage,
                       .bytes = made->bytes))
        {
            (*submits)++;
        }
        return;
    }

    if (made->call == (uint8_t)PRAET_HOOK_CLOSE)
    {
        EMBED_CALL(praet.close, PraetTransferCfg, .channel = made->channel);
        return;
    }

    EMBED_CALL(praet.poll, PraetCfg, .channel = made->channel, .on_complete = &s_completion_binding);
}

/**
 * @brief Runs one scenario and asserts every count and the completion order.
 *
 * @param[in] scenario Scenario to run [BORROWS].
 * @note Every assertion carries the scenario's name. A ctest failure says which row broke and on
 *       which arm without anyone reading the table.
 */
static void praet_check_one(const PraetScenario *scenario)
{
    s_received_completions = 0u;
    s_received_moved = 0u;
    praet_engine_arm(scenario);

    uint16_t opened = 0u;
    uint16_t submitted = 0u;

    for (uint16_t index = 0u; index < scenario->program_count; index++)
    {
        praet_make_call(&scenario->program[index], &opened, &submitted);
    }

    const PraetEngineTally tally = praet_engine_tally();

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, tally.overrun, scenario->name);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(scenario->expect_opens, opened, scenario->name);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(scenario->expect_submits, submitted, scenario->name);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(scenario->expect_settling, tally.settling, scenario->name);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(scenario->expect_deferred, tally.deferred, scenario->name);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(scenario->expect_completions, s_received_completions, scenario->name);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(scenario->expect_moved, s_received_moved, scenario->name);

    // What the engine sent against what the application received. A library that dropped a completion
    // or invented one differs here and nowhere else
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(tally.completions, s_received_completions, scenario->name);

    TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(PRAET_ORDER_MAX, s_received_completions, scenario->name);
    for (uint16_t index = 0u; index < scenario->expect_order_count; index++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(scenario->expect_order[index], s_received_order[index], scenario->name);
    }
}

/**
 * @brief Checks that the control scenario comes out clean.
 *
 * @note The first row, and the one that has to pass before any other means anything. A harness that
 *       cannot produce a clean column has not shown it can tell the columns apart.
 */
void test_the_control_is_clean(void)
{
    praet_check_one(&praet_scenarios[0]);
}

/**
 * @brief Checks every scenario, on both arms, against the outcome the generator computed.
 *
 * @note The expectations come from gen_praet_scenarios.py, which walks each program against its
 *       script. Nothing here derives them, and so agreement is evidence.
 */
void test_every_scenario_matches_its_oracle(void)
{
    const size_t count = sizeof praet_scenarios / sizeof praet_scenarios[0];

    for (size_t index = 0u; index < count; index++)
    {
        praet_check_one(&praet_scenarios[index]);
    }
}

/**
 * @brief The pool the schedule cases attach a channel over.
 *
 * @note A pool declared the way any caller declares one. PraetAttach takes one, and the name
 *       lets the address and the extent come from the same declaration.
 * @note Declared on both arms. A build with no recovery records nothing about a transfer, and the
 *       attach still names a pool and the compiler still proves it exists.
 */
ParsMemoriaeInternae(s_case_pool, 128);

/**
 * @brief Which pool each channel these cases use is over.
 *
 * @note One per channel, and the channel number is a literal because it becomes part of the symbol.
 *       PraetAttach and PraetSubmit both name that symbol. So a reach by either for a pool a channel
 *       is not over fails on an identifier nobody declared.
 * @note Every one of them is over the same pool here. Two contexts, or two channels over two pools,
 *       collide on nothing, because the context and the channel are both in the name.
 */
PraetChannel(s_schedule, 0, s_case_pool);
PraetChannel(s_schedule, 1, s_case_pool);
PraetChannel(s_schedule, 2, s_case_pool);
PraetChannel(s_schedule, 3, s_case_pool);
PraetChannel(s_schedule, 4, s_case_pool);

/**
 * @brief Attaches a channel and waits out whatever settle the part's knob asks for.
 *
 * @param[in] channel Channel to attach.
 * @note Every case but the settle one wants a channel it can submit on. How long that takes is
 *       PRAET_SETTLE_MICROS, which is per part and arrives at compile time. A case cannot pick a
 *       convenient number for itself.
 */
static void praet_case_attach_and_settle(embed_word channel)
{
    // Reaches praet_ordo_adnectere directly, past PraetAttach. The channel arrives here as a
    // parameter. The surface pastes the channel number into the symbol that carries the binding,
    // and so it needs a literal. The cases that drive the surface itself write one
    TEST_ASSERT_TRUE_MESSAGE(praet_ordo_adnectere(&s_schedule, channel, mmgr_pars_storage_s_case_pool,
                                                  (embed_word)s_case_pool_bytes, PRAET_CASE_REGION),
                             "the attach failed");
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_SETTLE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, channel) & PRAET_SETTLING,
                                     "the channel is still settling after its settle elapsed");
}

/**
 * @brief Submits a transfer over the case destination.
 *
 * @param[in] channel Channel to run on.
 * @param[in] length  Bytes to ask for.
 * @return            What the submit answered.
 * @note Absorbs the two forms praet_ordo_relatio takes. A case reads the same on both arms.
 *       Where PRAET_RECOVERY is off the entry takes no bytes, because nothing would record them.
 */
static embed_bool praet_case_submit(embed_word channel, embed_word length)
{
    // Not PraetSubmit, because that bounds a span whose length stands at compile time and this takes
    // one as an argument. The cases that check the bound call PraetSubmit with literals
    (void)length;
#if PRAET_RECOVERY
    return praet_ordo_relatio(&s_schedule, channel, 0u, length);
#else
    return praet_ordo_relatio(&s_schedule, channel);
#endif
}

/**
 * @brief Feeds the watchdog for one channel and says how far it got.
 *
 * @param[in] channel  Channel that showed a sign of life.
 * @param[in] position Bytes moved so far.
 * @note Absorbs the two forms praet_ordo_efficere takes, the same way praet_case_submit does. Where
 *       PRAET_RECOVERY is off a kick is a sign of life, alone.
 */
static void praet_case_kick(embed_word channel, embed_word position)
{
#if PRAET_RECOVERY
    praet_ordo_efficere(&s_schedule, channel, position);
#else
    (void)position;
    praet_ordo_efficere(&s_schedule, channel);
#endif
}

/**
 * @brief Polls one channel through the library, then feeds the schedule what the port reported.
 *
 * @param[in] channel Channel to poll.
 * @note Two calls, and both are the caller's. The call praet.poll drives the port through the library,
 *       and praet_ordo_poll is what turns whatever that produced into current flag words. The order
 *       matters: the port runs first so the orchestrator reads what this poll learned.
 * @note Nothing here reads a byte count or feeds a watchdog. The orchestrator asks the port itself,
 *       through praet_hw_progress, and so this is two calls and not a driver.
 * @note A caller's one move with the result is to read the flag word. Neither call reports anything
 *       back to them.
 */
static void praet_joined_poll(embed_word channel)
{
    EMBED_CALL(praet.poll, PraetCfg, .channel = (uint8_t)channel, .peripheral = 0u, .loopback = EMBED_FALSE,
               .on_complete = &s_joined_binding);

    praet_ordo_poll(&s_schedule);
}

/**
 * @brief The microsecond a pump gives back when what it waited for never happened.
 *
 * @note The largest value a word holds. No window a case pumps through comes near it, which keeps it
 *       apart from every microsecond a pump can legitimately return.
 * @note The complement runs at int width under the integer promotions, and the outer cast puts the
 *       result back at the word's own width.
 */
#define PRAET_PUMP_NEVER ((embed_word) ~(embed_word)0u)

/**
 * @brief Runs the schedule for @p micros microseconds, one microsecond at a time.
 *
 * @param[in] micros Microseconds to run.
 * @note The pump. A jump over a whole window and one service leaves two observations and nothing
 *       between them. A flag that went up early and came back down then looks identical to one that
 *       never went up. This services at every microsecond, and so no edge falls between two calls.
 * @note Every microsecond advanced raises the set volatile, because the passage of time changes what
 *       a service call would find. Every poll here therefore walks, and the short circuit is rare. A
 *       loop that polls faster than the clock moves reaches it, which is a different fixture from
 *       this one.
 */
static void praet_pump(embed_word micros)
{
    for (embed_word tick = 0u; tick < micros; tick++)
    {
        praet_ordo_advance(&s_schedule, 1u);
        praet_ordo_poll(&s_schedule);
    }
}

/**
 * @brief Pumps until every bit in @p mask goes up or clears on @p channel, and says when.
 *
 * @param[in] channel      Channel to watch.
 * @param[in] mask         Bits the wait is about. Every one of them has to hold.
 * @param[in] until_raised EMBED_TRUE to wait for them to come up, EMBED_FALSE for them to go down.
 * @param[in] limit        Microseconds to pump before the pump stops.
 * @return                 Microseconds pumped when the bits first matched, or PRAET_PUMP_NEVER.
 * @note Gives back the first microsecond the bits matched, the difference between a measure of
 *       a deadline and a notice that one went by. A window honored a microsecond early and one honored on
 *       time both finish with the flag set, and only the microsecond separates them.
 * @note One entry with the direction as an argument and not two entries. Every case passes a literal, and so
 *       the comparison folds and the two waits cannot drift apart.
 * @note Tests the flag word before it pumps anything. A state that already holds reports zero.
 * @note The same shape as the thing it times. A limit, a sign of life, and a verdict when the limit
 *       goes by is what the keepalive is, and PRAET_PUMP_NEVER is this one's PRAET_STALLED. It
 *       costs nothing, as long as the next note holds.
 * @warning The limit comes from PRAET_KEEPALIVE_MICROS or PRAET_SETTLE_MICROS, which are numbers the
 *          caller declared. Reading it out of context->keepalive_deadline or context->settle_deadline
 *          would make every case here agree with the schedule by construction and pass forever,
 *          whatever the schedule did.
 */
static embed_word praet_pump_until(embed_word channel, uint32_t mask, embed_bool until_raised, embed_word limit)
{
    for (embed_word waited = 0u; waited <= limit; waited++)
    {
        const embed_bool raised = ((praet_ordo_flags(&s_schedule, channel) & mask) == mask) ? EMBED_TRUE : EMBED_FALSE;

        if (raised == until_raised)
        {
            return waited;
        }
        if (waited == limit)
        {
            break;
        }

        praet_ordo_advance(&s_schedule, 1u);
        praet_ordo_poll(&s_schedule);
    }
    return PRAET_PUMP_NEVER;
}

/**
 * @brief Checks that a fresh context has every channel detached and empty.
 */
void test_a_channel_starts_detached(void)
{
    praet_ordo_reset(&s_schedule);

    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        TEST_ASSERT_EQUAL_UINT32(PRAET_SEPARATUS, praet_ordo_flags(&s_schedule, channel));
    }
}

/**
 * @brief Checks that attach claims the vector and a completed detach releases it.
 *
 * @note The claim is the reason no vector stays reserved for a mover with nothing to move. A channel
 *       that came back detached while still claimed would be the defect this case exists for.
 */
void test_attach_claims_the_vector_and_detach_releases_it(void)
{
    praet_ordo_reset(&s_schedule);

    praet_case_attach_and_settle(2u);
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 2u) & PRAET_CLAIMED) != 0u);

    praet_ordo_separare(&s_schedule, 2u);
    praet_ordo_poll(&s_schedule);

    const uint32_t after = praet_ordo_flags(&s_schedule, 2u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_SEPARATUS, after & PRAET_CORE_MASK, "the channel is still attached");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_CLAIMED, "a detached channel is still holding a vector");
}

/**
 * @brief Checks what PRAET_SETTLE_MICROS actually buys, at whatever value this build set it to.
 *
 * @note The two arms are not two behaviors, they are one behavior at two settings. A part whose
 *       engine is up at once has nothing to wait for and the channel is usable immediately. A part
 *       whose engine takes time refuses until that time elapsed. Both have to hold, and a build only
 *       ever compiles one of them, and so the sweep is what covers the other.
 */
void test_a_settling_channel_takes_no_transfer(void)
{
    praet_ordo_reset(&s_schedule);
    TEST_ASSERT_TRUE(PraetAttach(s_schedule, 0, s_case_pool, PRAET_CASE_REGION));

#if PRAET_SETTLE_MICROS == 0u
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 0u) & PRAET_SETTLING,
                                     "an engine that settles in no time reported settling");
    TEST_ASSERT_TRUE_MESSAGE(praet_case_submit(0u, 16u), "a channel with nothing to wait for refused a transfer");
#else
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 0u) & PRAET_SETTLING) != 0u,
                             "the channel never reported settling");
    TEST_ASSERT_FALSE_MESSAGE(praet_case_submit(0u, 16u), "a settling channel took a transfer");

    // One microsecond short of the deadline. This proves the wait is the length the knob asks for, and
    // that the first service call that happens along does not clear it
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_SETTLE_MICROS - 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 0u) & PRAET_SETTLING) != 0u,
                             "the settle cleared before it had elapsed");

    praet_ordo_advance(&s_schedule, 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 0u) & PRAET_SETTLING,
                                     "the settle never cleared");
    TEST_ASSERT_TRUE_MESSAGE(praet_case_submit(0u, 16u), "a settled channel refused a transfer");
#endif
}

/**
 * @brief Checks that a transfer reads busy until the port says it finished.
 *
 * @note No timer decides this. How long a transfer runs is not something the library can know. A
 *       clock that promoted busy to ok would be its guess at hardware.
 */
void test_a_transfer_reports_ok_when_the_port_says_so(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    TEST_ASSERT_EQUAL_UINT32(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK);

    praet_case_kick(1u, 32u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "a kick was read as a completion");

    praet_ordo_completed(&s_schedule, 1u, EMBED_FALSE);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_OK, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the transfer never reported ok");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_ERROR,
                                     "a clean completion reported an error");

#if PRAET_RECOVERY
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64u, praet_ordo_situs(&s_schedule, 1u),
                                     "a finished transfer did not account for every byte");
#endif
}

/**
 * @brief Checks that a transfer the port says failed reports an error and keeps what it moved.
 *
 * @note The case of the failed half of praet_ordo_completed. It sets PRAET_ERROR and clears it, which
 *       no other case does.
 * @note A failed transfer does not account for the length of its submit. Everything the port
 *       reported is still true and the rest never happened. So the position stays where the last
 *       report left it and a caller can still say which bytes hold writes.
 */
void test_a_failed_completion_reports_an_error(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_case_kick(1u, 20u);

    praet_ordo_completed(&s_schedule, 1u, EMBED_TRUE);

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_OK, after & PRAET_CORE_MASK, "a failed transfer left the channel busy");
    TEST_ASSERT_TRUE_MESSAGE((after & PRAET_ERROR) != 0u, "the failure was not recorded");

#if PRAET_RECOVERY
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(20u, praet_ordo_situs(&s_schedule, 1u),
                                     "a failed transfer claimed to have moved everything");
#endif
}

/**
 * @brief Checks that a channel which finished takes another transfer, and starts clean.
 *
 * @note The case of the ok to busy transition: a second submit on one channel, which
 *       praet_ordo_relatio accepts once the last transfer finished.
 * @note The second submit clears what the first one left. An error, a stall or a recovery from the
 *       last transfer could carry into the next one. A caller would then read a verdict that
 *       belongs to a transfer that is over.
 */
void test_a_finished_channel_takes_another_transfer(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);

    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_ordo_completed(&s_schedule, 1u, EMBED_TRUE);
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 1u) & PRAET_ERROR) != 0u);

    TEST_ASSERT_TRUE_MESSAGE(praet_case_submit(1u, 32u), "a channel that had finished refused a second transfer");

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, after & PRAET_CORE_MASK, "the second transfer did not start");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_ERROR, "the last transfer's error carried into this one");

#if PRAET_RECOVERY
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_situs(&s_schedule, 1u),
                                     "the second transfer started with the first one's position");
#endif
}

/**
 * @brief Checks that the next transfer clears a recovery's record.
 *
 * @note The case that runs a transfer on a recovered channel, which clears PRAET_ABANDONED,
 *       PRAET_SCRUBBED and PRAET_MEASURED.
 * @note A caller that reads a backout from the transfer before last would act on it. The next
 *       transfer clears the statuses the last one left where it starts, and this is the case that
 *       walks that path.
 */
void test_a_recovery_is_cleared_by_the_next_transfer(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_case_kick(1u, 20u);

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE));
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 1u) & PRAET_SCRUBBED) != 0u);

    TEST_ASSERT_TRUE_MESSAGE(praet_case_submit(1u, 32u), "a recovered channel refused the next transfer");

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & (PRAET_SCRUBBED | PRAET_ABANDONED | PRAET_MEASURED),
                                     "a recovery's record carried into the transfer after it");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_STALLED, "the stall carried into the transfer after it");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is no recovery to clear");
#endif
}

/**
 * @brief Checks that an active channel nobody kicks reads stalled, and stays busy.
 *
 * @note Stalled says the engine stopped. It never says the transfer finished, and so the core has to
 *       stay busy. The caller is the single one who decides what to do about bytes whose state is now
 *       unknown.
 */
void test_the_watchdog_marks_a_stalled_channel(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS - 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "the watchdog fired before its window elapsed");

    praet_ordo_advance(&s_schedule, 1u);
    praet_ordo_poll(&s_schedule);

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_TRUE_MESSAGE((after & PRAET_STALLED) != 0u, "the watchdog never fired");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, after & PRAET_CORE_MASK, "a stall was read as the transfer finishing");
}

/**
 * @brief Checks that a kick clears a stall and pushes the window out again.
 *
 * @note A channel that moves again has no stall. The same call that says so is the single one that
 *       reports how far it got.
 */
void test_a_kick_clears_a_stall_and_pushes_the_window(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED) != 0u);

    praet_case_kick(1u, 16u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "a channel that moved again is still marked stalled");

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS - 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "the kick did not push the window out");
}

/**
 * @brief Checks that the settle clears on the microsecond the knob names.
 *
 * @note What the pump is for. The jump-driven case proves the flag is up at one point and down at
 *       another. This walks every microsecond in between and reports the first one where it cleared.
 *       A settle that ended early passes that case and fails this one.
 * @note Ignored where PRAET_SETTLE_MICROS is zero. A channel with nothing to wait for never reports
 *       a settle, and there is no edge to time.
 */
void test_the_settle_clears_on_the_microsecond_it_was_asked_for(void)
{
#if PRAET_SETTLE_MICROS == 0u
    TEST_IGNORE_MESSAGE("PRAET_SETTLE_MICROS is zero, so nothing settles and there is no edge to time");
#else
    praet_ordo_reset(&s_schedule);
    TEST_ASSERT_TRUE(PraetAttach(s_schedule, 0, s_case_pool, PRAET_CASE_REGION));
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 0u) & PRAET_SETTLING) != 0u,
                             "the channel never reported settling");

    const embed_word cleared =
        praet_pump_until(0u, (uint32_t)PRAET_SETTLING, EMBED_FALSE, (embed_word)PRAET_SETTLE_MICROS * 2u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE((embed_word)PRAET_SETTLE_MICROS, cleared,
                                     "the settle did not clear on the microsecond it was asked for");
#endif
}

/**
 * @brief Checks that the watchdog fires on the microsecond its window closes.
 *
 * @note The keepalive window is a number a caller sets against their own part. Their due is that it
 *       means exactly what it says. One microsecond early is a transfer called dead while it still
 *       moved.
 */
void test_the_watchdog_fires_on_the_microsecond_the_window_closes(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    const embed_word fired =
        praet_pump_until(1u, (uint32_t)PRAET_STALLED, EMBED_TRUE, (embed_word)PRAET_KEEPALIVE_MICROS * 2u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE((embed_word)PRAET_KEEPALIVE_MICROS, fired,
                                     "the watchdog did not fire on the microsecond its window closed");
}

/**
 * @brief Checks that a kick re-arms the window to a full length.
 *
 * @note The dead time after a sign of life. A kick that arrives one microsecond before a window closes
 *       has to buy another whole window. An implementation that pushed the deadline by the remainder
 *       would stall a healthy channel a microsecond later.
 * @note The case the jumps could not make. A proof that the new window is a full one means a measure
 *       from the kick, at one microsecond of resolution.
 */
void test_a_kick_rearms_the_window_to_a_full_length(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_pump((embed_word)PRAET_KEEPALIVE_MICROS - 1u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "the watchdog fired before its window had closed");

    praet_case_kick(1u, 16u);

    const embed_word fired =
        praet_pump_until(1u, (uint32_t)PRAET_STALLED, EMBED_TRUE, (embed_word)PRAET_KEEPALIVE_MICROS * 2u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE((embed_word)PRAET_KEEPALIVE_MICROS, fired,
                                     "a kick bought less than a whole window");
}

/**
 * @brief Checks that a kick that reports less than the last one does not shrink the recorded extent.
 *
 * @note A position that went backwards is nonsense from the port, and to take it would shrink the
 *       extent a backout has to cover. Bytes with writes in them keep them whatever the engine says
 *       afterwards. So the high water mark is the only reading that is safe to keep.
 * @note Reports as ignored where PRAET_RECOVERY is off, because that build records no extent.
 *       A case that quietly passed instead would make the run look like it covered the same ground.
 */
void test_a_kick_that_goes_backwards_does_not_shrink_the_extent(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_case_kick(1u, 48u);
    praet_case_kick(1u, 16u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(48u, praet_ordo_situs(&s_schedule, 1u),
                                     "a backwards kick shrank the extent a backout has to cover");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so no extent is recorded to shrink");
#endif
}

/**
 * @brief Checks that a backout of a stalled transfer records the extent and leaves the bytes alone.
 *
 * @note Reports as ignored where PRAET_RECOVERY is off, since praet_ordo_resolve does not exist on
 *       that build.
 */
void test_backing_out_records_what_was_touched(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_case_kick(1u, 20u);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED) != 0u);

    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_ADNEXUS, after & PRAET_CORE_MASK,
                                     "a resolved channel did not come back attached");
    TEST_ASSERT_TRUE_MESSAGE((after & PRAET_ABANDONED) != 0u, "the backout was not recorded");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_SCRUBBED, "a backout claimed the bytes were scrubbed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_STALLED, "the stall survived being resolved");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is nothing to back a transfer out with");
#endif
}

/**
 * @brief Checks that the recovery that zeroes records that the bytes counted as unsafe.
 *
 * @note Nothing in the schedule writes the caller's storage, since it holds no pointer a write could
 *       go through. The flag records the decision and the caller zeroes its own bytes.
 * @note Reads the region byte back too. PRAET_SCRUBBED is the highest status and the region starts
 *       above it. A region placed one byte too low overlaps this one flag, alone. A scrub would
 *       then flip the low bit of a descriptor that has no business with change.
 */
void test_zeroing_records_that_the_bytes_were_scrubbed(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);

    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE));

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_TRUE_MESSAGE((after & PRAET_SCRUBBED) != 0u, "the scrub was not recorded");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & PRAET_ABANDONED, "a scrub also claimed to be a backout");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)PRAET_CASE_REGION << PRAET_REGION_SHIFT, after & PRAET_REGION_MASK,
                                     "the scrub reached into the region descriptor");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so no scrub can be recorded");
#endif
}

/**
 * @brief Checks that a healthy channel refuses a resolution.
 *
 * @note State is only ever unknown after the watchdog said so. A resolution of a channel that still
 *       moves would throw a live transfer away.
 */
void test_a_healthy_channel_refuses_to_be_resolved(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    TEST_ASSERT_FALSE_MESSAGE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE),
                              "a channel that never stalled was resolved");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the refused resolve changed the state anyway");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is no resolve to refuse");
#endif
}

/**
 * @brief Checks that the touched extent rounds up to the word the engine was in the middle of.
 *
 * @note The poll samples the position every few ticks, and so it is exact to word granularity. It can
 *       sit up to one word behind the writes. A scrub only as far as the sample would leave a partial
 *       word, and so the round is here and not in the caller.
 */
void test_touched_rounds_up_to_the_word(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    const embed_word word_bytes = (embed_word)sizeof(embed_word);

    praet_case_kick(1u, word_bytes + 1u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(word_bytes + 1u, praet_ordo_situs(&s_schedule, 1u),
                                     "the sample was not recorded as taken");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(word_bytes * 2u, praet_ordo_commotus_est(&s_schedule, 1u),
                                     "the touched extent did not round up to the word");

    praet_case_kick(1u, word_bytes * 2u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(word_bytes * 2u, praet_ordo_commotus_est(&s_schedule, 1u),
                                     "a sample already on a word boundary was rounded further");

    praet_case_kick(1u, 64u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64u, praet_ordo_commotus_est(&s_schedule, 1u),
                                     "the touched extent ran past the length that was submitted");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is no touched extent to round");
#endif
}

/**
 * @brief Checks that the round of the touched extent up does not wrap at the top of the word.
 *
 * @note A length within one word of the largest a word holds is a 64 KB region on a 16-bit part. It
 *       is where a round up runs off the end of the type. A round first and a clamp afterwards reads
 *       the wrapped value as a small number. That tells a caller almost none of its buffer has
 *       writes at the exact moment nearly all of it has.
 * @note Reaches praet_ordo_relatio directly, past PraetSubmit. A length this large cannot come
 *       through the surface, because PraetSubmit compares it against the pool's own extent before
 *       anything runs. What is under test here is the entry's arithmetic, which is still reachable by
 *       anyone who calls it with numbers nobody checked.
 * @note Nothing runs this transfer. The context never writes through the pointer it received. So the
 *       length is the sole subject of this case and no storage of that size has to exist.
 */
void test_the_touched_extent_does_not_wrap_at_the_top_of_the_word(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);

    const embed_word full_length = (embed_word) ~(embed_word)0u;

    TEST_ASSERT_TRUE(praet_ordo_relatio(&s_schedule, 1u, 0u, full_length));

    praet_case_kick(1u, full_length);

    const embed_word touched = praet_ordo_commotus_est(&s_schedule, 1u);

    TEST_ASSERT_TRUE_MESSAGE(touched == full_length, "the rounding wrapped and reported almost nothing touched");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is no touched extent to round");
#endif
}

/**
 * @brief Checks that the boundary word check measures the word the engine was inside, and says so.
 *
 * @note What the check buys is a statement about one word. The library holds the destination and not
 *       the source. So it reports what that word now contains, and the caller compares it against the
 *       intent. The contract here ends there.
 * @note The checksum covers the word the sample landed inside. A case that changes only a
 *       byte in that word has to move it. A case that changes a byte outside it must not.
 */
void test_the_boundary_word_check_measures_one_word(void)
{
#if PRAET_RECOVERY && PRAET_CRC_VALUE(PRAET_SUITE_CRC_CHOICE)
    const embed_word word_bytes = (embed_word)sizeof(embed_word);

    // A sample one byte into a word, and so the engine stopped mid-word and there is something to measure
    const embed_word sampled = word_bytes + 1u;

    // The pool's storage, where the pool's own name would not do. The name is a const view and this
    // stands in for the engine as it writes the bytes
    for (embed_word walk = 0u; walk < s_case_pool_bytes; walk++)
    {
        mmgr_pars_storage_s_case_pool[walk] = (uint8_t)walk;
    }

    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_case_kick(1u, sampled);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 1u) & PRAET_MEASURED) != 0u,
                             "the boundary word was not measured");

    const uint32_t first = praet_ordo_boundary_crc(&s_schedule, 1u);

    // A byte inside the boundary word. The measurement is over that word, and so this has to move it
    mmgr_pars_storage_s_case_pool[word_bytes] ^= 0xFFu;

    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_case_kick(1u, sampled);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    TEST_ASSERT_TRUE_MESSAGE(praet_ordo_boundary_crc(&s_schedule, 1u) != first,
                             "a byte inside the boundary word did not change the measurement");

    // Put it back, then change a byte outside the word. That one must not reach the measurement
    mmgr_pars_storage_s_case_pool[word_bytes] ^= 0xFFu;
    mmgr_pars_storage_s_case_pool[word_bytes * 3u] ^= 0xFFu;

    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    praet_case_kick(1u, sampled);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(first, praet_ordo_boundary_crc(&s_schedule, 1u),
                                     "a byte outside the boundary word reached the measurement");
#else
    TEST_IGNORE_MESSAGE("this context answered the boundary word question with the DISABLE token");
#endif
}

/**
 * @brief Checks that a sample already on a word boundary reports no measurement.
 *
 * @note There is no partial word under it, and so the word boundary was already the exact answer and
 *       there is nothing a checksum could add. A report of PRAET_MEASURED there would hand a caller a
 *       number that means nothing.
 */
void test_a_sample_on_a_boundary_measures_nothing(void)
{
#if PRAET_RECOVERY && PRAET_CRC_VALUE(PRAET_SUITE_CRC_CHOICE)
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_case_kick(1u, (embed_word)sizeof(embed_word) * 2u);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_MEASURED,
                                     "a sample on a word boundary reported a measurement");
#else
    TEST_IGNORE_MESSAGE("this context answered the boundary word question with the DISABLE token");
#endif
}

/**
 * @brief Checks that a context which said no never measures anything.
 *
 * @note The other arm of the token. The branch is still in the source; what this build proves is that
 *       the answer the declaration gave is the single one that runs.
 */
void test_a_context_that_said_no_measures_nothing(void)
{
#if PRAET_RECOVERY && !PRAET_CRC_VALUE(PRAET_SUITE_CRC_CHOICE)
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_case_kick(1u, (embed_word)sizeof(embed_word) + 1u);
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_MEASURED,
                                     "a context that said no measured the boundary word anyway");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_boundary_crc(&s_schedule, 1u),
                                     "a context that said no left a checksum behind");
#else
    TEST_IGNORE_MESSAGE("this context did not answer the boundary word question with the DISABLE token");
#endif
}

/**
 * @brief Checks that a build without recovery still detects a stall and records no recovery.
 *
 * @note The other half of the capability. Recovery off takes away what anyone can say about the
 *       bytes. It does not take away the knowledge that the engine stopped, because the watchdog is
 *       the keepalive and that is there either way.
 * @note The two recovery statuses keep their bits on every build, and so this reads them on a build that
 *       has no way to set them. A flag word means one thing everywhere or it means nothing.
 */
void test_a_build_without_recovery_still_stalls_and_records_none(void)
{
#if PRAET_RECOVERY
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is on, so this build has the recovery machinery");
#else
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);

    const uint32_t after = praet_ordo_flags(&s_schedule, 1u);

    TEST_ASSERT_TRUE_MESSAGE((after & PRAET_STALLED) != 0u, "the watchdog is gone with the recovery machinery");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, after & PRAET_CORE_MASK, "a stall was read as the transfer finishing");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, after & (PRAET_ABANDONED | PRAET_SCRUBBED),
                                     "a build with no recovery recorded one anyway");
#endif
}

/**
 * @brief Checks that ticks off the declared clock scale into the microseconds the deadlines are in.
 *
 * @note The whole point of a declared frequency. A port reads a counter and hands over ticks. Every
 *       deadline in this module is microseconds, and this is the single one place the two meet.
 * @note Driven against the watchdog, with no counter read back. The property under test is that a
 *       window measured in microseconds closes after the right number of ticks.
 */
void test_ticks_scale_into_microseconds(void)
{
    const embed_word per_micro = (embed_word)PRAET_TICKS_PER_MICRO;

    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));

    // One microsecond short of the window, counted in ticks
    praet_ordo_advance_ticks(&s_schedule, ((embed_word)PRAET_KEEPALIVE_MICROS - 1u) * per_micro);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "the ticks scaled up to more microseconds than they are worth");

    // A tick short of the last microsecond, which must not close the window either
    praet_ordo_advance_ticks(&s_schedule, per_micro - 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "a part of a microsecond was counted as a whole one");

    praet_ordo_advance_ticks(&s_schedule, per_micro);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED) != 0u,
                             "the window never closed, so the ticks scaled down to nothing");
}

/**
 * @brief Checks that the declared frequency is what the scale actually uses.
 *
 * @note A conversion that ignored the frequency would still pass the case above at one megahertz,
 *       where a tick is a microsecond. This states the relationship against the declared number,
 *       and so the two cannot agree by coincidence.
 */
void test_the_scaling_uses_the_declared_frequency(void)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((embed_word)(PRAET_CLOCK_HZ / 1000000u), (embed_word)PRAET_TICKS_PER_MICRO,
                                     "the ticks per microsecond do not come from the declared frequency");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, PRAET_CLOCK_MICROS((embed_word)PRAET_TICKS_PER_MICRO),
                                     "a microsecond of ticks did not convert to one microsecond");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, PRAET_CLOCK_MICROS((embed_word)PRAET_TICKS_PER_MICRO - 1u),
                                     "a partial microsecond of ticks was rounded up");
}

/**
 * @brief Checks that a span submitted through the surface lands where the pool says it does.
 *
 * @note What PraetAttach and PraetSubmit buy together. The address came from the pool named at the
 *       attach and the offset came from the submit. A transfer starts at a place neither call
 *       stated on its own.
 * @note The pool is 128 bytes and this submits 32 at offset 64, which PRAET_SPAN_FITS settled before
 *       anything ran. A span that did not fit would not compile, and so no case here
 *       submits one.
 */
void test_a_span_lands_where_the_pool_puts_it(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);

    TEST_ASSERT_TRUE(PraetSubmit(s_schedule, 1, s_case_pool, 64u, 32u));

    praet_ordo_efficere(&s_schedule, 1u, 32u);
    praet_ordo_completed(&s_schedule, 1u, EMBED_FALSE);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(32u, praet_ordo_situs(&s_schedule, 1u),
                                     "the transfer did not account for the span it was given");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&mmgr_pars_storage_s_case_pool[64], s_schedule.start[1],
                                  "the transfer did not start at the offset into the attached pool");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_s_case_pool, s_schedule.bound[1],
                                  "the channel is not bound to the pool it was attached over");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(s_case_pool_bytes, s_schedule.bound_bytes[1],
                                     "the bound extent did not come from the pool's own declaration");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so nothing records where a span landed");
#endif
}

/**
 * @brief The engine's script for the joined cases: open, take a transfer, finish it on the next poll.
 *
 * @note Software arm. A completion comes out only when a poll asks for it. That makes the moment
 *       the schedule learns of it a thing the case chooses.
 * @note One hook call is one tick. The open is tick one, the submit is tick two, and a cycle of one
 *       tick puts the completion due at tick three, the poll.
 */
static const PraetEngineStep s_joined_steps[] = {
    {.hook = (uint8_t)PRAET_HOOK_OPEN,
     .channel = 0u,
     .accepted = 1u,
     .complete_when = 0u,
     .completions = 0u,
     .moved = 0u,
     .settle_ticks = 0u,
     .cycle_ticks = 0u,
     .progress = 0u},
    {.hook = (uint8_t)PRAET_HOOK_SUBMIT,
     .channel = 0u,
     .accepted = 1u,
     .complete_when = (uint8_t)PRAET_COMPLETE_WHEN_DUE,
     .completions = 1u,
     .moved = 64u,
     .settle_ticks = 0u,
     .cycle_ticks = 3u,
     .progress = 0u},
    // Two polls that report how far the engine has got and release nothing, because the cycle has not
    // elapsed. The third is where the completion comes out
    {.hook = (uint8_t)PRAET_HOOK_POLL,
     .channel = PRAET_RELEASE_EVERY_CHANNEL,
     .accepted = 1u,
     .complete_when = 0u,
     .completions = 0u,
     .moved = 0u,
     .settle_ticks = 0u,
     .cycle_ticks = 0u,
     .progress = 20u},
    {.hook = (uint8_t)PRAET_HOOK_POLL,
     .channel = PRAET_RELEASE_EVERY_CHANNEL,
     .accepted = 1u,
     .complete_when = 0u,
     .completions = 0u,
     .moved = 0u,
     .settle_ticks = 0u,
     .cycle_ticks = 0u,
     .progress = 48u},
    {.hook = (uint8_t)PRAET_HOOK_POLL,
     .channel = PRAET_RELEASE_EVERY_CHANNEL,
     .accepted = 1u,
     .complete_when = 0u,
     .completions = 0u,
     .moved = 0u,
     .settle_ticks = 0u,
     .cycle_ticks = 0u,
     .progress = 0u},
};

/**
 * @brief The scenario the joined cases arm.
 *
 * @note Carries no program and no expectations. The case makes its own calls and reads the schedule
 *       afterward, and so the test covers the join alone.
 */
static const PraetScenario s_joined_scenario = {
    .name = "the port drives the schedule",
    .arm = (uint8_t)PRAET_ARM_SOFTWARE,
    .program = NULL,
    .program_count = 0u,
    .steps = s_joined_steps,
    .step_count = (uint16_t)(sizeof s_joined_steps / sizeof s_joined_steps[0]),
    .expect_opens = 0u,
    .expect_submits = 0u,
    .expect_completions = 0u,
    .expect_moved = 0u,
    .expect_settling = 0u,
    .expect_deferred = 0u,
    .expect_order = NULL,
    .expect_order_count = 0u,
};

/**
 * @brief Checks that a transfer the engine finishes is what moves the channel out of busy.
 *
 * @note The join, end to end. The schedule marks a channel busy on a submit and has no way to learn
 *       it finished. Nothing here predicts how long a transfer runs. The port's completion
 *       callback is the only thing that can say so, and this drives it through the real hooks.
 * @note Both halves run on logical channel one, and that leaves them the same channel. The engine
 *       schedules every logical channel over itself, and so the shared number alone ties the two halves together.
 */
void test_a_port_completion_moves_the_channel_out_of_busy(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    praet_engine_arm(&s_joined_scenario);

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(praet.open, PraetCfg, .channel = 1u, .peripheral = 0u, .loopback = EMBED_FALSE,
                                        .on_complete = &s_joined_binding),
                             "the engine refused the open");

    TEST_ASSERT_TRUE_MESSAGE(praet_case_submit(1u, 64u), "the schedule refused the transfer");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = 1u,
                                        .buf = mmgr_pars_storage_s_case_pool, .bytes = 64u),
                             "the engine refused the transfer");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the channel is not busy while the engine holds the transfer");

    praet_joined_poll(1u);
    praet_joined_poll(1u);
    praet_joined_poll(1u);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, s_received_completions, "the engine did not report a completion");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_OK, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the completion did not reach the schedule");

#if PRAET_RECOVERY
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64u, praet_ordo_situs(&s_schedule, 1u),
                                     "the finished transfer did not account for every byte");
#endif
}

/**
 * @brief Checks that the schedule stays busy while the engine holds the transfer.
 *
 * @note The other side of the join. A poll that releases nothing leaves the channel where it was. So
 *       the schedule reads busy for exactly as long as the engine still works.
 * @note The script is the same one, and this case stops before the poll that releases. It proves
 *       that the completion changed the state, and not the submit or the passage of hook
 *       calls.
 */
void test_the_schedule_stays_busy_while_the_engine_holds_it(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    praet_engine_arm(&s_joined_scenario);

    TEST_ASSERT_TRUE(EMBED_CALL(praet.open, PraetCfg, .channel = 1u, .peripheral = 0u, .loopback = EMBED_FALSE,
                                .on_complete = &s_joined_binding));
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    TEST_ASSERT_TRUE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = 1u, .buf = mmgr_pars_storage_s_case_pool,
                                .bytes = 64u));

    const PraetEngineTally tally = praet_engine_tally();

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, tally.deferred, "the engine is not holding the transfer");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, s_received_completions, "a completion arrived before it was due");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the channel left busy without the engine finishing anything");
}

/**
 * @brief Checks that what a poll reports is what the schedule records as moved.
 *
 * @note The figure a backout rests on. Everything before the position holds writes and the rest does
 *       not, and so where it comes from decides whether that statement is worth anything. It comes from
 *       the port, the only thing that can see a controller's count.
 * @note Two polls before the transfer is due, each one further along, then the one that
 *       finishes it. The recorded position follows the port at every step.
 */
void test_the_recorded_position_comes_from_the_port(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    praet_engine_arm(&s_joined_scenario);

    TEST_ASSERT_TRUE(EMBED_CALL(praet.open, PraetCfg, .channel = 1u, .peripheral = 0u, .loopback = EMBED_FALSE,
                                .on_complete = &s_joined_binding));
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    TEST_ASSERT_TRUE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = 1u, .buf = mmgr_pars_storage_s_case_pool,
                                .bytes = 64u));

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_situs(&s_schedule, 1u),
                                     "a transfer nothing has reported on has moved something");

    praet_joined_poll(1u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(20u, praet_ordo_situs(&s_schedule, 1u),
                                     "the first report did not reach the schedule");

    praet_joined_poll(1u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(48u, praet_ordo_situs(&s_schedule, 1u),
                                     "the second report did not reach the schedule");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "a progress report was read as the transfer finishing");

    praet_joined_poll(1u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_OK, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the completion did not reach the schedule");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64u, praet_ordo_situs(&s_schedule, 1u),
                                     "the finished transfer did not account for every byte");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so no position is recorded");
#endif
}

/**
 * @brief Checks that a backout of a stalled transfer covers the extent the port last reported.
 *
 * @note The whole point of the reports. The engine stops after it says it reached 20 bytes, the
 *       watchdog notices, and what a caller gets is an extent that came off the controller.
 *       This library made none of it up.
 * @note The touched extent rounds that sample up to a whole word. The sample is exact to word
 *       granularity and the engine can be part way into the next one.
 */
void test_a_backout_covers_what_the_port_last_reported(void)
{
#if PRAET_RECOVERY
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    praet_engine_arm(&s_joined_scenario);

    TEST_ASSERT_TRUE(EMBED_CALL(praet.open, PraetCfg, .channel = 1u, .peripheral = 0u, .loopback = EMBED_FALSE,
                                .on_complete = &s_joined_binding));
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    TEST_ASSERT_TRUE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = 1u, .buf = mmgr_pars_storage_s_case_pool,
                                .bytes = 64u));

    praet_joined_poll(1u);
    TEST_ASSERT_EQUAL_UINT32(20u, praet_ordo_situs(&s_schedule, 1u));

    // Nothing reports for a whole window, and so the engine stopped where it last said it was
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED) != 0u,
                             "the engine stopped and the watchdog did not notice");

    const embed_word word_bytes = (embed_word)sizeof(embed_word);
    const embed_word expected = (embed_word)(((20u + word_bytes - 1u) / word_bytes) * word_bytes);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, praet_ordo_commotus_est(&s_schedule, 1u),
                                     "the extent to back out did not come from what the port reported");

    TEST_ASSERT_TRUE(praet_ordo_resolve(&s_schedule, 1u, PRAET_RESTITUERE_ET_REDINTEGRARE));
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 1u) & PRAET_ABANDONED) != 0u,
                             "the backout was not recorded");
#else
    TEST_IGNORE_MESSAGE("PRAET_RECOVERY is off, so there is nothing to back out");
#endif
}

/**
 * @brief Checks that a refused attach leaves the settle timer where it was.
 *
 * @note A successful attach is what starts the settle of the engine, because the attach is the moment
 *       the channel gets its order to work. A refused attach told it nothing. A move of the deadline
 *       there would hold off every other channel on the strength of a call that did nothing.
 */
void test_a_refused_attach_does_not_start_the_timer(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);

    const embed_word settled_at = s_schedule.settle_deadline;

    praet_ordo_advance(&s_schedule, 100u);

    // Channel one is already attached, and so this fails
    TEST_ASSERT_FALSE_MESSAGE(praet_ordo_adnectere(&s_schedule, 1u, mmgr_pars_storage_s_case_pool,
                                                   (embed_word)s_case_pool_bytes, PRAET_CASE_REGION),
                              "a channel that was already attached took a second attach");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(settled_at, s_schedule.settle_deadline,
                                     "a refused attach started the settle timer");

    // And one past the end, which fails before any read
    TEST_ASSERT_FALSE(praet_ordo_adnectere(&s_schedule, PRAET_CHANNELS, mmgr_pars_storage_s_case_pool,
                                           (embed_word)s_case_pool_bytes, PRAET_CASE_REGION));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(settled_at, s_schedule.settle_deadline,
                                     "an attach on a channel that does not exist started the settle timer");
}

/**
 * @brief Checks that a poll of the port keeps an active channel off the watchdog.
 *
 * @note A poll is the port's word that the engine is alive, and the kick is what the schedule does with
 *       that. A channel polled inside its window never stalls, and the same channel left alone for
 *       one window does.
 * @note The engine holds the transfer throughout. What is under test is the watchdog against the
 *       port's activity, and so nothing here completes anything.
 */
void test_polling_the_port_keeps_the_watchdog_fed(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(1u);
    praet_engine_arm(&s_joined_scenario);

    TEST_ASSERT_TRUE(EMBED_CALL(praet.open, PraetCfg, .channel = 1u, .peripheral = 0u, .loopback = EMBED_FALSE,
                                .on_complete = &s_joined_binding));
    TEST_ASSERT_TRUE(praet_case_submit(1u, 64u));
    TEST_ASSERT_TRUE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = 1u, .buf = mmgr_pars_storage_s_case_pool,
                                .bytes = 64u));

    // Most of the window goes by, then the port reports the engine is still alive. Position unchanged,
    // because this engine says how far it got only when it finishes
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS - 1u);
    praet_case_kick(1u, 0u);
    praet_ordo_poll(&s_schedule);

    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS - 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED,
                                     "a channel the port kept reporting on was marked stalled");

    // Nothing reports for a whole window this time
    praet_ordo_advance(&s_schedule, 1u);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 1u) & PRAET_STALLED) != 0u,
                             "a channel nothing reported on was not marked stalled");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 1u) & PRAET_CORE_MASK,
                                     "the stall was read as the transfer finishing");
}

/**
 * @brief Checks that a detach asked for mid-transfer waits until that transfer ends.
 *
 * @note A teardown while the engine still moves bytes would leave it with writes into storage the
 *       caller believes is free.
 */
void test_detach_waits_for_the_transfer_under_it(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(3u);
    TEST_ASSERT_TRUE(praet_case_submit(3u, 64u));

    praet_ordo_separare(&s_schedule, 3u);
    praet_ordo_poll(&s_schedule);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_BUSY, praet_ordo_flags(&s_schedule, 3u) & PRAET_CORE_MASK,
                                     "the detach tore down a channel the engine was still using");
    TEST_ASSERT_TRUE((praet_ordo_flags(&s_schedule, 3u) & PRAET_SEPARANS) != 0u);

    praet_ordo_completed(&s_schedule, 3u, EMBED_FALSE);
    praet_ordo_poll(&s_schedule);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(PRAET_SEPARATUS, praet_ordo_flags(&s_schedule, 3u) & PRAET_CORE_MASK,
                                     "the detach never completed once the transfer had");
}

/**
 * @brief Checks that the interrupt may raise set any number of times without loss or accumulation.
 *
 * @note One reader means the raises collapse. Nothing counts and nothing waits in line. A hundred
 *       raises and one raise reach the same state.
 */
void test_the_interrupt_can_raise_set_any_number_of_times(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(0u);
    TEST_ASSERT_TRUE(praet_case_submit(0u, 64u));
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);

    for (unsigned raised = 0u; raised < 100u; raised++)
    {
        praet_ordo_raise(&s_schedule);
    }

    praet_ordo_poll(&s_schedule);

    TEST_ASSERT_TRUE_MESSAGE((praet_ordo_flags(&s_schedule, 0u) & PRAET_STALLED) != 0u,
                             "a hundred raises did not reach the same state as one");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_schedule.praet_set_bitflag, "set was left raised after the reader ran");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_schedule.praet_busy_bitflag, "the lock was left held");
}

/**
 * @brief Checks that a service call that arrives while the lock is out declines and changes nothing.
 *
 * @note The re-entry case. This takes the lock directly, the state an interrupt that lands
 *       mid-update would find. The nested call must not rework state the outer one is partway
 *       through.
 */
void test_a_nested_service_declines_while_the_lock_is_taken(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(0u);
    TEST_ASSERT_TRUE(praet_case_submit(0u, 64u));
    praet_ordo_advance(&s_schedule, (embed_word)PRAET_KEEPALIVE_MICROS);

    const uint32_t before = praet_ordo_flags(&s_schedule, 0u);

    s_schedule.praet_busy_bitflag = 1u;
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before, praet_ordo_flags(&s_schedule, 0u),
                                     "a nested call changed state the outer call owned");
    TEST_ASSERT_TRUE_MESSAGE(s_schedule.praet_set_bitflag != 0u,
                             "a nested call cleared the raise the outer one had not answered yet");

    // The lock comes down and the same work is still there to do, because nothing consumed an event
    s_schedule.praet_busy_bitflag = 0u;
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(before, praet_ordo_flags(&s_schedule, 0u),
                                         "the work was lost once the lock came down");
}

/**
 * @brief Checks that the region descriptor an attach set survives every state change after it.
 *
 * @note The pool's declaration settles which memory a channel reaches. Nothing but an attach has
 *       business with a write to it, and a submit that clobbered it would be silent.
 */
void test_the_region_descriptor_survives_a_transfer(void)
{
    praet_ordo_reset(&s_schedule);
    praet_case_attach_and_settle(4u);

    const uint32_t expected = (uint32_t)PRAET_CASE_REGION << PRAET_REGION_SHIFT;

    TEST_ASSERT_EQUAL_UINT32(expected, praet_ordo_flags(&s_schedule, 4u) & PRAET_REGION_MASK);

    TEST_ASSERT_TRUE(praet_case_submit(4u, 64u));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, praet_ordo_flags(&s_schedule, 4u) & PRAET_REGION_MASK,
                                     "the submit clobbered the region");

    praet_ordo_completed(&s_schedule, 4u, EMBED_FALSE);
    praet_ordo_poll(&s_schedule);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, praet_ordo_flags(&s_schedule, 4u) & PRAET_REGION_MASK,
                                     "the completion clobbered the region");
}

/**
 * @brief Checks that the two arms are not the same engine.
 *
 * @note Each scenario comes out twice, interrupt then software. If the arm selection ever broke and
 *       both ran the same way, every row would still pass and half the table would measure
 *       nothing. This is the case that notices.
 */
void test_the_two_arms_are_not_the_same_engine(void)
{
    const size_t count = sizeof praet_scenarios / sizeof praet_scenarios[0];
    unsigned differing = 0u;

    for (size_t index = 0u; (index + 1u) < count; index += 2u)
    {
        const PraetScenario *const interrupt_arm = &praet_scenarios[index];
        const PraetScenario *const software_arm = &praet_scenarios[index + 1u];

        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)PRAET_ARM_INTERRUPT, interrupt_arm->arm, interrupt_arm->name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)PRAET_ARM_SOFTWARE, software_arm->arm, software_arm->name);

        if ((interrupt_arm->expect_completions != software_arm->expect_completions) ||
            (interrupt_arm->expect_deferred != software_arm->expect_deferred) ||
            (interrupt_arm->expect_order_count != software_arm->expect_order_count))
        {
            differing++;
        }
    }

    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(0u, differing,
                                          "no scenario tells the two arms apart, so the arm is untested");
}

/**
 * @brief A second pool. A descriptor has somewhere to read from and somewhere else to write to.
 *
 * @note Two pools, where two halves of one would not do. A descriptor names both ends and each has
 *       its proof against its own declaration. One pool would prove the same span twice.
 */
ParsMemoriaeInternae(s_case_source, 128);

/**
 * @brief One transfer that runs and stops.
 */
PraetOneShot(s_one_shot, s_case_source, 0u, s_case_pool, 0u, 64u, PRAET_MENSURA_VERBUM);

/**
 * @brief One transfer that runs itself again, forever.
 */
PraetCircular(s_circular, s_case_source, 0u, s_case_pool, 0u, 32u, PRAET_MENSURA_VERBUM);

/**
 * @brief Two transfers that hand off to each other, and write alternate halves.
 */
PraetPingPong(s_ping, s_pong, s_case_source, s_case_pool, 0u, 0u, 64u, 64u, 64u, PRAET_MENSURA_VERBUM);

/**
 * @brief A transfer inside one pool, with writes upward over its own source.
 *
 * @note The overlap case, and the direction comes from the offsets at compile time.
 */
PraetDescriptorWithin(s_shift_up, s_case_pool, 0u, 32u, 64u, PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief A transfer inside one pool, with writes downward over its own source.
 */
PraetDescriptorWithin(s_shift_down, s_case_pool, 32u, 0u, 64u, PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief A transfer inside one pool whose two spans do not touch.
 */
PraetDescriptorWithin(s_no_overlap, s_case_pool, 0u, 64u, 32u, PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief Checks that a one shot descriptor says where it reads, where it writes, and that it ends.
 *
 * @note What a descriptor is. Its declaration settles everything except who holds it. So this reads
 *       the object the declaration emitted, and nothing ran to produce it.
 */
void test_a_one_shot_descriptor_ends(void)
{
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_s_case_source, s_one_shot.source,
                                  "the descriptor does not read from the pool it named");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_s_case_pool, s_one_shot.destination,
                                  "the descriptor does not write to the pool it named");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(64u, s_one_shot.bytes, "the descriptor moves a length nobody asked for");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MENSURA_VERBUM, s_one_shot.ego_sum_mensura,
                                    "the descriptor steps a width nobody asked for");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_ADVANCES, s_one_shot.source_addressing,
                                    "a memory to memory read does not advance");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_ADVANCES, s_one_shot.destination_addressing,
                                    "a memory to memory write does not advance");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_OWNER_SOFTWARE, s_one_shot.owner,
                                    "a descriptor nobody gave the engine is not software's");
    TEST_ASSERT_NULL_MESSAGE(s_one_shot.next, "a one shot descriptor has something after it");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, praet_descriptor_chain_length(&s_one_shot, 8u),
                                     "a one shot is not a chain of one");
}

/**
 * @brief Checks that a circular descriptor points at itself.
 *
 * @note Circular is one descriptor whose next is its own address. There is no mode and no bit, and
 *       the walk that counts it stops at the head, where it would otherwise follow it forever.
 */
void test_a_circular_descriptor_points_at_itself(void)
{
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_circular, s_circular.next, "a circular descriptor does not run itself again");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, praet_descriptor_chain_length(&s_circular, 8u),
                                     "a cycle of one was not counted as one");
}

/**
 * @brief Checks that a ping-pong pair hands off in both directions and writes different halves.
 *
 * @note Ping-pong, and it is two descriptors whose next point at each other. What makes it a double
 *       buffer is that the two write different halves, the offsets of the declaration and not
 *       anything the library decided.
 */
void test_a_ping_pong_pair_hands_off_both_ways(void)
{
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_pong, s_ping.next, "the first half does not hand off to the second");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_ping, s_pong.next, "the second half does not hand back to the first");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, praet_descriptor_chain_length(&s_ping, 8u),
                                     "a cycle of two was not counted as two");

    TEST_ASSERT_TRUE_MESSAGE(s_ping.destination != s_pong.destination,
                             "both halves of a ping-pong write the same place");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&mmgr_pars_storage_s_case_pool[64], s_pong.destination,
                                  "the second half does not write where the declaration put it");
}

/**
 * @brief A register that stands in for one a part would name.
 *
 * @note A host has no peripherals, and so this is a byte of this file's own with a volatile view taken of
 *       it. What a real caller writes is the name out of their vendor's header, which the compiler has
 *       already read. No address in this tree is a number anybody here chose.
 */
static volatile uint8_t s_case_register;

/**
 * @brief The peripheral the cases below reach.
 */
PraetPeripheralDeclare(s_case_peripheral, &s_case_register);

/**
 * @brief One transfer that drains a pool into a register.
 */
PraetToPeripheral(s_drain, s_case_source, 0u, s_case_peripheral, &s_case_register, 0u, 64u, PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief One transfer that fills a pool from a register.
 */
PraetFromPeripheral(s_fill, s_case_peripheral, &s_case_register, 0u, s_case_pool, 0u, 64u, PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief Checks that a peripheral end stays put and the pool end advances.
 *
 * @note The one fact memcpy never needs. A peripheral is an address that does not move, and which
 *       side does not move alone makes a transfer a read or a write.
 * @note Neither way to walk either end. A pool and a register are not the same object, and a
 *       transfer between them cannot read what it already wrote.
 */
void test_a_peripheral_end_does_not_advance(void)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_ADVANCES, s_drain.source_addressing,
                                    "the pool a drain reads does not advance");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_FIXED, s_drain.destination_addressing,
                                    "the register a drain writes moves");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_case_register, s_drain.destination,
                                  "the drain does not write the register that was declared");

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_FIXED, s_fill.source_addressing, "the register a fill reads moves");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_ADDRESS_ADVANCES, s_fill.destination_addressing,
                                    "the pool a fill writes does not advance");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_case_register, s_fill.source,
                                  "the fill does not read the register that was declared");

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_NEUTRUM, s_drain.directio_motus_verbi,
                                    "a transfer to a register was given a direction");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_NEUTRUM, s_fill.directio_motus_verbi,
                                    "a transfer from a register was given a direction");
}

/**
 * @brief Checks that a transfer inside one pool works out which way to walk its bytes.
 *
 * @note What memor already answers for a copy, and for the same reason. Where the two ends overlap,
 *       one direction reads bytes the other wrote over. Settled once, before anything runs, and
 *       never asked again.
 * @note Both ends are offsets into one declared object, the entirety of reason the question has an
 *       answer at compile time. Two pointers would have no order here, and the language does not
 *       define a compare of pointers into separate objects.
 */
void test_a_transfer_inside_one_pool_picks_its_direction(void)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_DEORSUM, s_shift_up.directio_motus_verbi,
                                    "writing upward over its own source did not walk downward");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_SURSUM, s_shift_down.directio_motus_verbi,
                                    "writing downward over its own source did not walk upward");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_NEUTRUM, s_no_overlap.directio_motus_verbi,
                                    "two spans that do not touch were given a direction they do not need");
}

/**
 * @brief Checks that a transfer between two declared pools needs no direction.
 *
 * @note Two pools are two separately declared objects, and so they cannot overlap and neither direction
 *       is wrong. That is a fact about the declarations and not something measured, and so the
 *       answer settles with no look at either address.
 */
void test_a_transfer_between_two_pools_needs_no_direction(void)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_NEUTRUM, s_one_shot.directio_motus_verbi,
                                    "a transfer between two pools was given a direction");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(PRAET_MOTUS_NEUTRUM, s_ping.directio_motus_verbi,
                                    "a ping-pong half between two pools was given a direction");
}

/**
 * @brief Checks that a chain longer than the walk may follow reports the limit.
 *
 * @note The walk names a cycle when it returns to the head. A chain that closes further along looks
 *       the same as one that never ends. A report of the limit is the honest answer for a walk that
 *       cannot see where it is.
 */
void test_a_chain_walk_stops_at_the_limit(void)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, praet_descriptor_chain_length(&s_ping, 1u),
                                     "the walk followed more links than it was allowed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_descriptor_chain_length(&s_ping, 0u),
                                     "a walk allowed no links followed one");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, praet_descriptor_chain_length(NULL, 8u), "a walk from nowhere reached a link");
}

/**
 * @brief Prints which states and transitions this run reached.
 *
 * @note The examination arm, and it asserts nothing. A transition no case walked through is a hole in
 *       the cases. Whether that hole is worth a case is a reading of the report. A failure here
 *       would make the suite refuse to build over a judgment nobody made yet.
 * @note Last in the file, because the generated runner registers cases in the order of the file
 *       and this reports on everything ahead of it.
 * @note Ignored where PRAET_PROCURATOR is 0. A run that did not ask for the instrument says so, and
 *       prints no empty table.
 */
void test_zz_what_this_run_reached(void)
{
#if PRAET_PROCURATOR
    praet_procurator_report();
    TEST_PASS_MESSAGE("the examination arm reports, it does not gate");
#else
    TEST_IGNORE_MESSAGE("PRAET_PROCURATOR is 0, so this run recorded nothing");
#endif
}
