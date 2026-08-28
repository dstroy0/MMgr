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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_bench.h"

#include "clz/clz.h"
#include "fractio/fractio.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "proximus_operor/proximus_operor.h"
#include "verba_scribo/verba_scribo.h"

/**
 * @brief Where every emitter writes, big enough for the widest 32-bit value and a terminator.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_out[16];

/**
 * @brief Where the rows that measure against libc write, wide enough for anything either side emits.
 *
 * @note Separate from g_out, which the emitter rows size against a 32-bit value. A full 64-bit
 *       value is twenty digits and a %g at eighteen significant digits is longer still, so sharing
 *       one buffer would have snprintf truncating on rows where the library entry does not.
 */
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_wide[64];

/**
 * @brief A byte count the compiler cannot see the value of.
 *
 * @note Volatile on purpose. Handed a length it knows at compile time, GCC does not call memcpy at
 *       all: it lays the copy down as straight line moves, which measured under half a cycle a byte
 *       on the C6 and is not a library call being compared against a library call. A length read
 *       through this reaches memcpy the way a caller with a runtime length does.
 */
static volatile size_t g_len = 30u;

/**
 * @brief A whole number of words, so the copy has no tail bytes to finish with.
 *
 * @note Thirty bytes leaves two for proxim_tail, which walks them one at a time. Thirty two leaves
 *       none, so the pair of rows says whether the distance to memcpy is in the word run or in the
 *       stages either side of it.
 */
static volatile size_t g_len_whole = 32u;

/**
 * @brief The same text reached through a pointer the compiler cannot follow.
 *
 * @note Handed a literal, GCC knows its length and never emits a measure on the libc side at all.
 *       Read through this, both sides have to walk the string before they can copy it.
 */
static const char *volatile g_text = "the quick brown fox jumps over";

/**
 * @brief The three values the record row places, where the compiler cannot see them.
 *
 * @note Given literals, GCC evaluates the whole snprintf at build time and the libc arm measures an
 *       empty loop; the mmgr arm folds too, since every field is then a constant.
 */
static volatile uint64_t g_rec_u64 = 18446744073709551615ull;
static volatile uint32_t g_rec_hex = 0xDEADBEEFu;
static volatile double g_rec_real = -2.5;

/**
 * @brief The same value as a bit pattern, for the rows that take a double apart.
 */
static volatile uint64_t g_rec_bits = 0xC004000000000000ull;

/**
 * @brief The zero count, hidden so neither arm is unrolled against it.
 */
static volatile size_t g_zero_n = 6u;

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
 * @note The width is a fixed 64 rather than MMGR_WORD_BITS because clz.lead counts an mmgr_u64
 *       whatever the machine word is. Taking it off a 32-bit word underflows, and the estimate then
 *       indexes POW10 outside mapped flash, which faults on the part and cannot fault on a host.
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
    const unsigned lead = (unsigned)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)(v | 1u));
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
    return MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = out, .cap = 16u, .at = 0u, .val = v,
                     .base = 10u, .min = 1u);
}

/**
 * @brief Powers of ten the mantissa is measured and cut against, as verba_scribo holds them.
 *
 * @note Stands in for mmgr_verba_pow10, which is static to verba_scribo.c and so out of reach here.
 */
static const uint64_t POW10_64[20] = {
    1ull,                 10ull,                 100ull,                 1000ull,
    10000ull,             100000ull,             1000000ull,             10000000ull,
    100000000ull,         1000000000ull,         10000000000ull,         100000000000ull,
    1000000000000ull,     10000000000000ull,     100000000000000ull,     1000000000000000ull,
    10000000000000000ull, 100000000000000000ull, 1000000000000000000ull, 10000000000000000000ull};

/**
 * @brief Index into POW10_64 of the power of ten the mantissa is cut at.
 *
 * @note Eight, so each piece is at most nine digits, which is the widest that fits a uint32_t.
 */
#define CUT 8u

/**
 * @brief What verba_digits does today: a descending divisor, most significant digit first.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  mant   The digits, as one integer.
 * @param[in]  digits How many of them to write.
 * @note Two 64-bit divisions per digit - the quotient and the divisor step - and neither part has a
 *       64-bit divider, so both are libgcc calls.
 */
static void digits_descending(char *out, uint64_t mant, unsigned digits)
{
    uint64_t left = mant;
    uint64_t divisor = POW10_64[digits - 1u];

    for (unsigned index = 0; index < digits; index++)
    {
        const uint64_t digit = left / divisor;

        out[index] = (char)('0' + (uint32_t)digit);
        left -= digit * divisor;
        divisor /= 10u;
    }
}

/**
 * @brief The same digits, cut into 32-bit pieces and emitted with the pair walk already in verba.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  mant   The digits, as one integer.
 * @param[in]  digits How many of them to write, one through twenty.
 * @note Nothing here is new machinery: the cut is one division against a power of ten verba already
 *       holds, and each piece goes through emit_recip, which is verba_emit10 under another name.
 * @note Nine digits or fewer take no 64-bit division at all, ten through eighteen take one, and only
 *       nineteen and twenty take two. verba_g caps at MMGR_G_MAX_SIG, which is eighteen, so the
 *       second cut is reachable from verba_fixed alone.
 */
static void digits_split(char *out, uint64_t mant, unsigned digits)
{
    if (digits <= 9u)
    {
        emit_recip(out, (uint32_t)mant, digits);
        return;
    }

    const uint64_t rest = mant / POW10_64[CUT];
    // The remainder is taken off the quotient rather than asked for separately, so the compiler has
    // one division to answer here rather than a division and a modulo
    const uint32_t low = (uint32_t)(mant - (rest * POW10_64[CUT]));

    if (digits <= 17u)
    {
        emit_recip(out, (uint32_t)rest, digits - CUT);
    }
    else
    {
        // Past seventeen digits the piece above the cut no longer fits a uint32_t, so it is cut again
        const uint64_t top = rest / POW10_64[CUT];
        const uint32_t mid = (uint32_t)(rest - (top * POW10_64[CUT]));

        emit_recip(out, (uint32_t)top, digits - (2u * CUT));
        emit_recip(out + (digits - (2u * CUT)), mid, CUT);
    }
    emit_recip(out + (digits - CUT), low, CUT);
}

/**
 * @brief put_n pulled into the caller, which is how a caller reaches it in a build with LTO on.
 *
 * @param[out] out  Where the text goes [BORROWS].
 * @param[in]  text Text to write [BORROWS].
 * @param[in]  len  Bytes of it.
 * @return          The offset past what was written.
 * @note The same entry and the same bounds test as the row above, with the call boundary removed.
 *       The gap between the two is what the entry layer costs and nothing else.
 */
MMGR_FLATTEN static size_t verba_put_n_flat(char *out, const char *text, size_t len)
{
    return MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = out, .cap = 64u, .at = 0u, .text = text,
                     .text_len = len);
}

/**
 * @brief Digits in a 64-bit value, from its leading zero count, as verba_digits10 counts them.
 *
 * @param[in] v Value to measure.
 * @return      How many decimal digits it needs.
 */
static unsigned dc_clz64(uint64_t v)
{
    // Explicit cast narrows the iword the clz entry returns; the value is forced non-zero so the
    // count is defined for every input, and the width is a fixed 64 because clz.lead counts an
    // mmgr_u64 whatever the machine word is
    const unsigned lead = (unsigned)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)(v | 1u));
    const unsigned bits = 64u - lead;
    const unsigned est = (bits * 1233u) >> 12u;

    return est + (((v | 1u) >= POW10_64[est]) ? 1u : 0u);
}

/**
 * @brief The cut as verba_emit20 performs it, with the width test inside.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  value  Value to write.
 * @param[in]  digits How many to write.
 */
static void emit20(char *out, uint64_t value, unsigned digits)
{
    if (value <= 0xFFFFFFFFU)
    {
        // Explicit cast narrows to the width emit_recip takes; the test above established it fits
        emit_recip(out, (uint32_t)value, digits);
        return;
    }

    const uint64_t rest = value / POW10_64[8];
    const uint32_t low = (uint32_t)(value - (rest * POW10_64[8]));

    if (rest <= 0xFFFFFFFFU)
    {
        emit_recip(out, (uint32_t)rest, digits - 8u);
    }
    else
    {
        const uint64_t top = rest / POW10_64[8];
        const uint32_t mid = (uint32_t)(rest - (top * POW10_64[8]));

        emit_recip(out, (uint32_t)top, digits - 16u);
        emit_recip(out + (digits - 16u), mid, 8u);
    }
    emit_recip(out + (digits - 8u), low, 8u);
}

/**
 * @brief verba_uint's base ten path as it stands: count, then hand everything to the cut.
 *
 * @param[out] out Where the digits go [BORROWS].
 * @param[in]  val Value to write.
 * @return         Digits written.
 * @note The A arm. The width test that keeps a 32-bit value off the cut lives inside emit20.
 */
static size_t uint_test_inside(char *out, uint64_t val)
{
    const unsigned digits = dc_clz64(val);

    emit20(out, val, digits);
    return digits;
}

/**
 * @brief The same, with the width test hoisted into the caller as verba_uint used to carry it.
 *
 * @param[out] out Where the digits go [BORROWS].
 * @param[in]  val Value to write.
 * @return         Digits written.
 * @note The B arm. This is the narrow path that was deleted from verba_uint without a row of its
 *       own, on the reasoning that both tests fold to the same code. That is what this measures.
 */
static size_t uint_test_outside(char *out, uint64_t val)
{
    const unsigned digits = dc_clz64(val);

    if (val <= 0xFFFFFFFFU)
    {
        // Explicit cast narrows to the width emit_recip takes; the test above established it fits
        emit_recip(out, (uint32_t)val, digits);
    }
    else
    {
        emit20(out, val, digits);
    }
    return digits;
}

/**
 * @brief The word types proxim_words copies through, mirrored here so both arms below are honest.
 *
 * @note MMGR_ALIAS is what makes the question worth asking: a store through a type carrying it may
 *       alias anything the compiler knows of, so the A arm's loop cannot keep its own pointers in
 *       registers across the store.
 */
typedef mmgr_migro_word bench_aequus_word_t MMGR_ALIAS;

/**
 * @brief The unaligned word type, for a source that did not come to rest on a boundary.
 */
typedef mmgr_migro_word bench_proxim_word_t MMGR_RAW;

/**
 * @brief What proxim_read carries between its three stages.
 */
typedef struct
{
    uint8_t *dst;       /**< Destination [BORROWS]. */
    const uint8_t *src; /**< Source [BORROWS]. */
    size_t bytes;       /**< Bytes still to copy. */
} BenchCopyCtx;

/**
 * @brief The word run as proximus_operor writes it: both addresses advanced through the context.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note The A arm. Every store goes through a type that may alias the context holding the pointers,
 *       so the addresses are reloaded after each one.
 */
static void copy_words_args(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    do
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        args->dst += MMGR_RAW_WORD;
        args->src += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);
}

/**
 * @brief The same run with both addresses walked in locals and written back once at the end.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note The B arm. Identical bytes moved and identical loads and stores; the only difference is
 *       where the two addresses live while the loop runs.
 */
static void copy_words_locals(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    uint8_t *to = args->dst;
    const uint8_t *from = args->src;

    do
    {
        *(bench_aequus_word_t *)to = *(const bench_aequus_word_t *)from;
        to += MMGR_RAW_WORD;
        from += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);

    args->dst = to;
    args->src = from;
}

/**
 * @brief The same run taking two words an iteration, with a single word finishing an odd count.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note Four words an iteration was measured and lost: at thirty bytes it runs once and leaves three
 *       words to a second loop, so the wider test costs more than the iterations it saves. Two is
 *       the smallest step that halves the loop arithmetic without leaving that much behind.
 */
static void copy_words_two(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    while (w >= (2u * MMGR_RAW_WORD))
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        *(bench_aequus_word_t *)(args->dst + MMGR_RAW_WORD) =
            *(const bench_aequus_word_t *)(args->src + MMGR_RAW_WORD);
        args->dst += 2u * MMGR_RAW_WORD;
        args->src += 2u * MMGR_RAW_WORD;
        w -= 2u * MMGR_RAW_WORD;
    }
    if (w != 0u)
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        args->dst += MMGR_RAW_WORD;
        args->src += MMGR_RAW_WORD;
    }
}

/**
 * @brief The same run written as a counted loop over a word index.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note The trip count is worked out before the loop starts and the body indexes off it, which is
 *       the shape a compiler can turn into a hardware loop. The pointer walking do-while it is
 *       measured against carries a decrement and a branch per word that it cannot: the count comes
 *       down inside the body, so the loop is not countable at entry and no zero overhead loop is
 *       available to it.
 */
static void copy_words_counted(BenchCopyCtx *args)
{
    const size_t words = args->bytes / MMGR_RAW_WORD;

    if (words == 0u)
    {
        return;
    }
    args->bytes -= words * MMGR_RAW_WORD;

    bench_aequus_word_t *const to = (bench_aequus_word_t *)args->dst;
    const bench_aequus_word_t *const from = (const bench_aequus_word_t *)args->src;

    for (size_t index = 0; index < words; index++)
    {
        to[index] = from[index];
    }

    args->dst += words * MMGR_RAW_WORD;
    args->src += words * MMGR_RAW_WORD;
}

/**
 * @brief The whole copy in one function, with the three counts worked out before anything moves.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note proxim_read runs three stages, each taking the context by pointer and each drawing its share
 *       off args->bytes as it goes. The word run on its own already matches memcpy; the distance is
 *       the bookkeeping carried between the stages. This settles the head, word and tail counts once
 *       and walks each of them in a counted loop over locals.
 * @warning Copies forward, so a dst above src within one region would read bytes it has written.
 */
static void copy_read_flat(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit casts hold the negation and the mask at uintptr_t, then bring the count back to size_t
    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    const size_t head = (skew < bytes) ? skew : bytes;
    const size_t left = bytes - head;
    const size_t words = left / MMGR_RAW_WORD;
    const size_t tail = left - (words * MMGR_RAW_WORD);

    for (size_t index = 0; index < head; index++)
    {
        dst[index] = src[index];
    }
    dst += head;
    src += head;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_aequus_word_t *const from = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = from[index];
        }
    }
    else
    {
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_proxim_word_t *const from = (const bench_proxim_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = from[index];
        }
    }
    dst += words * MMGR_RAW_WORD;
    src += words * MMGR_RAW_WORD;

    for (size_t index = 0; index < tail; index++)
    {
        dst[index] = src[index];
    }
}

/**
 * @brief The copy with the odd bytes taken by one overlapping word rather than a byte loop.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note The standard shape a hand written memcpy uses: once at least one word is being moved, the
 *       last word of the copy can be written as a whole word ending on the final byte, overlapping
 *       bytes the word run already placed. It costs one unaligned store and removes the tail loop
 *       entirely. Whether that trades well depends on what an unaligned store costs on the part,
 *       which is the question this row asks.
 * @warning Copies forward, so a dst above src within one region would read bytes it has written.
 */
static void copy_read_overlap(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    if (bytes < MMGR_RAW_WORD)
    {
        for (size_t index = 0; index < bytes; index++)
        {
            dst[index] = src[index];
        }
        return;
    }

    // Explicit casts hold the negation and the mask at uintptr_t, then bring the count back to size_t
    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    uint8_t *const base = dst;
    const uint8_t *const from_base = src;
    const size_t total = bytes;

    for (size_t index = 0; index < skew; index++)
    {
        dst[index] = src[index];
    }
    dst += skew;
    src += skew;

    const size_t words = (total - skew) / MMGR_RAW_WORD;
    bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        const bench_aequus_word_t *const wfrom = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }
    }
    else
    {
        const bench_proxim_word_t *const wfrom = (const bench_proxim_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }
    }

    // One word ending on the last byte, which rewrites bytes the run above already placed. Reaching
    // back a whole word is why the short case above is handled separately
    *(bench_proxim_word_t *)(base + total - MMGR_RAW_WORD) =
        *(const bench_proxim_word_t *)(from_base + total - MMGR_RAW_WORD);
}

/**
 * @brief The copy with one test at the top for the case both addresses are already on a boundary.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note The head stage exists for a destination that arrives off a boundary, which in this library
 *       is the exception rather than the rule: memory here arrives aligned. This asks what the
 *       common case costs when it is not made to walk the machinery that serves the exception.
 * @warning Copies forward, so a dst above src within one region would read bytes it has written.
 */
static void copy_read_fastpath(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit cast holds both addresses at uintptr_t for the one mask that answers for both
    if (((((uintptr_t)dst) | ((uintptr_t)src)) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        const size_t words = bytes / MMGR_RAW_WORD;
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_aequus_word_t *const wfrom = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }

        for (size_t index = words * MMGR_RAW_WORD; index < bytes; index++)
        {
            dst[index] = src[index];
        }
        return;
    }

    copy_read_flat(dst, src, bytes);
}

/**
 * @brief The word run as one dispatch into straight line moves, for a run that is short.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note Eight words or fewer is one jump and then unconditional moves: no test and no branch per
 *       word. A thirty byte copy is seven words and two odd bytes, so the loop it replaces spends
 *       seven compares and seven branches that this spends once. Anything longer keeps the loop,
 *       where the dispatch would buy nothing.
 * @warning Copies forward, so a dst above src within one region would read bytes it has written.
 */
static void copy_read_dispatch(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit casts hold the negation and the mask at uintptr_t, then bring the count back to size_t
    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    const size_t head = (skew < bytes) ? skew : bytes;
    size_t index = 0;

    while (index != head)
    {
        dst[index] = src[index];
        index++;
    }
    dst += head;
    src += head;

    const size_t left = bytes - head;
    const size_t words = left / MMGR_RAW_WORD;
    bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) != 0u)
    {
        copy_read_flat(dst, src, left);
        return;
    }

    const bench_aequus_word_t *const from = (const bench_aequus_word_t *)src;

    switch (words)
    {
        default:
        {
            size_t at = 0;

            while (at != words)
            {
                to[at] = from[at];
                at++;
            }
            break;
        }
        case 8u:
            to[7] = from[7];
            // fall through
        case 7u:
            to[6] = from[6];
            // fall through
        case 6u:
            to[5] = from[5];
            // fall through
        case 5u:
            to[4] = from[4];
            // fall through
        case 4u:
            to[3] = from[3];
            // fall through
        case 3u:
            to[2] = from[2];
            // fall through
        case 2u:
            to[1] = from[1];
            // fall through
        case 1u:
            to[0] = from[0];
            // fall through
        case 0u:
            break;
    }

    const size_t done = words * MMGR_RAW_WORD;

    for (size_t at = done; at != left; at++)
    {
        dst[at] = src[at];
    }
}

/**
 * @brief Source and destination for the copy check, with slack for every offset it walks.
 */
static uint8_t g_check_src[192];
static uint8_t g_check_dst[192];

/**
 * @brief Checks proxim.read against a plain byte copy at every offset pair and length.
 *
 * @return The number of disagreements, which must be zero.
 * @note The host suite cannot reach a target's assembly, so the copy is checked on the part itself
 *       before any number taken from it means anything. Walks both alignments through a whole word
 *       and every length up to sixty-four, which covers the head, the word run and the tail in every
 *       combination, and checks the bytes on both sides of the run for a write that went too far.
 */
static uint32_t copy_is_correct(void)
{
    uint32_t bad = 0u;

    for (uint32_t index = 0; index < sizeof g_check_src; index++)
    {
        // Explicit cast narrows the counter into the byte the pattern stores
        g_check_src[index] = (uint8_t)(index + 1u);
    }

    for (uint32_t soff = 0; soff < 8u; soff++)
    {
        for (uint32_t doff = 0; doff < 8u; doff++)
        {
            for (uint32_t len = 0; len <= 64u; len++)
            {
                memset(g_check_dst, 0xA5, sizeof g_check_dst);

                MMGR_CALL(proxim.read, ProximusCfg, .dst = g_check_dst + doff, .at = g_check_src + soff,
                          .size = len);

                if (doff != 0u)
                {
                    bad += (g_check_dst[doff - 1u] != 0xA5u) ? 1u : 0u;
                }
                bad += (g_check_dst[doff + len] != 0xA5u) ? 1u : 0u;

                for (uint32_t index = 0; index < len; index++)
                {
                    if (g_check_dst[doff + index] != g_check_src[soff + index])
                    {
                        bad++;
                        break;
                    }
                }
            }
        }
    }
    return bad;
}

/**
 * @brief What a caller writes with libc to get what one verba_put_n call gives them.
 *
 * @param[out] out Destination buffer [BORROWS].
 * @param[in]  cap Bytes available in out.
 * @param[in]  at  Offset to write at.
 * @param[in]  src Text to write [BORROWS].
 * @param[in]  len Bytes of it.
 * @return         The offset past the text, or cap when it does not fit.
 * @note The same refusal verba_room performs, then the copy. Written out here because memcpy on its
 *       own answers a different question: it cannot decline, and it does not say where the next
 *       write goes, so a caller holding a bounded buffer writes both halves either way.
 */
static size_t libc_put_n(char *out, size_t cap, size_t at, const char *src, size_t len)
{
    if ((at >= cap) || (len > ((cap - at) - 1u)))
    {
        return cap;
    }
    memcpy(out + at, src, len);
    return at + len;
}

/**
 * @brief A run of zeros written a byte at a time, each byte testing for room of its own.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap once one did not fit.
 * @note The shape verba_zeros has: one verba_ch a zero, and verba_ch tests the room it needs before
 *       each store. The count is known before the first one.
 */
static size_t zeros_per_byte(char *out, size_t cap, size_t at, size_t n)
{
    while (n-- != 0u)
    {
        if ((at >= cap) || (1u > ((cap - at) - 1u)))
        {
            return cap;
        }
        out[at] = '0';
        at += 1u;
    }
    return at;
}

/**
 * @brief The same run with the room tested once for all of it.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note Same bytes, same refusal: a run that does not fit writes nothing and reports cap, which is
 *       what the per byte form reaches by failing on the first zero that does not fit.
 */
static size_t zeros_one_test(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {
        out[at + k] = '0';
    }
    return at + n;
}

/**
 * @brief One room test, then a byte loop the compiler is not allowed to turn into a fill.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note The arm that separates the two changes. Testing the room once and filling with a plain loop
 *       lets the compiler call memset, which is a fixed cost that wins on a long run and loses on a
 *       short one; this keeps the single room test and denies it the call, so what it measures is
 *       the room test alone.
 */
static size_t zeros_one_test_noopt(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {
        // Volatile denies the compiler the memset it would otherwise emit for a constant fill
        *(volatile char *)(out + at + k) = '0';
    }
    return at + n;
}

/**
 * @brief One room test, then the library's own fill.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note memor.set rather than a loop the compiler may or may not turn into one, which is the form
 *       the library would actually carry.
 */
static size_t zeros_memor_set(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = out + at, .bytes = n, .val = (uint8_t)'0');
    return at + n;
}

/**
 * @brief The room tested once, with a short run taken by hand and a long one by the fill.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note Both call sites are in verba_g and they sit on opposite sides of the crossover: the leading
 *       run is bounded at three by the exponent test that selects the form, and the trailing run
 *       reaches about seventeen. A threshold covers both.
 */
static size_t zeros_hybrid(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    if (n <= 8u)
    {
        for (size_t k = 0; k < n; k++)
        {
            // Volatile denies the compiler the call this branch exists to avoid
            *(volatile char *)(out + at + k) = '0';
        }
        return at + n;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = out + at, .bytes = n, .val = (uint8_t)'0');
    return at + n;
}

/**
 * @brief The room tested once, a short run laid down straight, a long one left to the fill.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note The candidate, written the way the library would carry it. A run of eight or fewer is a
 *       dispatch into straight line stores, which gives the compiler nothing to turn into a memset;
 *       the earlier arms had to store through a volatile to keep it from doing so, and that is not
 *       something the library would carry. Anything longer goes to memor.set, whose call is worth
 *       paying once the bytes outnumber it.
 * @warning The switch falls through on purpose, each case laying one byte and dropping to the next.
 */
static size_t zeros_built(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
        default:
            MMGR_CALL(memor.set, MemoriaCfg, .dst = to, .bytes = n, .val = (uint8_t)'0');
            break;
        case 8u:
            to[7] = '0';
            // fall through
        case 7u:
            to[6] = '0';
            // fall through
        case 6u:
            to[5] = '0';
            // fall through
        case 5u:
            to[4] = '0';
            // fall through
        case 4u:
            to[3] = '0';
            // fall through
        case 3u:
            to[2] = '0';
            // fall through
        case 2u:
            to[1] = '0';
            // fall through
        case 1u:
            to[0] = '0';
            // fall through
        case 0u:
            break;
    }
    return at + n;
}

/**
 * @brief The candidate with the long run written a word at a time through proxim.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note The version before this reached for memor.set, which verba_scribo does not depend on and
 *       would have had to be given a dependency for. proximus_operor is already one of its
 *       dependencies and already carries a store: proxim.put writes a word at any address, so a run
 *       needs no head walk to reach a boundary and no module has to be added to write one.
 * @warning The switch falls through on purpose, each case laying one byte and dropping to the next.
 */
static size_t zeros_proxim(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
        default:
        {
            // Every lane of a word holding the digit, so the run below is one store a word
            const uint64_t lanes = ((~(uint64_t)0) / 0xFFu) * (uint64_t)'0';
            size_t k = 0u;

            while ((n - k) >= MMGR_RAW_WORD)
            {
                MMGR_CALL(proxim.put, ProximusCfg, .dst = (uint8_t *)to + k, .val = lanes);
                k += MMGR_RAW_WORD;
            }
            while (k != n)
            {
                to[k] = '0';
                k += 1u;
            }
            break;
        }
        case 8u:
            to[7] = '0';
            // fall through
        case 7u:
            to[6] = '0';
            // fall through
        case 6u:
            to[5] = '0';
            // fall through
        case 5u:
            to[4] = '0';
            // fall through
        case 4u:
            to[3] = '0';
            // fall through
        case 3u:
            to[2] = '0';
            // fall through
        case 2u:
            to[1] = '0';
            // fall through
        case 1u:
            to[0] = '0';
            // fall through
        case 0u:
            break;
    }
    return at + n;
}

/**
 * @brief The same shape with the long run written the way a fill is actually written.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note The arm before this stored every word through proxim.put, which takes any address and so
 *       compiles to a byte sequence on a part with no unaligned store - which is why it came out
 *       level with the byte loop it was meant to replace. A fill walks the bytes up to a boundary
 *       first and then stores whole words through the aligned put, which is one instruction. Same
 *       three stages memor_set has, written here out of the store verba_scribo already depends on.
 * @warning The switch falls through on purpose, each case laying one byte and dropping to the next.
 */
static size_t zeros_aligned_fill(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
        default:
        {
            // Every lane of a word holding the digit
            const uint64_t lanes = ((~(uint64_t)0) / 0xFFu) * (uint64_t)'0';
            // Explicit casts hold the negation and the mask at uintptr_t, then bring the count back
            const size_t skew = (size_t)((0u - (uintptr_t)to) & (uintptr_t)(MMGR_RAW_WORD - 1u));
            const size_t head = (skew < n) ? skew : n;
            uint8_t *put = (uint8_t *)to;
            size_t left = n;

            // The head memor_set does not have: it stores through the aligned put with no walk to a
            // boundary first, so it needs a destination that already sits on one. This one writes
            // wherever the digits ended
            for (size_t k = 0; k < head; k++)
            {
                *put++ = (uint8_t)'0';
            }
            left -= head;

            size_t words = left - (left & (MMGR_RAW_WORD - 1u));

            // Four words an iteration, advancing the pointer rather than indexing off a base: at one
            // word a pass the bump, the counter and the branch cost as much as the store
            while (words >= (4u * MMGR_RAW_WORD))
            {
                MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put, .val = lanes);
                MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + MMGR_RAW_WORD, .val = lanes);
                MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + (2u * MMGR_RAW_WORD), .val = lanes);
                MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + (3u * MMGR_RAW_WORD), .val = lanes);
                put += 4u * MMGR_RAW_WORD;
                words -= 4u * MMGR_RAW_WORD;
                left -= 4u * MMGR_RAW_WORD;
            }
            while (words != 0u)
            {
                MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put, .val = lanes);
                put += MMGR_RAW_WORD;
                words -= MMGR_RAW_WORD;
                left -= MMGR_RAW_WORD;
            }
            while (left != 0u)
            {
                *put++ = (uint8_t)'0';
                left -= 1u;
            }
            break;
        }
        case 8u:
            to[7] = '0';
            // fall through
        case 7u:
            to[6] = '0';
            // fall through
        case 6u:
            to[5] = '0';
            // fall through
        case 5u:
            to[4] = '0';
            // fall through
        case 4u:
            to[3] = '0';
            // fall through
        case 3u:
            to[2] = '0';
            // fall through
        case 2u:
            to[1] = '0';
            // fall through
        case 1u:
            to[0] = '0';
            // fall through
        case 0u:
            break;
    }
    return at + n;
}

/**
 * @brief One room test, then the zeros stored where the pattern pass cannot reach them.
 *
 * @param[out] out Destination [BORROWS].
 * @param[in]  cap Bytes available.
 * @param[in]  at  Offset to write at.
 * @param[in]  n   Zeros to write.
 * @return         The offset past them, or cap when the run does not fit.
 * @note No switch and no fill: at every length verba_g asks for, storing the bytes beats both. A
 *       memset costs about sixty cycles before it writes anything, and the runs here are three at
 *       the leading end and about seventeen at the trailing one, so none of them reach the length
 *       that would pay for the call.
 * @note The pass that would replace this loop with that call is -ftree-loop-distribute-patterns,
 *       which is on at -O2. This function is compiled with it off, so the loop stays a loop and the
 *       stores need no volatile to survive.
 */
static size_t zeros_plain(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {
        out[at + k] = '0';
    }
    return at + n;
}

/**
 * @brief The biased exponent field, reached the way libc offers it.
 *
 * @param[in] value Value to take apart.
 * @return          What frexp reports as the exponent.
 * @note Not the same number fract.exp returns - frexp gives the exponent of a mantissa in [0.5, 1)
 *       and fract.exp gives the stored field - but it is the call a caller makes to ask the
 *       question, and it is the cost of asking it.
 */
static int frexp_exponent(double value)
{
    int found = 0;

    (void)frexp(value, &found);
    return found;
}

/**
 * @brief The bit pattern of a double, reached without a union.
 *
 * @param[in] value Value to read.
 * @return          Its storage as a 64-bit pattern.
 * @note The portable spelling a caller writes when it has no union to hand: copy the storage into an
 *       integer of the same width. fract.to_bits reads the union member instead.
 */
static uint64_t bits_via_memcpy(double value)
{
    uint64_t held = 0u;

    memcpy(&held, &value, sizeof held);
    return held;
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

        // Correctness before any timing. A copy that is wrong is not fast, it is broken, and the
        // host suite cannot see a path the target assembler selected.
        printf("DB copy_check      disagreements=%u\n", (unsigned)copy_is_correct());

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

        // The float path's digit writer, which is where the 64-bit software division lives.
        {
            static const uint64_t mantissas[] = {123456ull, 1234567890ull, 12345678901234567ull,
                                                 123456789012345678ull};
            static const unsigned widths[] = {6u, 10u, 17u, 18u};

            for (unsigned which = 0; which < 4u; which++)
            {
                DBENCH_AB("digits", 20000u, widths[which],
                          (digits_descending(g_out, mantissas[which], widths[which]), DBENCH_KEEP(g_out)),
                          (digits_split(g_out, mantissas[which], widths[which]), DBENCH_KEEP(g_out)));
            }
        }

        // Every entry against the libc call a caller reaches for instead. snprintf is what newlib
        // gives an embedded target, and the whole of it counts: parse the format, walk the varargs,
        // convert, write. The A arm is the library, the B arm is libc.
        {
            static const char text[] = "the quick brown fox jumps over";
            const uint32_t iters = 5000u;

            // Two rows, because the two sides are not doing the same work unless they are told the
            // same things. GCC knows the length of a literal, so it folds the snprintf arm into a
            // memcpy; put is only handed the pointer, so it measures the string before copying it.
            // The first row is put measuring, against snprintf; the second hands put the length it
            // already has and compares against the copy libc actually performs.
            DBENCH_AB("s:put", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = text)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%s", text)));

            DBENCH_AB("s:put_n", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = text,
                                            .text_len = sizeof text - 1u)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            // The width test inside the cut against the width test hoisted into the caller. This is
            // the row that should have been run before verba_uint's narrow path was deleted: the
            // widest value that still fits 32 bits is the case that regressed, and the 64-bit value
            // is here to show the wide path was not paid for by the narrow one.
            DBENCH_AB("s:uint32", iters, 10u, DBENCH_KEEP(uint_test_inside(g_wide, 4294967295ull)),
                      DBENCH_KEEP(uint_test_outside(g_wide, 4294967295ull)));

            DBENCH_AB("s:uint64", iters, 20u,
                      DBENCH_KEEP(uint_test_inside(g_wide, 18446744073709551615ull)),
                      DBENCH_KEEP(uint_test_outside(g_wide, 18446744073709551615ull)));

            // The word run, addresses through the context against addresses in locals. Same loads,
            // same stores, same bytes; the only difference is where the two addresses sit while the
            // loop runs, and whether the may_alias store forces them back out to memory each time.
            {
                BenchCopyCtx one = {.dst = (uint8_t *)g_wide, .src = (const uint8_t *)text, .bytes = g_len};
                BenchCopyCtx two = {.dst = (uint8_t *)g_wide, .src = (const uint8_t *)text, .bytes = g_len};

                // The two arms write to different halves of the check buffer on purpose. Pointed at
                // the same destination they copy identical bytes from an identical source, and the
                // compiler is free to keep one and drop the other, which reads as both being fast.
                DBENCH_AB("s:words", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len,
                           copy_words_args(&one), DBENCH_KEEP(g_check_dst)),
                          (two.dst = g_check_dst + 64u, two.src = g_check_src + 64u, two.bytes = g_len,
                           copy_words_locals(&two), DBENCH_KEEP(g_check_dst)));

                // The pointer walking loop against the counted one. A countable loop is what lets the
                // compiler reach for a hardware loop and drop the per word branch.
                DBENCH_AB("s:counted", iters, sizeof text - 1u,
                          (one.dst = (uint8_t *)g_wide, one.src = (const uint8_t *)text, one.bytes = g_len,
                           copy_words_args(&one), DBENCH_KEEP(g_wide)),
                          (two.dst = (uint8_t *)g_wide, two.src = (const uint8_t *)text, two.bytes = g_len,
                           copy_words_counted(&two), DBENCH_KEEP(g_wide)));

                // One word an iteration against two. The loop's own arithmetic is the whole gap to a
                // hand written memcpy: it moves a word in about three cycles and this moves one in
                // about seven, and the difference is the decrement and the branch, not the move.
                // The bare word run against memcpy, which is what says whether the loop itself is
                // the distance or whether the stages around it are.
                DBENCH_AB("s:wordsvlibc", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len,
                           copy_words_args(&one), DBENCH_KEEP(g_check_dst)),
                          (memcpy(g_check_dst + 64u, g_check_src + 64u, g_len), DBENCH_KEEP(g_check_dst)));

                DBENCH_AB("s:words2", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len,
                           copy_words_args(&one), DBENCH_KEEP(g_check_dst)),
                          (two.dst = g_check_dst + 64u, two.src = g_check_src + 64u, two.bytes = g_len,
                           copy_words_two(&two), DBENCH_KEEP(g_check_dst)));
            }

            // One dispatch into straight line moves against the loop. This is the row that asks
            // whether the per word branch is what the copy is actually paying for.
            //
            // The source is the large check buffer rather than the literal every other row uses.
            // The dispatch cannot be shown to leave the literal alone: the head is only zero because
            // the destination happens to be aligned, and the compiler will not assume that, so it
            // reads the run as reaching a word past a thirty one byte array and refuses to build.
            DBENCH_AB("s:dispatch", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = (const char *)g_check_src,
                                 .size = g_len),
                       DBENCH_KEEP(g_wide)),
                      (copy_read_dispatch((uint8_t *)g_wide, g_check_src, g_len), DBENCH_KEEP(g_wide)));

            // Two more shapes for the stages around the word run, each against the copy as it is.
            DBENCH_AB("s:overlap", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len),
                       DBENCH_KEEP(g_wide)),
                      (copy_read_overlap((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:fastpath", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len),
                       DBENCH_KEEP(g_wide)),
                      (copy_read_fastpath((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)));

            // The library's three stage copy against the same work in one function, both against
            // memcpy. The word run alone already matches memcpy, so what this asks is whether the
            // distance is the bookkeeping the three stages carry between them.
            DBENCH_AB("s:flat", iters, sizeof text - 1u,
                      (copy_read_flat((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, g_len), DBENCH_KEEP(g_wide)));

            // The same copy at a length that divides into whole words. Against the thirty byte row,
            // the difference is proxim_tail walking two bytes one at a time.
            DBENCH_AB("s:copy32", iters, 32u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len_whole),
                       DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, g_len_whole), DBENCH_KEEP(g_wide)));

            // The copy on its own, with no room test and no entry above it, so the row above can be
            // read: whatever this one does not account for is what verba_put_n adds on top.
            DBENCH_AB("s:copy", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text,
                                 .size = sizeof text - 1u),
                       DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            // And the same entry with the call boundary gone, which is what a caller compiled with
            // link time optimization actually gets, against the memcpy libc actually performs.
            DBENCH_AB("s:put_flat", iters, sizeof text - 1u,
                      DBENCH_KEEP(verba_put_n_flat(g_wide, text, sizeof text - 1u)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            // The same two at a single byte. What put_n costs over the copy at one byte is fixed
            // overhead; what it costs over the copy at thirty is that same overhead plus whatever
            // scales, and the two rows together say which of the two it is.
            DBENCH_AB("s:copy1", iters, 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = 1u),
                       DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_n1", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = text,
                                            .text_len = 1u)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            // put_n against what a caller actually writes to get what put_n gives them. The memcpy
            // rows above compare against a copy that cannot refuse: no capacity, no offset, no
            // answer for where the next write goes. A caller filling a bounded buffer writes the
            // test as well, and that test is part of the cost on both sides or on neither.
            DBENCH_AB("s:put_safe", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = text,
                                            .text_len = g_len)),
                      DBENCH_KEEP(libc_put_n(g_wide, sizeof g_wide, 0u, text, g_len)));

            // put against what libc costs a caller who also has to measure. The row above hands the
            // snprintf arm a literal whose length GCC knows, so that arm never measures anything;
            // this one hides the pointer, so both sides walk the string before copying it.
            DBENCH_AB("s:put_meas", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = g_text)),
                      (memcpy(g_wide, g_text, strlen(g_text)), DBENCH_KEEP(g_wide)));

            // The same thirty bytes with the length hidden from the compiler on both arms, so this
            // is verba's copy against the memcpy libc actually links, not against inlined moves.
            DBENCH_AB("s:put_run", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .text = text,
                                            .text_len = g_len)),
                      (memcpy(g_wide, text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:ch", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .ch = 'x')),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%c", 'x')));

            DBENCH_AB("s:u32", iters, 10u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .val = 4294967295u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%u", 4294967295u)));

            // u64_clip had no row. It writes the same digits u64 does, right aligned in a column,
            // and it is the one entry in the module still walking a 64-bit divide and modulo per
            // digit - the thing verba_emit20 replaced everywhere else. Against the same snprintf
            // asked for the same column.
            DBENCH_AB("s:u64clip", iters, 20u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u,
                                            .val = 18446744073709551615ull, .columns = 24u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%24llu", 18446744073709551615ull)));

            DBENCH_AB("s:u64", iters, 20u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u,
                                            .val = 18446744073709551615ull)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%llu", 18446744073709551615ull)));

            DBENCH_AB("s:i64", iters, 19u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u,
                                            .sval = -9223372036854775807ll)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%lld", -9223372036854775807ll)));

            DBENCH_AB("s:hex", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .val = 0xDEADBEEFu)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%x", 0xDEADBEEFu)));

            // The two float forms, which is where the digit writer measured above actually lives
            DBENCH_AB("s:g", iters, 17u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .real = 3.14159265358979,
                                            .sig = 17u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*g", 17, 3.14159265358979)));

            DBENCH_AB("s:fixed", iters, 6u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = g_wide,
                                            .cap = sizeof g_wide, .at = 0u, .real = 3.14159265358979,
                                            .decimals = 6u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*f", 6, 3.14159265358979)));
        }

        // A whole record against the one snprintf call a caller writes instead. This is where the
        // per entry wins are supposed to compound: six fields, three of them values, and the libc
        // side pays one format parse for the lot while the mmgr side pays none at all.
        {
            const mmgr_fval fields[] = {MMGR_VSTR("id="),  MMGR_VU64(g_rec_u64), MMGR_VSTR(" x="),
                                        MMGR_VHEXW(g_rec_hex, 8), MMGR_VSTR(" f="), MMGR_VFIXW(g_rec_real, 4)};

            DBENCH_AB("s:record", 2000u, 34u,
                      DBENCH_KEEP(MMGR_CALL(numer.emit, NumerosCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .vals = fields, .nvals = sizeof fields / sizeof fields[0])),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "id=%llu x=%08lx f=%.4f",
                                           (unsigned long long)g_rec_u64, (unsigned long)g_rec_hex,
                                           g_rec_real)));
        }

        // Taking a double apart, against the libc that answers the same questions. fractio reads the
        // storage as a bit pattern through a union and masks; newlib reaches signbit and frexp,
        // which on these parts are calls into soft float. The sign row is the closest to like for
        // like, since signbit is the one of the three that is only a bit test either way.
        {
            const uint32_t iters = 5000u;

            // The zero run, tested once against tested once a byte. verba_zeros walks verba_ch per
            // zero and verba_ch tests the room it needs before every store, though the count is
            // settled before the first. Three widths, since a g with a small exponent writes one or
            // two and a fixed with a large one writes a dozen.
            for (unsigned which = 0; which < 3u; which++)
            {
                static const size_t runs[] = {2u, 6u, 18u};

                g_zero_n = runs[which];

                DBENCH_AB("s:z_fill", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_one_test(g_wide, sizeof g_wide, 0u, g_zero_n)));

                // The room test on its own, with the compiler denied the memset that made the row
                // above measure the fill rather than the test.
                DBENCH_AB("s:z_test", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_one_test_noopt(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_memor", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_memor_set(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_hybrid", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_hybrid(g_wide, sizeof g_wide, 0u, g_zero_n)));

                // The candidate as the library would carry it: no volatile, a dispatch into
                // straight line stores under the crossover and memor.set over it.
                DBENCH_AB("s:z_built", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_built(g_wide, sizeof g_wide, 0u, g_zero_n)));

                // The same shape with the long run written through proxim, which verba_scribo
                // already depends on, rather than memor.set, which it does not.
                DBENCH_AB("s:z_proxim", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_proxim(g_wide, sizeof g_wide, 0u, g_zero_n)));

                // The long run walked to a boundary and then stored a word at a time through the
                // aligned put, which is what a fill does and what the row above left out.
                DBENCH_AB("s:z_align", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_aligned_fill(g_wide, sizeof g_wide, 0u, g_zero_n)));

                // One room test and a plain loop, with the pattern pass turned off for this file so
                // the loop is not replaced by a call that costs sixty cycles before it writes.
                DBENCH_AB("s:z_plain", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_plain(g_wide, sizeof g_wide, 0u, g_zero_n)));
            }

            DBENCH_AB("s:sign", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(fract.sign, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(signbit(g_rec_real) ? 1 : 0));

            DBENCH_AB("s:exp", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(fract.exp, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(frexp_exponent(g_rec_real)));

            DBENCH_AB("s:to_bits", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(fract.to_bits, FractioCfg, .val = g_rec_real)),
                      DBENCH_KEEP(bits_via_memcpy(g_rec_real)));
        }

        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_out));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("verba")
