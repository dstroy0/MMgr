/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_ordo.h
 * @brief The schedule context: one flag word per channel, the two volatiles that coordinate the
 *        interrupt, and the reader/setter that is the only thing touching either.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note A context exists only where DMA has to schedule internally. A channel is not a context.
 * @note Two separate concerns live here and must not be folded together. The quaternary core in the
 *       flag word is the channel's DMA state, where busy means the engine is moving bytes. The busy
 *       volatile below is the reader/setter's lock, and means it is mid-update.
 * @note Time is microseconds throughout. DMA timing is tight enough that a millisecond is not a unit
 *       anything here can be expressed in.
 */
#ifndef MMGR_TEST_PRAET_ORDO_H
#define MMGR_TEST_PRAET_ORDO_H

#include "memoriam_praetereo/memoriam_praetereo.h"

// PRAET_CHANNELS, PRAET_SETTLE_MICROS, PRAET_KEEPALIVE_MICROS and PRAET_RECOVERY come from here. None
// has a value that is right for every part, so an unset one takes a default and raises a warning
// naming itself. What stops the build is praet_iudex.h, below, once all of them have spoken
#include "praet_praefinitum.h"

// Every bit of the flag word, and the assertions that keep the map from drifting. Kept apart from the
// entries because the map is one thing and it is the same on every build - a status a build never
// sets still owns its bit
#include "praet_tabula_vexillorum.h"

// Where the microseconds come from, and what a tick is worth against them. Every deadline below is
// microseconds, so nothing here means anything without it
#include "praet_horologiorum_custos.h"

// Last, and after every knob has reported. This is the one place a configuration stops the build, so
// that a build missing several knobs hears about all of them rather than the first
#include "praet_iudex.h"

// The examination arm's recorder. Expands to nothing unless a build asked for it, and the one
// function that writes a flag word is what calls into it
#include "praet_procurator.h"

EMBED_BEGIN_DECLS

#if PRAET_RECOVERY

/**
 * @brief Recovery that backs the transfer out and leaves the bytes as they are.
 *
 * @note What you want where the destination is about to be overwritten anyway, or where a partial
 *       result is still worth reading.
 */
#define PRAET_RESTITUERE_ET_REDINTEGRARE 0u

/**
 * @brief Recovery that treats the transfer's bytes as unsafe.
 *
 * @note What you want where a half-written buffer would be read as a whole one. Which of the two is
 *       right depends on where in a program the stall happened, so it is the caller's call and not
 *       this library's.
 */
#define PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE 1u

#endif

/**
 * @brief The two answers a declaration may give about the boundary word check.
 *
 * @note Spelled at length on purpose. This is not a knob that rides along with recovery being on - it
 *       is its own deliberate yes or no, made once per context, and the name is meant to be
 *       impossible to skim past in a declaration.
 * @note Real enumerators rather than bare tokens, so a misspelling is an undeclared identifier at the
 *       declaration instead of quietly reading as the off arm.
 */
typedef enum
{
    AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE = 0,
    AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE = 1
} PraetRecoveryWordBoundaryBitwiseCrc;

/**
 * @brief Marks a declaration so that every use of it reports, carrying a message.
 *
 * @param[in] message_ Text the diagnostic reports, as a string literal.
 * @note Aliased under this module's prefix rather than spelled at the use sites, which is what
 *       embed_compiler_directives.h asks a consumer to do with an attribute.
 * @note The deprecated attribute rather than the error or warning one. Those two report at a call
 *       that survives compilation, and a declaration at file scope has no call in it - measured, both
 *       silent. This one reports at any use, and a typedef naming the type is a use.
 * @note Nothing marked with this is deprecated. It is the only attribute that reports from a
 *       declarator, and what it is being used for is to make a deliberate answer visible rather than
 *       to retire anything. The message says which answer, so the line reads correctly whatever GCC
 *       prefixes it with.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(deprecated) is 0. The answer is then silent
 *          and only a wrong token still fails, which is the half that cannot be missed.
 */
#if EMBED_HAS_ATTRIBUTE(deprecated)
#define PRAET_DENUNTIATIO_ATTR(message_) __attribute__((deprecated(message_)))
#else
#define PRAET_DENUNTIATIO_ATTR(message_)
#endif

/**
 * @brief A type that exists only for the AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE token.
 *
 * @note The declarator pastes the token onto this and typedefs the result, so a token that is neither
 *       of the two fails on an unknown type name that has the offending token in it. Pasting onto a
 *       macro instead gave a syntax error several lines down that named nothing - measured, on the
 *       misspelling this exists to catch.
 * @note That same typedef is what makes the answer report. Both arms carry the attribute, because a
 *       choice that only speaks up one way trains everyone to read its silence as the safe answer,
 *       and neither answer here is safe by default.
 */
typedef PRAET_DENUNTIATIO_ATTR("AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE - this context measures no boundary word, so a "
                            "recovery is exact to the word and no finer") unsigned char
    PraetCrcResponsum_AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE;

/**
 * @brief A type that exists only for the AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE token.
 */
typedef PRAET_DENUNTIATIO_ATTR("AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE - this context checksums the word a stalled "
                            "transfer was inside, one word, bit at a time, on the recovery path only") unsigned char
    PraetCrcResponsum_AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE;

/**
 * @brief Pastes @p token_ onto the answer type prefix.
 *
 * @param[in] token_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens.
 * @return           The answer type for that token.
 */
#define PRAET_CRC_RESPONSUM_IN_LOCO_FIGERE(token_) PraetCrcResponsum_##token_

/**
 * @brief The answer type @p token_ names, after expanding it.
 *
 * @param[in] token_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens, or a macro holding one.
 * @return           That token's answer type.
 * @note This is what refuses a plain 1 or TRUE in the answer's place. Those are values, and the two
 *       tokens are the only things that name a type here.
 */
#define PRAET_CRC_RESPONSUM(token_) PRAET_CRC_RESPONSUM_IN_LOCO_FIGERE(token_)

/**
 * @brief The value the AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE token stands for.
 *
 * @note Pasted onto rather than read, so the declarator gets the answer at preprocessing time and can
 *       assert on it.
 */
#define PRAET_CRC_VALUE_AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE 0

/**
 * @brief The value the AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE token stands for.
 */
#define PRAET_CRC_VALUE_AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE 1

/**
 * @brief Pastes @p token_ onto the value prefix.
 *
 * @param[in] token_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens.
 * @return           The value macro for that token.
 */
#define PRAET_CRC_VALUE_IN_LOCO_FIGERE(token_) PRAET_CRC_VALUE_##token_

/**
 * @brief The value @p token_ stands for, after expanding it.
 *
 * @param[in] token_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens, or a macro holding one.
 * @return           1 for the ENABLE token, 0 for the DISABLE token.
 * @note Usable in an #if, which is what lets a caller compile one thing or another around their own
 *       answer without keeping a second macro in step with it.
 */
#define PRAET_CRC_VALUE(token_) PRAET_CRC_VALUE_IN_LOCO_FIGERE(token_)

#if PRAET_RECOVERY

/**
 * @brief Storage the boundary word check needs, where a declaration asked for it.
 *
 * @param crc_choice_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens.
 * @return            An initializer for the members that carry the choice.
 */
#define PRAET_ORDINEM_INCIPE(crc_choice_) {.word_boundary_crc = PRAET_CRC_VALUE_##crc_choice_}

#else

/**
 * @brief An empty context, for a build with no recovery machinery to configure.
 *
 * @param crc_choice_ One of the two AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC tokens, unused here.
 * @return            A zero initializer.
 * @note The token is still demanded by the declarator and still reports. What changes is that there
 *       is no member to put it in, and the assertion there refuses the ENABLE arm outright.
 */
#define PRAET_ORDINEM_INCIPE(crc_choice_) {0}

#endif

/**
 * @brief Declares one schedule context and answers the boundary word question for it.
 *
 * @param name_       Name of the context this declares.
 * @param crc_choice_ AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE or
 *                    AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE.
 * @note The instantiation is the declaration. This emits initialized data carrying the answer, so
 *       nothing runs to make a context usable and the answer cannot be changed afterwards.
 * @note Reports whichever token it was given. A caller who wanted a quieter build takes the message
 *       away by not declaring a context, which is the only honest way out of it.
 * @warning A token that is neither of the two fails on the typedef, with the token it was given in
 *          the message. Asking for the check on a build with PRAET_RECOVERY off fails the assertion
 *          below, which names what to do about it.
 */
#define PraetOrdoContext(name_, crc_choice_)                                                                       \
    typedef PRAET_CRC_RESPONSUM(crc_choice_) name_##_boundary_word_answer;                                                \
    EMBED_STATIC_ASSERT(PRAET_RECOVERY || (PRAET_CRC_VALUE(crc_choice_) == 0),                                         \
                        "AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE needs PRAET_RECOVERY set to 1, since the word it "   \
                        "measures is the one a recovery is deciding about");                                           \
    static PraetOrdo name_ = PRAET_ORDINEM_INCIPE(crc_choice_)

/**
 * @brief Everything one context schedules over.
 *
 * @param flags              One uint32 per channel: quaternary core at the bottom, statuses above it,
 *                           region descriptor in the byte at PRAET_REGION_SHIFT. Plain, because the
 *                           two volatiles below are what coordinate against the interrupt. Fixed at
 *                           32 bits rather than embed_word, because the layout is a fixed set of bits
 *                           and an environment building at a 16-bit word has nowhere to put the
 *                           region byte.
 * @param bound              First byte of the memory each attached channel moves, as the pool named
 *                           at its attach [BORROWS]. Present only where PRAET_RECOVERY is on.
 * @param bound_bytes        How many bytes are at that address, as the pool's own extent said.
 *                           Present only where PRAET_RECOVERY is on.
 * @param start              First byte of each running channel's transfer, which is bound plus the
 *                           offset a submit named. The caller's storage, and the caller's to write -
 *                           nothing here writes through it [BORROWS]. Present only where
 *                           PRAET_RECOVERY is on.
 * @param length             Bytes each running channel was asked to move. Present only where
 *                           PRAET_RECOVERY is on.
 * @param position           Bytes each running channel has moved, as of its last kick. Present only
 *                           where PRAET_RECOVERY is on.
 * @param boundary_crc       Checksum of the word a recovery found the engine inside, where the
 *                           declaration asked for the check. Meaningful only where the channel reads
 *                           PRAET_MEASURED.
 * @param word_boundary_crc  What the declaration answered about the boundary word check. Written once
 *                           by PraetOrdoContext and never again, which is what lets the one
 *                           branch reading it fold away where the answer was no.
 * @param keepalive_deadline Microsecond mark each running channel must be kicked before.
 * @param settle_deadline    Microsecond mark the engine finishes settling at. One timer, because
 *                           settling is the engine coming up and not a property of a channel.
 * @param praet_set_bitflag  Raised by the interrupt, and by anything that changes what a service call
 *                           would find. Volatile, so it is not cached across the preemption.
 * @param praet_busy_bitflag The reader/setter's lock. Volatile, and held while it updates, which is
 *                           what makes it ignore the interrupt for that span.
 * @param elapsed_micros     Microseconds elapsed, as the caller advances them.
 * @note Both volatiles carry the module prefix. `set` and `busy` are words a caller is entitled to
 *       have taken for a macro of their own, and a macro over a member name breaks the struct at
 *       every use with a diagnostic that points nowhere near the cause.
 * @warning The context sits at an address the linker resolves and is written by offset from it.
 *          Nothing computes where a member lives, and it cannot be relocated or copied - its address
 *          is its identity.
 */
typedef struct
{
    uint32_t flags[PRAET_CHANNELS];
    embed_word keepalive_deadline[PRAET_CHANNELS];
#if PRAET_RECOVERY
    uint8_t *bound[PRAET_CHANNELS];
    embed_word bound_bytes[PRAET_CHANNELS];
    uint8_t *start[PRAET_CHANNELS];
    embed_word length[PRAET_CHANNELS];
    embed_word position[PRAET_CHANNELS];
    uint32_t boundary_crc[PRAET_CHANNELS];
    embed_bool word_boundary_crc;
#endif
    embed_word settle_deadline;
    volatile embed_word praet_set_bitflag;
    volatile embed_word praet_busy_bitflag;
    embed_word elapsed_micros;
} PraetOrdo;

/**
 * @brief Returns how far the port says @p channel has got, in bytes.
 *
 * @param[in] channel Channel to read.
 * @return            Bytes moved so far. Zero from a port with nothing to say.
 * @note The fifth port hook, and the one the four in memoriam_praetereo.h do not have. Every
 *       controller this library targets exposes a remaining count, and the orchestrator needs it to
 *       feed a watchdog with something a caller can act on.
 * @note Reached only from praet_ordo_poll. Nothing above it sees a byte count that has not been
 *       through the flag word first, which is what keeps how progress is tracked out of a caller's
 *       business.
 * @note Carries a weak refusing default, the same as the four hooks in src. It lives in
 *       test/support/praet_port_default.c and returns zero, and praet_ordo_take_progress treats
 *       zero as no movement. A build that supplies no port walks every channel and still marks a
 *       stalled one, because an unkicked keepalive window is what says a channel stopped.
 * @warning The default cannot sit in this suite's own translation unit. test_praet_correctness.c
 *          includes praet_engine.c, which defines this hook strongly, and a weak definition beside
 *          a strong one in a single unit is a duplicate. The separate file is what leaves the
 *          linker a choice to make.
 */
uint16_t praet_hw_progress(embed_word channel);

/**
 * @brief Clears a context back to every channel detached.
 *
 * @param[out] context Context to clear [BORROWS].
 * @note A declaration emits this as initialized data. The entry exists so one context can be reused.
 * @warning Leaves the boundary word answer alone. That came from the declaration and is not state a
 *          reset has any business changing.
 */
void praet_ordo_reset(PraetOrdo *context);

/**
 * @brief Attaches a channel over a block of bytes, claims its vector, and starts the settle timer.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel to attach.
 * @param[in]     bound   First byte of the memory this channel moves. The caller's [BORROWS].
 * @param[in]     bytes   How many bytes are there.
 * @param[in]     region  Region descriptor for this channel, as a byte.
 * @return                EMBED_TRUE where the channel was detached and is now attached.
 * @note Attach and detach are the only entry and exit, and configuration takes no part in either.
 *       How long the engine takes to come up is a property of the part, so it arrives as
 *       PRAET_SETTLE_MICROS at compile time and not as an argument here.
 * @note Reach this through PraetAttach, which takes the pool by name. Calling it directly means
 *       stating an address and an extent separately, and nothing here can tell whether the two go
 *       together.
 * @note Attaching does not claim the pool. A cellblock and a ring both write their own records into
 *       the bytes they dress, so MMGR_PARS_CLAIMED_ONCE exists to keep two of those off one pool.
 *       This writes no records into the bytes, and an engine filling ring segments while a consumer
 *       drains them is the arrangement the module is for.
 * @warning Fails closed. A channel that is not detached is left exactly as it was.
 * @warning Two channels may be attached over one block, and nothing here reports it. Which of them
 *          runs when is the caller's plan, and this library does not have it.
 */
embed_bool praet_ordo_adnectere(PraetOrdo *context, embed_word channel, uint8_t *bound, embed_word bytes,
                                 embed_word region);

/**
 * @brief Attaches a channel over a declared pool, by name.
 *
 * @param context_ Context the channel belongs to.
 * @param channel_ Channel to attach.
 * @param pool_    Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @param region_  Region descriptor for this channel, as a byte.
 * @return         What praet_ordo_adnectere answered.
 * @note The attach surface on a buffer, and how a caller is meant to reach a channel. Naming the pool
 *       is what makes the address and the extent come from one place: mmgr_pars_storage_##pool_ and
 *       pool_##_bytes are both emitted by the declaration, and neither exists for anything that was
 *       not declared as a pool.
 * @note Not a question of who owns the bytes. What is being tested is whether whoever hands them over
 *       has every answer about them: the address, the extent, and which channel is over them. A
 *       caller holding all three has said everything this needs, and where the bytes came from is
 *       their business.
 * @note Both names have internal linkage, so a translation unit that did not declare the pool cannot
 *       reach either and does not compile. That is what makes the answers first hand, and a caller
 *       giving all of them is what makes these bytes legal to touch. The caller said so.
 * @warning The region descriptor is still stated here. Which memory a pool sits in was settled by
 *          which of the two declarations wrote it, and neither emits a marker this could read, so
 *          deriving it needs something added where the pool is declared.
 */
#define PraetAttach(context_, channel_, pool_, region_)                                                                \
    (PRAET_ALVEUS_SUPERARE(context_, channel_, pool_),                                                                 \
     praet_ordo_adnectere(&(context_), (channel_), mmgr_pars_storage_##pool_, (embed_word)pool_##_bytes,              \
                           (embed_word)(region_)))

/**
 * @brief Declares which pool a channel of a context is over.
 *
 * @param context_ Context the channel belongs to.
 * @param channel_ Channel number, as a literal, since it is pasted into the symbol this emits.
 * @param pool_    Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @note Written once per channel, at file scope, beside the context's own declaration. It emits an
 *       enumerator whose name carries the context, the channel and the pool, and that name is the
 *       binding: PraetAttach and PraetSubmit both name it, so reaching either with a pool the channel
 *       was not declared over is an undeclared identifier that prints the triple that was written.
 * @note The same shape locus_carcerum uses. MMGR_CARCER_BODY pastes the site and the pool into
 *       prisonsite_##_##name_##_ctx and MMGR_CARCER_MEM makes the pool the member's name, so a
 *       cellblock's entries cannot be handed another cellblock's bytes. Here the channel joins the
 *       paste, because a context has several and they may be over different pools.
 * @note Costs nothing. An enumerator emits no storage and is settled while the unit compiles.
 * @warning Channel numbers are settled at compile time, which is what lets this paste one. A channel
 *          chosen at run time cannot be bound this way and reaches praet_ordo_adnectere directly,
 *          where nothing relates the address to the extent.
 */
#define PraetChannel(context_, channel_, pool_)                                                                        \
    EMBED_STATIC_ASSERT((channel_) < PRAET_CHANNELS,                                                                   \
                        #context_ " has no channel " #channel_ ", because PRAET_CHANNELS says how many it carries");   \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##pool_) > 0u,                                                        \
                        #pool_ " has no bytes for " #context_ " channel " #channel_ " to move");                       \
    enum                                                                                                               \
    {                                                                                                                  \
        context_##_channel##channel_##_is_over_##pool_ = 1                                                             \
    }

/**
 * @brief Names the binding a channel was declared with, and fails where there is none.
 *
 * @param context_ Context the channel belongs to.
 * @param channel_ Channel number, as a literal.
 * @param pool_    Pool the caller believes the channel is over.
 * @note Reads the enumerator PraetChannel emitted. Nothing is computed from it, and the reference is
 *       the whole point: a triple nobody declared names an identifier that does not exist.
 */
#define PRAET_ALVEUS_SUPERARE(context_, channel_, pool_) ((void)context_##_channel##channel_##_is_over_##pool_)

/**
 * @brief Asks for a channel to be detached.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel to detach.
 * @note Writes the request and reports nothing. The caller reads the result out of the flag word,
 *       which is what gives a void teardown something to report through.
 * @note A second request against a channel already detaching changes nothing. Hardware teardown
 *       writes the disable and polls the busy bit, so calling again is the normal path.
 */
void praet_ordo_separare(PraetOrdo *context, embed_word channel);

#if PRAET_RECOVERY

/**
 * @brief Marks a channel busy over @p length bytes at @p offset into what it was attached over.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel to run on.
 * @param[in]     offset  Bytes into the attached memory the transfer starts at.
 * @param[in]     length  Bytes it was asked to move.
 * @return                EMBED_TRUE where the channel took it.
 * @note Takes no pointer. The address came from the pool named at the attach, so an offset and a
 *       length are the whole of what a transfer adds. Handing an address here would let a caller
 *       state one that has nothing to do with what the channel is attached over.
 * @note Start, length and position are what make a backout possible. Without the three, a stalled
 *       transfer leaves a buffer nobody can say anything about, which is why this is the form
 *       PRAET_RECOVERY selects and why it is not optional once that is on.
 * @note Takes no duration. How long a transfer runs is not something this library can know, so
 *       nothing here predicts it - the port reports completion when it happens.
 * @warning Refuses a channel that is settling, already busy, detaching, or not attached. Fails
 *          closed: a refused submit changes no state.
 * @warning Does not test the offset and the length against the attached extent. That is settled
 *          before anything runs, by PraetSubmit, which has the pool's own extent to compare against.
 *          Reaching this directly with numbers nobody checked is how a transfer runs off the end.
 */
embed_bool praet_ordo_relatio(PraetOrdo *context, embed_word channel, embed_word offset, embed_word length);

/**
 * @brief Fails the build when a span runs past the pool it names.
 *
 * @param pool_   Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @param offset_ Bytes into that pool the span starts at.
 * @param length_ Bytes in the span.
 * @note The bound, and it is settled before anything runs. A span that does not fit gives a negative
 *       bitfield width, and the member's name is what the compiler prints, which is the same way
 *       MMGR_PARS_CLAIMED_ONCE says what it means.
 * @note Usable in an expression, which a static assertion is not. That is what lets the check sit in
 *       front of a call whose answer the caller reads.
 * @note Measured at -O0 and at -O2: a span that fits reports nothing at either, and one that does not
 *       fails at both. Nothing here needs the optimizer, unlike a check riding on a call that has to
 *       be folded away.
 * @warning Reads pool_##_bytes, so it holds for a span whose offset and length are known while
 *          compiling. A length computed at run time is not bounded by this and is not bounded
 *          anywhere else either.
 */
#define PRAET_SPAN_FITS(pool_, offset_, length_)                                                                       \
    ((void)sizeof(struct {                                                                                            \
        int this_span_runs_past_the_pool_the_channel_was_attached_over                                                 \
            : (((offset_) + (length_)) <= sizeof(mmgr_pars_storage_##pool_)) ? 1 : -1;                                 \
    }))

/**
 * @brief Feeds the watchdog for one channel and records how far it has got.
 *
 * @param[in,out] context  Context the channel belongs to [BORROWS].
 * @param[in]     channel  Channel that showed a sign of life.
 * @param[in]     position Bytes moved so far.
 * @note What the port calls when the engine has moved. A sign of life and how far it got are the
 *       same event, so one call carries both.
 * @note Pushes the keepalive window out and clears a stall that had been recorded, because a channel
 *       that is moving again is not stalled.
 * @note Says nothing about the transfer being finished. That is what praet_ordo_completed is.
 */
void praet_ordo_efficere(PraetOrdo *context, embed_word channel, embed_word position);

/**
 * @brief Returns how far a channel's transfer has got.
 *
 * @param[in] context Context the channel belongs to [BORROWS].
 * @param[in] channel Channel to read.
 * @return            Bytes moved so far.
 * @note With the start and length the caller submitted, this is the whole picture: the bytes at
 *       [start, start + position) were written and the rest were not. That is what a caller needs to
 *       back a stalled transfer out, or to zero exactly what was touched instead of the whole buffer.
 */
embed_word praet_ordo_situs(const PraetOrdo *context, embed_word channel);

/**
 * @brief Returns the bytes a stalled transfer may have written, rounded up to a whole word.
 *
 * @param[in] context Context the channel belongs to [BORROWS].
 * @param[in] channel Channel to read.
 * @return            Bytes at start that must be treated as written, never above the length.
 * @note The position is sampled every few poll ticks, so it is exact to word granularity and can be
 *       up to one word behind what the engine actually wrote. A caller zeroing only as far as the
 *       position would leave a partial word unscrubbed, so the rounding lives here instead of in
 *       every caller that has to remember it.
 * @note This is the number to scrub. praet_ordo_situs is what was sampled.
 */
embed_word praet_ordo_commotus_est(const PraetOrdo *context, embed_word channel);

/**
 * @brief Returns the checksum of the word a recovery found the engine inside.
 *
 * @param[in] context Context the channel belongs to [BORROWS].
 * @param[in] channel Channel to read.
 * @return            The checksum, where the channel reads PRAET_MEASURED. Zero otherwise.
 * @note Read PRAET_MEASURED first. Zero is a legal checksum, so the value alone does not say whether
 *       a word was measured.
 * @note The caller is what makes this useful. This library holds the destination and never the
 *       source, so it can say what the boundary word now contains and not whether that is what was
 *       meant to be there. Comparing the two is the caller's, which is the only place both are known.
 */
uint32_t praet_ordo_boundary_crc(const PraetOrdo *context, embed_word channel);

#else

/**
 * @brief Marks a channel busy and starts its keepalive window.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel to run on.
 * @return                EMBED_TRUE where the channel took it.
 * @note Takes no bytes. Where PRAET_RECOVERY is off nothing records what a transfer was pointed at,
 *       so a start and a length would be stored and never read.
 * @warning Refuses a channel that is settling, already busy, detaching, or not attached. Fails
 *          closed: a refused submit changes no state.
 */
embed_bool praet_ordo_relatio(PraetOrdo *context, embed_word channel);

/**
 * @brief Fails the build when a span runs past the pool it names.
 *
 * @param pool_   Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @param offset_ Bytes into that pool the span starts at.
 * @param length_ Bytes in the span.
 * @note The same check as on the other arm, and present here for the same reason it is there. A build
 *       with no recovery records nothing about a transfer, and a span that runs off the end of the
 *       pool is still a span that runs off the end of the pool.
 */
#define PRAET_SPAN_FITS(pool_, offset_, length_)                                                                       \
    ((void)sizeof(struct {                                                                                            \
        int this_span_runs_past_the_pool_the_channel_was_attached_over                                                 \
            : (((offset_) + (length_)) <= sizeof(mmgr_pars_storage_##pool_)) ? 1 : -1;                                 \
    }))

/**
 * @brief Feeds the watchdog for one channel.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel that showed a sign of life.
 * @note A sign of life and nothing else. How far the engine got is what PRAET_RECOVERY buys, and this
 *       build does not carry it.
 * @note Pushes the keepalive window out and clears a stall that had been recorded, because a channel
 *       that is moving again is not stalled.
 */
void praet_ordo_efficere(PraetOrdo *context, embed_word channel);

#endif

/**
 * @brief Submits a span of a declared pool on a channel attached over that pool.
 *
 * @param context_ Context the channel belongs to.
 * @param channel_ Channel to run on.
 * @param pool_    Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @param offset_  Bytes into that pool the transfer starts at.
 * @param length_  Bytes it moves.
 * @return         What praet_ordo_relatio answered.
 * @note The submit surface on a buffer, and the counterpart of PraetAttach. Naming the pool again is
 *       what gives the bound something to compare against, since the extent lives with the
 *       declaration and not with the channel.
 * @note The bound is checked here and never inside the entry. Sizes and spans are settled while
 *       compiling, so a test at run time would be paying for a question that was already answered.
 * @note Names the binding PraetChannel declared, so submitting a pool this channel is not over fails
 *       on an identifier carrying the context, the channel and the pool that was written. That is the
 *       same guarantee locus_carcerum gets from pasting the site and the pool into one symbol.
 */
#if PRAET_RECOVERY
#define PraetSubmit(context_, channel_, pool_, offset_, length_)                                                       \
    (PRAET_ALVEUS_SUPERARE(context_, channel_, pool_), PRAET_SPAN_FITS(pool_, offset_, length_),                       \
     praet_ordo_relatio(&(context_), (channel_), (embed_word)(offset_), (embed_word)(length_)))
#else
#define PraetSubmit(context_, channel_, pool_, offset_, length_)                                                       \
    (PRAET_ALVEUS_SUPERARE(context_, channel_, pool_), PRAET_SPAN_FITS(pool_, offset_, length_),                       \
     praet_ordo_relatio(&(context_), (channel_)))
#endif

/**
 * @brief Reports that a channel's transfer finished.
 *
 * @param[in,out] context Context the channel belongs to [BORROWS].
 * @param[in]     channel Channel that finished.
 * @param[in]     failed  EMBED_TRUE where it ended in an error rather than a completion.
 * @note Completion arrives from the port and never from a timer. A timer deciding a transfer had
 *       finished would be this library guessing at hardware.
 */
void praet_ordo_completed(PraetOrdo *context, embed_word channel, embed_bool failed);

#if PRAET_RECOVERY

/**
 * @brief Resolves a stalled channel by the recovery the caller picks.
 *
 * @param[in,out] context  Context the channel belongs to [BORROWS].
 * @param[in]     channel  Channel to resolve.
 * @param[in]     recovery PRAET_RESTITUERE_ET_REDINTEGRARE or PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE.
 * @return                 EMBED_TRUE where a stalled channel was resolved.
 * @note Returns the channel to attached and records which recovery was taken, so a later reader can
 *       tell a backed-out transfer from one whose bytes were treated as unsafe.
 * @note Nothing here writes the caller's storage. This context holds no pointer to it, so a caller
 *       choosing PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE zeroes its own bytes and this records that it did.
 * @warning Refuses a channel that is not stalled. Fails closed: state is only ever unknown after the
 *          watchdog said so, and resolving a healthy channel would discard a live transfer.
 */
embed_bool praet_ordo_resolve(PraetOrdo *context, embed_word channel, embed_word recovery);

#endif

/**
 * @brief Advances the clock by @p micros.
 *
 * @param[in,out] context Context to advance [BORROWS].
 * @param[in]     micros  Microseconds to add.
 * @note Raises set, because time passing is one of the things that changes what a service call would
 *       find - a keepalive window can elapse without anything else happening.
 */
void praet_ordo_advance(PraetOrdo *context, embed_word micros);

/**
 * @brief Advances the clock by @p ticks, scaled against the declared frequency.
 *
 * @param[in,out] context Context to advance [BORROWS].
 * @param[in]     ticks   Ticks read off the clock this build declared.
 * @note What a port calls, because a counter reads in ticks and every deadline here is microseconds.
 *       The scaling is one compile-time constant, so a build whose clock runs at a whole megahertz
 *       pays a multiply and a shift for it.
 * @warning Ticks that do not add up to a whole microsecond are dropped rather than carried. A port
 *          feeding this one tick at a time on a fast clock never advances anything, which is why a
 *          port reads the counter and passes the difference instead of counting calls.
 */
void praet_ordo_advance_ticks(PraetOrdo *context, embed_word ticks);

/**
 * @brief Stands in for the interrupt raising the set volatile.
 *
 * @param[in,out] context Context the interrupt belongs to [BORROWS].
 * @note All an interrupt does. It may run any number of times and the raises collapse, because there
 *       is exactly one reader and setting a raised flag is a no-op. Nothing is counted and nothing is
 *       queued.
 */
void praet_ordo_raise(PraetOrdo *context);

/**
 * @brief Drives the port and brings every flag word up to date. The caller's poll.
 *
 * @param[in,out] context Context to poll [BORROWS].
 * @note What a caller calls, and the only thing they have to. Everything a channel does between an
 *       attach and a completion happens here: the port is asked how far each running channel has
 *       got, the watchdog is fed with the answer, a settle that has elapsed is cleared, a channel
 *       nothing reported on is marked stalled, and a detach whose transfer has finished completes.
 * @note Reports nothing. What a caller wants is in the flag word, which this leaves current, and one
 *       load reads it. A return value here would be a second way to learn the same thing, and the two
 *       would eventually disagree.
 * @note Also the reader/setter, and the first call on interrupt exit. It reads set, takes busy, does
 *       the above, then unsets both volatiles. Taking busy is what makes it ignore the interrupt for
 *       that span, and the interrupt may keep raising set throughout without anything being lost,
 *       because this recomputes every channel from what it can see rather than consuming an event.
 * @note Does nothing where set was not raised, and where busy was already held. A nested call while
 *       the lock is out is the re-entry case, and it declines rather than reworking state the outer
 *       call is partway through.
 * @note Every state change a channel undergoes goes through here, which is what puts the whole of the
 *       access control in one function. That fell out of the lock: one reader means one place.
 */
void praet_ordo_poll(PraetOrdo *context);

/**
 * @brief Returns one channel's flag word.
 *
 * @param[in] context Context the channel belongs to [BORROWS].
 * @param[in] channel Channel to read.
 * @return            The whole word: quaternary core at the bottom, statuses and region above.
 * @note One load. Knowing a channel is idle does not require waiting for a callback.
 */
uint32_t praet_ordo_flags(const PraetOrdo *context, embed_word channel);

EMBED_END_DECLS

#endif
