/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file main.c
 * @brief Integer to decimal, four ways, on the part rather than on a host.
 *
 * verba_uint writes one digit an iteration with a divide and a remainder, and counts the digits
 * first by walking a power of ten table. Both halves have known faster forms, and which one wins is
 * a property of the part: the S3 has a hardware divider and the C6 does not, so the same C compiles
 * to `quou`/`remu` on one and a reciprocal multiply on the other. That is not decidable by reading.
 *
 * Four emitters, all writing the same bytes:
 *   one   - a divide and a remainder per digit, which is what the library does today
 *   pair  - two digits an iteration off a 200 byte table, one divide per pair
 *   recip - the same pairs, with the divide written as a 32x32 high multiply so the compiler cannot
 *           choose a divide instruction
 *   jeaiii - no divide and no per-digit loop: a fixed point cursor multiplied forward, two digits
 *           read out per step
 *
 * And two digit counters, since every conversion pays one before it writes anything:
 *   scan - compare against each power of ten in turn, which is what the library does today
 *   clz  - one leading zero count, a shift, and a single table compare to correct
 *
 * Values are benched at 1, 3, 5 and 10 digits. A digit count decides the loop trip count, so an
 * emitter that wins at ten digits can lose at one, and the crossover is the reading.
 */
#include <stdint.h>
#include <string.h>

#include "device_bench.h"

#include "clz/clz.h"
#include "verba_scribo/verba_scribo.h"

/**
 * @brief Where every emitter writes, big enough for the widest 32-bit value and a terminator.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_out[16];

/**
 * @brief Powers of ten the scan counter compares against, as verba_scribo carries them.
 */
static const uint32_t POW10[10] = {1u,       10u,       100u,       1000u,       10000u,
                                   100000u,  1000000u,  10000000u,  100000000u,  1000000000u};

/**
 * @brief Every two digit combination, so a pair is one load rather than two divides.
 */
static const char PAIRS[201] = "00010203040506070809"
                               "10111213141516171819"
                               "20212223242526272829"
                               "30313233343536373839"
                               "40414243444546474849"
                               "50515253545556575859"
                               "60616263646566676869"
                               "70717273747576777879"
                               "80818283848586878889"
                               "90919293949596979899";

/**
 * @brief Digits in v, by comparing against each power of ten in turn.
 *
 * @param[in] v Value to measure.
 * @return      How many decimal digits it needs.
 * @note What verba_uint does today. Ten iterations worst case, before a digit is written.
 */
static unsigned dc_scan(uint32_t v)
{
    unsigned digits = 1u;

    while ((digits <= 9u) && (v >= POW10[digits]))
    {
        digits++;
    }
    return digits;
}

/**
 * @brief Digits in v, from its leading zero count.
 *
 * @param[in] v Value to measure.
 * @return      How many decimal digits it needs.
 * @note clz.lead counts a 64-bit value whatever the machine word is - ClzCtx::val is mmgr_u64 - so
 *       the bit index comes off 64, not off MMGR_WORD_BITS. Taking it off the word gives 32 - 61 on
 *       a 32-bit target, which underflows and indexes POW10 outside mapped flash.
 * @note log10 is approximated from log2 by multiplying by 1233 and shifting by twelve, which is
 *       1233/4096 against log10(2) = 0.30103; the estimate is exact or one low, and the single
 *       table compare corrects it. Branchless apart from that compare, and no loop.
 * @note The correction compares the forced-odd value rather than v, so zero counts as one digit.
 *       No other input moves: every power of ten above one is even, so setting the low bit
 *       cannot carry a value up across a threshold.
 */
static unsigned dc_clz(uint32_t v)
{
    // Explicit cast narrows the iword the clz entry returns to the unsigned the shift wants; v is
    // forced non-zero so the count is defined for every input
    const unsigned lead = (unsigned)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_word)(v | 1u));
    const unsigned bits = 64u - lead;
    const unsigned est = ((bits * 1233u) >> 12u);

    return est + (((v | 1u) >= POW10[est]) ? 1u : 0u);
}

/**
 * @brief A divide and a remainder per digit, written from the last digit back.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  v      Value to write.
 * @param[in]  digits How many it needs, from a counter above.
 * @note The library's current shape.
 */
static void emit_one(char *out, uint32_t v, unsigned digits)
{
    for (unsigned i = digits; i-- > 0u;)
    {
        out[i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
}

/**
 * @brief Two digits an iteration off the pair table.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  v      Value to write.
 * @param[in]  digits How many it needs.
 * @note One divide per pair rather than two per digit. The remainder is computed as v minus the
 *       quotient times a hundred, so the compiler has one division to answer rather than two.
 */
static void emit_pair(char *out, uint32_t v, unsigned digits)
{
    unsigned i = digits;

    while (i >= 2u)
    {
        const uint32_t q = v / 100u;
        const uint32_t r = v - (q * 100u);

        i -= 2u;
        out[i] = PAIRS[r * 2u];
        out[i + 1u] = PAIRS[(r * 2u) + 1u];
        v = q;
    }
    if (i != 0u)
    {
        out[0] = (char)('0' + v);
    }
}

/**
 * @brief Divides by a hundred with a high multiply rather than a divide instruction.
 *
 * @param[in] v Value to divide.
 * @return      v / 100.
 * @note 0x51EB851F over 2^37 is 1/100 to more precision than a 32-bit input can expose. Written
 *       this way because the S3's compiler picks its hardware divider for `/ 100` and the divider
 *       is the slower of the two; the C6's compiler already picks the multiply.
 */
static inline uint32_t div100(uint32_t v)
{
    // Explicit widening to 64 bits keeps the whole product, and the narrowing cast takes back the
    // quotient once the shift has discarded the fractional half
    return (uint32_t)(((uint64_t)v * 0x51EB851FULL) >> 37u);
}

/**
 * @brief The pair emitter with the divide forced into a multiply.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  v      Value to write.
 * @param[in]  digits How many it needs.
 */
static void emit_recip(char *out, uint32_t v, unsigned digits)
{
    unsigned i = digits;

    while (i >= 2u)
    {
        const uint32_t q = div100(v);
        const uint32_t r = v - (q * 100u);

        i -= 2u;
        out[i] = PAIRS[r * 2u];
        out[i + 1u] = PAIRS[(r * 2u) + 1u];
        v = q;
    }
    if (i != 0u)
    {
        out[0] = (char)('0' + v);
    }
}

/**
 * @brief Takes the next two digits off the top of a fixed point cursor.
 *
 * @param[out] out_ Where the pair goes [BORROWS].
 * @param[in,out] n_ Cursor, whose high word holds the next two digits [BORROWS].
 * @note The low word is the remaining fraction; multiplying it by a hundred brings the next pair
 *       into the high word. No divide, no remainder, no compare.
 */
#define JEAIII_PAIR(out_, n_)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t d_ = (uint32_t)((n_) >> 32u);                                                                   \
        (out_)[0] = PAIRS[d_ * 2u];                                                                                    \
        (out_)[1] = PAIRS[(d_ * 2u) + 1u];                                                                             \
        (n_) = ((n_)&0xFFFFFFFFULL) * 100ULL;                                                                          \
    } while (0)

/**
 * @brief Anhalt's shape: one scaling multiply, then a multiply and a pair lookup per two digits.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  v      Value to write.
 * @param[in]  digits How many it needs, from a counter above.
 * @note The value is scaled once into a fixed point fraction whose high word is the leading digit
 *       pair, and every step after that is a multiply by a hundred and a table read. No division
 *       instruction and no per-digit branch, which is the point on a part whose divider is slow or
 *       absent.
 * @note The scaling constant and shift depend on how wide the value is, which is why the digit
 *       count is taken first and passed in. The constants are Anhalt's.
 * @note Written to an even width and copied down, so an odd digit count drops one leading zero
 *       rather than needing a second set of constants.
 */
static void emit_jeaiii(char *out, uint32_t v, unsigned digits)
{
    char tmp[12];
    unsigned wide;
    uint64_t n;

    if (digits <= 2u)
    {
        n = 0u;
        wide = 2u;
        tmp[0] = PAIRS[v * 2u];
        tmp[1] = PAIRS[(v * 2u) + 1u];
    }
    else if (digits <= 4u)
    {
        wide = 4u;
        n = (uint64_t)v * 42949673ULL;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
    }
    else if (digits <= 6u)
    {
        wide = 6u;
        n = (uint64_t)v * 429497ULL;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
    }
    else if (digits <= 8u)
    {
        wide = 8u;
        n = ((uint64_t)v * 281474978ULL) >> 16u;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
        JEAIII_PAIR(&tmp[6], n);
    }
    else
    {
        wide = 10u;
        // Nine digits and ten take different constants: the scaling has to land the leading digit
        // in the high word either way, and one value of the multiplier cannot do both without
        // losing the last digit to rounding
        n = (digits == 9u) ? (((uint64_t)v * 1441151882ULL) >> 25u) : (((uint64_t)v * 1441151881ULL) >> 25u);
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
        JEAIII_PAIR(&tmp[6], n);
        JEAIII_PAIR(&tmp[8], n);
    }

    for (unsigned i = 0; i < digits; i++)
    {
        out[i] = tmp[(wide - digits) + i];
    }
}

/**
 * @brief The library's algorithm as it stood: count by walking the table, then a divide per digit.
 *
 * @note The A arm of the adoption row. Writing it out here rather than reverting the library is what
 *       lets both arms run in the same image, on the same part, in the same conditions.
 */
static size_t was(char *out, uint32_t v)
{
    const unsigned d = dc_scan(v);

    emit_one(out, v, d);
    return d;
}

/**
 * @brief The library entry pulled into the caller, which is how a caller reaches it in a real build.
 *
 * @note MMGR_FLATTEN asks the compiler to inline everything this calls, which under link-time
 *       optimization reaches the entry body. Both arms of the row below are then inlined, so what is
 *       compared is the arithmetic rather than one side paying for a call the other does not.
 */
MMGR_FLATTEN static size_t verba_uint_flat(char *out, uint32_t v)
{
    return MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = out, .cap = 16u, .at = 0u, .val = v, .base = 10u, .min = 1u);
}

/**
 * @brief One pass: every emitter and both counters, at four digit widths.
 */
void dbench_run(void)
{
    static const uint32_t vals[] = {7u, 314u, 65535u, 4294967295u};
    static const unsigned wid[] = {1u, 3u, 5u, 10u};

    for (;;)
    {
        DBENCH_BANNER("verba itoa emitters and digit counters");

        for (unsigned vi = 0; vi < (sizeof vals / sizeof vals[0]); vi++)
        {
            const uint32_t v = vals[vi];
            const unsigned d = wid[vi];
            const uint32_t iters = 20000u;

            // Counting first, on its own: every conversion pays one before it writes a byte, and
            // the scan's cost rises with the digit count while the clz form's does not.
            DBENCH_AB("count", iters, d, DBENCH_KEEP(dc_scan(v)), DBENCH_KEEP(dc_clz(v)));

            // Then the emitters, each against the one the library has today.
            DBENCH_AB("pair", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_pair(g_out, v, d), DBENCH_KEEP(g_out)));

            DBENCH_AB("recip", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_recip(g_out, v, d), DBENCH_KEEP(g_out)));

            DBENCH_AB("jeaiii", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_jeaiii(g_out, v, d), DBENCH_KEEP(g_out)));

            // The adoption, end to end: the whole old algorithm against the library entry as it
            // now stands. This one carries the entry call on the B arm and not the A arm, so it
            // is the honest figure a caller sees rather than the arithmetic alone.
            // The calling convention, priced on its own: the same entry, the same arithmetic, called
            // the ordinary way on one arm and pulled into the caller on the other. The gap is what
            // MMGR_CALL and the argument pack cost, and nothing else.
            DBENCH_AB("call", iters, d,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_out, .cap = sizeof g_out, .at = 0u,
                                            .val = v, .base = 10u, .min = 1u)),
                      DBENCH_KEEP(verba_uint_flat(g_out, v)));

            // And the arithmetic on its own, both arms inlined.
            DBENCH_AB("arith", iters, d, DBENCH_KEEP(was(g_out, v)), DBENCH_KEEP(verba_uint_flat(g_out, v)));
        }

        // What the harness costs with no work in it. Every row above carries this.
        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_out));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("verba")
