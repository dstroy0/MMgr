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
#include <string.h>

#include "device_bench.h"

#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"

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

        DBENCH_DONE();
    }
}

DBENCH_MAIN("memoria")
