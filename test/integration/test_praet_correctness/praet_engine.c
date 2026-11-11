/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_engine.c
 * @brief The scripted engine behind the four mmgr_praet_hw_ hooks.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Every symbol the library declares weak has a strong definition here, and so the linker takes
 *       these over the defaults in memoriam_praetereo.c that refuse. That is the same seam a board support
 *       file uses, and it holds with link-time optimization on.
 * @note One engine schedules every logical channel. A completion waits on the channel that
 *       earned it. It comes out when its cycle elapses and the arm lets it. So the order
 *       completions arrive in is the scenario's to choose and need not match the order submitted.
 * @note One hook call is one tick. Settle time and cycle time both count in them, which keeps the
 *       whole fixture deterministic and free of a clock.
 * @warning Included by test_praet_correctness.c and never compiled on its own. mmgr_add_suite
 *          compiles the one suite source and the shared files under test/support. These hooks
 *          must not reach any other suite, since test_memoriam_praetereo exists to prove the weak
 *          defaults refuse. A strong definition linked into it would answer for them.
 */
#include "praet_engine.h"

/**
 * @brief Scenario the engine runs now.
 *
 * @note Null until praet_engine_arm runs. Every hook refuses against a null scenario, as
 *       an unarmed engine should look like.
 */
static const PraetScenario *s_scenario;

/**
 * @brief Index of the reaction the next hook call consumes.
 */
static uint16_t s_step_index;

/**
 * @brief Hook calls since the arm of the scenario.
 */
static uint16_t s_tick;

/**
 * @brief What the run produced so far.
 */
static PraetEngineTally s_tally;

/**
 * @brief Completion callback the last accepted open registered.
 *
 * @note Kept by pointer and never copied, as mmgr_praet_open documents. The case owns the binding and keeps
 *       it alive for the whole scenario [BORROWS].
 */
static const PraetCallbackCfg *s_binding;

/**
 * @brief Tick at which each channel finishes its settle.
 */
static uint16_t s_settled_at[PRAET_ENGINE_CHANNELS];

/**
 * @brief Completions that wait against each logical channel.
 */
static uint8_t s_deferred_completions[PRAET_ENGINE_CHANNELS];

/**
 * @brief Bytes each deferred completion reports, per logical channel.
 */
static uint16_t s_deferred_moved[PRAET_ENGINE_CHANNELS];

/**
 * @brief Tick at which each channel's deferred completions come due.
 */
static uint16_t s_due_at[PRAET_ENGINE_CHANNELS];

/**
 * @brief Bytes each channel moved, as the engine last reported them.
 *
 * @note What a controller's rest count stands for here. A poll reaction with a progress
 *       figure writes it, and a completion writes the count it reported. A finished transfer reads
 *       as one that moved everything it said it did.
 */
static uint16_t s_progress[PRAET_ENGINE_CHANNELS];

void praet_engine_arm(const PraetScenario *scenario)
{
    s_scenario = scenario;
    s_step_index = 0u;
    s_tick = 0u;
    s_binding = NULL;

    for (uint8_t channel = 0u; channel < PRAET_ENGINE_CHANNELS; channel++)
    {
        s_settled_at[channel] = 0u;
        s_deferred_completions[channel] = 0u;
        s_deferred_moved[channel] = 0u;
        s_due_at[channel] = 0u;
        s_progress[channel] = 0u;
    }

    s_tally.completions = 0u;
    s_tally.deferred = 0u;
    s_tally.settling = 0u;
    s_tally.overrun = 0u;
}

PraetEngineTally praet_engine_tally(void)
{
    PraetEngineTally out = s_tally;

    out.deferred = 0u;
    for (uint8_t channel = 0u; channel < PRAET_ENGINE_CHANNELS; channel++)
    {
        out.deferred = (uint16_t)(out.deferred + s_deferred_completions[channel]);
    }
    return out;
}

uint16_t praet_engine_progress(uint8_t channel)
{
    if (channel >= PRAET_ENGINE_CHANNELS)
    {
        return 0u;
    }
    return s_progress[channel];
}

/**
 * @brief The port's answer to how far a channel has got.
 *
 * @param[in] channel Channel to read.
 * @return            Bytes moved so far, as the script last reported them.
 * @note praet_ordo.h declares this and the orchestrator calls it. Its definition here makes
 *       this file the port for that hook as well as for the four in memoriam_praetereo.h.
 */
uint16_t praet_hw_progress(embed_word channel)
{
    return praet_engine_progress((uint8_t)channel);
}

/**
 * @brief Takes the reaction that answers @p hook, or NULL where the script ran out.
 *
 * @param[in] hook Hook under answer, as a PraetHook.
 * @return         The reaction, or NULL where the script ran out or answers a different hook.
 * @note Advances the tick, because every hook call is one regardless of whether the script had an answer.
 * @note A reaction scripted for another hook comes back NULL and raises overrun. A script out of
 *       step with the program is a defect in the fixture. A read of it as a refusal would hide
 *       that.
 */
static const PraetEngineStep *praet_engine_take(uint8_t hook)
{
    s_tick++;

    if ((s_scenario == NULL) || (s_step_index >= s_scenario->step_count))
    {
        s_tally.overrun++;
        return NULL;
    }

    const PraetEngineStep *const step = &s_scenario->steps[s_step_index];

    if (step->hook != hook)
    {
        s_tally.overrun++;
        return NULL;
    }
    s_step_index++;
    return step;
}

/**
 * @brief Delivers one completion to the registered callback.
 *
 * @param[in] channel Logical channel the completion reports.
 * @param[in] moved   Bytes the completion reports.
 * @note Counted here, outside the callback. A case whose callback does nothing still
 *       produces a tally.
 * @note data stays null. The correctness arm reads the counts and the channel order. A pointer
 *       into the engine's own storage would be a fact about this engine and not about the
 *       library.
 */
static void praet_engine_report(uint8_t channel, uint16_t moved)
{
    s_tally.completions++;

    // A finished transfer moved what it reported, and so the progress figure ends where the completion
    // says it does. Written before the callback runs, since the callback reads it
    if (channel < PRAET_ENGINE_CHANNELS)
    {
        s_progress[channel] = moved;
    }

    if ((s_binding == NULL) || (s_binding->callback == NULL))
    {
        return;
    }

    const mmgr_praet_event event = {
        .data = NULL,
        .completion_ms = 0u,
        .completion_us = 0u,
        .bytes = moved,
        .sequence = s_tally.completions,
        .channel = channel,
        .peripheral = 0u,
        .direction = 0u,
    };

    s_binding->callback(&event, s_binding->user);
}

/**
 * @brief Releases the completions deferred on one channel, where they are due.
 *
 * @param[in] channel Channel to release.
 * @note A channel whose cycle did not elapse keeps its completions. A busy timer reports this
 *       while the transfer is still active.
 */
static void praet_engine_release_one(uint8_t channel)
{
    if (s_tick < s_due_at[channel])
    {
        return;
    }
    while (s_deferred_completions[channel] != 0u)
    {
        s_deferred_completions[channel]--;
        praet_engine_report(channel, s_deferred_moved[channel]);
    }
}

/**
 * @brief Delivers every due completion, on the interrupt arm alone.
 *
 * @note Runs at the end of every hook call. The interrupt arm reports with no poll. A
 *       completion that comes due during a close or during a submit on another channel arrives there.
 *       The software arm has no vector and reports only when polled.
 */
static void praet_engine_service_interrupt(void)
{
    if ((s_scenario == NULL) || (s_scenario->arm != (uint8_t)PRAET_ARM_INTERRUPT))
    {
        return;
    }
    for (uint8_t channel = 0u; channel < PRAET_ENGINE_CHANNELS; channel++)
    {
        praet_engine_release_one(channel);
    }
}

/**
 * @brief Runs a reaction's completions, either now or deferred on its channel.
 *
 * @param[in] step    Reaction the engine accepted [BORROWS].
 * @param[in] channel Logical channel the call named.
 * @note A refused reaction never reaches this. A refusal with a completion report is not a state
 *       hardware produces, and so the engine does not offer it.
 * @note A channel at or above PRAET_ENGINE_CHANNELS has nowhere to wait, and so it raises overrun.
 *       The scenario named a channel the fixture cannot schedule.
 */
static void praet_engine_schedule(const PraetEngineStep *step, uint8_t channel)
{
    if (step->complete_when == (uint8_t)PRAET_COMPLETE_INSIDE_CALL)
    {
        for (uint8_t sent = 0u; sent < step->completions; sent++)
        {
            praet_engine_report(channel, step->moved);
        }
        return;
    }

    if (step->complete_when != (uint8_t)PRAET_COMPLETE_WHEN_DUE)
    {
        return;
    }
    if (channel >= PRAET_ENGINE_CHANNELS)
    {
        s_tally.overrun++;
        return;
    }

    // One byte count and one due tick per channel. A second hold against a channel that is still
    // holds one would silently restate the first one's. The scenario releases before it holds again
    if (s_deferred_completions[channel] != 0u)
    {
        s_tally.overrun++;
        return;
    }

    s_deferred_completions[channel] = step->completions;
    s_deferred_moved[channel] = step->moved;
    s_due_at[channel] = (uint16_t)(s_tick + step->cycle_ticks);
}

/**
 * @brief Releases whatever a poll reaction named, where it is due.
 *
 * @param[in] selector Channel to release, or PRAET_RELEASE_EVERY_CHANNEL for all of them.
 * @note Every-channel releases lowest first. A scenario that cares about the order names its
 *       channels one poll at a time instead.
 */
static void praet_engine_release(uint8_t selector)
{
    if (selector == PRAET_RELEASE_EVERY_CHANNEL)
    {
        for (uint8_t channel = 0u; channel < PRAET_ENGINE_CHANNELS; channel++)
        {
            praet_engine_release_one(channel);
        }
        return;
    }
    if (selector >= PRAET_ENGINE_CHANNELS)
    {
        s_tally.overrun++;
        return;
    }
    praet_engine_release_one(selector);
}

embed_bool mmgr_praet_hw_open(const PraetCfg *args)
{
    const PraetEngineStep *const step = praet_engine_take((uint8_t)PRAET_HOOK_OPEN);

    if ((step == NULL) || (step->accepted == 0u))
    {
        praet_engine_service_interrupt();
        return EMBED_FALSE;
    }

    s_binding = args->on_complete;
    if (args->channel < PRAET_ENGINE_CHANNELS)
    {
        s_settled_at[args->channel] = (uint16_t)(s_tick + step->settle_ticks);
    }
    praet_engine_schedule(step, args->channel);
    praet_engine_service_interrupt();
    return EMBED_TRUE;
}

embed_bool mmgr_praet_hw_tx_submit(const PraetTransferCfg *args)
{
    const PraetEngineStep *const step = praet_engine_take((uint8_t)PRAET_HOOK_SUBMIT);

    if ((step == NULL) || (step->accepted == 0u))
    {
        praet_engine_service_interrupt();
        return EMBED_FALSE;
    }

    // A channel still in its settle takes no transfer, whatever the script answered. The settle is a rule
    // the engine enforces up front, the same way a burst constraint would be
    if ((args->channel < PRAET_ENGINE_CHANNELS) && (s_tick < s_settled_at[args->channel]))
    {
        s_tally.settling++;
        praet_engine_service_interrupt();
        return EMBED_FALSE;
    }

    praet_engine_schedule(step, args->channel);
    praet_engine_service_interrupt();
    return EMBED_TRUE;
}

void mmgr_praet_hw_close(const PraetTransferCfg *args)
{
    const PraetEngineStep *const step = praet_engine_take((uint8_t)PRAET_HOOK_CLOSE);

    if (step != NULL)
    {
        praet_engine_schedule(step, args->channel);
    }
    praet_engine_service_interrupt();
}

void mmgr_praet_hw_poll(const PraetCfg *args)
{
    const PraetEngineStep *const step = praet_engine_take((uint8_t)PRAET_HOOK_POLL);

    if (step == NULL)
    {
        praet_engine_service_interrupt();
        return;
    }
    // How far the polled channel got, as a controller's rest count would say. Written before
    // any completion comes out. A poll that both reports progress and finishes the transfer
    // leaves the completion's own count in place
    if ((step->progress != 0u) && (args->channel < PRAET_ENGINE_CHANNELS))
    {
        s_progress[args->channel] = step->progress;
    }

    praet_engine_schedule(step, args->channel);

    if (s_scenario->arm == (uint8_t)PRAET_ARM_SOFTWARE)
    {
        praet_engine_release(step->channel);
        return;
    }
    praet_engine_service_interrupt();
}
