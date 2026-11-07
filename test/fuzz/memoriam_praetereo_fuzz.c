/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoriam_praetereo_fuzz.c
 * @brief A libFuzzer harness over the DMA layer, built with DMA on and the module compiled in, once with recovery
 *        and once lean: any sequence of attaches, detaches, submits, kicks, completions, resolves, clock advances,
 *        raises and polls on any channel against a model of every channel's flag word, keepalive deadline and
 *        position, with the port's progress answered by the input; the descriptor chain walk against a walk of
 *        its own; and the public entries refusing while no port replaces the hooks.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "memoriam_praetereo/memoriam_praetereo.h"
#include "memoriam_praetereo/praet_descriptor.h"
#include "memoriam_praetereo/praet_ordo.h"

#include "fuzz_input.h"

#include <string.h>

#if PRAET_RECOVERY
PraetOrdoContext(s_ordo, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE);
#else
PraetOrdoContext(s_ordo, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_DISABLE);
#endif

#define FUZZ_POOL_BYTES 256u
#define FUZZ_CHANNELS_TRIED (PRAET_CHANNELS + 2u)
#define WITHOUT(bits_) ((uint32_t)~(uint32_t)(bits_))
#define TRANSFER_STATUSES (PRAET_ERROR | PRAET_STALLED | PRAET_ABANDONED | PRAET_SCRUBBED | PRAET_MEASURED)

static EMBED_ALIGN(sizeof(embed_word)) uint8_t s_pool[FUZZ_POOL_BYTES];
static uint16_t s_progress[PRAET_CHANNELS];

/** @brief The port's progress, as the input set it for each channel. */
uint16_t praet_hw_progress(embed_word channel)
{
    return (channel < PRAET_CHANNELS) ? s_progress[channel] : 0u;
}

/** @brief The model of one context: each channel's flag word, deadline, length and position, and the clock. */
typedef struct
{
    uint32_t flags[PRAET_CHANNELS];
    embed_word keepalive[PRAET_CHANNELS];
    embed_word length[PRAET_CHANNELS];
    embed_word position[PRAET_CHANNELS];
    embed_word elapsed;
    embed_word settle_deadline;
    int raised;
} OrdoModel;

static OrdoModel s_model;

static uint32_t core_of(uint32_t flags)
{
    return flags & PRAET_CORE_MASK;
}

static void model_attach(embed_word channel, embed_word region, embed_bool answered)
{
    const int takes = (channel < PRAET_CHANNELS) && (core_of(s_model.flags[channel]) == PRAET_SEPARATUS);
    fuzz_check((answered != 0) == takes, "an attach takes a detached channel and only one");
    if (!takes)
    {
        return;
    }
    const embed_word deadline = s_model.elapsed + (embed_word)PRAET_SETTLE_MICROS;
    if (deadline > s_model.settle_deadline)
    {
        s_model.settle_deadline = deadline;
    }
    uint32_t now = (uint32_t)PRAET_ADNEXUS | (uint32_t)PRAET_CLAIMED;
    if (s_model.elapsed < s_model.settle_deadline)
    {
        now |= (uint32_t)PRAET_SETTLING;
    }
    s_model.flags[channel] = now | PRAET_REGION_FIELD(region);
    s_model.length[channel] = 0u;
    s_model.position[channel] = 0u;
    s_model.raised = 1;
}

static void model_detach(embed_word channel)
{
    if ((channel >= PRAET_CHANNELS) || (core_of(s_model.flags[channel]) == PRAET_SEPARATUS))
    {
        return;
    }
    s_model.flags[channel] |= (uint32_t)PRAET_SEPARANS;
    s_model.raised = 1;
}

static void model_submit(embed_word channel, embed_word length, embed_bool answered)
{
    int takes = channel < PRAET_CHANNELS;
    if (takes)
    {
        const uint32_t was = s_model.flags[channel];
        takes = ((core_of(was) == PRAET_ADNEXUS) || (core_of(was) == PRAET_OK)) &&
                ((was & (uint32_t)(PRAET_SETTLING | PRAET_SEPARANS)) == 0u);
    }
    fuzz_check((answered != 0) == takes, "a submit takes an attached channel that is settled, idle and staying");
    if (!takes)
    {
        return;
    }
    s_model.flags[channel] =
        (s_model.flags[channel] & WITHOUT(PRAET_CORE_MASK | TRANSFER_STATUSES)) | (uint32_t)PRAET_BUSY;
    s_model.keepalive[channel] = s_model.elapsed + (embed_word)PRAET_KEEPALIVE_MICROS;
    s_model.length[channel] = length;
    s_model.position[channel] = 0u;
    s_model.raised = 1;
}

static void model_kick(embed_word channel, embed_word position)
{
    if ((channel >= PRAET_CHANNELS) || (core_of(s_model.flags[channel]) != PRAET_BUSY))
    {
        return;
    }
    s_model.flags[channel] &= WITHOUT(PRAET_STALLED);
    s_model.keepalive[channel] = s_model.elapsed + (embed_word)PRAET_KEEPALIVE_MICROS;
    if (position > s_model.position[channel])
    {
        s_model.position[channel] = (position > s_model.length[channel]) ? s_model.length[channel] : position;
    }
    s_model.raised = 1;
}

static void model_completed(embed_word channel, int failed)
{
    if ((channel >= PRAET_CHANNELS) || (core_of(s_model.flags[channel]) != PRAET_BUSY))
    {
        return;
    }
    uint32_t now = (s_model.flags[channel] & WITHOUT(PRAET_CORE_MASK | PRAET_STALLED)) | (uint32_t)PRAET_OK;
    if (failed)
    {
        now |= (uint32_t)PRAET_ERROR;
    }
    else
    {
        s_model.position[channel] = s_model.length[channel];
    }
    s_model.flags[channel] = now;
    s_model.keepalive[channel] = 0u;
    s_model.raised = 1;
}

#if PRAET_RECOVERY
static void model_resolve(embed_word channel, embed_word recovery, embed_bool answered)
{
    const int takes = (channel < PRAET_CHANNELS) && ((s_model.flags[channel] & (uint32_t)PRAET_STALLED) != 0u);
    fuzz_check((answered != 0) == takes, "a resolve takes a stalled channel and only one");
    if (!takes)
    {
        return;
    }
    uint32_t now = (s_model.flags[channel] & WITHOUT(PRAET_CORE_MASK | PRAET_STALLED)) | (uint32_t)PRAET_ADNEXUS;
    now |= (recovery == (embed_word)PRAET_RESTITUERE_ET_AD_NIHILUM_REDIGERE) ? (uint32_t)PRAET_SCRUBBED
                                                                             : (uint32_t)PRAET_ABANDONED;
    if ((s_ordo.word_boundary_crc != EMBED_FALSE) &&
        ((s_model.position[channel] % (embed_word)sizeof(embed_word)) != 0u))
    {
        now |= (uint32_t)PRAET_MEASURED;
    }
    s_model.flags[channel] = now;
    s_model.keepalive[channel] = 0u;
    s_model.raised = 1;
}
#endif

static void model_poll(void)
{
    int moved = 0;
#if PRAET_RECOVERY
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        if (core_of(s_model.flags[channel]) != PRAET_BUSY)
        {
            continue;
        }
        const embed_word reached = (embed_word)s_progress[channel];
        if (reached <= s_model.position[channel])
        {
            continue;
        }
        s_model.position[channel] = (reached > s_model.length[channel]) ? s_model.length[channel] : reached;
        s_model.keepalive[channel] = s_model.elapsed + (embed_word)PRAET_KEEPALIVE_MICROS;
        s_model.flags[channel] &= WITHOUT(PRAET_STALLED);
        moved = 1;
    }
#endif
    if (!s_model.raised && !moved)
    {
        return;
    }
    const int settled = s_model.elapsed >= s_model.settle_deadline;
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        uint32_t now = s_model.flags[channel];
        if (core_of(now) == PRAET_SEPARATUS)
        {
            continue;
        }
        if (((now & (uint32_t)PRAET_SETTLING) != 0u) && settled)
        {
            now &= WITHOUT(PRAET_SETTLING);
        }
        if ((core_of(now) == PRAET_BUSY) && (s_model.elapsed >= s_model.keepalive[channel]))
        {
            now |= (uint32_t)PRAET_STALLED;
        }
        if (((now & (uint32_t)PRAET_SEPARANS) != 0u) && (core_of(now) != PRAET_BUSY))
        {
            now = (uint32_t)PRAET_SEPARATUS;
            s_model.keepalive[channel] = 0u;
            s_model.length[channel] = 0u;
            s_model.position[channel] = 0u;
        }
        s_model.flags[channel] = now;
    }
    s_model.raised = 0;
}

static void compared(void)
{
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        const uint32_t flags = praet_ordo_flags(&s_ordo, channel);
        if (flags != s_model.flags[channel])
        {
            fprintf(stderr, "channel %u flags 0x%08x, model 0x%08x\n", (unsigned)channel, (unsigned)flags,
                    (unsigned)s_model.flags[channel]);
        }
        fuzz_check(flags == s_model.flags[channel], "a channel's flag word is the model's");
        fuzz_check((flags & WITHOUT(PRAET_MAP_MASK)) == 0u, "a flag word carries no bit the map leaves out");
        fuzz_check(((flags & (uint32_t)PRAET_STALLED) == 0u) || (core_of(flags) == PRAET_BUSY) ||
                       ((flags & (uint32_t)PRAET_SEPARANS) != 0u),
                   "only a running channel reads stalled");
#if PRAET_RECOVERY
        fuzz_check(praet_ordo_situs(&s_ordo, channel) == s_model.position[channel],
                   "a channel's position is the model's");
        const embed_word moved = praet_ordo_commotus_est(&s_ordo, channel);
        fuzz_check(moved <= s_model.length[channel], "the bytes to scrub never pass the length");
        fuzz_check(moved >= s_model.position[channel], "the bytes to scrub cover the position");
        fuzz_check((moved == s_model.length[channel]) || ((moved % (embed_word)sizeof(embed_word)) == 0u),
                   "the bytes to scrub are whole words or the whole length");
        if (core_of(flags) == PRAET_SEPARATUS)
        {
            fuzz_check(praet_ordo_boundary_crc(&s_ordo, channel) == 0u,
                       "a detached channel carries no boundary checksum");
        }
#endif
    }
}

static void descriptors(FuzzInput *input)
{
    PraetDescriptor chain[12];
    memset(chain, 0, sizeof chain);
    const size_t count = 1u + fuzz_below(input, 12u);
    for (size_t at = 0u; at < count; at += 1u)
    {
        const uint8_t pick = fuzz_byte(input);
        chain[at].next = ((pick & 3u) == 0u) ? NULL : &chain[pick % count];
    }
    const embed_word limit = (embed_word)fuzz_below(input, 20u);
    embed_word expected = 0u;
    if (limit != 0u)
    {
        expected = 1u;
        const PraetDescriptor *walk = chain[0].next;
        while ((walk != NULL) && (walk != &chain[0]) && (expected < limit))
        {
            expected += 1u;
            walk = walk->next;
        }
    }
    fuzz_check(praet_descriptor_chain_length(&chain[0], limit) == expected,
               "the chain walk counts to the end, the start or the limit");
    fuzz_check(praet_descriptor_chain_length(NULL, limit) == 0u, "no chain counts nothing");
}

static void public_entries(FuzzInput *input)
{
    static const PraetCallbackCfg s_binding = {NULL, NULL};
    const uint8_t channel = (uint8_t)(fuzz_byte(input) % PRAET_CHANNELS);
    fuzz_check(!EMBED_CALL(praet.open, PraetCfg, .channel = channel, .on_complete = &s_binding),
               "an open is refused while no port replaces the hook");
    fuzz_check(!EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = channel, .buf = s_pool, .bytes = 4u),
               "a submit is refused while no port replaces the hook");
    EMBED_CALL(praet.close, PraetTransferCfg, .channel = channel);
    EMBED_CALL(praet.poll, PraetCfg, .channel = channel, .on_complete = &s_binding);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    praet_ordo_reset(&s_ordo);
    memset(&s_model, 0, sizeof s_model);
    memset(s_progress, 0, sizeof s_progress);
    s_model.elapsed = s_ordo.elapsed_micros;
    s_model.settle_deadline = s_ordo.settle_deadline;
    s_model.raised = s_ordo.praet_set_bitflag != 0u;
    for (embed_word channel = 0u; channel < PRAET_CHANNELS; channel++)
    {
        s_model.flags[channel] = praet_ordo_flags(&s_ordo, channel);
    }
    compared();

    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        const embed_word channel = (embed_word)(fuzz_byte(&input) % FUZZ_CHANNELS_TRIED);
        switch (op % 12u)
        {
        case 0: {
            const embed_word region = (embed_word)(fuzz_byte(&input) % (PRAET_REGION_ID_HIGHEST + 1u));
            const embed_bool answered = praet_ordo_adnectere(&s_ordo, channel, s_pool, FUZZ_POOL_BYTES, region);
            model_attach(channel, region, answered);
            break;
        }
        case 1:
            praet_ordo_separare(&s_ordo, channel);
            model_detach(channel);
            break;
        case 2:
        case 3: {
#if PRAET_RECOVERY
            const embed_word offset = (embed_word)fuzz_below(&input, FUZZ_POOL_BYTES);
            const embed_word length = (embed_word)fuzz_below(&input, FUZZ_POOL_BYTES - offset + 1u);
            const embed_bool answered = praet_ordo_relatio(&s_ordo, channel, offset, length);
            model_submit(channel, length, answered);
#else
            const embed_bool answered = praet_ordo_relatio(&s_ordo, channel);
            model_submit(channel, 0u, answered);
#endif
            break;
        }
        case 4: {
#if PRAET_RECOVERY
            const embed_word position = (embed_word)fuzz_below(&input, FUZZ_POOL_BYTES + 8u);
            praet_ordo_efficere(&s_ordo, channel, position);
            model_kick(channel, position);
#else
            praet_ordo_efficere(&s_ordo, channel);
            model_kick(channel, 0u);
#endif
            break;
        }
        case 5: {
            const int failed = (fuzz_byte(&input) & 1u) != 0u;
            praet_ordo_completed(&s_ordo, channel, (embed_bool)failed);
            model_completed(channel, failed);
            break;
        }
        case 6: {
#if PRAET_RECOVERY
            const embed_word recovery = (embed_word)(fuzz_byte(&input) % 3u);
            const embed_bool answered = praet_ordo_resolve(&s_ordo, channel, recovery);
            model_resolve(channel, recovery, answered);
#endif
            break;
        }
        case 7: {
            const embed_word micros = (embed_word)fuzz_below(&input, 2u * PRAET_KEEPALIVE_MICROS + 2u);
            praet_ordo_advance(&s_ordo, micros);
            s_model.elapsed += micros;
            s_model.raised = 1;
            break;
        }
        case 8: {
            const embed_word ticks = (embed_word)fuzz_below(&input, 60000u);
            praet_ordo_advance_ticks(&s_ordo, ticks);
            s_model.elapsed += ticks / (embed_word)PRAET_TICKS_PER_MICRO;
            s_model.raised = 1;
            break;
        }
        case 9:
            praet_ordo_raise(&s_ordo);
            s_model.raised = 1;
            break;
        case 10:
            if (channel < PRAET_CHANNELS)
            {
                s_progress[channel] = (uint16_t)fuzz_below(&input, FUZZ_POOL_BYTES + 8u);
            }
            praet_ordo_poll(&s_ordo);
            model_poll();
            break;
        default:
            if ((op & 0x10u) != 0u)
            {
                descriptors(&input);
            }
            else
            {
                public_entries(&input);
            }
            break;
        }
        compared();
    }
    return 0;
}
