/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_schedule.c
 * @brief Attach, detach, submit, the watchdog, and the reader/setter that is the only thing touching
 *        the two volatiles.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note No atomics anywhere. Exclusion comes from the lock the reader/setter takes, and from the
 *       interrupt being structurally unable to take it - the interrupt only ever raises set.
 * @note Nothing here predicts how long a transfer takes. The port kicks the watchdog while the
 *       engine is moving and reports completion when it happens, and an unkicked window says the
 *       channel stopped rather than that it finished.
 * @warning Included by test_praet_correctness.c rather than compiled on its own. mmgr_add_suite
 *          builds the one suite source and the shared files under test/support.
 */
#include "praet_schedule.h"

/**
 * @brief Every bit of a flag word except the ones in @p bits_.
 *
 * @param[in] bits_ Bits to clear.
 * @return          The complement, at the flag word's width.
 * @note The integer promotions run the complement at int width, so the result arrives as a negative
 *       int on its way back into an unsigned flag word. The outer cast puts it at the width these
 *       masks work at.
 */
#define PRAET_WITHOUT(bits_) ((uint32_t) ~(uint32_t)(bits_))

/**
 * @brief Statuses a transfer leaves behind, cleared when the next one starts.
 *
 * @note Named once. A submit that cleared a status this list forgot would carry the last transfer's
 *       verdict into the next one.
 */
#define PRAET_TRANSFER_STATUSES (PRAET_ERROR | PRAET_STALLED | PRAET_ABANDONED | PRAET_SCRUBBED | PRAET_MEASURED)

/**
 * @brief Every bit the entries below take out of a flag word they are keeping.
 *
 * @note The union of what submit, kick, completed, resolve and the settle clear in service mask away.
 *       Those five keep the rest of the word, so the two assertions under this settle at compile time
 *       what no run could: a clear cannot reach a bit outside the map, and cannot reach the region
 *       descriptor at all.
 * @note The detach path in service is not in this list, because it does not clear a word - it assigns
 *       PRAET_DETACHED over the whole one. Dropping the region with it is the point: a detached
 *       channel reaches no memory, and the next attach states the region again.
 */
#define PRAET_CLEARED_BITS (PRAET_CORE_MASK | PRAET_TRANSFER_STATUSES | PRAET_SETTLING)

EMBED_STATIC_ASSERT((PRAET_TRANSFER_STATUSES & PRAET_EVERY_STATUS) == PRAET_TRANSFER_STATUSES,
                    "PRAET_TRANSFER_STATUSES carries a bit that is not a status");

// The region descriptor was settled where the pool was declared, and no entry here has business
// touching it. This is the compile-time half of that: nothing below can clear it, whatever it writes
EMBED_STATIC_ASSERT((PRAET_CLEARED_BITS & PRAET_REGION_MASK) == 0u,
                    "an entry clears a bit that belongs to the region descriptor");

EMBED_STATIC_ASSERT((PRAET_CLEARED_BITS & (uint32_t)~PRAET_MAP_MASK) == 0u,
                    "an entry clears a bit the map does not account for");

/**
 * @brief Writes one channel's flag word, and is the only thing here that does.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel to write.
 * @param[in]     now     The word to leave behind.
 * @note Every entry that changes a channel's state goes through this. That is what lets the
 *       examination arm record a complete picture rather than the sites somebody remembered to
 *       instrument, and it is the same reason the whole access control ended up in one function.
 * @note A word that did not change is not written and is not recorded. Recording it would say a
 *       transition happened where nothing moved.
 * @warning praet_schedule_reset writes directly and does not come through here. Clearing a context
 *          back to its declared state is the fixture starting over, and counting that as a run of
 *          transitions would fill the table with one set per case.
 */
static void praet_schedule_write_flags(PraetSchedule *context, embed_word channel, uint32_t now)
{
    const uint32_t was = context->flags[channel];

    if (now == was)
    {
        return;
    }

    praet_examine_transition(channel, was, now);
    context->flags[channel] = now;
}

void praet_schedule_reset(PraetSchedule *context)
{
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        context->flags[channel] = (uint32_t)PRAET_DETACHED;
        context->keepalive_deadline[channel] = 0u;
#if PRAET_RECOVERY
        context->bound[channel] = NULL;
        context->bound_bytes[channel] = 0u;
        context->start[channel] = NULL;
        context->length[channel] = 0u;
        context->position[channel] = 0u;
        context->boundary_crc[channel] = 0u;
#endif
    }
    context->settle_deadline = 0u;
    context->elapsed_micros = 0u;
    context->praet_set_bitflag = 0u;
    context->praet_busy_bitflag = 0u;
}

embed_bool praet_schedule_attach(PraetSchedule *context, embed_word channel, uint8_t *bound, embed_word bytes,
                                 embed_word region)
{
    if (channel >= PRAET_CHANNELS)
    {
        return EMBED_FALSE;
    }
    if ((context->flags[channel] & PRAET_CORE_MASK) != PRAET_DETACHED)
    {
        return EMBED_FALSE;
    }

    // The engine settles once, when it first comes up. A later attach takes what is left of that
    // deadline rather than restarting it, since the engine is already up by then
    const embed_word deadline = context->elapsed_micros + (embed_word)PRAET_SETTLE_MICROS;

    if (deadline > context->settle_deadline)
    {
        context->settle_deadline = deadline;
    }

    uint32_t now = (uint32_t)PRAET_ATTACHED | (uint32_t)PRAET_CLAIMED;

    if (context->elapsed_micros < context->settle_deadline)
    {
        now |= (uint32_t)PRAET_SETTLING;
    }

    // The field macro widens to the flag word before the shift, which is what a 16-bit embed_word
    // needs - the region byte would otherwise shift off the top of its own type and land as zero
    now |= PRAET_REGION_FIELD(region);

    praet_schedule_write_flags(context, channel, now);
#if PRAET_RECOVERY
    context->bound[channel] = bound;
    context->bound_bytes[channel] = bytes;
#else
    // A build with no recovery records nothing about a transfer, so it has nowhere to keep these and
    // nothing that would read them. The pool was still named at the attach, and the compiler still
    // proved it exists
    (void)bound;
    (void)bytes;
#endif
    context->praet_set_bitflag = 1u;
    return EMBED_TRUE;
}

void praet_schedule_detach(PraetSchedule *context, embed_word channel)
{
    if (channel >= PRAET_CHANNELS)
    {
        return;
    }
    if ((context->flags[channel] & PRAET_CORE_MASK) == PRAET_DETACHED)
    {
        return;
    }

    praet_schedule_write_flags(context, channel, context->flags[channel] | (uint32_t)PRAET_DETACHING);
    context->praet_set_bitflag = 1u;
}

#if PRAET_RECOVERY
embed_bool praet_schedule_submit(PraetSchedule *context, embed_word channel, embed_word offset, embed_word length)
#else
embed_bool praet_schedule_submit(PraetSchedule *context, embed_word channel)
#endif
{
    if (channel >= PRAET_CHANNELS)
    {
        return EMBED_FALSE;
    }

    const uint32_t was = context->flags[channel];
    const uint32_t core = was & PRAET_CORE_MASK;

    if ((core != PRAET_ATTACHED) && (core != PRAET_OK))
    {
        return EMBED_FALSE;
    }
    if ((was & (uint32_t)(PRAET_SETTLING | PRAET_DETACHING)) != 0u)
    {
        return EMBED_FALSE;
    }

    // The region descriptor rides along untouched. Which memory a channel reaches was settled where
    // the pool was declared, and a submit has no business restating it
    praet_schedule_write_flags(context, channel,
                               (was & PRAET_WITHOUT(PRAET_CORE_MASK | PRAET_TRANSFER_STATUSES)) |
                                   (uint32_t)PRAET_BUSY);
    context->keepalive_deadline[channel] = context->elapsed_micros + (embed_word)PRAET_KEEPALIVE_MICROS;
#if PRAET_RECOVERY
    // The address comes from what the channel was attached over. A submit states how far into it the
    // transfer starts, and PraetSubmit is what proved that offset and length fit before this ran
    context->start[channel] = context->bound[channel] + offset;
    context->length[channel] = length;
    context->position[channel] = 0u;
#endif
    context->praet_set_bitflag = 1u;
    return EMBED_TRUE;
}

#if PRAET_RECOVERY
void praet_schedule_kick(PraetSchedule *context, embed_word channel, embed_word position)
#else
void praet_schedule_kick(PraetSchedule *context, embed_word channel)
#endif
{
    if (channel >= PRAET_CHANNELS)
    {
        return;
    }
    if ((context->flags[channel] & PRAET_CORE_MASK) != PRAET_BUSY)
    {
        return;
    }

    // A channel that is moving again is not stalled, so the status comes off with the same kick that
    // pushes the window out
    praet_schedule_write_flags(context, channel, context->flags[channel] & PRAET_WITHOUT(PRAET_STALLED));
    context->keepalive_deadline[channel] = context->elapsed_micros + (embed_word)PRAET_KEEPALIVE_MICROS;

#if PRAET_RECOVERY
    // A position that went backwards is the port reporting nonsense, and taking it would shrink the
    // extent a backout has to cover
    if (position > context->position[channel])
    {
        context->position[channel] = (position > context->length[channel]) ? context->length[channel] : position;
    }
#endif
    context->praet_set_bitflag = 1u;
}

void praet_schedule_completed(PraetSchedule *context, embed_word channel, embed_bool failed)
{
    if (channel >= PRAET_CHANNELS)
    {
        return;
    }
    if ((context->flags[channel] & PRAET_CORE_MASK) != PRAET_BUSY)
    {
        return;
    }

    uint32_t now = context->flags[channel] & PRAET_WITHOUT(PRAET_CORE_MASK | PRAET_STALLED);

    now |= (uint32_t)PRAET_OK;
    if (failed != EMBED_FALSE)
    {
        now |= (uint32_t)PRAET_ERROR;
    }
#if PRAET_RECOVERY
    else
    {
        // A transfer that finished moved everything it was asked to, whatever the last kick sampled
        context->position[channel] = context->length[channel];
    }
#endif

    praet_schedule_write_flags(context, channel, now);
    context->keepalive_deadline[channel] = 0u;
    context->praet_set_bitflag = 1u;
}

#if PRAET_RECOVERY

/**
 * @brief The CRC-32 polynomial, reflected.
 *
 * @note The ordinary one, so a caller checking this against a checksum of their source can use any
 *       CRC-32 they already have rather than one of ours.
 */
#define PRAET_CRC_POLYNOMIAL 0xEDB88320u

/**
 * @brief Checksums the word a stalled transfer was in the middle of.
 *
 * @param[in] context Context the channel belongs to [BORROWS].
 * @param[in] channel Channel to measure.
 * @return            The checksum, or zero where the sample landed on a word boundary.
 * @note One word, bit at a time. A table would be a kilobyte in the image to save eight iterations on
 *       a path that runs once per stalled transfer.
 * @note Reads the caller's storage and writes none of it.
 */
static uint32_t praet_boundary_crc(const PraetSchedule *context, embed_word channel)
{
    const embed_word word_bytes = (embed_word)sizeof(embed_word);
    const embed_word sampled = context->position[channel];
    const embed_word remainder = sampled % word_bytes;

    // A sample already on a boundary has no partial word under it, so there is nothing to measure and
    // the word boundary was already the exact answer
    if (remainder == 0u)
    {
        return 0u;
    }

    const embed_word first = sampled - remainder;
    const embed_word length = context->length[channel];
    const embed_word last = ((first + word_bytes) > length) ? length : (first + word_bytes);
    const uint8_t *const bytes = context->start[channel];

    uint32_t running = 0xFFFFFFFFu;

    for (embed_word walk = first; walk < last; walk++)
    {
        running ^= (uint32_t)bytes[walk];

        for (unsigned turn = 0u; turn < 8u; turn++)
        {
            // The low bit spread across the whole word, so the polynomial is taken or not without a
            // branch. Eight branches per byte on a recovery path is eight chances to mispredict
            const uint32_t taken = (uint32_t)0u - (running & 1u);

            running = (running >> 1) ^ (PRAET_CRC_POLYNOMIAL & taken);
        }
    }

    return running ^ 0xFFFFFFFFu;
}

embed_bool praet_schedule_resolve(PraetSchedule *context, embed_word channel, embed_word recovery)
{
    if (channel >= PRAET_CHANNELS)
    {
        return EMBED_FALSE;
    }

    const uint32_t was = context->flags[channel];

    if ((was & (uint32_t)PRAET_STALLED) == 0u)
    {
        return EMBED_FALSE;
    }

    uint32_t now = was & PRAET_WITHOUT(PRAET_CORE_MASK | PRAET_STALLED);

    now |= (uint32_t)PRAET_ATTACHED;
    now |= (recovery == (embed_word)PRAET_RECOVER_ZERO) ? (uint32_t)PRAET_SCRUBBED : (uint32_t)PRAET_ABANDONED;

    // The one optional branch, and the only place it appears. Everything before the sample is written
    // and everything past the rounded boundary is not, so the whole ambiguity is the word the engine
    // was inside - and a context that answered no at its declaration folds this away entirely
    if (context->word_boundary_crc != EMBED_FALSE)
    {
        const embed_word word_bytes = (embed_word)sizeof(embed_word);

        if ((context->position[channel] % word_bytes) != 0u)
        {
            context->boundary_crc[channel] = praet_boundary_crc(context, channel);
            now |= (uint32_t)PRAET_MEASURED;
        }
    }

    praet_schedule_write_flags(context, channel, now);
    context->keepalive_deadline[channel] = 0u;
    context->praet_set_bitflag = 1u;
    return EMBED_TRUE;
}
#endif

void praet_schedule_advance(PraetSchedule *context, embed_word micros)
{
    context->elapsed_micros += micros;
    context->praet_set_bitflag = 1u;
}

void praet_schedule_advance_ticks(PraetSchedule *context, embed_word ticks)
{
    praet_schedule_advance(context, PRAET_CLOCK_MICROS(ticks));
}

void praet_schedule_raise(PraetSchedule *context)
{
    context->praet_set_bitflag = 1u;
}

/**
 * @brief Asks the port how far every running channel has got, and records what it says.
 *
 * @param[in,out] context Context to update [BORROWS].
 * @return                EMBED_TRUE where any channel moved.
 * @note Runs ahead of the short circuit in praet_schedule_poll, because it is what can make the
 *       answer to "did anything happen" true. A part with no interrupt to raise the set volatile has
 *       nothing else that could, and a poll that short circuited before asking would never learn the
 *       engine had moved - measured, on the arm where progress is reported and no interrupt exists.
 * @note Clears a stall and pushes the keepalive window for a channel that moved, since a channel
 *       reporting progress is a channel that has not stopped.
 */
static embed_bool praet_schedule_take_progress(PraetSchedule *context)
{
    embed_bool moved = EMBED_FALSE;

#if PRAET_RECOVERY
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        if ((context->flags[channel] & PRAET_CORE_MASK) != PRAET_BUSY)
        {
            continue;
        }

        const embed_word reached = (embed_word)praet_hw_progress(channel);

        // A figure that went backwards is the port reporting nonsense, and taking it would shrink the
        // extent a backout has to cover
        if (reached <= context->position[channel])
        {
            continue;
        }

        context->position[channel] = (reached > context->length[channel]) ? context->length[channel] : reached;
        context->keepalive_deadline[channel] = context->elapsed_micros + (embed_word)PRAET_KEEPALIVE_MICROS;
        praet_schedule_write_flags(context, channel, context->flags[channel] & PRAET_WITHOUT(PRAET_STALLED));
        moved = EMBED_TRUE;
    }
#else
    (void)context;
#endif

    return moved;
}

void praet_schedule_poll(PraetSchedule *context)
{
    // Taking the lock is what makes this ignore the interrupt for the span below. A nested call finds
    // it held and declines rather than reworking state the outer call is partway through
    if (context->praet_busy_bitflag != 0u)
    {
        return;
    }
    context->praet_busy_bitflag = 1u;

    const embed_bool moved = praet_schedule_take_progress(context);

    // The short circuit, and it is still here. What changed is that the port is asked first, so a
    // channel that moved is one of the things that can make this false. Nothing happened means no
    // interrupt raised anything and the port reports no movement, and the walk below would then write
    // every flag word back exactly as it found it
    if ((context->praet_set_bitflag == 0u) && (moved == EMBED_FALSE))
    {
        context->praet_busy_bitflag = 0u;
        return;
    }

    // Lifted out of the walk. One deadline serves the whole context, so asking whether it has elapsed
    // once and reading the answer per channel replaces a comparison per channel with a load
    const embed_bool settled = (context->elapsed_micros >= context->settle_deadline) ? EMBED_TRUE : EMBED_FALSE;

    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        const uint32_t was = context->flags[channel];
        uint32_t now = was;

        if ((now & PRAET_CORE_MASK) == PRAET_DETACHED)
        {
            continue;
        }

        // The bit comes first in both tests below, so a channel that is not settling and a channel
        // that is not running each cost one mask and nothing else. A timer nothing is waiting on is
        // never compared against
        if (((now & (uint32_t)PRAET_SETTLING) != 0u) && (settled != EMBED_FALSE))
        {
            now &= PRAET_WITHOUT(PRAET_SETTLING);
        }

        // The watchdog says the engine stopped moving. It never says the transfer finished, so the
        // core stays busy and the caller decides what to do about bytes whose state is now unknown
        if (((now & PRAET_CORE_MASK) == PRAET_BUSY) &&
            (context->elapsed_micros >= context->keepalive_deadline[channel]))
        {
            now |= (uint32_t)PRAET_STALLED;
        }

        // A detach finishes only once the transfer under it has. Tearing down while the engine is
        // still moving bytes would leave it writing into storage the caller believes is released
        if (((now & (uint32_t)PRAET_DETACHING) != 0u) && ((now & PRAET_CORE_MASK) != PRAET_BUSY))
        {
            now = (uint32_t)PRAET_DETACHED;
            context->keepalive_deadline[channel] = 0u;
#if PRAET_RECOVERY
            context->bound[channel] = NULL;
            context->bound_bytes[channel] = 0u;
            context->start[channel] = NULL;
            context->length[channel] = 0u;
            context->position[channel] = 0u;
            context->boundary_crc[channel] = 0u;
#endif
        }

        praet_schedule_write_flags(context, channel, now);
    }

    // Both volatiles come down together. A raise that landed during the span above is dropped, and
    // nothing is lost by it: this recomputes every channel from what it can see rather than consuming
    // an event, so the next call reaches the same answer from the same state
    context->praet_set_bitflag = 0u;
    context->praet_busy_bitflag = 0u;
}

uint32_t praet_schedule_flags(const PraetSchedule *context, embed_word channel)
{
    if (channel >= PRAET_CHANNELS)
    {
        return (uint32_t)PRAET_DETACHED;
    }
    return context->flags[channel];
}

#if PRAET_RECOVERY
embed_word praet_schedule_position(const PraetSchedule *context, embed_word channel)
{
    if (channel >= PRAET_CHANNELS)
    {
        return 0u;
    }
    return context->position[channel];
}

embed_word praet_schedule_touched(const PraetSchedule *context, embed_word channel)
{
    if (channel >= PRAET_CHANNELS)
    {
        return 0u;
    }

    const embed_word word_bytes = (embed_word)sizeof(embed_word);
    const embed_word sampled = context->position[channel];
    const embed_word length = context->length[channel];

    // The sample can be up to one word behind what the engine wrote, so round up to the word it was
    // in the middle of. A caller zeroing only as far as the sample would leave a partial word
    const embed_word remainder = sampled % word_bytes;

    if (remainder == 0u)
    {
        return sampled;
    }

    const embed_word needed = word_bytes - remainder;
    const embed_word headroom = length - sampled;

    // Compared against the headroom rather than added and clamped afterwards. A length near the top
    // of the word wraps the addition, and the clamp then reads the wrapped value as a small number
    if (needed >= headroom)
    {
        return length;
    }

    // The addition runs at int width and comes back narrower. The test above is what makes it safe:
    // it only reaches here where the length has room for another whole word
    return (embed_word)(sampled + needed);
}

uint32_t praet_schedule_boundary_crc(const PraetSchedule *context, embed_word channel)
{
    if (channel >= PRAET_CHANNELS)
    {
        return 0u;
    }
    return context->boundary_crc[channel];
}
#endif
