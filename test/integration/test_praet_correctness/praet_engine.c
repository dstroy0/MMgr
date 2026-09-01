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
 * @note Every symbol the library declares weak is defined here strongly, so the linker takes these
 *       over the refusing defaults in memoriam_praetereo.c. That is the same seam a board support
 *       file uses, and it holds with link-time optimization on.
 * @note One engine schedules every logical channel. A completion is held against the channel that
 *       earned it and comes out when its cycle has elapsed and the arm lets it, so the order
 *       completions arrive in is the scenario's to choose and need not match the order submitted.
 * @note One hook call is one tick. Settling and cycle time are both counted in them, which keeps the
 *       whole fixture deterministic and free of a clock.
 * @warning Included by test_praet_correctness.c rather than compiled on its own. mmgr_add_suite
 *          compiles the one suite source and the shared files under test/support, and these hooks
 *          must not reach any other suite - test_memoriam_praetereo exists to prove the weak
 *          defaults refuse, and a strong definition linked into it would answer for them.
 */
#include "praet_engine.h"

/**
 * @brief Scenario the engine is currently running.
 *
 * @note Null until praet_engine_arm runs. Every hook refuses against a null scenario, which is what
 *       an unarmed engine should look like.
 */
static const PraetScenario *s_scenario;

/**
 * @brief Index of the reaction the next hook call consumes.
 */
static uint16_t s_step_index;

/**
 * @brief Hook calls made since the scenario was armed.
 */
static uint16_t s_tick;

/**
 * @brief What the run has produced so far.
 */
static PraetEngineTally s_tally;

/**
 * @brief Completion callback the last accepted open registered.
 *
 * @note Held rather than copied, as mmgr_praet_open documents. The case owns the binding and keeps
 *       it alive for the whole scenario [BORROWS].
 */
static const PraetCallbackCfg *s_binding;

/**
 * @brief Tick at which each channel finishes settling.
 */
static uint16_t s_settled_at[PRAET_ENGINE_CHANNELS];

/**
 * @brief Completions waiting against each logical channel.
 */
static uint8_t s_held_completions[PRAET_ENGINE_CHANNELS];

/**
 * @brief Bytes each held completion reports, per logical channel.
 */
static uint16_t s_held_moved[PRAET_ENGINE_CHANNELS];

/**
 * @brief Tick at which each channel's held completions come due.
 */
static uint16_t s_due_at[PRAET_ENGINE_CHANNELS];

/**
 * @brief Bytes each channel has moved, as the engine last reported them.
 *
 * @note What a controller's remaining count stands for here. A poll reaction carrying a progress
 *       figure writes it, and a completion writes the count it reported, so a finished transfer reads
 *       as having moved everything it said it did.
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
        s_held_completions[channel] = 0u;
        s_held_moved[channel] = 0u;
        s_due_at[channel] = 0u;
        s_progress[channel] = 0u;
    }

    s_tally.completions = 0u;
    s_tally.held = 0u;
    s_tally.settling = 0u;
    s_tally.overrun = 0u;
}

PraetEngineTally praet_engine_tally(void)
{
    PraetEngineTally out = s_tally;

    out.held = 0u;
    for (uint8_t channel = 0u; channel < PRAET_ENGINE_CHANNELS; channel++)
    {
        out.held = (uint16_t)(out.held + s_held_completions[channel]);
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
 * @note praet_schedule.h declares this and the orchestrator calls it. Defining it here is what makes
 *       this file the port for that hook as well as for the four in memoriam_praetereo.h.
 */
uint16_t praet_hw_progress(embed_word channel)
{
    return praet_engine_progress((uint8_t)channel);
}

/**
 * @brief Takes the reaction answering @p hook, or NULL where the script has run out.
 *
 * @param[in] hook Hook being answered, as a PraetHook.
 * @return         The reaction, or NULL where the script is spent or answers a different hook.
 * @note Advances the tick, because every hook call is one whether or not the script had an answer.
 * @note A reaction scripted for another hook comes back NULL and raises overrun. A script that has
 *       drifted out of step with the program is a defect in the fixture, and reading it as a refusal
 *       would hide that.
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
 * @note Counted here rather than in the callback, so a case whose callback does nothing still
 *       produces a tally.
 * @note data is left null. The correctness arm reads the counts and the channel order, and a pointer
 *       into the engine's own storage would be a fact about this engine instead of about the
 *       library.
 */
static void praet_engine_report(uint8_t channel, uint16_t moved)
{
    s_tally.completions++;

    // A finished transfer moved what it reported, so the progress figure ends where the completion
    // says it does. Written before the callback runs, since the callback is what reads it
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
 * @brief Releases the completions held against one channel, where they are due.
 *
 * @param[in] channel Channel to release.
 * @note A channel whose cycle has not elapsed keeps its completions. That is what a busy timer
 *       reports when the transfer is still running.
 */
static void praet_engine_release_one(uint8_t channel)
{
    if (s_tick < s_due_at[channel])
    {
        return;
    }
    while (s_held_completions[channel] != 0u)
    {
        s_held_completions[channel]--;
        praet_engine_report(channel, s_held_moved[channel]);
    }
}

/**
 * @brief Delivers every due completion, on the interrupt arm alone.
 *
 * @note Runs at the end of every hook call. The interrupt arm reports without being asked, so a
 *       completion coming due during a close or during a submit on another channel arrives there.
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
 * @brief Runs a reaction's completions, either now or held against its channel.
 *
 * @param[in] step    Reaction that was accepted [BORROWS].
 * @param[in] channel Logical channel the call named.
 * @note A refused reaction never reaches this. Refusing and reporting a completion is not a state
 *       hardware produces, so the engine does not offer it.
 * @note A channel at or above PRAET_ENGINE_CHANNELS has nowhere to be held, so it raises overrun.
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

    // One byte count and one due tick per channel, so a second hold against a channel that is still
    // holding would silently restate the first one's. The scenario releases before it holds again
    if (s_held_completions[channel] != 0u)
    {
        s_tally.overrun++;
        return;
    }

    s_held_completions[channel] = step->completions;
    s_held_moved[channel] = step->moved;
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

    // A channel still settling takes no transfer, whatever the script answered. Settling is a rule
    // the engine enforces rather than a reaction, the same way a burst constraint would be
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
    // How far the polled channel has got, as a controller's remaining count would say. Written before
    // any completion is released, so a poll that both reports progress and finishes the transfer
    // leaves the completion's own count standing
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
