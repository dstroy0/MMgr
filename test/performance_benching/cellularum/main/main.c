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
#include <stdlib.h>
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

/**
 * @brief Where the copy row writes, so it does not disturb the buffer the compare rows read.
 *
 * @note cmp, eq and starts all depend on g_a and g_b agreeing for their whole length. A copy row
 *       writing into g_b would leave them agreeing anyway, but only by accident of what it copied.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_c[CAP];

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
 * @brief Text for the four converters, reached through a volatile so neither arm is folded away.
 *
 * @note Handed a literal, GCC evaluates strtol and strtod at compile time and the libc arm measures
 *       an empty loop. Read through these, both sides do the conversion they were asked for.
 * @note One value per shape the converters are asked for: a plain integer, one that fills the width,
 *       a decimal fraction, and one carrying an exponent, which is the path that reaches transformo.
 */
static const char *volatile g_int = "1234567";
static const char *volatile g_int_wide = "9223372036854775807";
static const char *volatile g_real = "3.14159265358979";
static const char *volatile g_real_exp = "1.7976931348623157e+308";

/**
 * @brief The mantissa g_real parses to, held where the compiler cannot fold the arithmetic on it.
 */
static volatile uint64_t g_scale_mant = 314159265358979ull;

/**
 * @brief Bytes from p up to the next word boundary, or cap when that is nearer.
 *
 * @param[in] p   Address to measure from [BORROWS].
 * @param[in] cap Bytes available.
 * @return        Bytes to walk one at a time before an aligned load is safe.
 */
static size_t head_bytes(const char *p, size_t cap)
{
    // Explicit cast reads the address as an integer so its low bits can be tested; the value is
    // never dereferenced through it and never converted back
    const size_t off = (size_t)((uintptr_t)p & (uintptr_t)(MMGR_SWAR_BYTES - 1u));
    const size_t need = (off == 0u) ? 0u : (MMGR_SWAR_BYTES - off);

    return (need > cap) ? cap : need;
}

/**
 * @brief Whether two terminated strings agree, loading through the unaligned word as cellul_eq does.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy, terminator included.
 * @return        Whether both end together with no difference before it.
 * @note The A arm, and the shape cellul_agree_cs carries: word.load reads from any address, so on a
 *       part without an unaligned load it compiles to a byte sequence, twice a step here because
 *       the compare reads two words where a length scan reads one.
 */
static mmgr_bool eq_unaligned(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            // Explicit cast narrows the two tests into the mmgr_bool container
            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The same walk, loading through the aligned word once both strings sit on a boundary.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy, terminator included.
 * @return        Whether both end together with no difference before it.
 * @note The B arm. cellul_len already walks a head to reach the aligned load, on the reasoning that
 *       the unaligned one is ten instructions where the aligned one is one. cellul_agree_cs does
 *       not, and reads two words a step rather than one, so this asks what that is costing.
 * @note Falls back to the unaligned walk when the two do not share an offset within a word, since
 *       no head can bring both onto a boundary at once.
 */
static mmgr_bool eq_aligned(const char *a, const char *b, size_t cap)
{
    // Explicit casts read both addresses as integers so their low bits can be compared
    const size_t skew_a = (size_t)((uintptr_t)a & (uintptr_t)(MMGR_SWAR_BYTES - 1u));
    const size_t skew_b = (size_t)((uintptr_t)b & (uintptr_t)(MMGR_SWAR_BYTES - 1u));

    if (skew_a != skew_b)
    {
        return eq_unaligned(a, b, cap);
    }

    const size_t lead = head_bytes(a, cap);
    size_t at = 0u;

    while (at != lead)
    {
        if ((a[at] != b[at]) || (a[at] == '\0'))
        {
            // Explicit cast narrows the comparison into the mmgr_bool container
            return (mmgr_bool)(a[at] == b[at]);
        }
        at++;
    }

    const size_t full = lead + (((cap - lead) / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES);

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            // Explicit cast narrows the two tests into the mmgr_bool container
            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

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

            // The two predicates and the bounded copy, which had no rows either. eq and starts are
            // the strcmp and strncmp shapes. copy is the strlcpy one - bounded, always terminated,
            // reporting what it wrote - and its libc side is spelled out rather than called, since
            // newlib has no strlcpy and strncpy answers a different question: it pads the whole
            // destination and leaves it unterminated when the source fills it.
            DBENCH_AB("eq", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(strcmp(g_a, g_b) == 0));

            DBENCH_AB("starts", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u,
                                            .other_cap = n + 1u)),
                      DBENCH_KEEP(strncmp(g_a, g_b, n) == 0));

            DBENCH_AB("copy", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP((strncpy(g_c, g_a, n), g_c[n] = '\0', (uintptr_t)g_c)));

            // eq loses to the ROM's strcmp and the gap widens with length, which points at the load
            // rather than at the walk: cellul_agree_cs reads through the unaligned word twice a
            // step, where cellul_len walks a head first and reads through the aligned one.
            DBENCH_AB("eq_align", iters, n, DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)),
                      DBENCH_KEEP(eq_aligned(g_a, g_b, n + 1u)));

            // The entry against the same loop written out here. Both walk two words a step with the
            // same test, so a difference that grows with the length is not the call overhead and
            // not the load; it is something the entry's walk does per word that this one does not.
            DBENCH_AB("eq_entry", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));
        }

        // The four converters, which had no row at all. These are the read side of what verba does
        // on the write side, and the libc they are against is not the ROM's assembly but newlib's
        // own strtol and strtod - and strtod carries a soft float multiply chain on both parts.
        // Length is not swept here: a converter's work is set by the digits in the text it is given,
        // so each row names its own input rather than taking one off the haystack.
        {
            const uint32_t iters = 5000u;

            DBENCH_AB("to_long", iters, 7u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_long, TransfiguroCfg, .src = g_int)),
                      DBENCH_KEEP(strtol(g_int, NULL, 10)));

            DBENCH_AB("to_ulong", iters, 19u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_ulong, TransfiguroCfg, .src = g_int_wide)),
                      DBENCH_KEEP(strtoul(g_int_wide, NULL, 10)));

            DBENCH_AB("to_double", iters, 16u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = g_real)),
                      DBENCH_KEEP(strtod(g_real, NULL)));

            DBENCH_AB("to_double_exp", iters, 23u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = g_real_exp)),
                      DBENCH_KEEP(strtod(g_real_exp, NULL)));

            DBENCH_AB("to_float", iters, 16u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = g_real)),
                      DBENCH_KEEP(strtof(g_real, NULL)));
        }

        // Where to_double's time goes. to_ulong puts digit accumulation at about thirteen cycles a
        // digit, so a fifteen digit parse is roughly two hundred of to_double's eleven hundred and
        // the rest is muto_scale. For this input muto_scale takes its exact path: the mantissa is
        // under 2^53 and the exponent is inside twenty two, so the answer is one soft double divide
        // by a power of ten held exactly. These two rows price that divide against the multiply by
        // its reciprocal, which is the obvious alternative and is not the same function - a divide
        // by an exact power of ten rounds correctly, and a multiply by its inverse does not, since
        // the inverse is not representable. The row says what that correctness is costing, nothing
        // more; it is not a proposal.
        {
            const uint32_t iters = 5000u;

            // The volatile is read inside each arm, not hoisted into a local first. Read once ahead
            // of the loop the whole expression is loop invariant and the compiler lifts it out, and
            // both arms then measure the empty harness.
            DBENCH_AB("soft_divmul", iters, 8u, DBENCH_KEEP((double)g_scale_mant / 1e14),
                      DBENCH_KEEP((double)g_scale_mant * 1e-14));
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
