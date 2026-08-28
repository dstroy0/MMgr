/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Text and number formatting into a caller'scale buffer, one field at a time.
 *
 * @note Every call takes the offset to write at and returns the offset past what it wrote, so calls chain.
 * @note A call that will not fit returns args->cap, which every later call then sees as no room left.
 * @note verba_finish stores the terminator and reports the length; nothing before it terminates the buffer.
 */
#include "verba_scribo/verba_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"
#include "fractio/fractio.h"
#include "proximus_operor/proximus_operor.h"
#include "transformo/transformo.h"

/**
 * @brief Digit characters for bases up to sixteen, indexed by the digit value.
 *
 * @note Lower case, so verba_hex and the \\u escapes in verba_json both write lower case letters.
 */
static const char mmgr_hex_lower[] = "0123456789abcdef";

/**
 * @brief The letter that follows a backslash for each control byte JSON gives a short escape.
 *
 * @note Only 8, 9, 10, 12 and 13 have one, giving \\byte, \\t, \\mantissa, \\f and \\r.
 * @note A zero entry sends that byte down verba_json'scale \\u00 path instead, so every control byte is covered.
 */
static const char JSON_CTRL_ESC[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0};

/**
 * @brief Expands to 19u, the largest power of ten that fits in a uint64_t.
 *
 * @note Bounds the digit-counting loops, which stop once the index passes it.
 */
#define MMGR_VERBA_POW10_MAX 19u

/**
 * @brief Ten raised to 0 through 19, indexed by the exponent itself.
 *
 * @note The digit counters compare against these to find how many decimal digits a value needs.
 * @note verba_g and verba_fixed also use them as the scale for a given number of significant digits or decimals.
 */
static const uint64_t mmgr_verba_pow10[MMGR_VERBA_POW10_MAX + 1u] = {1ull,
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
 * @brief Every two digit combination, so a pair costs one table read rather than two divides.
 *
 * @note 200 characters and a terminator. Indexed by twice the value, which is why the pairs are
 *       written out rather than computed.
 */
static const char mmgr_verba_pairs[201] = "00010203040506070809"
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
 * @brief Returns how many decimal digits value needs.
 *
 * @param[in] value Value to measure.
 * @return      1 through 20.
 * @note One leading zero count and one table compare, so the cost does not rise with the digit count.
 *       Walking mmgr_verba_pow10 instead cost 11 cycles a digit on an ESP32-S3 and 6 on an ESP32-C6,
 *       paid before a single digit was written; this measured at the bench'scale own floor on both.
 * @note log10 comes off log2 by multiplying by 1233 and shifting twelve, which is 0.301025 against
 *       log10(2) = 0.30103. The estimate is exact or one low, and the compare corrects it.
 * @note clz.lead counts a 64-bit value whatever the machine word is, so the bit index comes off 64.
 * @note The compare reads the forced-odd value, so zero counts as one digit. No other input moves:
 *       every power of ten above one is even, so setting the low bit cannot cross a threshold.
 */
MMGR_INLINE size_t verba_digits10(uint64_t value)
{
    // Explicit cast takes the iword the clz entry returns into a 32-bit unsigned; the value is forced
    // non-zero so the count is defined for every input. The width is fixed rather than mmgr_word
    // because the multiply below reaches 78912, which does not fit a 16-bit word
    const uint32_t lead = (uint32_t)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)(value | 1u));
    const uint32_t bits = 64u - lead;
    const uint32_t est = (bits * 1233u) >> 12u;

    return (size_t)est + (((value | 1u) >= mmgr_verba_pow10[est]) ? 1u : 0u);
}

MMGR_STATIC_ASSERT(((64u * 1233u) >> 12u) <= MMGR_VERBA_POW10_MAX,
                   "verba_digits10 indexes mmgr_verba_pow10 with its estimate, which must stay inside it");

/**
 * @brief Expands to the multiplicative inverse of five in 64 bits, so five times this is one.
 *
 * @note Divides a value already known to be a multiple of five, exactly and with no division: the
 *       product is the quotient outright.
 */
#define MMGR_VERBA_INV5 0xCCCCCCCCCCCCCCCDull

/**
 * @brief Expands to the largest 64-bit value divisible by five, over five.
 *
 * @note A value times MMGR_VERBA_INV5 lands at or below this exactly when five divides it, which is
 *       what makes the one product a divisibility test as well as a divider.
 */
#define MMGR_VERBA_FIFTH_MAX 0x3333333333333333ull

/**
 * @brief Divides value by a hundred without a divide instruction.
 *
 * @param[in] value Value to divide.
 * @return      value / 100.
 * @note 0x51EB851F over 2^37 is 1/100 to more precision than a 32-bit input can expose.
 * @note Written as a multiply rather than left as `/ 100u` because the two targets disagree: the
 *       ESP32-S3'scale compiler picks its hardware divider, which is the slower of the two, while the
 *       ESP32-C6 has no divider and picks the multiply anyway. Forcing it costs the C6 nothing and
 *       measured 1.27x on the S3 at ten digits.
 */
MMGR_INLINE uint32_t verba_div100(uint32_t value)
{
    // Explicit widening keeps the whole product, and the narrowing cast takes the quotient back once
    // the shift has discarded the fractional half
    return (uint32_t)(((uint64_t)value * 0x51EB851FULL) >> 37u);
}

/**
 * @brief Writes digits characters of value at out, most significant first.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  value      Value to write, which must need no more than digits characters.
 * @param[in]  digits How many to write; a value shorter than this is padded with leading zeros.
 * @note Two digits an iteration off mmgr_verba_pairs, so a pair costs one divide rather than two.
 * @warning out must be writable for digits bytes.
 */
MMGR_INLINE void verba_emit10(char *out, uint32_t value, size_t digits)
{
    size_t index = digits;

    while (index >= 2u)
    {
        const uint32_t q = verba_div100(value);
        // The remainder is taken from the quotient rather than asked for separately, so the compiler
        // answers one division here rather than two
        const uint32_t r = value - (q * 100u);

        index -= 2u;
        out[index] = mmgr_verba_pairs[r * 2u];
        out[index + 1u] = mmgr_verba_pairs[(r * 2u) + 1u];
        value = q;
    }
    if (index != 0u)
    {
        // Explicit cast narrows the sum into the char the buffer holds; value is a single digit here
        out[0] = (char)('0' + (uint32_t)value);
    }
}

/**
 * @brief Expands to 8u, the power of ten a 64-bit value is cut at to reach verba_emit10.
 *
 * @note Eight rather than nine, so that a value of MMGR_G_MAX_SIG digits still leaves at most nine
 *       above the cut, which is the widest that fits the uint32_t verba_emit10 takes.
 */
#define MMGR_VERBA_CUT 8u

/**
 * @brief Writes digits characters of a 64-bit value at out, most significant first.
 *
 * @param[out] out    Where the digits go [BORROWS].
 * @param[in]  value  Value to write, which must need no more than digits characters.
 * @param[in]  digits How many to write, 1 through 20; a shorter value is padded with leading zeros.
 * @note Cuts the value into pieces that each fit a uint32_t and hands every piece to verba_emit10,
 *       rather than walking a descending 64-bit divisor. Neither target has a 64-bit divider, so a
 *       divisor walk is two libgcc calls for every digit written; this is one for the whole run, or
 *       two past eighteen digits, and none at all for a value that already fits 32 bits.
 * @note Both tests are on what the value holds, not on how many digits were asked for. A value of
 *       ten digits still inside 32 bits takes no cut at all, and a digit count raised by args->min
 *       only pads: verba_emit10 writes leading zeros once its value reaches zero. Branching on the
 *       digit count instead sends a ten digit uint32_t down the cut and cost verba_uint half again
 *       as much, 220 cycles to 332, which is the whole reason the test is written this way.
 * @note Measured on an ESP32-S3 at 240 MHz against the descending divisor this replaced: 1189 to 94
 *       at six digits, 3746 to 275 at seventeen. It took verba_g from 4551 cycles to 1647 and the
 *       same call on an ESP32-C6 from 5258 to 1580.
 * @warning out must be writable for digits bytes, and digits must be enough to hold value.
 */
MMGR_INLINE void verba_emit20(char *out, uint64_t value, size_t digits)
{
    if (value <= 0xFFFFFFFFU)
    {
        // Explicit cast narrows to the width verba_emit10 takes; the test above established it fits
        verba_emit10(out, (uint32_t)value, digits);
        return;
    }

    const uint64_t rest = value / mmgr_verba_pow10[MMGR_VERBA_CUT];
    // The remainder is taken from the quotient rather than asked for separately, so the compiler
    // answers one division here rather than a division and a modulo
    const uint32_t low = (uint32_t)(value - (rest * mmgr_verba_pow10[MMGR_VERBA_CUT]));

    if (rest <= 0xFFFFFFFFU)
    {
        verba_emit10(out, (uint32_t)rest, digits - MMGR_VERBA_CUT);
    }
    else
    {
        // A value this large leaves more than 32 bits above the cut, so it is cut a second time
        const uint64_t top = rest / mmgr_verba_pow10[MMGR_VERBA_CUT];
        const uint32_t mid = (uint32_t)(rest - (top * mmgr_verba_pow10[MMGR_VERBA_CUT]));

        verba_emit10(out, (uint32_t)top, digits - (2u * MMGR_VERBA_CUT));
        verba_emit10(out + (digits - (2u * MMGR_VERBA_CUT)), mid, MMGR_VERBA_CUT);
    }
    verba_emit10(out + (digits - MMGR_VERBA_CUT), low, MMGR_VERBA_CUT);
}

/**
 * @brief Arguments for the verba backends.
 *
 * @note Mirrors VerbaCfg without its const qualifiers, then adds four members the float path passes between calls.
 * @note Each backend reads only the members its own documentation names; the rest stay zero in the literal.
 */
typedef struct
{
    char *out;           /**< Destination buffer [BORROWS]. */
    size_t cap;          /**< Bytes available in out. */
    size_t at;           /**< Offset to write at. */
    const char *text;    /**< Text to write [BORROWS]. */
    size_t text_len;     /**< Bytes of text, or the count of zeros verba_zeros writes. */
    char ch;             /**< Single character to write. */
    uint64_t val;        /**< Unsigned value to write. */
    int64_t sval;        /**< Signed value to write. */
    double real;         /**< Floating point value to write. */
    uint8_t base;        /**< Numeric base, 8, 10 or 16. */
    uint8_t min;         /**< Fewest digits to write, padding with leading zeros. */
    uint8_t columns;     /**< Fewest columns to fill, padding with leading spaces. */
    uint8_t sig;         /**< Significant digits verba_g keeps. */
    uint8_t decimals;    /**< Digits after the point verba_fixed writes. */
    uint64_t mant;       /**< Digits verba_digits writes, as one integer. */
    mmgr_u64 bits;       /**< The double'scale bit pattern, passed between the float calls. */
    uint8_t digits;      /**< How many digits verba_digits takes from mant. */
    uint8_t point_after; /**< Digits verba_digits writes before the point; 0 writes no point. */
} VerbaCtx;

/**
 * @brief Returns whether want bytes fit at args->at with a byte still to spare.
 *
 * @param[in] args    Buffer, capacity and the offset to write at [BORROWS].
 * @param[in] want Bytes the caller means to write.
 * @return         MMGR_TRUE when they fit and one byte is left over.
 * @note The spare byte is the terminator verba_finish stores, so it is held back on every test.
 * @note The args->at below args->cap test comes first, so the subtraction that follows never wraps.
 * @note This is the only backend that takes a second argument rather than reading everything from the struct.
 */
MMGR_INLINE mmgr_bool verba_room(const VerbaCtx *args, size_t want)
{
    return (mmgr_bool)((args->at < args->cap) && (want <= ((args->cap - args->at) - 1u)));
}

/**
 * @brief Writes args->text_len bytes of args->text at args->at.
 *
 * @param[in] args Buffer, capacity, offset, text and its length [BORROWS].
 * @return      The offset past the text, or args->cap when it does not fit.
 * @note Writes nothing at all when it does not fit, rather than writing what it can.
 * @note Copies through proxim.read, so args->text needs no particular alignment.
 * @warning args->text must be readable for args->text_len bytes.
 */
MMGR_INLINE size_t verba_put_n(const VerbaCtx *args)
{
    if (!verba_room(args, args->text_len))
    {
        return args->cap;
    }

    MMGR_CALL(proxim.read, ProximusCfg, .dst = args->out + args->at, .at = args->text, .size = args->text_len);
    return args->at + args->text_len;
}

/**
 * @brief Writes the whole of args->text at args->at, measuring it first.
 *
 * @param[in] args Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past the text, or args->cap when it does not fit.
 * @note args->text_len when the caller knows it, and only otherwise a measure. A length settled before
 *       the build is one this has no reason to derive again, and most text handed here is a literal.
 * @note A text_len of 0 measures, which is the same answer an empty string gives either way.
 * @note Writes nothing when the text does not fit, since verba_put_n is all or nothing.
 * @warning args->text must not be NULL here, unlike in verba_put_clip, verba_xml and verba_json.
 */
MMGR_INLINE size_t verba_put(const VerbaCtx *args)
{
    const size_t sl = (args->text_len != 0u)
                          ? args->text_len
                          : MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = args->text, .cap = args->cap);

    return MMGR_CALL(verba_put_n, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text,
                     .text_len = sl);
}

/**
 * @brief Writes as much of args->text as fits at args->at, cutting it short rather than refusing.
 *
 * @param[in] args Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is args->at when nothing was.
 * @note Bounds cellul.len by the room left, so the length measured is already the length that fits.
 * @note Returns args->at rather than args->cap when it writes nothing, so a later call can still write.
 * @note A NULL args->text writes nothing, where verba_put would pass the NULL on to cellul.len.
 */
MMGR_INLINE size_t verba_put_clip(const VerbaCtx *args)
{
    if ((args->text == NULL) || (args->at >= args->cap))
    {
        return args->at;
    }

    const size_t room = (args->cap - args->at) - 1u;
    const size_t sl = MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = args->text, .cap = room);

    MMGR_CALL(proxim.read, ProximusCfg, .dst = args->out + args->at, .at = args->text, .size = sl);
    return args->at + sl;
}

/**
 * @brief Writes args->ch at args->at.
 *
 * @param[in] args Buffer, capacity, offset and the character [BORROWS].
 * @return      args->at plus one, or args->cap when there is no room.
 * @note The building block the escape and digit calls write through, one character at a time.
 */
MMGR_INLINE size_t verba_ch(const VerbaCtx *args)
{
    if (!verba_room(args, 1u))
    {
        return args->cap;
    }

    args->out[args->at] = args->ch;
    return args->at + 1u;
}

/**
 * @brief Writes args->val in base ten at args->at, right aligned in at least args->columns columns.
 *
 * @param[in] args Buffer, capacity, offset, the value and the column count [BORROWS].
 * @return      The offset past what was written, which is args->at when there was no room.
 * @note Pads on the left with spaces, where verba_uint pads with leading zeros.
 * @note Takes args->columns as a floor, so a value needing more digits than that widens the field.
 * @note Writes the digits from the last one back, taking args->val down by a factor of ten each time.
 * @note The digit count comes off verba_digits10, so it costs the same whatever the value is.
 */
MMGR_INLINE size_t verba_u64_clip(const VerbaCtx *args)
{
    uint64_t value = args->val;
    const size_t digits = verba_digits10(value);
    const size_t width = (digits < args->columns) ? args->columns : digits;

    if (!verba_room(args, width))
    {
        return args->at;
    }

    const size_t pad = width - digits;

    for (size_t index = pad; index-- > 0;)
    {
        args->out[args->at + index] = ' ';
    }

    // The digits through the same cut every other entry here uses. This one was left walking a
    // 64-bit divide and a modulo per digit, which is two libgcc calls a digit on both parts, when
    // verba_emit20 was already in the file. Measured on an ESP32-S3 at twenty digits in a column of
    // twenty four: 708 cycles to 517
    verba_emit20(args->out + args->at + pad, value, digits);
    return args->at + width;
}

/**
 * @brief Writes args->val in base args->base at args->at, in at least args->min digits.
 *
 * @param[in] args Buffer, capacity, offset, the value, the base and the least digit count [BORROWS].
 * @return      The offset past the digits, or args->cap when they do not fit.
 * @note Base 16 and base 8 count by shifting; every other base counts off its leading zeros.
 * @note Every base ten value goes to verba_emit20, whatever its width: it cuts the value into pieces
 *       that fit a uint32_t and writes each two digits an iteration. A value inside 32 bits reaches
 *       the same pair walk it always did, without a width test here to send it there.
 * @note Raising the count to args->min before the room test is what pads the result with leading zeros.
 * @warning Any args->base other than 8 or 16 is written in base ten, whatever value it holds.
 */
MMGR_INLINE size_t verba_uint(const VerbaCtx *args)
{
    uint64_t value = args->val;
    const mmgr_word bits_per_digit = (args->base == 16) ? 4U : ((args->base == 8) ? 3U : 0U);
    const mmgr_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ULL << bits_per_digit) - 1U) : 0U;

    mmgr_word digits = 1;
    if (power_of_two)
    {
        uint64_t probe = value;

        while ((probe >>= bits_per_digit) != 0)
        {
            digits++;
        }
    }
    else
    {
        // Explicit cast narrows the count into the word the walks below index with; it is at most 20
        digits = (mmgr_word)verba_digits10(value);
    }

    if (digits < args->min)
    {
        digits = args->min;
    }
    if (!verba_room(args, digits))
    {
        return args->cap;
    }

    if (power_of_two)
    {
        for (mmgr_word index = digits; index-- > 0;)
        {
            args->out[args->at + index] = mmgr_hex_lower[value & digit_mask];
            value >>= bits_per_digit;
        }
    }
    else
    {
        verba_emit20(args->out + args->at, value, digits);
    }
    return args->at + digits;
}

/**
 * @brief Writes args->sval in base ten at args->at, with a leading minus when it is negative.
 *
 * @param[in] args Buffer, capacity, offset and the signed value [BORROWS].
 * @return      The offset past the digits, or args->cap when they do not fit.
 * @note Writes the sign through verba_ch, then hands the magnitude to verba_uint at base ten.
 * @note The magnitude is taken as -(sv + 1) plus one, which stays in range for the most negative value.
 */
MMGR_INLINE size_t verba_i64(const VerbaCtx *args)
{
    const int64_t sv = args->sval;
    size_t at = args->at;

    if (sv < 0)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                     .val = (sv < 0) ? ((uint64_t)(-(sv + 1)) + 1U) : (uint64_t)sv, .base = 10u, .min = 1u);
}

/**
 * @brief Writes args->text_len zero characters at args->at.
 *
 * @param[in] args Buffer, capacity, offset and the count in text_len [BORROWS].
 * @return      The offset past the zeros, or args->cap once one of them did not fit.
 * @note Takes the count from text_len rather than a count member of its own.
 * @note verba_g uses this for the run of zeros on either side of a point.
 */
MMGR_INLINE size_t verba_zeros(const VerbaCtx *args)
{
    const size_t want = args->text_len;

    // One room test for the whole run rather than one a zero. verba_ch tests before every store and
    // the count is settled before the first, so the walk this replaces asked the same question as
    // many times as there were zeros to write
    if (!verba_room(args, want))
    {
        return args->cap;
    }

    char *const to = args->out + args->at;

    // A plain loop, and this module is compiled with -fno-tree-loop-distribute-patterns so that it
    // stays one. That pass would rewrite this as a memset, which costs about sixty cycles before it
    // writes a byte; the runs verba_g asks for are three at the leading end and about seventeen at
    // the trailing one, so none of them is long enough to earn the call back. With the pass off the
    // compiler still merges and unrolls the stores, which is why this beats laying the bytes down by
    // hand as well. Measured on an ESP32-S3 against the per zero walk: 32 cycles to 26 at two zeros,
    // 60 to 34 at six, and 144 to 58 at eighteen
    for (size_t k = 0; k < want; k++)
    {
        to[k] = '0';
    }
    return args->at + want;
}

/**
 * @brief Writes args->text at args->at, replacing the four characters XML gives entities.
 *
 * @param[in] args Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is args->at for a NULL args->text.
 * @note Replaces &amp; with &amp;amp;, &lt; with &amp;lt;, &gt; with &amp;gt; and the double quote with &amp;quot;.
 * @note The apostrophe is written as it stands, so this suits element text and double quoted attributes.
 * @note Walks to the terminator, so args->text is bounded by its own terminator rather than by args->cap.
 */
MMGR_INLINE size_t verba_xml(const VerbaCtx *args)
{
    size_t at = args->at;

    if (args->text == NULL)
    {
        return at;
    }

    for (const char *cursor = args->text; *cursor; cursor++)
    {
        const char *rep = NULL;

        switch (*cursor)
        {
        case '&':
            rep = "&amp;";
            break;
        case '<':
            rep = "&lt;";
            break;
        case '>':
            rep = "&gt;";
            break;
        case '"':
            rep = "&quot;";
            break;
        default:
            break;
        }

        if (rep != NULL)
        {
            at = MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .text = rep);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = *cursor);
        }
    }
    return at;
}

/**
 * @brief Writes args->text at args->at as a quoted JSON string, escaping what JSON requires.
 *
 * @param[in] args Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past the closing quote, or args->cap once something did not fit.
 * @note Writes the opening and closing quotes itself, so the result is a complete JSON string.
 * @note The double quote and the backslash escape as themselves; the five control bytes in JSON_CTRL_ESC take a letter.
 * @note Every other byte below 0x20 is written as \\u00 followed by two lower case hexadecimal digits.
 * @note A NULL args->text writes an empty pair of quotes, where verba_xml writes nothing at all.
 * @note Walks to the terminator, so args->text is bounded by its own terminator rather than by args->cap.
 */
MMGR_INLINE size_t verba_json(const VerbaCtx *args)
{
    const char *const src = (args->text != NULL) ? args->text : "";
    size_t at = MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .text = "\"");

    for (const char *cursor = src; *cursor; cursor++)
    {
        const uint8_t ch = (uint8_t)*cursor;
        const char two = ((ch == '"') || (ch == '\\')) ? (char)ch : ((ch < 0x20U) ? JSON_CTRL_ESC[ch] : 0);

        if (two != 0)
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '\\');
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = two);
        }
        else if (ch < 0x20U)
        {
            at = MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .text = "\\u00");
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                           .ch = mmgr_hex_lower[(ch >> 4) & 0xFU]);
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                           .ch = mmgr_hex_lower[ch & 0xFU]);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = (char)ch);
        }
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .text = "\"");
}

/**
 * @brief Writes args->mant as args->digits characters, with a point after args->point_after of them.
 *
 * @param[in] args Buffer, capacity, offset, the digits and where the point goes [BORROWS].
 * @return      The offset past the last digit, or args->cap once one of them did not fit.
 * @note The digits are laid down first by verba_emit20 and then placed one at a time, so the point
 *       goes in at the same index it always did and a destination too small still fills what it can.
 * @note A args->point_after of 0 writes no point, since the point is only inserted at a non-zero index.
 * @warning args->digits must be 1 through 20, which is what the scratch buffer holds.
 */
MMGR_INLINE size_t verba_digits(const VerbaCtx *args)
{
    char scratch[MMGR_VERBA_POW10_MAX + 1u];
    size_t at = args->at;

    verba_emit20(scratch, args->mant, args->digits);

    for (uint8_t index = 0; index < args->digits; index++)
    {
        if ((index == args->point_after) && (index != 0))
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '.');
        }

        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = scratch[index]);
    }
    return at;
}

/**
 * @brief Returns the bit pattern of args->real.
 *
 * @param[in] args The double to take apart [BORROWS].
 * @return      What fract.to_bits reported for args->real.
 * @note The only one of the four float helpers that reads args->real; the other three read args->bits.
 */
MMGR_INLINE mmgr_u64 verba_bits(const VerbaCtx *args)
{
    return MMGR_CALL(fract.to_bits, FractioCfg, .val = args->real);
}

/**
 * @brief Returns the biased exponent field of args->bits.
 *
 * @param[in] args The bit pattern to read [BORROWS].
 * @return      What fract.exp reported for args->bits.
 * @note A value of MMGR_DBL_EXP_ALL marks an infinity or a NaN, which is how the float calls test for one.
 */
MMGR_INLINE mmgr_u64 verba_exp(const VerbaCtx *args)
{
    return MMGR_CALL(fract.exp, FractioCfg, .bits = args->bits);
}

/**
 * @brief Returns the stored mantissa field of args->bits.
 *
 * @param[in] args The bit pattern to read [BORROWS].
 * @return      What fract.mant reported for args->bits.
 * @note The implied leading bit is not included, so the float calls put it back when the exponent is non-zero.
 */
MMGR_INLINE mmgr_u64 verba_mant(const VerbaCtx *args)
{
    return MMGR_CALL(fract.mant, FractioCfg, .bits = args->bits);
}

/**
 * @brief Returns the sign field of args->bits.
 *
 * @param[in] args The bit pattern to read [BORROWS].
 * @return      What fract.sign reported for args->bits, non-zero when the value is negative.
 * @note Reads the bit rather than comparing against zero, so a negative zero reports as negative.
 */
MMGR_INLINE mmgr_u64 verba_sign(const VerbaCtx *args)
{
    return MMGR_CALL(fract.sign, FractioCfg, .bits = args->bits);
}

/**
 * @brief Writes nan or inf at args->at, for a value whose exponent field is all ones.
 *
 * @param[in] args Buffer, capacity, offset and the bit pattern [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 * @note A non-zero mantissa gives nan, written without a sign whatever the sign bit holds.
 * @note A zero mantissa gives inf, with a leading minus when the sign bit is set.
 * @note Both are lower case and unquoted, so neither is valid JSON on its own.
 */
MMGR_INLINE size_t verba_non_finite(const VerbaCtx *args)
{
    if (MMGR_CALL(verba_mant, VerbaCtx, .bits = args->bits) != 0U)
    {
        return MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .text = "nan");
    }

    size_t at = args->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = args->bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .text = "inf");
}

/**
 * @brief Writes args->real at args->at in the shorter of a plain or an exponential form, to args->sig digits.
 *
 * @param[in] args Buffer, capacity, offset, the value and the significant digit count [BORROWS].
 * @return      The offset past what was written, or args->cap once something did not fit.
 * @note A args->sig of 0 is taken as 1, and anything above MMGR_G_MAX_SIG is held there.
 * @note An exponent field of all ones goes to verba_non_finite, and a zero value writes a single 0.
 * @note The decimal exponent is first estimated by multiplying the binary one by 78913 and shifting right 18,
 *       which is 0.30103 to five places, then corrected by muto.scale_to_u64 in at most four passes.
 * @note Trailing zeros are dropped from the digits before any of the four forms is chosen.
 * @note The four forms are exponential, digits with trailing zeros, digits with the point inside, and 0. with
 *       leading zeros, picked on where the decimal exponent falls against the digit count.
 */
MMGR_INLINE size_t verba_g(const VerbaCtx *args)
{
    const mmgr_u64 bits = verba_bits(args);

    if (MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .bits = bits);
    }

    uint8_t sig = (args->sig == 0) ? 1u : args->sig;
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }

    size_t at = args->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '-');
    }

    const mmgr_u64 be = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);
    mmgr_u64 mantissa = MMGR_CALL(verba_mant, VerbaCtx, .bits = bits);

    if ((be == 0U) && (mantissa == 0U))
    {
        return MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '0');
    }

    mmgr_iword scale = 1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS;
    if (be != 0U)
    {
        mantissa |= 1ULL << MMGR_DBL_MANT_BITS;
        scale = (mmgr_iword)(be - MMGR_DBL_BIAS - MMGR_DBL_MANT_BITS);
    }

    const mmgr_u64 limit = mmgr_verba_pow10[sig];

    mmgr_iword exponent =
        (mmgr_iword)(((mmgr_i64)(63 - MMGR_CALL(clz.lead, ClzCfg, .val = mantissa) + scale) * 78913) >> 18);
    mmgr_iword cursor = (mmgr_iword)((mmgr_iword)sig - 1 - exponent);
    mmgr_u64 mant = 0U;

    for (uint8_t guard = 0; guard < 4U; guard++)
    {
        mant = MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &mantissa, .e2 = scale, .ex = cursor, .above = 0U);
        if (mant >= limit)
        {
            exponent++;
            cursor--;
        }
        else if ((sig > 1U) && (mant < (limit / 10U)))
        {
            exponent--;
            cursor++;
        }
        else
        {
            break;
        }
    }

    uint8_t digits = sig;

    while (digits > 1u)
    {
        // Ten is two times five, and the low bit answers the two before anything is multiplied
        if ((mant & 1u) != 0u)
        {
            break;
        }

        const mmgr_u64 fifth = (mant >> 1) * MMGR_VERBA_INV5;

        // The product is the divisibility test and the quotient at once: at or below the bound five
        // divided the value, and the product is what dividing by ten would have given
        if (fifth > MMGR_VERBA_FIFTH_MAX)
        {
            break;
        }
        mant = fifth;
        digits--;
    }

    if ((exponent < -4) || (exponent >= (mmgr_i32)sig))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .mant = mant,
                       .digits = digits, .point_after = 1u);
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = 'e');
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                       .ch = (exponent < 0) ? '-' : '+');
        return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                         .val = (uint64_t)((exponent < 0) ? -exponent : exponent), .base = 10u, .min = 2u);
    }
    if (exponent >= ((mmgr_i32)digits - 1))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .mant = mant,
                       .digits = digits, .point_after = 0u);
        return MMGR_CALL(verba_zeros, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                         .text_len = (size_t)(exponent - (mmgr_i32)digits + 1));
    }
    if (exponent >= 0)
    {
        return MMGR_CALL(verba_digits, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .mant = mant,
                         .digits = digits, .point_after = (uint8_t)(exponent + 1));
    }

    at = MMGR_CALL(verba_put, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .text = "0.");
    at = MMGR_CALL(verba_zeros, VerbaCtx, .out = args->out, .cap = args->cap, .at = at,
                   .text_len = (size_t)(-exponent - 1));
    return MMGR_CALL(verba_digits, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .mant = mant,
                     .digits = digits, .point_after = 0u);
}

/**
 * @brief Writes args->real at args->at with exactly args->decimals digits after the point.
 *
 * @param[in] args Buffer, capacity, offset, the value and the decimal count [BORROWS].
 * @return      The offset past what was written, or args->cap once something did not fit.
 * @note An exponent field of all ones goes to verba_non_finite; the sign is written before anything else.
 * @note A magnitude too large for 64 bits of integer part falls back to verba_g at ten significant digits.
 * @note args->decimals is held at MMGR_FIXED_MAX_DECIMALS, and a value of 0 writes no point at all.
 * @note The fraction is rounded by muto.scale_to_u64, given the low bit of the integer part so a tie goes to even.
 * @note A fraction that rounds up to the whole scale carries into the integer part and is written as zeros.
 */
MMGR_INLINE size_t verba_fixed(const VerbaCtx *args)
{
    const mmgr_u64 bits = verba_bits(args);
    const mmgr_u64 klass = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);

    if (klass == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .bits = bits);
    }

    double value = args->real;
    size_t at = args->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '-');
        value = -value;
    }

    if (klass >= (MMGR_DBL_BIAS + 64))
    {
        return MMGR_CALL(verba_g, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .real = value, .sig = 10u);
    }

    uint8_t decimals = args->decimals;
    if (decimals > MMGR_FIXED_MAX_DECIMALS)
    {
        decimals = MMGR_FIXED_MAX_DECIMALS;
    }

    const mmgr_u64 scale = mmgr_verba_pow10[decimals];

    mmgr_u64 mant = MMGR_CALL(verba_mant, VerbaCtx, .bits = bits);
    mmgr_iword exp2 = 1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS;

    if (klass != 0U)
    {
        mant |= 1ULL << MMGR_DBL_MANT_BITS;
        exp2 = (mmgr_iword)(klass - MMGR_DBL_BIAS - MMGR_DBL_MANT_BITS);
    }

    mmgr_u64 ip = 0U;
    mmgr_u64 rem = 0U;

    if (exp2 >= 0)
    {
        ip = mant << (mmgr_word)exp2;
    }
    else
    {
        const mmgr_word shift = (mmgr_word)(-exp2);

        if (shift < 64U)
        {
            ip = mant >> shift;
            rem = mant - (ip << shift);
        }
        else
        {
            rem = mant;
        }
    }

    mmgr_u64 frac = MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &rem, .e2 = exp2, .ex = (mmgr_iword)decimals,
                              .above = (decimals == 0U) ? (mmgr_word)(ip & 1U) : 0U);

    if (frac >= scale)
    {
        ip++;
        frac = 0U;
    }

    at = MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .val = ip, .base = 10u,
                   .min = 1u);

    if (decimals != 0u)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .ch = '.');
        at = MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = at, .val = frac, .base = 10u,
                       .min = decimals);
    }
    return at;
}

/**
 * @brief Stores the terminator at args->at and reports the length.
 *
 * @param[in] args Buffer, capacity and the offset reached [BORROWS].
 * @return      args->at, or 0 when args->at already reached args->cap.
 * @note This is the only call that writes a terminator, so a buffer is not a string until it has run.
 * @note A return of 0 covers both an empty result and one that ran out of room; verba_ok tells them apart.
 */
MMGR_INLINE size_t verba_finish(const VerbaCtx *args)
{
    if (args->at >= args->cap)
    {
        return 0;
    }
    args->out[args->at] = '\0';
    return args->at;
}

/**
 * @brief Returns whether args->at is still below args->cap.
 *
 * @param[in] args Capacity and the offset reached [BORROWS].
 * @return      MMGR_TRUE while there is still room, MMGR_FALSE once a call returned args->cap.
 * @note Reads neither args->out nor any value member, so it touches no memory.
 */
MMGR_INLINE mmgr_bool verba_ok(const VerbaCtx *args)
{
    return (mmgr_bool)(args->at < args->cap);
}

/**
 * @brief Writes args->val in base ten, in at least args->min digits.
 *
 * @param[in,out] args Buffer, cursor, value and least digit count [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes the base at ten and forwards args->min, which is what separates it from verba_u32.
 */
MMGR_INLINE size_t verba_u32w(const VerbaCtx *args)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
                     .base = 10u, .min = args->min);
}

/**
 * @brief Writes args->val in base sixteen, in at least args->min digits.
 *
 * @param[in,out] args Buffer, cursor, value and least digit count [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes the base at sixteen, so the digits come out lower case.
 */
MMGR_INLINE size_t verba_hex(const VerbaCtx *args)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
                     .base = 16u, .min = args->min);
}

/**
 * @brief Writes args->val in base ten, with no padding.
 *
 * @param[in,out] args Buffer, cursor and value [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes both the base at ten and the least digit count at one, so args->min and args->base take no part.
 */
MMGR_INLINE size_t verba_u32(const VerbaCtx *args)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
                     .base = 10u, .min = 1u);
}

/**
 * @brief Writes args->val in base ten, with no padding.
 *
 * @param[in,out] args Buffer, cursor and value [BORROWS].
 * @return          The cursor past what was written.
 * @note The same walk as verba_u32, since args->val is 64 bits either way. Both names exist so a caller
 *       reads the width it means at the call.
 */
MMGR_INLINE size_t verba_u64(const VerbaCtx *args)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
                     .base = 10u, .min = 1u);
}

/**
 * @brief Returns whether args->real has its sign bit set.
 *
 * @param[in] args The value to test, as args->real [BORROWS].
 * @return      MMGR_TRUE when the sign bit is set.
 * @note Reads the bit rather than comparing against zero, so a negative zero returns MMGR_TRUE.
 */
MMGR_INLINE mmgr_bool verba_sign_bit(const VerbaCtx *args)
{
    // Explicit cast narrows the bit test into the mmgr_bool container
    return (mmgr_bool)(MMGR_CALL(verba_sign, VerbaCtx,
                                 .bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = args->real)) != 0U);
}

/**
 * @brief Returns whether args->real is an infinity.
 *
 * @param[in] args The value to test, as args->real [BORROWS].
 * @return      MMGR_TRUE for either infinity.
 * @note Wants the exponent field all ones and the mantissa zero, where verba_is_nan wants it non-zero.
 * @note Says nothing about the sign, so a negative infinity returns MMGR_TRUE too.
 */
MMGR_INLINE mmgr_bool verba_is_inf(const VerbaCtx *args)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = args->real);

    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) == 0U));
}

/**
 * @brief Returns whether args->real is a NaN.
 *
 * @param[in] args The value to test, as args->real [BORROWS].
 * @return      MMGR_TRUE for any NaN.
 * @note Wants the exponent field all ones and the mantissa non-zero, where verba_is_inf wants it zero.
 */
MMGR_INLINE mmgr_bool verba_is_nan(const VerbaCtx *args)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = args->real);

    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) != 0U));
}

/**
 * @brief Binds this module'scale four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] cfg  The argument pack this entry'scale table carries.
 * @param[in] name Name after the mmgr_verba_ and verba_ prefixes, which the two share.
 */
#define VERBA_ENTRY(ret, cfg, name, ...) GENERIC_ENTRY(mmgr_verba_, verba_, VerbaCtx, cfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in verba_scribo.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest. ok
 *       forwards cap and at alone, so it touches no memory, and the three classifiers forward real.
 * @note uint is the only entry that forwards args->base. u32w, hex, u32 and u64 each fix a base of their
 *       own in the backend above rather than at the call.
 */
VERBA_ENTRY(size_t, VerbaTextusCfg, put_n, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text,
            .text_len = args->text_len)
VERBA_ENTRY(size_t, VerbaTextusCfg, put, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text,
            .text_len = args->text_len)
VERBA_ENTRY(size_t, VerbaTextusCfg, put_clip, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text)
VERBA_ENTRY(size_t, VerbaTextusCfg, xml, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text)
VERBA_ENTRY(size_t, VerbaTextusCfg, json, .out = args->out, .cap = args->cap, .at = args->at, .text = args->text)
VERBA_ENTRY(size_t, VerbaLitteraCfg, ch, .out = args->out, .cap = args->cap, .at = args->at, .ch = args->ch)
VERBA_ENTRY(size_t, VerbaNumerusCfg, u64_clip, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
            .columns = args->columns)
VERBA_ENTRY(size_t, VerbaNumerusCfg, uint, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
            .base = args->base, .min = args->min)
VERBA_ENTRY(size_t, VerbaNumerusCfg, u32w, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
            .min = args->min)
VERBA_ENTRY(size_t, VerbaNumerusCfg, hex, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val,
            .min = args->min)
VERBA_ENTRY(size_t, VerbaNumerusCfg, u32, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val)
VERBA_ENTRY(size_t, VerbaNumerusCfg, u64, .out = args->out, .cap = args->cap, .at = args->at, .val = args->val)
VERBA_ENTRY(size_t, VerbaNumerusCfg, i64, .out = args->out, .cap = args->cap, .at = args->at, .sval = args->sval)
VERBA_ENTRY(size_t, VerbaFractioCfg, g, .out = args->out, .cap = args->cap, .at = args->at, .real = args->real,
            .sig = args->sig)
VERBA_ENTRY(size_t, VerbaFractioCfg, fixed, .out = args->out, .cap = args->cap, .at = args->at, .real = args->real,
            .decimals = args->decimals)
VERBA_ENTRY(size_t, VerbaFinisCfg, finish, .out = args->out, .cap = args->cap, .at = args->at)
VERBA_ENTRY(mmgr_bool, VerbaFinisCfg, ok, .cap = args->cap, .at = args->at)
VERBA_ENTRY(mmgr_bool, VerbaFractioCfg, sign_bit, .real = args->real)
VERBA_ENTRY(mmgr_bool, VerbaFractioCfg, is_inf, .real = args->real)
VERBA_ENTRY(mmgr_bool, VerbaFractioCfg, is_nan, .real = args->real)
