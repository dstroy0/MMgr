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

#include "carceribus/carceribus.h"
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Bytes the pool the allocator rows take from holds.
 */
#define ARENA_BYTES 4096u

/**
 * @brief A region with one unwatched pool, which is what the allocator rows take from.
 *
 * @note Declared, not initialised at run time: the storage, its alignment and the pool's state are
 *       all emitted as data, so nothing here runs before main and the first byte is an address the
 *       linker resolved. That is the half malloc cannot answer for at any speed.
 */
Carceribus(ram, MMGR_SOLUTA(pool, ARENA_BYTES));

/**
 * @brief The take size, hidden so neither allocator sees a constant.
 */
static volatile size_t g_take = 64u;

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

    if (MMGR_CALL(iteratio_infinita.seg_next, InfinCfg, .ring = &g_ring, .out = &idx))
    {
        seen |= (uintptr_t)MMGR_CALL(iteratio_infinita.seg_at, InfinCfg, .ring = &g_ring, .idx = idx);
        MMGR_CALL(iteratio_infinita.seg_publish, InfinCfg, .ring = &g_ring);
    }
    if (MMGR_CALL(iteratio_infinita.seg_front, InfinCfg, .ring = &g_ring, .out = &idx))
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

        if (MMGR_CALL(iteratio_infinita.loculus_hold, InfinCfg, .ring = &g_ring, .idx = idx, .src = g_a,
                      .bytes = RING_SPAN))
        {
            seen |= (uintptr_t)MMGR_CALL(iteratio_infinita.loculus_keepout, InfinCfg, .ring = &g_ring,
                                         .idx = idx);
            MMGR_CALL(iteratio_infinita.loculus_drop, InfinCfg, .ring = &g_ring, .idx = idx);
            MMGR_CALL(iteratio_infinita.loculus_mark, InfinCfg, .ring = &g_ring, .idx = idx);
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
              .off = 0u);
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
 * @brief One tenancy taken from the persist end and given straight back.
 *
 * @return The address it was handed, so the take is not discarded.
 */
static uintptr_t pool_take_give(void)
{
    void *const held = ram.pool.persist_capio(g_take);

    ram.pool.persist_reddo(held);
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
 * @note What a mark is for: the top is noted once, eight takes move it up, and restoring the mark
 *       releases all eight in a single store.
 */
static uintptr_t pool_run_mark(void)
{
    const size_t mark = ram.pool.interim_mark();
    uintptr_t seen = 0u;

    for (unsigned index = 0; index < 8u; index++)
    {
        seen |= (uintptr_t)ram.pool.interim_capio(g_take);
    }
    ram.pool.interim_reddo(mark);
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
        // compares carceribus with ProtoCore, not with malloc. One take and one give back at each
        // end, and then a run of eight taken and released in one move, which is what a mark is for
        // and what a heap has no equivalent of: the eight frees are the only way to answer it.
        {
            const uint32_t iters = 5000u;

            DBENCH_AB("pool_persist", iters, 64u, DBENCH_KEEP(pool_take_give()), DBENCH_KEEP(heap_take_give()));

            DBENCH_AB("pool_interim", iters, 64u, DBENCH_KEEP(pool_run_mark()), DBENCH_KEEP(heap_run_free()));
        }

        // The ring. There is no libc ring, so the counterpart is the one a caller writes instead:
        // two indices into a power of two buffer with memcpy moving the bytes and a split when a
        // span crosses the end. The first two rows are the index arithmetic on its own, which is
        // where the ring's own logic lives and where no bytes move at all; the third is a span in
        // and the same span out, which is copy bound and inherits whatever proxim.read costs.
        {
            const uint32_t iters = 5000u;

            (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_ring_buf,
                            .cap = RING_CAP, .nsegs = RING_SEGS);
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
                                            .buf = g_ring_buf, .cap = RING_CAP, .nsegs = RING_SEGS)),
                      DBENCH_KEEP((g_hand_head = 0u, g_hand_tail = 0u, (uintptr_t)1)));

            // The two layers a plain ring has no answer for at all, so these are absolute costs
            // rather than comparisons: a segment handshake, and a loculus taken and given back.
            (void)MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &g_ring, .buf = g_ring_buf,
                            .cap = RING_CAP, .nsegs = RING_SEGS);
            DBENCH_OP("ring_segment", iters, DBENCH_KEEP(ring_segment_cycle()));
            DBENCH_OP("ring_loculus", iters, DBENCH_KEEP(ring_loculus_cycle()));
        }

        DBENCH_DONE();
    }
}

DBENCH_MAIN("memoria")
