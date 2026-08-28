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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "device_bench.h"

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"

/**
 * @brief Inputs for the bit and lane rows, hidden so neither arm folds.
 */
static volatile uint64_t g_bits = 0x0000123456789000ull;
static volatile mmgr_word g_mask = (mmgr_word)0x8080008000800080ull;
static volatile mmgr_word g_word = (mmgr_word)0x6162630061626300ull;

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
 * @brief Zero, where the compiler cannot see that it is zero.
 *
 * @note Every code point in these loops is exclusive-ored with this. Without it the byte is a loop
 *       counter the compiler can enumerate, the class test folds for all 256 values, and both arms
 *       report the harness floor - which is exactly what the first version of these rows did.
 */
static volatile unsigned g_byte_bias = 0u;

/**
 * @brief The class to test, where the compiler cannot see which one it is.
 */
static volatile int g_class = 0;

/**
 * @brief Counts the code points isdigit accepts, over the whole byte range.
 *
 * @return How many of the 256 code points it accepts.
 * @note One function per class rather than one taking a function pointer, so the libc arm is a
 *       direct call the way a caller writes it and not an indirect one the mmgr arm does not pay.
 */
static uint32_t ascii_span_libc_digit(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {
        // Explicit cast takes the counter to the int ctype is defined over
        found += isdigit((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts the code points isalpha accepts, over the whole byte range.
 *
 * @return How many of the 256 code points it accepts.
 */
static uint32_t ascii_span_libc_alpha(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {
        // Explicit cast takes the counter to the int ctype is defined over
        found += isalpha((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts the code points isspace accepts, over the whole byte range.
 *
 * @return How many of the 256 code points it accepts.
 */
static uint32_t ascii_span_libc_space(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {
        // Explicit cast takes the counter to the int ctype is defined over
        found += isspace((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts the code points isprint accepts, over the whole byte range.
 *
 * @return How many of the 256 code points it accepts.
 */
static uint32_t ascii_span_libc_print(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {
        // Explicit cast takes the counter to the int ctype is defined over
        found += isprint((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief The agree walk with the terminator test reached through the lane entry, as it is written.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 */
static mmgr_bool agree_entry_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u) || (wa != wb))
        {
            return (mmgr_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The same walk with Mycroft's test written where it is used.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note Four operations either way. The question is whether reaching them through the lane entry
 *       costs anything a word, which is what clz turned out to be doing.
 */
static mmgr_bool agree_inline_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u) || (wa != wb))
        {
            return (mmgr_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief Mycroft's four operations, with the difference test moved ahead of them.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The control for the filter arm below, which changed the ordering as well as the test and so
 *       could not say which of the two it was measuring. This changes only the ordering.
 */
static mmgr_bool agree_reordered(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The same walk with the terminator test split into a cheap filter and a rare exact one.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note Mycroft's test is a subtract, a complement and two ands. The subtract and one and on their
 *       own - (w - ones) & high - never miss a zero: a zero lane borrows and its high bit is set
 *       whatever the lanes below it did. It over-reports, on a byte of 0x80 or above and on a borrow
 *       running into a lane, so a word it fires on still needs the exact test. A word it does not
 *       fire on is settled in two operations rather than four, and in text below 0x80 it never
 *       fires at all.
 */
static mmgr_bool agree_filter_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return MMGR_TRUE;
            }
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The walk with nothing in it but the two loads and the step.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        The words read, so the loads are not discarded.
 * @note The floor for the agree walk: whatever this costs a word, no version of the walk costs less.
 */
static mmgr_word part_loads(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    mmgr_word seen = 0u;
    size_t at = 0u;

    while (at != full)
    {
        seen |= MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        seen |= MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        at += MMGR_SWAR_BYTES;
    }
    return seen;
}

/**
 * @brief The same two loads with the difference test and its branch, and nothing else.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether the two agreed over every whole word.
 * @note Against part_loads this is what the compare and the branch cost a word.
 */
static mmgr_bool part_compare(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The compare walk with the terminator test added, which is the whole hot loop.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note Against part_compare this is what lane.has_zero and its branch cost a word.
 */
static mmgr_bool part_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The agree walk that marks a terminator rather than stopping at one.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The terminator has to be found once, not tested for once a word. Four words are read and
 *       their terminator flags and their differences are collected into two accumulators with no
 *       branch between them; one test then covers the block. A block that holds neither is settled
 *       in one branch instead of eight, and a block that holds either is walked again a word at a
 *       time to find which came first, which happens at most once in a call.
 * @note The rescan reads the same four words a second time. That is affordable because it runs once
 *       where the loop it replaces ran on every word.
 */
static mmgr_bool part_marked(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t block = 4u * MMGR_SWAR_BYTES;
    // Whole blocks only; whatever is left over is finished by the word walk below
    const size_t blocks = (full / block) * block;
    size_t at = 0u;

    while (at != blocks)
    {
        mmgr_word ends = 0u;
        mmgr_word diff = 0u;

        for (size_t k = 0; k < block; k += MMGR_SWAR_BYTES)
        {
            const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at + k);
            const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at + k);

            ends |= MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
            diff |= wa ^ wb;
        }

        if ((ends | diff) != 0u)
        {
            break;
        }
        at += block;
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The agree walk leaving on one combined test, with the exact one only where it leaves.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The hot path carries a difference and a two operation terminator filter folded into a single
 *       test: an exclusive or for the difference, and (w - ones) & high for the terminator, which
 *       never misses a zero but fires on a byte of 0x80 or above. Four operations and one branch
 *       where the exact test alone is four and two.
 * @note A word the test fires on is resolved once, outside the walk's arithmetic: a real difference
 *       returns, a real terminator returns, and a byte that only looked like one resumes the walk.
 *       The earlier attempt at this nested the tests three deep on the hot path and lost.
 */
static mmgr_bool part_onetest(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        // One test for both questions: the words differ, or a lane may hold the terminator
        if (((wa ^ wb) | ((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH)) != 0u)
        {
            if (wa != wb)
            {
                return MMGR_FALSE;
            }
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return MMGR_TRUE;
            }
            // A byte of 0x80 or above fired the filter without being a terminator; the words agree
            // and the walk carries on from the next one
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

/**
 * @brief The agree walk with each word's loads issued while the word before it is being tested.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The compiler will not do this itself. Both tests can leave the loop, so it cannot lift the
 *       next pair of loads above them - past an exit the load might not be one the walk is allowed
 *       to perform. The walk knows something the compiler does not: every word below full is
 *       readable whatever the tests say, so the next pair can issue before the current pair is
 *       examined and the load latency runs underneath the terminator arithmetic instead of after it.
 * @note The last word is left to the tail below, since there is no next one to read for it.
 */
static mmgr_bool part_pipelined(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    if (full == 0u)
    {
        return MMGR_TRUE;
    }

    mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a);
    mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b);

    // Stops one word short: the body reads the word after the one it tests
    while (at + MMGR_SWAR_BYTES != full)
    {
        const mmgr_word na = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at + MMGR_SWAR_BYTES);
        const mmgr_word nb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at + MMGR_SWAR_BYTES);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        wa = na;
        wb = nb;
        at += MMGR_SWAR_BYTES;
    }

    if (wa != wb)
    {
        return MMGR_FALSE;
    }
    return MMGR_TRUE;
}

/**
 * @brief cellul_diff_lanes, copied from the library rather than restated.
 *
 * @param[in] d Exclusive or of the two words.
 * @return      The lanes that differ, flagged in their high bit.
 */
static mmgr_word arm_diff_lanes(mmgr_word d)
{
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d);
}

/**
 * @brief cellul_agree_at, copied from the library rather than restated.
 *
 * @param[in] wa       First word.
 * @param[in] wb       Second word, which differs from wa.
 * @param[in] end_wins Whether a terminator in the same lane as the difference counts as a match.
 * @return             Whether the two agree up to and including where they end.
 * @note The version this replaced was written from memory and was wrong twice over: it took the
 *       differing lanes as (wa ^ wb) & high, which misses a difference in any bit but the top one of
 *       a byte, and then compared the two masks against each other rather than the lane indices
 *       lane.first reports. It never ran on the fixture, so the answers were right and the row was
 *       still worthless - a cold block of the wrong size measures the wrong cold block.
 */
static mmgr_bool arm_at(mmgr_word wa, mmgr_word wb, mmgr_bool end_wins)
{
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
    const mmgr_word x = arm_diff_lanes(wa ^ wb);
    const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
    const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

    // Explicit cast narrows the lane comparison into the mmgr_bool container
    return (mmgr_bool)(end_wins ? (lz <= lx) : (lz < lx));
}

/**
 * @brief The whole agree walk as the library carries it: aligned run, fallback run and tail.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The A arm, and the shape cellul_agree_cs has now: the run that executes and two blocks that
 *       do not, all in one function. The bare walk measured 2.52 cycles a byte and the library 3.56,
 *       and this is the structural difference between them.
 */
static mmgr_bool arm_all_inline(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    // Explicit casts read both addresses as integers so one mask answers for both
    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)a) | ((uintptr_t)b)) &
                                         (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, MMGR_FALSE);
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(lz < lx);
        }
    }
    return MMGR_FALSE;
}

/**
 * @brief Everything that is not the aligned run, moved out of the walk's own function.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @param[in] at   Offset the aligned run stopped at.
 * @param[in] full Offset the whole words end at.
 * @param[in] rest Bytes after them, fewer than one word.
 * @return         Whether the two agree.
 */
static mmgr_bool arm_slow(const char *a, const char *b, size_t at, size_t full, size_t rest)
{
    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(lz < lx);
        }
    }
    return MMGR_FALSE;
}

/**
 * @brief The same walk with only the aligned run left in the function.
 *
 * @param[in] a   First string [BORROWS].
 * @param[in] b   Second string [BORROWS].
 * @param[in] cap Bytes either may occupy.
 * @return        Whether both end together with no difference before it.
 * @note The B arm. Identical work for identical input; the fallback run and the tail are reached
 *       through a call instead of sitting beside the loop that does run.
 */
static mmgr_bool arm_slow_out(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    // Explicit casts read both addresses as integers so one mask answers for both
    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)a) | ((uintptr_t)b)) &
                                         (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, MMGR_FALSE);
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return arm_slow(a, b, at, full, rest);
}

/**
 * @brief The word type the single pass copy stores through, aligned and allowed to alias bytes.
 */
typedef mmgr_word bench_word_t MMGR_ALIAS;

/**
 * @brief The two regions for the copy rows, reached so the compiler cannot see which objects.
 */
static char *volatile g_cp_dst;
static const char *volatile g_cp_src;

/**
 * @brief A bounded copy that finds the terminator and writes the bytes in the same walk.
 *
 * @param[out] dst Destination [BORROWS].
 * @param[in]  src Source [BORROWS].
 * @param[in]  cap Bytes available in dst, terminator included.
 * @return         Bytes copied, not counting the terminator.
 * @note cellul_copy measures with cellul_len and then copies with proxim.read, which is two walks
 *       over the same bytes. This is one: a word is read, tested for a terminator, and stored when
 *       it holds none. Same contract - always terminated, reports what it wrote.
 * @note The word run needs both sides on a boundary, since the store is as wide as the load. Where
 *       they are not co-aligned it falls back to a byte walk, which is still one pass.
 */
static size_t copy_single(char *dst, const char *src, size_t cap)
{
    if (cap == 0u)
    {
        return 0u;
    }

    const size_t limit = cap - 1u;
    size_t at = 0u;

    // Explicit casts read both addresses as integers so one mask answers for both
    if (((((uintptr_t)dst) | ((uintptr_t)src)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u)
    {
        const size_t full = (limit / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;

        while (at != full)
        {
            const mmgr_word w = MMGR_CALL(word.load_al, ScrutWordCfg, .at = src + at);

            if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) != 0u)
            {
                break;
            }
            // Explicit cast stores a whole word at an address the test above put on a boundary
            *(bench_word_t *)(dst + at) = w;
            at += MMGR_SWAR_BYTES;
        }
    }

    while ((at != limit) && (src[at] != '\0'))
    {
        dst[at] = src[at];
        at += 1u;
    }
    dst[at] = '\0';
    return at;
}

/**
 * @brief Counts whitespace bytes with the six comparison chain cellul_is_ws carries.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them are whitespace.
 * @note The A arm. Six tests joined by short circuits, so a byte that is not whitespace - which is
 *       every byte in this fixture - runs all six and fails all six.
 */
static uint32_t ws_span_chain(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        const char ch = g_a[at];

        found += ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r') || (ch == '\f') || (ch == '\v'))
                     ? 1u
                     : 0u;
    }
    return found;
}

/**
 * @brief Counts whitespace bytes with a range test and one comparison.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them are whitespace.
 * @note The B arm, and the same set: tab, newline, vertical tab, form feed and carriage return are
 *       9 through 13 with nothing else between them, and space is the only other one. Subtracting 9
 *       and comparing unsigned takes all five in one test, so the whole predicate is two.
 */
static uint32_t ws_span_range(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        // Explicit cast makes the subtraction unsigned, so a byte below 9 wraps high and fails the
        // range rather than passing it as a negative
        const unsigned ch = (unsigned)(unsigned char)g_a[at];

        found += (((ch - 9u) <= 4u) || (ch == 32u)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts whitespace bytes in the haystack through cellul.ws.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them the entry calls whitespace.
 */
static uint32_t ws_span_mmgr(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        found += MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts whitespace bytes in the haystack through isspace.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them ctype calls whitespace.
 */
static uint32_t ws_span_libc(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        // Explicit cast takes the byte to the int ctype is defined over, unsigned so a high byte
        // does not arrive negative
        found += isspace((int)(unsigned char)g_a[at]) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts decimal digits in the haystack through cellul.digit.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them the entry calls a digit.
 */
static uint32_t digit_span_mmgr(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        found += MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts decimal digits in the haystack through isdigit.
 *
 * @param[in] n Bytes to walk.
 * @return      How many of them ctype calls a digit.
 */
static uint32_t digit_span_libc(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        // Explicit cast takes the byte to the int ctype is defined over, unsigned so a high byte
        // does not arrive negative
        found += isdigit((int)(unsigned char)g_a[at]) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief Counts the code points ascii.in puts in a class, over the whole byte range.
 *
 * @param[in] kind Class to test against.
 * @return         How many of the 256 code points are in it.
 * @note A single class test is a few cycles and would sit under the harness floor, so a row walks
 *       the whole range and the count is what keeps the work from being discarded.
 */
static uint32_t ascii_span_mmgr(MmgrAsciiClass kind)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {
        // Explicit cast narrows the counter into the code point the entry takes
        found += MMGR_CALL(ascii.in, AsciiCfg, .kind = kind, .byte = (uint8_t)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

/**
 * @brief What cellul_agree_cs walks with, so the arm below reads its addresses the same way.
 */
typedef struct
{
    const char *src;   /**< First string [BORROWS]. */
    const char *other; /**< Second string [BORROWS]. */
    size_t cap;        /**< Bytes either may occupy. */
} BenchEqCtx;

/**
 * @brief The same equality walk, reaching both addresses through a context on every step.
 *
 * @param[in] args The two strings and the extent [BORROWS].
 * @return         Whether both end together with no difference before it.
 * @note The A arm against eq_unaligned, which holds the two addresses as parameters instead. The
 *       loop, the loads and the test are identical; the only difference is where the addresses live
 *       while it runs. cellul_agree_cs is written the first way and measures about 2.4 times what
 *       the second does per byte, so this asks whether that is the reason.
 */
static mmgr_bool eq_via_ctx(const BenchEqCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
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
 * @brief The context walk with both addresses and the extent read once before the loop starts.
 *
 * @param[in] args The two strings and the extent [BORROWS].
 * @return         Whether both end together with no difference before it.
 * @note The B arm against the entry itself. The pack still arrives as one pointer, which is what the
 *       api asks for; what changes is that its members are read once here rather than once a word.
 *       The loads inside the body take the address of a compound literal, so the compiler cannot
 *       show the context is unchanged across them and reloads both addresses every step.
 * @note This is the same hoist that measured 1.00 in proxim_words, where the pointers are advanced
 *       each iteration and so are already live. Here they are only read, and it is worth 2.18x.
 */
static mmgr_bool eq_via_ctx_hoisted(const BenchEqCtx *args)
{
    const char *const a = args->src;
    const char *const b = args->other;
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
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

            // The entry against the same job done in one walk instead of two. Both regions reach the
            // arm through pointers the compiler cannot trace, so it is not handed alignment or
            // identity the entry does not get - the mistake that hid the aligned load three times.
            g_cp_dst = g_c;
            g_cp_src = g_a;
            DBENCH_AB("copy_one", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(copy_single(g_cp_dst, g_cp_src, n + 1u)));

            // has is find reduced to a yes or no, so its counterpart is the same strstr with its
            // result thrown away. ws and digit test one byte at a position, so a row walks the whole
            // buffer counting hits; a single test would sit under the harness floor. Neither buffer
            // is a constant here - fill writes them at run time - so nothing folds.
            DBENCH_AB("has", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = g_a, .cap = n + 1u,
                                            .other = g_needle, .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_needle) != NULL));

            DBENCH_AB("ws_scan", iters, n, DBENCH_KEEP(ws_span_mmgr(n)), DBENCH_KEEP(ws_span_libc(n)));

            // ws costs three times what its sibling digit does, on the same buffer through the same
            // call, and the only difference between them is that digit is a range test and ws is a
            // chain of six. This is that chain against the same set written as a range.
            DBENCH_AB("ws_range", iters, n, DBENCH_KEEP(ws_span_chain(n)), DBENCH_KEEP(ws_span_range(n)));

            DBENCH_AB("digit_scan", iters, n, DBENCH_KEEP(digit_span_mmgr(n)), DBENCH_KEEP(digit_span_libc(n)));

            // eq loses to the ROM's strcmp and the gap widens with length, which points at the load
            // rather than at the walk: cellul_agree_cs reads through the unaligned word twice a
            // step, where cellul_len walks a head first and reads through the aligned one.
            DBENCH_AB("eq_align", iters, n, DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)),
                      DBENCH_KEEP(eq_aligned(g_a, g_b, n + 1u)));

            // eq costs 4.57 cycles a byte where diff, the same walk without the terminator test,
            // costs 2.02. Four operations should not be worth ten cycles a word, so this asks
            // whether reaching them through the lane entry is what they actually cost - which is
            // what clz turned out to be doing. Both regions arrive through untraceable pointers.
            g_cp_src = g_a;
            g_cp_dst = (char *)g_b;
            DBENCH_AB("eq_zero", iters, n,
                      DBENCH_KEEP(agree_entry_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_inline_zero(g_cp_src, g_cp_dst, n + 1u)));

            // Mycroft's four operations against a two operation filter that never misses a zero,
            // with the exact test kept for the words it fires on. The fill is letters below 0x80,
            // so it fires on none of them, which is the case this is for.
            // Two changes, measured apart. The ordering alone first: Mycroft either side, with the
            // difference test moved ahead of it.
            DBENCH_AB("eq_order", iters, n,
                      DBENCH_KEEP(agree_inline_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_reordered(g_cp_src, g_cp_dst, n + 1u)));

            // Then the filter alone, against that same reordering, so the only thing between these
            // two arms is four operations against two.
            DBENCH_AB("eq_filter", iters, n,
                      DBENCH_KEEP(agree_reordered(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_filter_zero(g_cp_src, g_cp_dst, n + 1u)));

            // The whole walk as the library carries it against the same walk with the fallback run
            // and the tail moved out of its function. Neither block executes on this input; the
            // question is what they cost by being there. Both arms take their regions through
            // pointers the compiler cannot trace back to an object.
            DBENCH_AB("eq_cold", iters, n,
                      DBENCH_KEEP(arm_all_inline(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(arm_slow_out(g_cp_src, g_cp_dst, n + 1u)));

            // The walk built up a piece at a time, so the deltas say which piece owns the cycles
            // rather than another guess at the shape. Loads alone, then the compare and its branch,
            // then the terminator test and its branch, which together are the whole hot loop.
            DBENCH_AB("part_cmp", iters, n, DBENCH_KEEP(part_loads(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_compare(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_zero", iters, n, DBENCH_KEEP(part_compare(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)));

            // The whole hot loop against one that marks the terminator rather than stopping at it:
            // four words collected into two accumulators, one branch for the block instead of two a
            // word, and a second walk over the block that fires only when something was found.
            DBENCH_AB("part_mark", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_marked(g_cp_src, g_cp_dst, n + 1u)));

            // Both tests folded into one, with the two operation filter standing in for the exact
            // terminator test and the exact one run only where the fold fires.
            DBENCH_AB("part_one", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_onetest(g_cp_src, g_cp_dst, n + 1u)));

            // The next word's loads issued before the current word is tested, which the compiler
            // cannot do for itself because either test can leave the loop.
            DBENCH_AB("part_pipe", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_pipelined(g_cp_src, g_cp_dst, n + 1u)));

            // The entry against the same loop written out here. Both walk two words a step with the
            // same test, so a difference that grows with the length is not the call overhead and
            // not the load; it is something the entry's walk does per word that this one does not.
            DBENCH_AB("eq_entry", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

            // The same loop reaching its two addresses through a context against holding them as
            // parameters. Everything else about the two is identical, so this is the whole of what
            // separates the entry's walk from the one written out here, if anything does.
            {
                const BenchEqCtx ctx = {.src = g_a, .other = g_b, .cap = n + 1u};

                DBENCH_AB("eq_ctx", iters, n, DBENCH_KEEP(eq_via_ctx(&ctx)),
                          DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

                // The entry as it stands against the same walk with the context read once up front.
                // This is the row that decides whether the hoist belongs in cellul_agree_cs, so the
                // A arm is the real entry and not a copy of it.
                DBENCH_AB("eq_hoist", iters, n,
                          DBENCH_KEEP(
                              MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                          DBENCH_KEEP(eq_via_ctx_hoisted(&ctx)));
            }
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

        // The class test against the target's ctype, which had no bench of its own anywhere. Each
        // row walks the whole 256 code points, since one test is a few cycles and would sit under
        // the harness floor. ascii.in reads one bit out of a sixteen byte mask picked by the class;
        // newlib's ctype reads a byte out of a 257 entry table and masks it, so the two are the same
        // shape and the row is a fair one.
        {
            const uint32_t iters = 2000u;

            // The class is read through a volatile too. Passed as an enumerator it is a constant the
            // entry folds against, and the mmgr arm reports the floor whatever the bytes do.
            g_class = (int)MMGR_ASCII_NUM;
            DBENCH_AB("ascii_num", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_digit()));

            g_class = (int)MMGR_ASCII_ALPHA;
            DBENCH_AB("ascii_alpha", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_alpha()));

            g_class = (int)MMGR_ASCII_SPACE;
            DBENCH_AB("ascii_space", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_space()));

            g_class = (int)MMGR_ASCII_PRINT;
            DBENCH_AB("ascii_print", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_print()));
        }

        // The primitives every string row above is built out of, against the compiler builtin that
        // answers the same question. These are not library calls in libc; they are what GCC emits
        // for __builtin_clzll and its family, which on these parts is a sequence rather than an
        // instruction, since neither carries a count leading zeros or a population count.
        {
            const uint32_t iters = 20000u;

            DBENCH_AB("clz_lead", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)g_bits)),
                      DBENCH_KEEP(__builtin_clzll(g_bits)));

            DBENCH_AB("clz_trail", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(clz.trail, ClzCfg, .val = (mmgr_u64)g_bits)),
                      DBENCH_KEEP(__builtin_ctzll(g_bits)));

            DBENCH_AB("lane_count", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(lane.count, ScrutLaneCfg, .mask = g_mask)),
                      DBENCH_KEEP(__builtin_popcountll((unsigned long long)g_mask)));

            // The bench arm above divides ctz by eight and ignores the empty mask, which the entry
            // has to answer for: its contract reports MMGR_SWAR_BYTES when no lane is set, and ctz
            // of zero is undefined. This arm carries that guard, so it is the same function and the
            // row says what a substitution would actually be worth.
            DBENCH_AB("lane_first", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(lane.first, ScrutLaneCfg, .mask = g_mask)),
                      DBENCH_KEEP((g_mask == 0u) ? MMGR_SWAR_BYTES
                                                 : (__builtin_ctzll((unsigned long long)g_mask) / 8u)));

            // The two loads, which is the question that decided proxim_words and was worth nothing
            // in cellul_agree_cs. Here it is priced on its own rather than inside a walk.
            DBENCH_AB("word_load", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(word.load, ScrutWordCfg, .at = g_a + 1)),
                      DBENCH_KEEP(MMGR_CALL(word.load_al, ScrutWordCfg, .at = g_a)));

            DBENCH_AB("lane_zero", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = g_word)),
                      DBENCH_KEEP((g_word - (mmgr_word)0x0101010101010101ull) & ~g_word &
                                  (mmgr_word)0x8080808080808080ull));
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
