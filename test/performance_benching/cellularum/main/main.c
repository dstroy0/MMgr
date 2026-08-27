/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file main.c
 * @brief The cellularum string entries against the target's own libc, across input lengths.
 *
 * Run on the part rather than on a host. A desktop libc answers these with SSE or AVX, which reads
 * 16 to 48 bytes per instruction, and neither of these parts has anything of the kind; comparing a
 * machine-word SWAR against a vector unit measures the vector unit. The libc reached here is
 * ESP-ROM: strnlen, strchr, memcmp and strstr resolve to hand-written assembly in the part's mask
 * ROM, running without flash-cache pressure, while the library runs from flash through the icache.
 *
 * Lengths run from 8 bytes up, because a fixed prologue cannot show at one size alone and the
 * crossover against libc is the reading. Both arms see the same aligned buffer at the same address.
 *
 * The needle length is passed rather than measured, so find is timed doing the work it was asked
 * for instead of re-deriving what the caller already knew.
 */
#include <string.h>

#include "device_bench.h"

#include "cellularum_laboro/cellularum_laboro.h"

#define CAP 4096u

/**
 * @brief Haystack and comparison buffers, aligned and fixed for the whole run.
 *
 * @note The library is built for memory that arrives aligned, so an unaligned fixture would time a
 *       head walk it never performs in a real build.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_a[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_b[CAP];

static const char *const g_needle = "qx";

/**
 * @brief A needle whose first byte is common in the fill and whose pair never occurs.
 *
 * @note 'a' lands every fifteen bytes and is always followed by 'b', so an anchor on the first byte
 *       fires constantly and the verify always fails. g_needle is the opposite: 'q' is not in the
 *       alphabet at all, so an anchor never fires. The two bracket what a search can be asked to do.
 */
static const char *const g_hot = "ao";
#define NLEN 2u

/**
 * @brief Fills both buffers with n bytes that contain neither the needle nor the sought byte.
 *
 * @param[in] n Bytes to fill, leaving room for the terminator.
 * @note The alphabet stops short of 'q' followed by 'x' and never reaches 'z', so every scan runs
 *       the whole length rather than stopping early on a hit.
 */
static void fill(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        g_a[i] = (char)('a' + (i % 15));
        g_b[i] = (char)('a' + (i % 15));
    }
    g_a[n] = '\0';
    g_b[n] = '\0';
}

/**
 * @brief cellul.len with the entry pulled into the caller, to price the call the entries carry.
 *
 * @param[in] s   Bytes to measure [BORROWS].
 * @param[in] cap Readable extent.
 * @return        What cellul.len returns, from the same code.
 * @note MMGR_FLATTEN asks the compiler to inline everything this calls, which under link-time
 *       optimization reaches the entry body. Nothing in the library changes: this is the lever a
 *       caller has, exercised here so the price of the call is on record rather than assumed.
 * @note Measured against dispatch_len8 and direct_len8, which call the same entry the ordinary way.
 */
MMGR_FLATTEN static size_t len_flat(const char *s, size_t cap)
{
    return MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = cap);
}

/**
 * @brief One pass: every case at every length, then the dispatch cost on its own.
 */
void dbench_run(void)
{
    static const size_t lens[] = {8u, 16u, 32u, 64u, 128u, 512u, 2048u};

    for (;;)
    {
        DBENCH_BANNER("cellularum vs libc");

        for (unsigned li = 0; li < (sizeof lens / sizeof lens[0]); li++)
        {
            const size_t n = lens[li];
            const uint32_t iters = (n <= 64u) ? 20000u : ((n <= 512u) ? 4000u : 1000u);

            fill(n);

            DBENCH_AB("len", iters, n, DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(strnlen(g_a, n + 1u)));

            DBENCH_AB("chr", iters, n,
                      DBENCH_KEEP(
                          MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .byte = (uint8_t)'z')),
                      DBENCH_KEEP(strchr(g_a, 'z')));

            DBENCH_AB("cmp", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n)),
                      DBENCH_KEEP(memcmp(g_a, g_b, n)));

            DBENCH_AB("find", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u,
                                            .other = g_needle, .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_needle)));

            // The same search with a needle whose first byte is common. The fill cycles 'a' to 'o',
            // so 'a' turns up every fifteen bytes and 'o' never follows it - a walk that anchors on
            // one byte and verifies the other is asked to verify constantly and still never matches.
            // "qx" above is the opposite case, since 'q' does not occur at all. A walk is only
            // honestly measured against both.
            DBENCH_AB("find_hot", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u,
                                            .other = g_hot, .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_hot)));
        }

        // What the harness costs with no work in it: the loop, the counter and the volatile store,
        // and nothing else. Every row above carries this, so a ratio at a small n is mostly this
        // number on both sides and says less about the two functions than it appears to. Subtract it
        // before reading anything at n=8.
        fill(8u);
        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_a));

        // The same, plus one call the optimiser is not allowed to remove or inline away, which is
        // the floor any entry answers to.
        DBENCH_OP("floor_call", 20000u, DBENCH_KEEP(strnlen(g_a, 1u)));

        // What MMGR_CALL costs before any work happens. On Cortex-M4 the compound literal became a
        // memset of the whole argument type per call rather than folding into registers; on both
        // parts here the two rows come out identical, so it folds and costs nothing.
        DBENCH_OP("dispatch_len8", 20000u,
                  DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = 9u)));
        DBENCH_OP("direct_len8", 20000u,
                  DBENCH_KEEP(mmgr_cellul_len(&(CatenaFinitaCfg){.src = g_a, .cap = 9u})));

        // The same work with the entry pulled into the caller. The gap against the two rows above is
        // what the entry call costs, and every short-length row carries it.
        DBENCH_OP("flat_len8", 20000u, DBENCH_KEEP(len_flat(g_a, 9u)));
        fill(64u);
        DBENCH_OP("flat_len64", 20000u, DBENCH_KEEP(len_flat(g_a, 65u)));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("cellularum")
