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
#include "transformo/transformo.h"
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
 * @brief The value the float profile rows place, and the two halves it resolves to.
 *
 * @note Hidden from the compiler for the reason every other value here is: handed the literal, GCC
 *       takes the double apart at build time and the front half of the entry disappears, which is
 *       precisely the part these rows exist to price.
 * @note 3.14159265358979 at six decimals is an integer part of 3 and a fraction of 141593. The two
 *       digit rows are given those directly, so they write the same digits the entry writes.
 */
static volatile double g_rec_real2 = 3.14159265358979;

/**
 * @brief A value below one, whose decimal exponent is negative.
 *
 * @note The exact power of ten path covers small positive exponents only, so this one reaches the
 *       pow5 walk instead, which is the half of the pass the pi rows never touch.
 */
static volatile double g_rec_small = 0.000123456789012345;
static volatile uint64_t g_fix_ip = 3u;
static volatile uint64_t g_fix_frac = 141593u;

/**
 * @brief The fraction bits verba_fixed hands the scaling pass for that value.
 *
 * @note Not volatile, because the entry takes its address and reads through the pointer: the
 *       compiler cannot see the value at build time either way, and a volatile here would price a
 *       reload the entry does not perform.
 */
static uint64_t g_fix_rem = 0x121FB54442D18ull;

/**
 * @brief A zero mantissa, which the scaling pass answers before it does any work.
 */
static uint64_t g_fix_zero = 0u;

/**
 * @brief How many power comparisons differed only in the low word of the significand.
 *
 * @note Counted apart from the disagreements, because the two are not the same finding: the low
 *       word feeds a halfway bit and a sticky bit, and the answer comes out of the high one.
 */
static uint32_t g_pow_lo_only = 0u;

/**
 * @brief The seventeen significant digits verba_g writes for that value, as one integer.
 */
static volatile uint64_t g_g_mant = 31415926535897932ull;

/**
 * @brief The same width of mantissa for a round value, which is the strip's other end.
 *
 * @note Two at seventeen significant digits. Sixteen trailing zeros, so the loop that drops them
 *       runs sixteen times where pi's runs once, and round numbers are not the rare input.
 */
static volatile uint64_t g_g_round = 20000000000000000ull;

/**
 * @brief Powers of ten the scan counter compares against, as verba_scribo carries them.
 */
static const uint32_t POW10[10] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u, 1000000000u};

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
        (n_) = ((n_) & 0xFFFFFFFFULL) * 100ULL;                                                                        \
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
    return MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = out, .cap = 16u, .at = 0u, .val = v, .base = 10u,
                     .min = 1u);
}

/**
 * @brief Powers of ten the mantissa is measured and cut against, as verba_scribo holds them.
 *
 * @note Stands in for mmgr_verba_pow10, which is static to verba_scribo.c and so out of reach here.
 */
static const uint64_t POW10_64[20] = {1ull,
                                      10ull,
                                      100ull,
                                      1000ull,
                                      10000ull,
                                      100000ull,
                                      1000000ull,
                                      10000000ull,
                                      100000000ull,
                                      1000000000ull,
                                      10000000000ull,
                                      100000000000ull,
                                      1000000000000ull,
                                      10000000000000ull,
                                      100000000000000ull,
                                      1000000000000000ull,
                                      10000000000000000ull,
                                      100000000000000000ull,
                                      1000000000000000000ull,
                                      10000000000000000000ull};

/**
 * @brief Index into POW10_64 of the power of ten the mantissa is cut at.
 *
 * @note Eight, so each piece is at most nine digits, which is the widest that fits a uint32_t.
 */
#define CUT 8u

/**
 * @brief Drops trailing zeros a digit at a time, as verba_g does.
 *
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many there are.
 * @return            How many are left once the zeros are gone.
 * @note The A arm. One modulo to test and one division to drop, per zero.
 */
static unsigned strip_loop(uint64_t mant, unsigned digits)
{
    while ((digits > 1u) && ((mant % 10u) == 0u))
    {
        mant /= 10u;
        digits--;
    }
    return digits;
}

/**
 * @brief The same walk with one division a zero rather than a modulo and a division.
 *
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many there are.
 * @return            How many are left once the zeros are gone.
 * @note The quotient is taken first and the remainder derived from it, so each step is one division
 *       by a constant rather than a modulo and a division by the same constant. Both compile to a
 *       reciprocal multiply, so this asks whether the compiler was already sharing one between them.
 */
static unsigned strip_once(uint64_t mant, unsigned digits)
{
    while (digits > 1u)
    {
        const uint64_t q = mant / 10u;

        if ((mant - (q * 10u)) != 0u)
        {
            break;
        }
        mant = q;
        digits--;
    }
    return digits;
}

/**
 * @brief The multiplicative inverse of five in 64 bits, so five times this is one.
 *
 * @note Divides a value already known to be a multiple of five, exactly and with no division: the
 *       product is the quotient outright.
 */
#define INV5 0xCCCCCCCCCCCCCCCDull

/**
 * @brief The largest 64-bit value divisible by five, over five.
 *
 * @note A value times INV5 lands at or below this exactly when five divides it, which is what turns
 *       the inverse into a divisibility test as well as a divider.
 */
#define FIFTH_MAX 0x3333333333333333ull

/**
 * @brief The same walk with the divide written as a reciprocal multiply.
 *
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many there are.
 * @return            How many are left once the zeros are gone.
 * @note Ten is two times five. The low bit answers the two, and multiplying the halved value by the
 *       inverse of five answers the five and produces the quotient in the same operation: a product
 *       at or below FIFTH_MAX means five divided it, and that product is the result.
 * @note No division of any width. div100 above is written for the same reason - the compiler reaches
 *       for the part's divider, and on these parts the divider is the slower of the two, or absent.
 */
static unsigned strip_recip(uint64_t mant, unsigned digits)
{
    while (digits > 1u)
    {
        // The low bit is the divisibility by two, and the product below is both the test for five
        // and the quotient it would produce
        if ((mant & 1u) != 0u)
        {
            break;
        }

        const uint64_t fifth = (mant >> 1) * INV5;

        if (fifth > FIFTH_MAX)
        {
            break;
        }
        mant = fifth;
        digits--;
    }
    return digits;
}

/**
 * @brief Finds the same count against the powers of ten already held.
 *
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many there are.
 * @return            How many are left once the zeros are gone.
 * @note The B arm. The count of trailing zeros is the largest k whose power of ten divides the
 *       mantissa, and the table that holds those powers is already here. Halving the range answers
 *       it in five tests for every width a caller can ask for, where the walk takes one per zero.
 */
static unsigned strip_pow(uint64_t mant, unsigned digits)
{
    unsigned low = 0u;
    unsigned high = digits - 1u;

    while (low < high)
    {
        // The midpoint is taken high so the range always shrinks: a low of k and a high of k+1
        // would otherwise settle on k and test it forever
        const unsigned mid = low + ((high - low + 1u) / 2u);

        if ((mant % POW10_64[mid]) == 0u)
        {
            low = mid;
        }
        else
        {
            high = mid - 1u;
        }
    }
    return digits - low;
}

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
 * @brief The copy entry pulled into the caller, so the entry boundary can be priced on its own.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note At a length that divides into whole words the copy has no head and no tail, and it is still
 *       twenty cycles behind memcpy. That distance was asserted to be the argument pack and the
 *       three stages reading their context, on no row at all. This is the row: the same entry, the
 *       same bytes, called the ordinary way against the same thing inlined into the caller.
 */
MMGR_FLATTEN static void proxim_read_flat(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    MMGR_CALL(proxim.read, ProximusCfg, .dst = dst, .at = src, .size = bytes);
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
        *(bench_aequus_word_t *)(args->dst + MMGR_RAW_WORD) = *(const bench_aequus_word_t *)(args->src + MMGR_RAW_WORD);
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
    default: {
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
 * @brief The 128-bit product of two 64-bit values, as muto_mul leaves it.
 */
typedef struct
{
    uint64_t hi; /**< Top 64 bits. */
    uint64_t lo; /**< Bottom 64 bits. */
} BenchProduct;

/**
 * @brief The multiply as transformo writes it, with both halves held at 64 bits.
 *
 * @param[in]  a   First operand.
 * @param[in]  b   Second operand.
 * @param[out] out Where the product goes [BORROWS].
 * @note The A arm. Each operand is split at 32 bits, but the halves keep the 64-bit type they were
 *       masked out of, so every one of the four partial products is written as a 64 by 64 multiply.
 *       Whether that costs anything depends on whether the compiler narrows them itself from the
 *       mask, which is the question the row asks rather than assumes.
 */
static void bench_mul_wide(uint64_t a, uint64_t b, BenchProduct *out)
{
    const uint64_t half = (uint64_t)0xFFFFFFFFu;
    const uint64_t a0 = a & half;
    const uint64_t a1 = a >> 32;
    const uint64_t b0 = b & half;
    const uint64_t b1 = b >> 32;

    const uint64_t p00 = a0 * b0;
    const uint64_t p01 = a0 * b1;
    const uint64_t p10 = a1 * b0;
    const uint64_t p11 = a1 * b1;
    const uint64_t mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    out->lo = (p00 & half) | (mid << 32);
    out->hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief The same product with each half narrowed to 32 bits before it is multiplied.
 *
 * @param[in]  a   First operand.
 * @param[in]  b   Second operand.
 * @param[out] out Where the product goes [BORROWS].
 * @note The B arm. Identical arithmetic and identical result; the only difference is that each
 *       partial product is a 32 by 32 multiply widening to 64, which both these parts carry as a
 *       pair of instructions, rather than a 64 by 64 one.
 */
static void bench_mul_narrow(uint64_t a, uint64_t b, BenchProduct *out)
{
    // Explicit casts narrow each half to the width it actually holds, so the products below are
    // 32 by 32 widening multiplies rather than full 64-bit ones
    const uint32_t a0 = (uint32_t)a;
    const uint32_t a1 = (uint32_t)(a >> 32);
    const uint32_t b0 = (uint32_t)b;
    const uint32_t b1 = (uint32_t)(b >> 32);

    const uint64_t p00 = (uint64_t)a0 * b0;
    const uint64_t p01 = (uint64_t)a0 * b1;
    const uint64_t p10 = (uint64_t)a1 * b0;
    const uint64_t p11 = (uint64_t)a1 * b1;
    const uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;

    out->lo = ((uint64_t)(uint32_t)p00) | (mid << 32);
    out->hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/**
 * @brief A 128-bit significand and the binary exponent that goes with it.
 */
typedef struct
{
    uint64_t hi;   /**< Top 64 bits. */
    uint64_t lo;   /**< Bottom 64 bits. */
    int32_t fe2;   /**< Binary exponent. */
    uint32_t rest; /**< Set once bits have been discarded below the kept 128. */
} BenchWide;

/**
 * @brief Shifts a significand up until its top bit is set, as muto_norm does.
 *
 * @param[in,out] w The significand and its exponent [BORROWS].
 * @note Both arms below end with this, so it is not what separates them.
 */
static void bench_norm(BenchWide *w)
{
    if (w->hi == 0u)
    {
        if (w->lo == 0u)
        {
            return;
        }
        w->hi = w->lo;
        w->lo = 0u;
        w->fe2 -= 64;
    }

    // Explicit cast narrows the iword the clz entry returns to the shift count
    const int32_t n = (int32_t)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)w->hi);
    if (n != 0)
    {
        w->hi = (w->hi << n) | (w->lo >> (64 - n));
        w->lo <<= n;
        w->fe2 -= n;
    }
}

/**
 * @brief One application of a power of five, as muto_mul_pow5 performs it.
 *
 * @param[in,out] w   The significand and its exponent [BORROWS].
 * @param[in]     ghi Top half of the 128-bit power [BORROWS].
 * @param[in]     glo Bottom half of it.
 * @param[in]     ge2 The power's own binary exponent.
 * @note The A arm. A 128 by 128 product needs four multiplies and the columns summed with their
 *       carries, and apply_pow10 runs one of these per set bit of the decimal exponent.
 */
static void bench_pow_128(BenchWide *w, uint64_t ghi, uint64_t glo, int32_t ge2)
{
    BenchProduct hh;
    BenchProduct hl;
    BenchProduct lh;
    BenchProduct ll;

    bench_mul_narrow(w->hi, ghi, &hh);
    bench_mul_narrow(w->hi, glo, &hl);
    bench_mul_narrow(w->lo, ghi, &lh);
    bench_mul_narrow(w->lo, glo, &ll);

    uint64_t carry = 0u;
    uint64_t col1 = ll.hi + hl.lo;
    carry += (col1 < ll.hi) ? 1u : 0u;
    const uint64_t col1b = col1 + lh.lo;
    carry += (col1b < col1) ? 1u : 0u;
    col1 = col1b;

    uint64_t col2 = hh.lo + hl.hi;
    uint64_t carry2 = (col2 < hh.lo) ? 1u : 0u;
    const uint64_t col2b = col2 + lh.hi;
    carry2 += (col2b < col2) ? 1u : 0u;
    col2 = col2b + carry;
    carry2 += (col2 < col2b) ? 1u : 0u;

    if ((ll.lo != 0u) || (col1 != 0u))
    {
        w->rest = 1u;
    }
    w->hi = hh.hi + carry2;
    w->lo = col2;
    w->fe2 = w->fe2 + ge2 + 128;
    bench_norm(w);
}

/**
 * @brief The same significand multiplied by one exact 64-bit power of ten.
 *
 * @param[in,out] w The significand and its exponent [BORROWS].
 * @param[in]     g The power of ten, exact in 64 bits.
 * @note The B arm. A 128 by 64 product is two multiplies rather than four, its top 128 bits are one
 *       column sum rather than three, and one of these covers the whole decimal exponent rather than
 *       one per set bit of it.
 * @note No exponent is added for the power itself: g carries the whole of ten raised to the
 *       exponent, both its five and its two, where the pow5 tables hold only the five and
 *       apply_pow10 adds the two afterwards.
 */
static void bench_pow_64(BenchWide *w, uint64_t g)
{
    BenchProduct hh;
    BenchProduct lh;

    bench_mul_narrow(w->hi, g, &hh);
    bench_mul_narrow(w->lo, g, &lh);

    const uint64_t col = hh.lo + lh.hi;
    const uint64_t carry = (col < hh.lo) ? 1u : 0u;

    if (lh.lo != 0u)
    {
        w->rest = 1u;
    }
    w->hi = hh.hi + carry;
    w->lo = col;
    w->fe2 = w->fe2 + 64;
    bench_norm(w);
}

/**
 * @brief The power walk as apply_pow10 performs it: every step tested, always.
 *
 * @param[in,out] w  The significand and its exponent [BORROWS].
 * @param[in]     ex The decimal exponent to apply.
 * @note The A arm. The loop runs MMGR_POW5_STEPS times whatever the exponent is, so an exponent of
 *       zero still walks nine iterations testing bits that are not there.
 */
static void bench_walk_fixed(BenchWide *w, int32_t ex)
{
    const int32_t k = (ex < 0) ? -ex : ex;

    for (int32_t i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            const MmgrPow5 *const p = (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];

            bench_pow_128(w, p->hi, p->lo, (int32_t)p->e2);
        }
    }
    w->fe2 += ex;
}

/**
 * @brief The same walk, stopping once no bits are left rather than at a fixed count.
 *
 * @param[in,out] w  The significand and its exponent [BORROWS].
 * @param[in]     ex The decimal exponent to apply.
 * @note The B arm. Identical powers applied in the identical order; the exponent is shifted down as
 *       it goes so the loop ends when there is nothing left in it, which for a small exponent is
 *       most of the iterations and for a zero one is all of them.
 */
static void bench_walk_early(BenchWide *w, int32_t ex)
{
    int32_t k = (ex < 0) ? -ex : ex;

    // Bounded by the step count as well, so an exponent past the tables loses its high bits exactly
    // as it did before rather than reading off the end
    for (int32_t i = 0; (i < MMGR_POW5_STEPS) && (k != 0); ++i)
    {
        if ((k & 1) != 0)
        {
            const MmgrPow5 *const p = (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];

            bench_pow_128(w, p->hi, p->lo, (int32_t)p->e2);
        }
        k >>= 1;
    }
    w->fe2 += ex;
}

/**
 * @brief Powers of ten held exactly in 64 bits, for the walk that applies them a chunk at a time.
 */
static const uint64_t BENCH_POW10_U64[19] = {1ull,
                                             10ull,
                                             100ull,
                                             1000ull,
                                             10000ull,
                                             100000ull,
                                             1000000ull,
                                             10000000ull,
                                             100000000ull,
                                             1000000000ull,
                                             10000000000ull,
                                             100000000000ull,
                                             1000000000000ull,
                                             10000000000000ull,
                                             100000000000000ull,
                                             1000000000000000ull,
                                             10000000000000000ull,
                                             100000000000000000ull,
                                             1000000000000000000ull};

/**
 * @brief A positive exponent applied as exact powers of ten, as many as it takes.
 *
 * @param[in,out] w  The significand and its exponent [BORROWS].
 * @param[in]     ex The decimal exponent to apply, positive.
 * @note Ten to the eighteenth is the largest that fits 64 bits, so an exponent above it is applied
 *       in chunks of eighteen rather than by walking the bits of the wide tables. Each chunk is a
 *       128 by 64 multiply, which is two of the narrow multiplies; each bit of the walk it replaces
 *       is a 128 by 128 one, which is four, and there are as many of those as the exponent has bits
 *       set.
 */
static void bench_walk_exact(BenchWide *w, int32_t ex)
{
    int32_t left = ex;

    while (left > 0)
    {
        const int32_t take = (left > 18) ? 18 : left;

        bench_pow_64(w, BENCH_POW10_U64[take]);
        left -= take;
    }
    w->fe2 += 0;
}

/**
 * @brief Checks the chunked powers against the bit walk they would replace.
 *
 * @return The number of exponents where the two disagree.
 * @note Speed is not the only question here. Each application keeps the top 128 bits and records
 *       the rest as a sticky bit, so chaining more of them accumulates more truncation, and the
 *       chunked form chains ceil(ex/18) where the walk chains one per set bit. At an exponent of
 *       forty that is three against two. Whether that changes the significand is not something to
 *       reason about - it is something to compare, over every exponent either form would be given
 *       and over several starting significands.
 */
static uint32_t pow_is_correct(void)
{
    static const uint64_t seeds[5] = {0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x9E3779B97F4A7C15ull,
                                      0x0123456789ABCDEFull, 0xC000000000000000ull};
    uint32_t bad = 0u;

    for (unsigned which = 0; which < 5u; which++)
    {
        for (int32_t ex = 1; ex <= 60; ex++)
        {
            BenchWide walked = {seeds[which], 0u, -51, 0u};
            BenchWide chunked = {seeds[which], 0u, -51, 0u};

            bench_walk_fixed(&walked, ex);
            bench_walk_exact(&chunked, ex);

            // Where the two differ decides whether it matters. muto_to_u64 takes the answer out of
            // hi and reads lo only for a halfway bit and a sticky one, so a difference confined to
            // lo cannot move a rounded 64-bit result; a difference in hi or in the exponent can.
            if ((walked.hi != chunked.hi) || (walked.fe2 != chunked.fe2))
            {
                bad++;
            }
            else if (walked.lo != chunked.lo)
            {
                g_pow_lo_only++;
            }
        }
    }
    return bad;
}

/**
 * @brief The two operands the multiply rows use, hidden so neither arm folds.
 */
static volatile uint64_t g_mul_a = 0x123456789ABCDEFull;
static volatile uint64_t g_mul_b = 0xFEDCBA987654321ull;

/**
 * @brief Ten to the sixth, which is what six decimals asks the scaling pass for.
 *
 * @note Exact in 64 bits, as every power of ten up to the eighteenth is, and eighteen is where
 *       MMGR_FIXED_MAX_DECIMALS holds the fixed path.
 */
static volatile uint64_t g_pow_ten = 1000000ull;

/**
 * @brief The significand the two shapes are applied to, and where each leaves its result.
 */
static BenchWide g_wide_a;
static BenchWide g_wide_b;

/**
 * @brief Where the multiply rows leave their products.
 */
static BenchProduct g_mul_out;

/**
 * @brief Expands to the constant that replaces the division by ten to the eighth.
 *
 * @note The ceiling of two to the ninetieth over ten to the eighth. The approximation only ever
 *       rounds up, so the quotient is exact while the error accumulated over the input stays under
 *       one unit, and that holds to ten to the twentieth - past what a uint64_t can hold, so it is
 *       exact for every value one can be given.
 */
#define CUT_MAGIC 0xABCC77118461CEFDull

/**
 * @brief Expands to what comes off the high word to finish the shift by ninety.
 */
#define CUT_SHIFT 26u

/**
 * @brief Divides by ten to the eighth with a multiply rather than a division.
 *
 * @param[in] value Value to cut, up to twenty digits.
 * @return          value / 100000000.
 * @note The whole 128-bit product is formed and the quotient taken out of its high word, which is
 *       the only way to reach a reciprocal this wide without a 128-bit type.
 */
static uint64_t cut_magic(uint64_t value)
{
    BenchProduct p;

    bench_mul_narrow(value, CUT_MAGIC, &p);
    return p.hi >> CUT_SHIFT;
}

/**
 * @brief Checks the multiply against the division it replaces, on the part itself.
 *
 * @return The number of disagreements, which must be zero.
 * @note A constant derived on a host and then trusted is how a wrong answer gets shipped quietly.
 *       This walks every power of ten and its neighbours, both sides of the 32-bit boundary, and a
 *       spread of values between, and compares against the division the target itself performs.
 */
static uint32_t cut_is_correct(void)
{
    uint32_t bad = 0u;
    uint64_t probe = 1u;

    for (unsigned k = 0; k < 20u; k++)
    {
        const uint64_t around[3] = {probe - 1u, probe, probe + 1u};

        for (unsigned which = 0; which < 3u; which++)
        {
            if (cut_magic(around[which]) != (around[which] / POW10_64[8]))
            {
                bad++;
            }
        }
        probe *= 10u;
    }

    uint64_t walk = 7u;

    for (unsigned step = 0; step < 4000u; step++)
    {
        if (cut_magic(walk) != (walk / POW10_64[8]))
        {
            bad++;
        }
        // Wraps on purpose, which spreads the probe across the whole width instead of leaving it in
        // one decade
        walk = (walk * 6364136223846793005ull) + 1442695040888963407ull;
    }
    return bad;
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

                MMGR_CALL(proxim.read, ProximusCfg, .dst = g_check_dst + doff, .at = g_check_src + soff, .size = len);

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
    default: {
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
    default: {
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
 * @brief Digits placed one at a time, each testing for room of its own, with a point among them.
 *
 * @param[out] out    Destination [BORROWS].
 * @param[in]  cap    Bytes available.
 * @param[in]  at     Offset to write at.
 * @param[in]  src    The digits, already laid down [BORROWS].
 * @param[in]  n      How many.
 * @param[in]  point  Digits before the point; 0 writes no point.
 * @return            The offset past what was written, or cap once something did not fit.
 * @note The shape verba_digits has: verba_emit20 lays the digits into a scratch buffer and then a
 *       loop places them one at a time, each through verba_ch, which tests before every store. The
 *       count is settled before the first one.
 */
static size_t digits_per_char(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    for (size_t index = 0; index < n; index++)
    {
        if ((index == point) && (index != 0u))
        {
            if ((at >= cap) || (1u > ((cap - at) - 1u)))
            {
                return cap;
            }
            out[at] = '.';
            at += 1u;
        }
        if ((at >= cap) || (1u > ((cap - at) - 1u)))
        {
            return cap;
        }
        out[at] = src[index];
        at += 1u;
    }
    return at;
}

/**
 * @brief The same digits with the room tested once for the whole run, point included.
 *
 * @param[out] out    Destination [BORROWS].
 * @param[in]  cap    Bytes available.
 * @param[in]  at     Offset to write at.
 * @param[in]  src    The digits, already laid down [BORROWS].
 * @param[in]  n      How many.
 * @param[in]  point  Digits before the point; 0 writes no point.
 * @return            The offset past what was written, or cap when the run does not fit.
 * @note The point is one more byte and its position is known, so the whole width is known before
 *       anything is written and one test covers it. A run that does not fit writes nothing, where
 *       the per character form writes what fitted and then reports cap.
 */
static size_t digits_one_test(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    const size_t width = n + (((point != 0u) && (point < n)) ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    size_t put = at;

    for (size_t index = 0; index < n; index++)
    {
        if ((index == point) && (index != 0u))
        {
            out[put] = '.';
            put += 1u;
        }
        out[put] = src[index];
        put += 1u;
    }
    return put;
}

/**
 * @brief The same digits placed as two straight runs with the point between them.
 *
 * @param[out] out    Destination [BORROWS].
 * @param[in]  cap    Bytes available.
 * @param[in]  at     Offset to write at.
 * @param[in]  src    The digits, already laid down [BORROWS].
 * @param[in]  n      How many.
 * @param[in]  point  Digits before the point; 0 writes no point.
 * @return            The offset past what was written, or cap when the run does not fit.
 * @note Where the point falls is settled before anything is written, so asking once per character
 *       whether this is the character the point precedes is a test repeated n times for an answer
 *       that changes once. This writes the digits up to it, then the point, then the rest.
 */
static size_t digits_two_runs(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < n));
    const size_t width = n + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    const size_t lead = has_point ? point : n;
    size_t put = at;

    for (size_t index = 0; index < lead; index++)
    {
        out[put + index] = src[index];
    }
    put += lead;

    if (has_point)
    {
        out[put] = '.';
        put += 1u;

        for (size_t index = lead; index < n; index++)
        {
            out[put + (index - lead)] = src[index];
        }
        put += n - lead;
    }
    return put;
}

/**
 * @brief The whole placement as the library now performs it: emit into scratch, then copy it out.
 *
 * @param[out] out    Destination [BORROWS].
 * @param[in]  cap    Bytes available.
 * @param[in]  at     Offset to write at.
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many.
 * @param[in]  point  Digits before the point; 0 writes no point.
 * @return            The offset past what was written, or cap when the run does not fit.
 * @note The A arm, and unlike the rows above it this one includes the emit, so the two arms differ
 *       only in where the digits are laid down and what has to move afterwards.
 */
static size_t digits_scratch(char *out, size_t cap, size_t at, uint64_t mant, unsigned digits, size_t point)
{
    char scratch[24];
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < digits));
    const size_t width = digits + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    emit20(scratch, mant, digits);

    const size_t lead = has_point ? point : digits;
    size_t put = at;

    for (size_t index = 0; index < lead; index++)
    {
        out[put + index] = scratch[index];
    }
    put += lead;

    if (has_point)
    {
        out[put] = '.';
        put += 1u;

        for (size_t index = lead; index < digits; index++)
        {
            out[put + (index - lead)] = scratch[index];
        }
        put += digits - lead;
    }
    return put;
}

/**
 * @brief The same placement with the digits emitted where they are going.
 *
 * @param[out] out    Destination [BORROWS].
 * @param[in]  cap    Bytes available.
 * @param[in]  at     Offset to write at.
 * @param[in]  mant   The digits as one integer.
 * @param[in]  digits How many.
 * @param[in]  point  Digits before the point; 0 writes no point.
 * @return            The offset past what was written, or cap when the run does not fit.
 * @note The B arm. With no point the digits go straight to their place and nothing is copied at all.
 *       With one, they are emitted a byte along and only the digits ahead of the point slide back,
 *       which for the exponential form verba_g writes is a single byte rather than the whole run.
 * @warning The slide runs upward through overlapping bytes on purpose: each byte is read before the
 *          one below it is written, so the source of every step is still untouched.
 */
static size_t digits_inplace(char *out, size_t cap, size_t at, uint64_t mant, unsigned digits, size_t point)
{
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < digits));
    const size_t width = digits + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    if (!has_point)
    {
        emit20(out + at, mant, digits);
        return at + digits;
    }

    emit20(out + at + 1u, mant, digits);

    for (size_t index = 0; index < point; index++)
    {
        out[at + index] = out[at + index + 1u];
    }
    out[at + point] = '.';
    return at + width;
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
        printf("DB cut_check       disagreements=%u\n", (unsigned)cut_is_correct());
        g_pow_lo_only = 0u;
        printf("DB pow_check       hi_or_exp=%u lo_only=%u\n", (unsigned)pow_is_correct(), (unsigned)g_pow_lo_only);

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
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_out, .cap = sizeof g_out,
                                            .at = 0u, .val = v, .base = 10u, .min = 1u)),
                      DBENCH_KEEP(verba_uint_flat(g_out, v)));

            // And the arithmetic on its own, both arms inlined.
            DBENCH_AB("arith", iters, d, DBENCH_KEEP(was(g_out, v)), DBENCH_KEEP(verba_uint_flat(g_out, v)));
        }

        // What the harness costs with no work in it. Every row above carries this.

        // The float path's digit writer, which is where the 64-bit software division lives.
        {
            static const uint64_t mantissas[] = {123456ull, 1234567890ull, 12345678901234567ull, 123456789012345678ull};
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
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%s", text)));

            DBENCH_AB("s:put_n", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = sizeof text - 1u)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            // The width test inside the cut against the width test hoisted into the caller. This is
            // the row that should have been run before verba_uint's narrow path was deleted: the
            // widest value that still fits 32 bits is the case that regressed, and the 64-bit value
            // is here to show the wide path was not paid for by the narrow one.
            DBENCH_AB("s:uint32", iters, 10u, DBENCH_KEEP(uint_test_inside(g_wide, 4294967295ull)),
                      DBENCH_KEEP(uint_test_outside(g_wide, 4294967295ull)));

            DBENCH_AB("s:uint64", iters, 20u, DBENCH_KEEP(uint_test_inside(g_wide, 18446744073709551615ull)),
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
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
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
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
                          (memcpy(g_check_dst + 64u, g_check_src + 64u, g_len), DBENCH_KEEP(g_check_dst)));

                DBENCH_AB("s:words2", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
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
            DBENCH_AB(
                "s:dispatch", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = (const char *)g_check_src, .size = g_len),
                 DBENCH_KEEP(g_wide)),
                (copy_read_dispatch((uint8_t *)g_wide, g_check_src, g_len), DBENCH_KEEP(g_wide)));

            // Two more shapes for the stages around the word run, each against the copy as it is.
            DBENCH_AB(
                "s:overlap", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len), DBENCH_KEEP(g_wide)),
                (copy_read_overlap((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB(
                "s:fastpath", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len), DBENCH_KEEP(g_wide)),
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

            // The entry boundary on the copy, at the length where the head and the tail both do
            // nothing. Whatever separates these two arms is what MMGR_CALL and the argument pack
            // cost; whatever is left between the faster of them and memcpy is the copy itself.
            DBENCH_AB("s:copy_call", iters, 32u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len_whole),
                       DBENCH_KEEP(g_wide)),
                      (proxim_read_flat((uint8_t *)g_wide, (const uint8_t *)text, g_len_whole), DBENCH_KEEP(g_wide)));

            // The copy on its own, with no room test and no entry above it, so the row above can be
            // read: whatever this one does not account for is what verba_put_n adds on top.
            DBENCH_AB("s:copy", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = sizeof text - 1u),
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
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = 1u), DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_n1", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = 1u)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            // put_n against what a caller actually writes to get what put_n gives them. The memcpy
            // rows above compare against a copy that cannot refuse: no capacity, no offset, no
            // answer for where the next write goes. A caller filling a bounded buffer writes the
            // test as well, and that test is part of the cost on both sides or on neither.
            DBENCH_AB("s:put_safe", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = g_len)),
                      DBENCH_KEEP(libc_put_n(g_wide, sizeof g_wide, 0u, text, g_len)));

            // put against what libc costs a caller who also has to measure. The row above hands the
            // snprintf arm a literal whose length GCC knows, so that arm never measures anything;
            // this one hides the pointer, so both sides walk the string before copying it.
            DBENCH_AB("s:put_meas", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = g_text)),
                      (memcpy(g_wide, g_text, strlen(g_text)), DBENCH_KEEP(g_wide)));

            // The same thirty bytes with the length hidden from the compiler on both arms, so this
            // is verba's copy against the memcpy libc actually links, not against inlined moves.
            DBENCH_AB("s:put_run", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = g_len)),
                      (memcpy(g_wide, text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:ch", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .ch = 'x')),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%c", 'x')));

            DBENCH_AB("s:u32", iters, 10u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 4294967295u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%u", 4294967295u)));

            // u64_clip had no row. It writes the same digits u64 does, right aligned in a column,
            // and it is the one entry in the module still walking a 64-bit divide and modulo per
            // digit - the thing verba_emit20 replaced everywhere else. Against the same snprintf
            // asked for the same column.
            DBENCH_AB(
                "s:u64clip", iters, 20u,
                DBENCH_KEEP(MMGR_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                      .at = 0u, .val = 18446744073709551615ull, .columns = 24u)),
                DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%24llu", 18446744073709551615ull)));

            DBENCH_AB("s:u64", iters, 20u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 18446744073709551615ull)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%llu", 18446744073709551615ull)));

            DBENCH_AB("s:i64", iters, 19u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .sval = -9223372036854775807ll)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%lld", -9223372036854775807ll)));

            DBENCH_AB("s:hex", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 0xDEADBEEFu)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%x", 0xDEADBEEFu)));

            // The two float forms, which is where the digit writer measured above actually lives
            DBENCH_AB("s:g", iters, 17u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = 3.14159265358979, .sig = 17u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*g", 17, 3.14159265358979)));

            DBENCH_AB("s:fixed", iters, 6u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = 3.14159265358979, .decimals = 6u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*f", 6, 3.14159265358979)));

            // Where the float path's cycles actually sit. fixed is the most expensive entry in the
            // module by an order of magnitude and no row has ever said which part of it costs, so
            // these price the pieces it is built from against the whole. What the two digit writers
            // and the character do not account for is the front half: taking the double apart and
            // the scaling pass that rounds the fraction.
            //
            // 3.14159265358979 at six decimals is an integer part of 3 and a fraction of 141593,
            // which are the values the entry itself reaches with, so the parts below do the same
            // work on the same numbers rather than standing in for it.
            DBENCH_OP("f:whole", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_real2, .decimals = 6u)));

            DBENCH_OP("f:ip", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_fix_ip, .base = 10u, .min = 1u)));

            DBENCH_OP("f:frac", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_fix_frac, .base = 10u, .min = 6u)));

            DBENCH_OP("f:point", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .ch = '.')));

            // The scaling pass on its own, given exactly what verba_fixed hands it for this value.
            // 3.14159265358979 has a stored exponent of 1024 and a mantissa of 0x1921FB54442D18
            // with its implicit bit, so the binary exponent is -51, the integer part shifts out as
            // 3, and 0x121FB54442D18 is the remainder left for the fraction. Six decimals means
            // apply_pow10 walks the two set bits of six and runs muto_mul_pow5 twice.
            DBENCH_OP("f:scale", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 6,
                                            .above = 0u)));

            // What verba_g spends outside the scaling pass. It writes seventeen digits with a point
            // among them, and it runs the pass once per correction of its exponent estimate, up to
            // four times. The pass is priced above, so what these two do not account for is how
            // many times it ran.
            DBENCH_OP("g:whole", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_real2, .sig = 17u)));

            // The trailing zero strip verba_g runs before it picks a form. It tests one 64-bit
            // modulo per digit it considers dropping and performs a 64-bit division for each one it
            // drops, and neither part carries a 64-bit divider, so both are libgcc calls. The
            // seventeen digit mantissa here ends in a two, so the loop tests once and drops nothing:
            // this is the cheapest that strip ever is.
            DBENCH_OP("g:strip", iters, DBENCH_KEEP((g_g_mant % 10u) == 0u));

            // The walk against the table check, at both ends of the input. Pi ends in a two, so the
            // walk stops immediately and this is the case where it is cheapest; two at seventeen
            // significant digits carries sixteen trailing zeros, so the walk runs sixteen times,
            // and round numbers are not a rare input.
            DBENCH_AB("g:strip_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_pow(g_g_mant, 17u)));

            DBENCH_AB("g:strip_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_pow(g_g_round, 17u)));

            // The walk against itself with the remainder taken off the quotient, so each step is
            // one division by ten rather than a modulo and a division by ten. Both ends again,
            // since a shape that helps the sixteen zero case must not cost the common one.
            DBENCH_AB("g:strip_one_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_once(g_g_mant, 17u)));

            DBENCH_AB("g:strip_one_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_once(g_g_round, 17u)));

            // And the same walk with no division in it at all, against the one the library carries.
            DBENCH_AB("g:strip_rec_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_recip(g_g_mant, 17u)));

            DBENCH_AB("g:strip_rec_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_recip(g_g_round, 17u)));

            // The same entry on a value below one. Its decimal exponent is negative, so the cursor
            // runs past what an exact power of ten covers and the pass walks the pow5 tables - the
            // path the row above never takes and the one most values under one land on.
            DBENCH_OP("g:small", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_small, .sig = 17u)));

            DBENCH_OP("g:digits", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_g_mant, .base = 10u, .min = 17u)));

            // What the digit emit costs at the width verba_g asks for, and the cut inside it. The
            // value is seventeen digits, so it does not fit a uint32_t and emit20 divides it by ten
            // to the eighth to split it. That divisor is a constant, but a 64-bit division by a
            // constant is not always a multiply on a 32-bit part, and if it is a libgcc call it is
            // most of what the emit costs.
            DBENCH_OP("g:emit", iters, (emit20(g_wide, g_g_mant, 17u), DBENCH_KEEP(g_wide)));

            DBENCH_OP("g:cut", iters, DBENCH_KEEP(g_g_mant / POW10_64[8]));

            // The cut as a division against the cut as a multiply. The whole 128-bit product costs
            // four multiplies and the division it replaces is one call, so this is not obviously a
            // win by counting: it is a win or it is not, and the row says which.
            DBENCH_AB("g:cut_magic", iters, 8u, DBENCH_KEEP(g_g_mant / POW10_64[8]), DBENCH_KEEP(cut_magic(g_g_mant)));

            // The same call at four decimal exponents, which is what separates the pass's fixed
            // cost from what each power of five costs. apply_pow10 runs muto_mul_pow5 once per set
            // bit of the exponent, so 0, 1, 3 and 7 ask for none, one, two and three of them. The
            // slope across the four is one application; what is left at zero is seating the
            // mantissa, rounding it down to an integer, and the entry.
            // A zero mantissa answers at the top of the call, before it seats anything or rounds
            // anything, so this is the entry and nothing else. Against f:pow0, which does the same
            // entry and then seats and rounds without applying any power, the difference is what
            // seating and rounding cost.
            DBENCH_OP("f:pow_zero", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_zero, .e2 = -51, .ex = 6,
                                            .above = 0u)));

            DBENCH_OP("f:pow0", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 0,
                                            .above = 0u)));

            DBENCH_OP("f:pow1", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 1,
                                            .above = 0u)));

            DBENCH_OP("f:pow3", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 3,
                                            .above = 0u)));

            DBENCH_OP("f:pow7", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 7,
                                            .above = 0u)));

            // One pass on the negative side, which is the path a value below one takes. Against
            // f:scale, which is the same call at a small positive exponent and so goes through one
            // exact power of ten, this is the same work through the pow5 tables.
            DBENCH_OP("f:scale_neg", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = -4,
                                            .above = 0u)));

            // One pass at the exponent verba_g actually asks for on a value of about a ten
            // thousandth. Against g:small, which is the whole entry on that value, this says
            // whether the cost is the pass or the number of passes the correction loop runs.
            DBENCH_OP("f:scale20", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 20,
                                            .above = 0u)));

            DBENCH_OP("f:scale_neg13", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = -13,
                                            .above = 0u)));

            // The multiply that pass is built out of. Two applications of a power of five run it
            // four times each, so eight of these are what the row above mostly is. The halves are
            // masked out of a 64-bit value and kept there, so each partial product is written as a
            // 64 by 64 multiply; this asks whether narrowing them to the 32 bits they actually hold
            // buys anything, or whether the compiler had already worked that out from the mask.
            DBENCH_AB("f:mul", iters, 8u, (bench_mul_wide(g_mul_a, g_mul_b, &g_mul_out), DBENCH_KEEP(g_mul_out.hi)),
                      (bench_mul_narrow(g_mul_a, g_mul_b, &g_mul_out), DBENCH_KEEP(g_mul_out.hi)));

            // A positive exponent past what one exact power covers. Twenty is what verba_g asks for
            // on a value of about a ten thousandth, and it has two bits set, so the walk applies two
            // wide powers where two narrow ones would do.
            DBENCH_AB("f:walk20", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 20), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_exact(&g_wide_b, 20), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walk40", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 40), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_exact(&g_wide_b, 40), DBENCH_KEEP(g_wide_b.hi)));

            // The power walk at three exponents, testing every step against stopping when the bits
            // run out. A negative exponent is the case that still walks: the exact power of ten
            // path only covers small positive ones, so verba_g's values below one come through
            // here. Zero is the case where every iteration is dead.
            DBENCH_AB("f:walk0", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 0), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, 0), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walkneg1", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, -1), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, -1), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walkneg7", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, -7), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, -7), DBENCH_KEEP(g_wide_b.hi)));

            // Six decimals against the two shapes that can deliver it. The A arm is what the pass
            // does now: six is two set bits, so two applications of a 128 by 128 power of five. The
            // B arm is one application of ten to the sixth held exactly in 64 bits, which is what
            // the fixed path can always use, since MMGR_FIXED_MAX_DECIMALS is eighteen and ten to
            // the eighteenth is still under a 64-bit value.
            //
            // Both arms start from the same significand each pass, so neither is handed a value the
            // other has already normalized.
            DBENCH_AB("f:pow_shape", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_pow_128(&g_wide_a, mmgr_pow5_up[1].hi, mmgr_pow5_up[1].lo, (int32_t)mmgr_pow5_up[1].e2),
                       bench_pow_128(&g_wide_a, mmgr_pow5_up[2].hi, mmgr_pow5_up[2].lo, (int32_t)mmgr_pow5_up[2].e2),
                       DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_pow_64(&g_wide_b, g_pow_ten), DBENCH_KEEP(g_wide_b.hi)));
        }

        // A whole record against the one snprintf call a caller writes instead. This is where the
        // per entry wins are supposed to compound: six fields, three of them values, and the libc
        // side pays one format parse for the lot while the mmgr side pays none at all.
        {
            const mmgr_fval fields[] = {MMGR_VSTR("id="),         MMGR_VU64(g_rec_u64), MMGR_VSTR(" x="),
                                        MMGR_VHEXW(g_rec_hex, 8), MMGR_VSTR(" f="),     MMGR_VFIXW(g_rec_real, 4)};

            DBENCH_AB("s:record", 2000u, 34u,
                      DBENCH_KEEP(MMGR_CALL(numer.emit, NumerosCfg, .out = g_wide, .cap = sizeof g_wide, .vals = fields,
                                            .nvals = sizeof fields / sizeof fields[0])),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "id=%llu x=%08lx f=%.4f",
                                           (unsigned long long)g_rec_u64, (unsigned long)g_rec_hex, g_rec_real)));
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

            // The digit placement, tested once against tested once a character. Seventeen digits
            // with a point after one is what verba_g renders in exponential form, and six with no
            // point is the shorter case.
            {
                static const char laid[24] = "31415926535897932384";

                DBENCH_AB("s:d_point", iters, 17u,
                          DBENCH_KEEP(digits_per_char(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)),
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)));

                DBENCH_AB("s:d_plain", iters, 6u, DBENCH_KEEP(digits_per_char(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)),
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)));

                // The shape now in the library against the same run split at the point, so the
                // question of where the point goes is asked once rather than once a character.
                DBENCH_AB("s:d_runs", iters, 17u,
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)),
                          DBENCH_KEEP(digits_two_runs(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)));

                DBENCH_AB("s:d_runs6", iters, 6u, DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)),
                          DBENCH_KEEP(digits_two_runs(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)));

                // The whole placement including the emit, scratch buffer against writing the digits
                // where they are going. Three shapes verba_g actually asks for: a point after one
                // digit, which is the exponential form; a point in the middle; and none at all.
                DBENCH_AB("s:d_ip_exp", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 1u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 1u)));

                DBENCH_AB("s:d_ip_mid", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 9u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 9u)));

                DBENCH_AB("s:d_ip_none", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 0u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 0u)));
            }

            DBENCH_AB("s:sign", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.sign, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(signbit(g_rec_real) ? 1 : 0));

            DBENCH_AB("s:exp", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.exp, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(frexp_exponent(g_rec_real)));

            DBENCH_AB("s:to_bits", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.to_bits, FractioCfg, .val = g_rec_real)),
                      DBENCH_KEEP(bits_via_memcpy(g_rec_real)));
        }

        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_out));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("verba")
