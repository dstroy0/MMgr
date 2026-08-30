/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file main.c
 * @brief The memoria_operor region entries against the target's own libc, across input lengths.
 *
 * Run on the part rather than on a host, for the reason the cellularum bench gives: a desktop libc
 * answers these with SSE or AVX, and neither part here has anything of the kind. The libc reached
 * here is ESP-ROM - memcmp, memchr, memcpy and memset are hand-written assembly in the part's mask
 * ROM, running without flash-cache pressure, while the library runs from flash through the icache.
 *
 * cmp and chr are the two that walk; cpy and set are moves, and are here because the same libc
 * routines are what a caller would otherwise reach for.
 */
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "device_bench.h"

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "locus_carcerum/locus_carcerum.h"
#include "confinium_externum/confinium_externum.h"
#include "spatium/spatium.h"
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"
#include "endian/endian.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "memoria_operor/memoria_operor.h"
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Bytes the cellblock the allocator rows take from holds.
 */
#define ARENA_BYTES 4096u

/**
 * @brief A prison site with one minimum security cellblock, which the allocator rows take from.
 *
 * @note Declared, not initialized at run time. The storage, its alignment and the cellblock's state
 *       are all emitted as data, so nothing here runs before main and the first byte is an address
 *       the linker resolved. That is the half malloc cannot answer for at any speed.
 */
LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(general, ARENA_BYTES));

/**
 * @brief The take size, hidden so neither allocator sees a constant.
 */
static volatile size_t g_take = 64u;

/**
 * @brief The byte count the wire shape rows use, hidden so neither arm folds its branches away.
 */
static volatile size_t g_wire_n = 8u;

/**
 * @brief The two request sizes the placement rows use, hidden so the decision is not folded.
 *
 * @note One below the threshold and one above it, which are the two orders the entry tries the two
 *       memories in.
 */
static volatile size_t g_ext_small = 256u;
static volatile size_t g_ext_large = 65536u;

/**
 * @brief The compare length, hidden so neither arm can be specialised against it.
 */
static volatile size_t g_cmp_len = 8u;

/**
 * @brief The two regions, reached so the compiler cannot see which objects they are.
 *
 * @note Handed g_a and g_b directly, a walk taking plain pointers is told they are aligned globals
 *       that cannot alias, and it is optimised on that. A backend inside the library is told none of
 *       that, so a row comparing the two is not comparing the same situation.
 */
static const uint8_t *volatile g_cmp_a;
static const uint8_t *volatile g_cmp_b;

/**
 * @brief Values and a scratch word for the byte order rows, hidden from the compiler.
 *
 * @note A constant folds a byte reversal away entirely: the compiler reverses it at build time and
 *       both arms report the harness floor.
 */
static volatile uint64_t g_swap_val = 0x0123456789ABCDEFull;
static volatile unsigned g_swap_off = 0u;

#define CAP 4096u

/**
 * @brief Source, destination and comparison buffers, aligned and fixed for the whole run.
 *
 * @note The library is built for memory that arrives aligned, so an unaligned fixture would time a
 *       head walk it never performs in a real build.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_a[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_b[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_d[CAP];

/**
 * @brief The bit writer's byte loop as bitor_put carries it.
 *
 * @param[in,out] writer    Writer to append to [BORROWS].
 * @param[in]     val       Bits to write, from the low end.
 * @param[in]     bit_count How many.
 * @note The A arm. Every pass computes how much room the residue leaves, merges it, and then clears
 *       it - but it is cleared at the end of the first pass and never refilled inside the loop, so
 *       from the second byte on the room is always eight and the merge is always with zero.
 */
static void bitor_put_ref(mmgr_bitor *writer, uint64_t val, mmgr_word bit_count)
{
    const uint64_t mask = (bit_count >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << bit_count) - 1u);
    const size_t whole = (size_t)((writer->bit_count + bit_count) / 8u);
    uint64_t work = val & mask;
    mmgr_word left = bit_count;

    if (whole > (writer->cap - writer->bytes_written))
    {
        writer->overflow = MMGR_TRUE;
        writer->bit_count = 0;
        writer->residue = 0;
        return;
    }

    for (size_t i = 0; i < whole; i++)
    {
        const mmgr_word take = 8u - writer->bit_count;
        const uint8_t chunk = (uint8_t)(work & 0xFFu);

        // Explicit casts hold each step at uint8_t; the shift and the or promote away from it
        writer->out[writer->bytes_written + i] = (uint8_t)(writer->residue | (uint8_t)(chunk << writer->bit_count));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->bit_count = 0;
    }

    if (left != 0u)
    {
        // Explicit casts narrow the leftover into the uint8_t residue. left plus writer->bit_count
        // is under 8
        const uint8_t tail = (uint8_t)(work & ((1u << left) - 1u));

        writer->residue = (uint8_t)(writer->residue | (uint8_t)(tail << writer->bit_count));
        writer->bit_count += left;
    }
    writer->bytes_written += whole;
}

/**
 * @brief The same bytes with the residue merged once and the rest written straight.
 *
 * @param[in,out] writer    Writer to append to [BORROWS].
 * @param[in]     val       Bits to write, from the low end.
 * @param[in]     bit_count How many.
 * @note The B arm. The first byte is the only one the residue reaches, so it is taken on its own
 *       and every byte after it is a mask and a store. The cursor and the value are walked in
 *       locals and written back once, rather than reached through the writer on every pass.
 */
static void bitor_put_split(mmgr_bitor *writer, uint64_t val, mmgr_word bit_count)
{
    const uint64_t mask = (bit_count >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << bit_count) - 1u);
    const size_t whole = (size_t)((writer->bit_count + bit_count) / 8u);
    uint64_t work = val & mask;
    mmgr_word left = bit_count;

    if (whole > (writer->cap - writer->bytes_written))
    {
        writer->overflow = MMGR_TRUE;
        writer->bit_count = 0;
        writer->residue = 0;
        return;
    }

    uint8_t *const to = writer->out + writer->bytes_written;
    size_t index = 0u;

    if (whole != 0u)
    {
        const mmgr_word take = 8u - writer->bit_count;

        // Explicit casts hold each step at uint8_t; the shift and the or promote away from it
        to[0] = (uint8_t)(writer->residue | (uint8_t)((uint8_t)(work & 0xFFu) << writer->bit_count));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->bit_count = 0;
        index = 1u;

        // Past the first byte the residue is empty and the room is a whole byte, so nothing here
        // merges or shifts by a residue width that is known to be zero
        for (; index < whole; index++)
        {
            to[index] = (uint8_t)(work & 0xFFu);
            work >>= 8u;
            left -= 8u;
        }
    }

    if (left != 0u)
    {
        const uint8_t tail = (uint8_t)(work & ((1u << left) - 1u));

        writer->residue = (uint8_t)(writer->residue | (uint8_t)(tail << writer->bit_count));
        writer->bit_count += left;
    }
    writer->bytes_written += whole;
}

/**
 * @brief Checks the two bit writer shapes against each other over a stream of mixed widths.
 *
 * @return The number of bytes or counters where they disagree.
 * @note A bit writer that is fast and lays the bits down differently is not the same writer, and
 *       the widths that matter are the ones that straddle a byte, so this walks every width from
 *       one to sixty four in sequence rather than one width at a time.
 */
static uint32_t bitor_is_correct(void)
{
    static uint8_t one[512];
    static uint8_t two[512];
    uint32_t bad = 0u;

    mmgr_bitor a = MMGR_CALL(bitio.init, BitorumCfg, .out = one, .cap = sizeof one);
    mmgr_bitor b = MMGR_CALL(bitio.init, BitorumCfg, .out = two, .cap = sizeof two);
    uint64_t seed = 0x9E3779B97F4A7C15ull;

    for (mmgr_word width = 1u; width <= 64u; width++)
    {
        bitor_put_ref(&a, seed, width);
        bitor_put_split(&b, seed, width);
        seed = (seed * 6364136223846793005ull) + 1442695040888963407ull;
    }

    if ((a.bytes_written != b.bytes_written) || (a.bit_count != b.bit_count) || (a.residue != b.residue))
    {
        bad++;
    }

    for (size_t index = 0; index < a.bytes_written; index++)
    {
        if (one[index] != two[index])
        {
            bad++;
        }
    }
    return bad;
}

/**
 * @brief The gather byteio_take_be performs: every width tested, whatever the count.
 *
 * @param[in] at    Bytes to read [BORROWS].
 * @param[in] bytes How many, 1 through 8.
 * @return          The value, in the target's own order after one reversal.
 * @note The A arm, and the entry's shape as written. A count of eight loads the whole value in the
 *       first branch and then tests four, two and one on the way out, none of which can be true.
 */
static uint64_t take_shape_all(const uint8_t *at, size_t bytes)
{
    uint64_t v = 0u;
    size_t sh = 0u;

    if ((bytes & 8u) != 0u)
    {
        v = MMGR_CALL(proxim.load64, ProximusCfg, .at = at);
    }
    if ((bytes & 4u) != 0u)
    {
        v |= (uint64_t)MMGR_CALL(proxim.load32, ProximusCfg, .at = at) << sh;
        at += 4;
        sh += 32u;
    }
    if ((bytes & 2u) != 0u)
    {
        v |= (uint64_t)MMGR_CALL(proxim.load16, ProximusCfg, .at = at) << sh;
        at += 2;
        sh += 16u;
    }
    if ((bytes & 1u) != 0u)
    {
        v |= (uint64_t)(*at) << sh;
    }
    return MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)bytes);
}

/**
 * @brief The same gather with the whole-width case taken on its own, as put_be already does.
 *
 * @param[in] at    Bytes to read [BORROWS].
 * @param[in] bytes How many, 1 through 8.
 * @return          The value, in the target's own order after one reversal.
 * @note The B arm. Identical loads and one reversal either way; a count of eight simply stops
 *       asking about the widths it has already covered.
 */
static uint64_t take_shape_exit(const uint8_t *at, size_t bytes)
{
    uint64_t v = 0u;

    if ((bytes & 8u) != 0u)
    {
        v = MMGR_CALL(proxim.load64, ProximusCfg, .at = at);
    }
    else
    {
        size_t sh = 0u;

        if ((bytes & 4u) != 0u)
        {
            v |= (uint64_t)MMGR_CALL(proxim.load32, ProximusCfg, .at = at) << sh;
            at += 4;
            sh += 32u;
        }
        if ((bytes & 2u) != 0u)
        {
            v |= (uint64_t)MMGR_CALL(proxim.load16, ProximusCfg, .at = at) << sh;
            at += 2;
            sh += 16u;
        }
        if ((bytes & 1u) != 0u)
        {
            v |= (uint64_t)(*at) << sh;
        }
    }
    return MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)bytes);
}

/**
 * @brief The compare walk as memor_cmp carries it, reading both sides through the unaligned word.
 *
 * @param[in] a     First region [BORROWS].
 * @param[in] b     Second region [BORROWS].
 * @param[in] bytes Bytes to compare.
 * @return          Zero when they agree over the whole run.
 * @note The A arm. It must reproduce the entry's cost or the row beside it means nothing, which is
 *       the check an earlier arm in this work skipped and had to be retracted for.
 */
static mmgr_iword cmp_unaligned(const uint8_t *a, const uint8_t *b, size_t bytes)
{
    const size_t full = (bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (MMGR_CALL(word.load, ScrutWordCfg, .at = a + at) != MMGR_CALL(word.load, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

/**
 * @brief What memor_cmp walks with, so the arm below reaches its addresses the same way.
 */
typedef struct
{
    const uint8_t *src;   /**< First region [BORROWS]. */
    const uint8_t *other; /**< Second region [BORROWS]. */
    size_t bytes;         /**< Bytes to compare. */
} BenchCmpCtx;

/**
 * @brief The identical walk, reaching both addresses through a context on every step.
 *
 * @param[in] args The two regions and the count [BORROWS].
 * @return         Zero when they agree over the whole run.
 * @note The discriminator. The loop, the loads and the test are the same as cmp_unaligned; the only
 *       difference is where the two addresses live while it runs. If this costs what the entry costs
 *       then the entry's walk is not doing anything extra and the shape is the whole of it.
 */
static mmgr_iword cmp_via_ctx(const BenchCmpCtx *args)
{
    const size_t full = (args->bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at) !=
            MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

/**
 * @brief The context walk with both addresses read once before the loop rather than once a word.
 *
 * @param[in] args The two regions and the count [BORROWS].
 * @return         Zero when they agree over the whole run.
 * @note Tested on cellul.eq, where it was worth two thousandths of a cycle a byte, and then assumed
 *       to be worth nothing here without being run. It is run here.
 */
static mmgr_iword cmp_ctx_hoisted(const BenchCmpCtx *args)
{
    const uint8_t *const a = args->src;
    const uint8_t *const b = args->other;
    const size_t full = (args->bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (MMGR_CALL(word.load, ScrutWordCfg, .at = a + at) != MMGR_CALL(word.load, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

/**
 * @brief The same walk, reading through the aligned word once both sides sit on a boundary.
 *
 * @param[in] a     First region [BORROWS].
 * @param[in] b     Second region [BORROWS].
 * @param[in] bytes Bytes to compare.
 * @return          Zero when they agree over the whole run.
 * @note The B arm. This is the third time the aligned load has been asked about: it took thirty
 *       bytes from 143 to 91 in proxim_words and was worth exactly 1.00 in cellul_agree_cs, so it
 *       is not a technique to assume either way.
 */
static mmgr_iword cmp_aligned(const uint8_t *a, const uint8_t *b, size_t bytes)
{
    // Explicit casts read both addresses as integers so their low bits can be compared
    if (((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) != 0u)
    {
        return cmp_unaligned(a, b, bytes);
    }

    const size_t full = (bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at) !=
            MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

/**
 * @brief Bytes the ring rows work in, a power of two as init requires.
 */
#define RING_CAP 1024u

/**
 * @brief Segments the ring is divided into for the segment layer rows.
 */
#define RING_SEGS 4u

/**
 * @brief The span each ring row moves.
 */
#define RING_SPAN 64u

static mmgr_ring g_ring;
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_ring_buf[RING_CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_ring_out[RING_CAP];

/**
 * @brief Head and tail for the ring a caller writes instead, to compare the mmgr one against.
 *
 * @note Plain indices into a power of two buffer with memcpy doing the moving, split in two when a
 *       span crosses the end. That is the whole of what a hand rolled ring is, and it is the honest
 *       counterpart: there is no libc ring to measure against.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_hand_buf[RING_CAP];
static size_t g_hand_head;
static size_t g_hand_tail;

/**
 * @brief The same two indices, ordered the way the ring orders its own.
 *
 * @note infin_available reads head and tail with acquire loads, because the ring is safe for a
 *       producer and a consumer in separate contexts. The plain indices above are not, so a row
 *       against them is not measuring the same job. These are, and the pair of rows separates what
 *       the ordering costs from what the entry costs.
 */
static _Atomic size_t g_ord_head;
static _Atomic size_t g_ord_tail;

/**
 * @brief Writes a span into the hand rolled ring, splitting it at the end when it crosses.
 *
 * @param[in] src   Bytes to write [BORROWS].
 * @param[in] bytes How many.
 * @return          Whether it fitted.
 */
static mmgr_bool hand_put(const uint8_t *src, size_t bytes)
{
    if ((RING_CAP - (g_hand_head - g_hand_tail)) < bytes)
    {
        return MMGR_FALSE;
    }

    const size_t at = g_hand_head & (RING_CAP - 1u);
    const size_t first = ((at + bytes) > RING_CAP) ? (RING_CAP - at) : bytes;

    memcpy(g_hand_buf + at, src, first);
    if (first != bytes)
    {
        memcpy(g_hand_buf, src + first, bytes - first);
    }
    g_hand_head += bytes;
    return MMGR_TRUE;
}

/**
 * @brief Reads a span out of the hand rolled ring, splitting it at the end when it crosses.
 *
 * @param[out] dst   Where the bytes go [BORROWS].
 * @param[in]  bytes How many to take.
 * @return           How many were taken.
 */
static size_t hand_read(uint8_t *dst, size_t bytes)
{
    const size_t held = g_hand_head - g_hand_tail;
    const size_t take = (held < bytes) ? held : bytes;
    const size_t at = g_hand_tail & (RING_CAP - 1u);
    const size_t first = ((at + take) > RING_CAP) ? (RING_CAP - at) : take;

    memcpy(dst, g_hand_buf + at, first);
    if (first != take)
    {
        memcpy(dst + first, g_hand_buf, take - first);
    }
    g_hand_tail += take;
    return take;
}

/**
 * @brief One segment claimed, published, taken and released.
 *
 * @return A value the harness keeps, so the cycle is not discarded.
 * @note The segment layer has no hand rolled counterpart worth writing: it is the ring's own
 *       producer and consumer handshake, not a copy, so the row is what it costs rather than a
 *       comparison against anything.
 */
static uintptr_t ring_segment_cycle(void)
{
    size_t idx = 0u;
    uintptr_t seen = 0u;

    if (MMGR_CALL(iteratio_infinita.seg_next, InfinCfg, .ring = &g_ring, .out_index = &idx))
    {
        seen |= (uintptr_t)MMGR_CALL(iteratio_infinita.seg_at, InfinCfg, .ring = &g_ring, .index = idx);
        MMGR_CALL(iteratio_infinita.seg_publish, InfinCfg, .ring = &g_ring);
    }
    if (MMGR_CALL(iteratio_infinita.seg_front, InfinCfg, .ring = &g_ring, .out_index = &idx))
    {
        seen |= idx + 1u;
        MMGR_CALL(iteratio_infinita.seg_release, InfinCfg, .ring = &g_ring);
    }
    return seen;
}

/**
 * @brief One loculus found, held, read back and given up again.
 *
 * @return A value the harness keeps, so the cycle is not discarded.
 * @note Also no counterpart: a keepout is a region the ring promises not to touch, which a plain
 *       ring has no notion of at all.
 */
static uintptr_t ring_loculus_cycle(void)
{
    const mmgr_word ready = MMGR_CALL(iteratio_infinita.loculus_ready, InfinCfg, .ring = &g_ring);
    const mmgr_iword slot = MMGR_CALL(iteratio_infinita.loculus_next, InfinCfg, .ring = &g_ring, .mask = ready);
    uintptr_t seen = (uintptr_t)ready;

    if (slot >= 0)
    {
        // Explicit cast takes the reported index into the size_t the calls below name it with
        const size_t idx = (size_t)slot;

        if (MMGR_CALL(iteratio_infinita.loculus_hold, InfinCfg, .ring = &g_ring, .index = idx, .src = g_a,
                      .bytes = RING_SPAN))
        {
            seen |= (uintptr_t)MMGR_CALL(iteratio_infinita.loculus_keepout, InfinCfg, .ring = &g_ring,
                                         .index = idx);
            MMGR_CALL(iteratio_infinita.loculus_drop, InfinCfg, .ring = &g_ring, .index = idx);
            MMGR_CALL(iteratio_infinita.loculus_mark, InfinCfg, .ring = &g_ring, .index = idx);
        }
    }
    return seen;
}

/**
 * @brief One byte taken through read_byte, against the same byte taken by hand.
 *
 * @return The byte, so the take is not discarded.
 */
static uintptr_t ring_byte_mmgr(void)
{
    uint8_t held = 0u;

    (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_a, .bytes = 1u);
    (void)MMGR_CALL(iteratio_infinita.read_byte, InfinCfg, .ring = &g_ring, .dst = &held);
    return (uintptr_t)held;
}

/**
 * @brief The same one byte through the hand rolled ring.
 *
 * @return The byte, so the take is not discarded.
 */
static uintptr_t ring_byte_hand(void)
{
    uint8_t held = 0u;

    (void)hand_put(g_a, 1u);
    (void)hand_read(&held, 1u);
    return (uintptr_t)held;
}

/**
 * @brief A span looked at without taking it, then the tail moved past it.
 *
 * @return A value the harness keeps.
 */
static uintptr_t ring_peek_mmgr(void)
{
    (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_a, .bytes = RING_SPAN);
    MMGR_CALL(iteratio_infinita.peek, InfinCfg, .ring = &g_ring, .dst = g_ring_out, .bytes = RING_SPAN,
              .offset = 0u);
    MMGR_CALL(iteratio_infinita.consume, InfinCfg, .ring = &g_ring, .bytes = RING_SPAN);
    return (uintptr_t)g_ring_out[0];
}

/**
 * @brief The same, by hand: a copy that does not advance, then the tail moved past it.
 *
 * @return A value the harness keeps.
 */
static uintptr_t ring_peek_hand(void)
{
    (void)hand_put(g_a, RING_SPAN);

    const size_t at = g_hand_tail & (RING_CAP - 1u);
    const size_t first = ((at + RING_SPAN) > RING_CAP) ? (RING_CAP - at) : RING_SPAN;

    memcpy(g_ring_out, g_hand_buf + at, first);
    if (first != RING_SPAN)
    {
        memcpy(g_ring_out + first, g_hand_buf, RING_SPAN - first);
    }
    g_hand_tail += RING_SPAN;
    return (uintptr_t)g_ring_out[0];
}

/**
 * @brief One span in and the same span out of the mmgr ring.
 *
 * @return A value the harness keeps, so the pass is not discarded.
 */
static uintptr_t ring_round_mmgr(void)
{
    (void)MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &g_ring, .src = g_a, .bytes = RING_SPAN);
    return (uintptr_t)MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &g_ring, .dst = g_ring_out,
                                .bytes = RING_SPAN);
}

/**
 * @brief The same span in and out of the hand rolled ring.
 *
 * @return A value the harness keeps, so the pass is not discarded.
 */
static uintptr_t ring_round_hand(void)
{
    (void)hand_put(g_a, RING_SPAN);
    return (uintptr_t)hand_read(g_ring_out, RING_SPAN);
}

/**
 * @brief One cell allocated on the persistent tier and released straight back.
 *
 * @return The address it was handed, so the allocation is not discarded.
 */
static uintptr_t cellblock_persistent_alloc_release(void)
{
    void *const held = ram.general.persistent_buf_alloc(g_take);

    ram.general.persistent_buf_release(held);
    return (uintptr_t)held;
}

/**
 * @brief One allocation taken from the heap and given straight back.
 *
 * @return The address it was handed, so the take is not discarded.
 */
static uintptr_t heap_take_give(void)
{
    void *const held = malloc(g_take);

    free(held);
    return (uintptr_t)held;
}

/**
 * @brief Takes eight tenancies from the interim end and gives them all back with one mark.
 *
 * @return A value the harness keeps, so the run is not discarded.
 * @note What a mark is for. The top is noted once, eight allocations move it down, and restoring the
 *       mark releases all eight in a single store.
 */
static uintptr_t cellblock_temporary_mark_run(void)
{
    const size_t mark = ram.general.temporary_buf_mark();
    uintptr_t seen = 0u;

    for (unsigned index = 0; index < 8u; index++)
    {
        seen |= (uintptr_t)ram.general.temporary_buf_alloc(g_take);
    }
    ram.general.temporary_buf_release(mark);
    return seen;
}

/**
 * @brief The same eight tenancies from the heap, which has to be told about each one twice.
 *
 * @return A value the harness keeps, so the run is not discarded.
 * @note A heap has no mark, so the eight frees are the only way to give the run back.
 */
static uintptr_t heap_run_free(void)
{
    void *held[8];
    uintptr_t seen = 0u;

    for (unsigned index = 0; index < 8u; index++)
    {
        held[index] = malloc(g_take);
        seen |= (uintptr_t)held[index];
    }
    for (unsigned index = 0; index < 8u; index++)
    {
        free(held[index]);
    }
    return seen;
}

/**
 * @brief Fills the two read buffers with n identical bytes, none of them the byte chr seeks.
 *
 * @param[in] n Bytes to fill.
 * @note The two agree throughout, so cmp runs the whole length rather than stopping early, and the
 *       alphabet never reaches 0xFF, so chr does too.
 */
static void fill(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        // Explicit cast narrows the counter into the byte lane it fills
        g_a[i] = (uint8_t)('a' + (i % 15));
        g_b[i] = (uint8_t)('a' + (i % 15));
    }
}

/**
 * @brief One pass: every case at every length.
 */
void dbench_run(void)
{
    static const size_t lens[] = {8u, 16u, 32u, 64u, 128u, 512u, 2048u};

    for (;;)
    {
        DBENCH_BANNER("memoria vs libc");

        printf("DB bitor_check     disagreements=%u\n", (unsigned)bitor_is_correct());

        for (unsigned li = 0; li < (sizeof lens / sizeof lens[0]); li++)
        {
            const size_t n = lens[li];
            const uint32_t iters = (n <= 64u) ? 20000u : ((n <= 512u) ? 4000u : 1000u);

            fill(n);

            DBENCH_AB("cmp", iters, n,
                      DBENCH_KEEP(MMGR_CALL(memor.cmp, MemoriaCfg, .src = g_a, .other = g_b, .bytes = n)),
                      DBENCH_KEEP(memcmp(g_a, g_b, n)));

            // cmp is 2.9x behind memcmp on the C6 and the gap grows with the length, so it is per
            // byte rather than a fixed cost. The entry against the same walk written out here, and
            // then that walk against the aligned load, which is the only difference between them.
            // The length reaches both arms through a volatile. Taken from the lens table directly it
            // is a constant the compiler can specialise the local walk against and cannot specialise
            // the entry against, and the row then reports that difference as if it were the entry's
            // fault. That mistake has already been made once in this work and retracted.
            g_cmp_len = n;

            DBENCH_AB("cmp_entry", iters, n,
                      DBENCH_KEEP(MMGR_CALL(memor.cmp, MemoriaCfg, .src = g_a, .other = g_b, .bytes = g_cmp_len)),
                      DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)));

            DBENCH_AB("cmp_align", iters, n, DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)),
                      DBENCH_KEEP(cmp_aligned(g_a, g_b, g_cmp_len)));

            {
                const BenchCmpCtx ctx = {.src = g_a, .other = g_b, .bytes = g_cmp_len};

                DBENCH_AB("cmp_ctx", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)),
                          DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)));

                DBENCH_AB("cmp_hoist", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)),
                          DBENCH_KEEP(cmp_ctx_hoisted(&ctx)));

                // The plain pointer walk again, this time handed its two regions through pointers
                // the compiler cannot trace to an object. If it stays fast the shape is worth
                // taking into the backend; if it slows to the context walk, the fast arm was only
                // ever fast because it could see g_a and g_b.
                g_cmp_a = g_a;
                g_cmp_b = g_b;
                DBENCH_AB("cmp_opaque", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)),
                          DBENCH_KEEP(cmp_unaligned(g_cmp_a, g_cmp_b, g_cmp_len)));

                // The aligned load, asked again where it can mean something. The earlier row put it
                // at 1.00, but both arms there were handed g_a and g_b, so the compiler already knew
                // the addresses were on a boundary and emitted the same instruction either way. With
                // pointers it cannot trace, the unaligned load has to actually be unaligned.
                DBENCH_AB("cmp_al_opaque", iters, n,
                          DBENCH_KEEP(cmp_unaligned(g_cmp_a, g_cmp_b, g_cmp_len)),
                          DBENCH_KEEP(cmp_aligned(g_cmp_a, g_cmp_b, g_cmp_len)));
            }

            DBENCH_AB("chr", iters, n,
                      DBENCH_KEEP(MMGR_CALL(memor.chr, MemoriaCfg, .src = g_a, .bytes = n, .val = 0xFFu)),
                      DBENCH_KEEP(memchr(g_a, 0xFF, n)));

            DBENCH_AB("cpy", iters, n,
                      DBENCH_KEEP(
                          (MMGR_CALL(memor.cpy, MemoriaCfg, .dst = g_d, .src = g_a, .bytes = n), (uintptr_t)g_d)),
                      DBENCH_KEEP(memcpy(g_d, g_a, n)));

            DBENCH_AB("set", iters, n,
                      DBENCH_KEEP(
                          (MMGR_CALL(memor.set, MemoriaCfg, .dst = g_d, .bytes = n, .val = 0x5Au), (uintptr_t)g_d)),
                      DBENCH_KEEP(memset(g_d, 0x5A, n)));

            // The two moves, which had no row. Both are given regions that genuinely overlap, since
            // a move handed regions that do not is only a copy and would measure cpy again. The
            // destination sits one word inside the source for the upward case and one word outside
            // it for the downward one, which is the direction each is named for and the case where
            // choosing the wrong one corrupts the result rather than merely being slow.
            DBENCH_AB("move_up", iters, n,
                      DBENCH_KEEP((MMGR_CALL(memor.move_up, MemoriaCfg, .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d,
                                             .bytes = n),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP(memmove(g_d + MMGR_ALIGN_BYTES, g_d, n)));

            DBENCH_AB("move_down", iters, n,
                      DBENCH_KEEP((MMGR_CALL(memor.move_down, MemoriaCfg, .dst = g_d, .src = g_d + MMGR_ALIGN_BYTES,
                                             .bytes = n),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP(memmove(g_d, g_d + MMGR_ALIGN_BYTES, n)));
        }

        // Byte order, which had no bench anywhere. The counterpart is not a libc call: a caller
        // reaching for this writes __builtin_bswap and a store, and on both these parts that builtin
        // is a sequence rather than an instruction, since neither carries a byte reversal in its
        // base ISA. wr and rd are measured against the builtin plus the memory access they perform,
        // so both sides move the same bytes.
        {
            const uint32_t iters = 20000u;

            DBENCH_AB("rev32", iters, 4u,
                      DBENCH_KEEP(MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = g_swap_val,
                                            .width = MMGR_ENDIAN_32)),
                      DBENCH_KEEP(__builtin_bswap32((uint32_t)g_swap_val)));

            DBENCH_AB("rev64", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = g_swap_val,
                                            .width = MMGR_ENDIAN_64)),
                      DBENCH_KEEP(__builtin_bswap64(g_swap_val)));

            DBENCH_AB("wr32", iters, 4u,
                      DBENCH_KEEP(MMGR_CALL(magna_extremitas.wr, EndianCfg, .dst = g_d + g_swap_off,
                                            .val = g_swap_val, .width = MMGR_ENDIAN_32)),
                      DBENCH_KEEP((memcpy(g_d + g_swap_off, &(uint32_t){__builtin_bswap32((uint32_t)g_swap_val)},
                                          4u),
                                   (uintptr_t)g_d)));

            DBENCH_AB("rd32", iters, 4u,
                      DBENCH_KEEP(MMGR_CALL(magna_extremitas.rd, EndianCfg, .src = g_d + g_swap_off,
                                            .width = MMGR_ENDIAN_32)),
                      DBENCH_KEEP(__builtin_bswap32(*(const uint32_t *)(g_d + g_swap_off))));
        }

        // The pool against the heap, which had no row against libc anywhere - the bench that exists
        // compares locus_carcerum with ProtoCore, not with malloc. One allocate and one release at each
        // end, and then a run of eight taken and released in one move, which is what a mark is for
        // and what a heap has no equivalent of: the eight frees are the only way to answer it.
        {
            const uint32_t iters = 5000u;

            DBENCH_AB("cellblock_persistent", iters, 64u, DBENCH_KEEP(cellblock_persistent_alloc_release()),
                      DBENCH_KEEP(heap_take_give()));

            DBENCH_AB("cellblock_temporary", iters, 64u, DBENCH_KEEP(cellblock_temporary_mark_run()),
                      DBENCH_KEEP(heap_run_free()));
        }

        // The ring. There is no libc ring, so the counterpart is the one a caller writes instead:
        // two indices into a power of two buffer with memcpy moving the bytes and a split when a
        // span crosses the end. The first two rows are the index arithmetic on its own, which is
        // where the ring's own logic lives and where no bytes move at all; the third is a span in
        // and the same span out, which is copy bound and inherits whatever proxim.read costs.
        {
            const uint32_t iters = 5000u;

            (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_ring_buf,
                            .capacity = RING_CAP, .segment_count = RING_SEGS);
            g_hand_head = 0u;
            g_hand_tail = 0u;

            DBENCH_AB("ring_avail", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring)),
                      DBENCH_KEEP(g_hand_head - g_hand_tail));

            // The same question against two indices read the way the ring reads its own. The row
            // above is against a plain subtraction, which is not safe across two contexts and so is
            // not the same job; this one is, and the gap between the two rows is the ordering.
            DBENCH_AB("ring_avail_ord", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(iteratio_infinita.available, InfinCfg, .ring = &g_ring)),
                      DBENCH_KEEP(atomic_load_explicit(&g_ord_head, memory_order_acquire) -
                                  atomic_load_explicit(&g_ord_tail, memory_order_acquire)));

            DBENCH_AB("ring_vacant", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(iteratio_infinita.vacant, InfinCfg, .ring = &g_ring)),
                      DBENCH_KEEP(RING_CAP - (g_hand_head - g_hand_tail)));

            DBENCH_AB("ring_round", iters, RING_SPAN, DBENCH_KEEP(ring_round_mmgr()),
                      DBENCH_KEEP(ring_round_hand()));

            DBENCH_AB("ring_byte", iters, 1u, DBENCH_KEEP(ring_byte_mmgr()), DBENCH_KEEP(ring_byte_hand()));

            DBENCH_AB("ring_peek", iters, RING_SPAN, DBENCH_KEEP(ring_peek_mmgr()),
                      DBENCH_KEEP(ring_peek_hand()));

            DBENCH_AB("ring_init", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring,
                                            .buf = g_ring_buf, .capacity = RING_CAP,
                                            .segment_count = RING_SEGS)),
                      DBENCH_KEEP((g_hand_head = 0u, g_hand_tail = 0u, (uintptr_t)1)));

            // The two layers a plain ring has no answer for at all, so these are absolute costs
            // rather than comparisons: a segment handshake, and a loculus taken and given back.
            (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_ring_buf,
                            .capacity = RING_CAP, .segment_count = RING_SEGS);
            DBENCH_OP("ring_segment", iters, DBENCH_KEEP(ring_segment_cycle()));
            DBENCH_OP("ring_loculus", iters, DBENCH_KEEP(ring_loculus_cycle()));
        }

        // spatium and the two locus_carcerum entries that report rather than hand out memory. None of
        // these has a counterpart and none of them loops: a span is four fields and the questions
        // asked of it are field arithmetic. What a row can say is whether that is what they cost, or
        // whether one of them is doing something the shape does not suggest.
        {
            const uint32_t iters = 20000u;

            DBENCH_OP("span_from", iters,
                      DBENCH_KEEP(MMGR_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = g_take).cap));

            DBENCH_OP("span_after", iters,
                      DBENCH_KEEP(MMGR_CALL(spat.after, SpatiumCfg,
                                            .s = MMGR_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP),
                                            .n = g_take)
                                      .cap));

            DBENCH_OP("span_ok", iters,
                      DBENCH_KEEP(MMGR_CALL(spat.ok, SpatiumCfg,
                                            .s = MMGR_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP))));

            DBENCH_OP("cellblock_who_owns_buf", iters,
                      DBENCH_KEEP(ram.general.who_owns_buf((const void *)(g_d + g_swap_off))));

            DBENCH_OP("cellblock_buf_available", iters, DBENCH_KEEP(ram.general.buf_available()));
        }

        // The byte cost table, the last entry in the library with no row. It is one indexed load
        // from 256 bytes of static data, and the five files in that module differ only in what the
        // data says, so there is nothing in it to be slow. The row exists to say that rather than
        // to leave it assumed.
        {
            const uint32_t iters = 20000u;

            DBENCH_OP("ancorae_cost", iters,
                      DBENCH_KEEP(MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)g_swap_off)));
        }

        // The placement decision and the buffer pair, which had no rows because the module they are
        // in compiles to nothing unless MMGR_ENABLE_EXTRAM is set. This image sets it. None of the
        // five loops: place is comparisons and branches, and the pingpong entries are one field
        // each. What a row can say is whether that is what they cost.
        {
            const uint32_t iters = 20000u;
            PingPong pingpong;

            MMGR_CALL(exter.pingpong_init, ExternumCfg, .pingpong = &pingpong);

            // Below the threshold with room in both, which takes internal memory on the first test.
            DBENCH_OP("ext_place_dram", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.place, ExternumCfg, .size = g_ext_small,
                                            .dma_required = MMGR_FALSE, .free_dram = 65536u,
                                            .free_psram = 1048576u, .psram_threshold = 4096u,
                                            .dram_reserve = 8192u)));

            // At or above it, which tries external memory first.
            DBENCH_OP("ext_place_psram", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.place, ExternumCfg, .size = g_ext_large,
                                            .dma_required = MMGR_FALSE, .free_dram = 65536u,
                                            .free_psram = 1048576u, .psram_threshold = 4096u,
                                            .dram_reserve = 8192u)));

            // A DMA request, which only internal memory can answer. Both fits are worked out before
            // the branches, so this path pays for an external test whose answer it cannot use.
            DBENCH_OP("ext_place_dma", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.place, ExternumCfg, .size = g_ext_small,
                                            .dma_required = MMGR_TRUE, .free_dram = 65536u,
                                            .free_psram = 1048576u, .psram_threshold = 4096u,
                                            .dram_reserve = 8192u)));

            DBENCH_OP("ext_pp_fill", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.pingpong_fill, ExternumCfg, .pingpong = &pingpong)));

            DBENCH_OP("ext_pp_drain", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.pingpong_drain, ExternumCfg, .pingpong = &pingpong)));

            DBENCH_OP("ext_pp_swap", iters,
                      DBENCH_KEEP(MMGR_CALL(exter.pingpong_swap, ExternumCfg, .pingpong = &pingpong)));
        }

        // The bit writer, which had no rows anywhere. init and align are a handful of field stores;
        // put is the one that loops, and what it loops over is bytes.
        {
            const uint32_t iters = 20000u;
            mmgr_bitor bw = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP);

            DBENCH_OP("bit_init", iters,
                      DBENCH_KEEP(MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP).cap));

            // Reset each pass, so no row measures a writer that has already filled its buffer and
            // is refusing on the overflow latch rather than writing.
            DBENCH_OP("bit_put8", iters,
                      (bw = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       MMGR_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 8u),
                       DBENCH_KEEP(bw.bytes_written)));

            DBENCH_OP("bit_put64", iters,
                      (bw = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       MMGR_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 64u),
                       DBENCH_KEEP(bw.bytes_written)));

            // A width that does not divide into bytes, which is the case the residue exists for.
            DBENCH_OP("bit_put13", iters,
                      (bw = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       MMGR_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 13u),
                       DBENCH_KEEP(bw.bytes_written)));

            DBENCH_OP("bit_align", iters,
                      (bw = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       MMGR_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 13u),
                       MMGR_CALL(bitio.align, BitorumCfg, .writer = &bw), DBENCH_KEEP(bw.bytes_written)));

            // The loop as written against the residue merged once. Sixty four bits is eight bytes,
            // so seven of the eight passes in the A arm recompute a room of eight and merge with a
            // residue of zero.
            mmgr_bitor one;
            mmgr_bitor two;

            DBENCH_AB("bit_shape64", iters, 8u,
                      (one = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_ref(&one, g_swap_val, 64u), DBENCH_KEEP(one.bytes_written)),
                      (two = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_split(&two, g_swap_val, 64u), DBENCH_KEEP(two.bytes_written)));

            // And at a width of one byte, where there is no second pass for the split to help and
            // the question is whether taking the first byte on its own costs anything.
            DBENCH_AB("bit_shape8", iters, 1u,
                      (one = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_ref(&one, g_swap_val, 8u), DBENCH_KEEP(one.bytes_written)),
                      (two = MMGR_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_split(&two, g_swap_val, 8u), DBENCH_KEEP(two.bytes_written)));
        }

        // take_be's shape against the one its mirror already has. put_be takes the eight byte case
        // in one branch and returns; take_be gathers the eight bytes and then still tests four, two
        // and one on the way out, which is three tests that cannot be true. Both arms reverse once
        // at the end, as the entry does, so what separates them is the tests alone.
        {
            const uint32_t iters = 20000u;

            // The count comes through a volatile on both arms. Handed the literal, GCC folds the
            // whole branch structure and both arms report the harness floor, which is what the
            // first run of these rows did: 5.0 against 5.0 at eight bytes and at four.
            g_wire_n = 8u;
            DBENCH_AB("wire_take8_shape", iters, 8u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));

            // The four byte case as well, since an exit added for eight must not cost the counts
            // that never reached it.
            g_wire_n = 4u;
            DBENCH_AB("wire_take4_shape", iters, 4u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));

            g_wire_n = 7u;
            DBENCH_AB("wire_take7_shape", iters, 7u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));
        }

        // What mpint_fixed writes twice. It zeroes the whole field and then lays the value into the
        // end of it, so every byte of the value is stored as a zero and then stored again. Only the
        // run ahead of the value has to be cleared, and how wide that is is known before either
        // store: it is the field less the value.
        {
            const uint32_t iters = 20000u;
            const size_t field = 32u;
            const size_t vlen = 20u;

            DBENCH_AB("wire_mpint_fill", iters, (unsigned)field,
                      (MMGR_CALL(memor.set, MemoriaCfg, .dst = g_b, .val = (uint8_t)0, .bytes = field),
                       MMGR_CALL(memor.cpy, MemoriaCfg, .dst = g_b + (field - vlen), .src = g_a,
                                 .bytes = vlen),
                       DBENCH_KEEP(g_b)),
                      (MMGR_CALL(memor.set, MemoriaCfg, .dst = g_b, .val = (uint8_t)0,
                                 .bytes = field - vlen),
                       MMGR_CALL(memor.cpy, MemoriaCfg, .dst = g_b + (field - vlen), .src = g_a,
                                 .bytes = vlen),
                       DBENCH_KEEP(g_b)));
        }

        // The wire verbs, which had no row anywhere. There is no libc counterpart to any of them,
        // so these are absolute costs against the harness floor. What a row can say is which of the
        // six carries work and which is a store with a bounds test in front of it.
        //
        // Every append is given a span reset first, so none of them measures a span that has
        // already overflowed and is refusing on the latch rather than doing the work.
        {
            const uint32_t iters = 20000u;
            mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP);
            mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = g_a, .cap = CAP);
            uint64_t got = 0u;
            const uint8_t *blob = NULL;
            size_t blen = 0u;

            DBENCH_OP("wire_put", iters,
                      (MMGR_CALL(spat.reset, SpatiumCfg, .at = &w),
                       MMGR_CALL(byteio.put, OctetusCfg, .write_span = &w, .byte = 0x5Au), DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_put_be4", iters,
                      (MMGR_CALL(spat.reset, SpatiumCfg, .at = &w),
                       MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 4u),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_put_be8", iters,
                      (MMGR_CALL(spat.reset, SpatiumCfg, .at = &w),
                       MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 8u),
                       DBENCH_KEEP(w.pos)));

            // Seven bytes takes three stores where eight takes one, which is the shape the entry is
            // written for and the case a count that is not a power of two lands on.
            DBENCH_OP("wire_put_be7", iters,
                      (MMGR_CALL(spat.reset, SpatiumCfg, .at = &w),
                       MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 7u),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_raw", iters,
                      (MMGR_CALL(spat.reset, SpatiumCfg, .at = &w),
                       MMGR_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = g_a, .bytes = g_take),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_take_be4", iters,
                      (r.pos = 0u, r.err = MMGR_FALSE,
                       DBENCH_KEEP(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got,
                                             .bytes = 4u))));

            DBENCH_OP("wire_take_be8", iters,
                      (r.pos = 0u, r.err = MMGR_FALSE,
                       DBENCH_KEEP(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got,
                                             .bytes = 8u))));

            // rd_str reads a four byte length and then points at the run behind it, so the span it
            // reads from is built with a length that fits inside the buffer rather than whatever
            // g_a happens to hold.
            MMGR_CALL(spat.reset, SpatiumCfg, .at = &w);
            MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = 64u, .bytes = 4u);
            MMGR_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = g_a, .bytes = 64u);

            mmgr_cspan rs = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = g_d, .cap = CAP);

            DBENCH_OP("wire_rd_str", iters,
                      (rs.pos = 0u, rs.err = MMGR_FALSE,
                       DBENCH_KEEP(MMGR_CALL(byteio.rd_str, OctetusCfg, .read_span = &rs, .blob = &blob,
                                             .blob_bytes = &blen))));

            // mpint_fixed right aligns an integer into a whole field, so it fills the field every
            // call: a zero run ahead of the value and then the value itself. The field is small
            // here because that is what a fixed width integer field is.
            mmgr_span field = MMGR_CALL(spat.from, SpatiumCfg, .buf = g_b, .cap = 32u);

            DBENCH_OP("wire_mpint", iters,
                      DBENCH_KEEP(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &field, .src = g_a,
                                            .bytes = 20u)));
        }

        DBENCH_DONE();
    }
}

DBENCH_MAIN("memoria")
