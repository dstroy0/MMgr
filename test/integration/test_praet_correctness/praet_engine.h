/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_engine.h
 * @brief A scripted DMA engine for the host: the program a case runs, the engine's answers, and
 *        what a run leaves behind.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note This is a port. It supplies strong definitions of the four mmgr_praet_hw_ hooks, so what it
 *       exercises is the contract a board support file implements and a failure here is an
 *       integration defect.
 * @note The engine owns no bytes and no buffer. A transfer names the caller's storage and its
 *       extent, the same way memor.cpy does, and the engine reports how much of it moved.
 * @note Channels here are logical. One engine schedules all of them, which is what a part with fewer
 *       channels than a program wants actually does, and it is why completions can come back in an
 *       order the submissions did not.
 * @note Every behavior is scripted. Nothing decides anything at run time, so a failure is a fixture
 *       the next run reproduces exactly.
 */
#ifndef MMGR_TEST_PRAET_ENGINE_H
#define MMGR_TEST_PRAET_ENGINE_H

#include "memoriam_praetereo/memoriam_praetereo.h"

EMBED_BEGIN_DECLS

/**
 * @brief Logical channels one scenario may name.
 *
 * @note A ceiling on the fixture, not on the library. The engine schedules every logical channel
 *       over itself, so this bounds the pending table and nothing else.
 */
#define PRAET_ENGINE_CHANNELS 8u

/**
 * @brief Which of the four port hooks a call reaches, and which a reaction answers.
 */
typedef enum
{
    PRAET_HOOK_OPEN = 0,   /**< mmgr_praet_hw_open. */
    PRAET_HOOK_SUBMIT = 1, /**< mmgr_praet_hw_tx_submit. */
    PRAET_HOOK_CLOSE = 2,  /**< mmgr_praet_hw_close. */
    PRAET_HOOK_POLL = 3    /**< mmgr_praet_hw_poll. */
} PraetHook;

/**
 * @brief How the engine gets to deliver a completion once the transfer is due.
 *
 * @note The two are the same engine with one difference: whether it can report without being asked.
 *       A part with a spare interrupt takes the first; a part without one falls back to the second,
 *       which is what makes the module practical on a part that has no vector to give it.
 * @note Every scenario runs on both. The fixture is the same and only the arm changes, so a
 *       difference in outcome is the arm and nothing else.
 */
typedef enum
{
    PRAET_ARM_INTERRUPT = 0, /**< Reports at the first hook call at or after the transfer is due. */
    PRAET_ARM_SOFTWARE = 1   /**< Reports only when a poll selects the channel and it is due. */
} PraetEngineArm;

/**
 * @brief When a scripted reaction delivers the completions it was given.
 *
 * @note PRAET_COMPLETE_INSIDE_CALL is the re-entrancy case. The callback runs before the hook
 *       returns, so the library is called back while it is still inside a submit it has not
 *       finished. It ignores the cycle timer, since nothing has elapsed yet.
 * @note PRAET_COMPLETE_WHEN_DUE holds the completions against the logical channel until the cycle
 *       ticks have elapsed and the arm allows them out.
 */
typedef enum
{
    PRAET_COMPLETE_NEVER = 0,       /**< Accept and report nothing, ever. */
    PRAET_COMPLETE_INSIDE_CALL = 1, /**< Report before the hook returns. */
    PRAET_COMPLETE_WHEN_DUE = 2     /**< Hold against the channel until due and released. */
} PraetCompleteWhen;

/**
 * @brief Channel selector meaning every channel holding a due completion.
 *
 * @note A poll reaction carrying this releases them lowest channel first. A poll naming one channel
 *       releases that one and leaves the rest held.
 */
#define PRAET_RELEASE_EVERY_CHANNEL 0xFFu

/**
 * @brief One call the case makes into the library.
 *
 * @param call    Which entry the case reaches, as a PraetHook.
 * @param channel Logical channel the call names.
 * @param bytes   Bytes the call submits. Read by PRAET_HOOK_SUBMIT; the other three ignore it.
 * @note The program is part of the fixture. What the library is asked to do decides as much of the
 *       outcome as what the engine answers, so both live in the scenario.
 */
typedef struct
{
    uint8_t call;
    uint8_t channel;
    uint16_t bytes;
} PraetProgramStep;

/**
 * @brief One scripted reaction: what a hook answers, and what it reports afterward.
 *
 * @param hook          Which hook this reaction answers, as a PraetHook.
 * @param channel       Read by a poll reaction alone, naming the channel whose due completions it
 *                      releases, or PRAET_RELEASE_EVERY_CHANNEL. The other three hooks take their
 *                      channel from the call the case made.
 * @param accepted      What the hook returns. Read by the open and submit hooks; the other two
 *                      return nothing and ignore it.
 * @param complete_when When the completions are delivered, as a PraetCompleteWhen.
 * @param completions   How many completion callbacks this reaction delivers. Two for one transfer is
 *                      a case no part produces and the library still has to survive.
 * @param moved         Bytes each completion reports. Free of the submitted count on purpose, so a
 *                      short transfer and an over-count are both expressible.
 * @param settle_ticks  Read by an open reaction. Hook calls the channel spends settling before it
 *                      will take a transfer. Zero is a channel usable at once.
 * @param cycle_ticks   Read by a submit or close reaction. Hook calls before the completion is due.
 *                      Zero is due immediately.
 * @param progress      Read by a poll reaction. Bytes the polled channel has moved by then, as a
 *                      controller's remaining count would say. Zero reports nothing and leaves the
 *                      last figure standing, which is what every scenario written before this field
 *                      existed does.
 * @note A reaction that refuses delivers no completion whatever the other members hold. Refusing and
 *       reporting a completion is not a state hardware reaches.
 * @note progress is last on purpose. The generated scenarios initialize these positionally, so a
 *       field at the end leaves every one of them reporting no progress and behaving as it did.
 */
typedef struct
{
    uint8_t hook;
    uint8_t channel;
    uint8_t accepted;
    uint8_t complete_when;
    uint8_t completions;
    uint16_t moved;
    uint16_t settle_ticks;
    uint16_t cycle_ticks;
    uint16_t progress;
} PraetEngineStep;

/**
 * @brief One scenario: the program the case runs, the engine's script, and the outcome required.
 *
 * @param name               Text naming the scenario, printed when it fails.
 * @param arm                Which arm the engine runs, as a PraetEngineArm.
 * @param program            Calls the case makes, in order [BORROWS].
 * @param program_count      Calls at program.
 * @param steps              Reactions in the order the hooks are called [BORROWS].
 * @param step_count         Reactions at steps.
 * @param expect_opens       Opens that must come back EMBED_TRUE.
 * @param expect_submits     Submits that must come back EMBED_TRUE.
 * @param expect_completions Completion callbacks the case must receive.
 * @param expect_moved       Sum of the bytes those completions report.
 * @param expect_settling    Submits the engine must refuse because the channel had not settled.
 * @param expect_held        Completions still held when the program ends. Not always zero: a
 *                           software arm the case never polls, and a transfer whose cycle has not
 *                           elapsed, both legitimately leave one waiting.
 * @param expect_order       Logical channels of those completions, in the order they must arrive
 *                           [BORROWS].
 * @param expect_order_count Entries at expect_order.
 * @note The expectations are the oracle and the generator computes them. An expectation the engine
 *       derived would agree with itself and prove nothing.
 * @note expect_order is what a total count cannot say. Two completions arriving on the wrong
 *       channels sum to the same number as two arriving on the right ones.
 */
typedef struct
{
    const char *name;
    uint8_t arm;
    const PraetProgramStep *program;
    uint16_t program_count;
    const PraetEngineStep *steps;
    uint16_t step_count;
    uint16_t expect_opens;
    uint16_t expect_submits;
    uint16_t expect_completions;
    uint32_t expect_moved;
    uint16_t expect_settling;
    uint16_t expect_held;
    const uint8_t *expect_order;
    uint16_t expect_order_count;
} PraetScenario;

/**
 * @brief What a run of one scenario actually produced, from the engine's side.
 *
 * @param completions Completions the engine emitted.
 * @param held        Completions still held against a channel when the program ended.
 * @param settling    Submits the engine refused because the channel had not settled.
 * @param overrun     Hook calls the script had no reaction for, which is a script shorter than the
 *                    program or out of step with it.
 * @note The case counts what it received at its own callback. These are what the engine sent, and
 *       the gap between the two is a completion the library dropped or invented.
 */
typedef struct
{
    uint16_t completions;
    uint16_t held;
    uint16_t settling;
    uint16_t overrun;
} PraetEngineTally;

/**
 * @brief Points the engine at a scenario and clears the tally.
 *
 * @param[in] scenario Scenario the engine runs until the next arm [BORROWS].
 * @note Called before each case. The scenario outlives the run, since the engine reads its steps
 *       from the hooks rather than copying them.
 */
void praet_engine_arm(const PraetScenario *scenario);

/**
 * @brief Returns what the run so far produced.
 *
 * @return The tally, by value.
 * @note Read after the case has finished its program. A tally read part way through is a snapshot of
 *       an unfinished run.
 */
PraetEngineTally praet_engine_tally(void);

/**
 * @brief Returns how far the engine says @p channel has got.
 *
 * @param[in] channel Logical channel to read.
 * @return            Bytes moved so far, or 0 for a channel outside the fixture's table.
 * @note What a port reads off a controller's remaining count, and what it feeds a watchdog with. The
 *       library has no entry for it, so the application asks the port and passes the answer to
 *       praet_schedule_kick. That is the shape a real integration takes until the library grows one.
 * @note Set by a poll reaction carrying a progress figure, and set to the reported count when a
 *       transfer completes. A channel nothing has reported on holds its last figure.
 */
uint16_t praet_engine_progress(uint8_t channel);

EMBED_END_DECLS

#endif
