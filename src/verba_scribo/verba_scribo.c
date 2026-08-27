/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Text and number formatting into a caller's buffer, one field at a time.
 *
 * @note Every call takes the offset to write at and returns the offset past what it wrote, so calls chain.
 * @note A call that will not fit returns c->cap, which every later call then sees as no room left.
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
 * @note Only 8, 9, 10, 12 and 13 have one, giving \\b, \\t, \\n, \\f and \\r.
 * @note A zero entry sends that byte down verba_json's \\u00 path instead, so every control byte is covered.
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
static const uint64_t mmgr_verba_pow10[MMGR_VERBA_POW10_MAX + 1u] = {
    1ull,                 10ull,                 100ull,                 1000ull,
    10000ull,             100000ull,             1000000ull,             10000000ull,
    100000000ull,         1000000000ull,         10000000000ull,         100000000000ull,
    1000000000000ull,     10000000000000ull,     100000000000000ull,     1000000000000000ull,
    10000000000000000ull, 100000000000000000ull, 1000000000000000000ull, 10000000000000000000ull};

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
    mmgr_u64 bits;       /**< The double's bit pattern, passed between the float calls. */
    uint8_t digits;      /**< How many digits verba_digits takes from mant. */
    uint8_t point_after; /**< Digits verba_digits writes before the point; 0 writes no point. */
} VerbaCtx;

/**
 * @brief Returns whether want bytes fit at c->at with a byte still to spare.
 *
 * @param[in] c    Buffer, capacity and the offset to write at [BORROWS].
 * @param[in] want Bytes the caller means to write.
 * @return         MMGR_TRUE when they fit and one byte is left over.
 * @note The spare byte is the terminator verba_finish stores, so it is held back on every test.
 * @note The c->at below c->cap test comes first, so the subtraction that follows never wraps.
 * @note This is the only backend that takes a second argument rather than reading everything from the struct.
 */
MMGR_INLINE mmgr_bool verba_room(const VerbaCtx *c, size_t want)
{
    return (mmgr_bool)((c->at < c->cap) && (want <= ((c->cap - c->at) - 1u)));
}

/**
 * @brief Writes c->text_len bytes of c->text at c->at.
 *
 * @param[in] c Buffer, capacity, offset, text and its length [BORROWS].
 * @return      The offset past the text, or c->cap when it does not fit.
 * @note Writes nothing at all when it does not fit, rather than writing what it can.
 * @note Copies through proxim.read, so c->text needs no particular alignment.
 * @warning c->text must be readable for c->text_len bytes.
 */
MMGR_INLINE size_t verba_put_n(const VerbaCtx *c)
{
    if (!verba_room(c, c->text_len))
    {
        return c->cap;
    }

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->out + c->at, .at = c->text, .size = c->text_len);
    return c->at + c->text_len;
}

/**
 * @brief Writes the whole of c->text at c->at, measuring it first.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past the text, or c->cap when it does not fit.
 * @note c->text_len when the caller knows it, and only otherwise a measure. A length settled before
 *       the build is one this has no reason to derive again, and most text handed here is a literal.
 * @note A text_len of 0 measures, which is the same answer an empty string gives either way.
 * @note Writes nothing when the text does not fit, since verba_put_n is all or nothing.
 * @warning c->text must not be NULL here, unlike in verba_put_clip, verba_xml and verba_json.
 */
MMGR_INLINE size_t verba_put(const VerbaCtx *c)
{
    const size_t sl =
        (c->text_len != 0u) ? c->text_len : MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = c->text, .cap = c->cap);

    return MMGR_CALL(verba_put_n, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text, .text_len = sl);
}

/**
 * @brief Writes as much of c->text as fits at c->at, cutting it short rather than refusing.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is c->at when nothing was.
 * @note Bounds cellul.len by the room left, so the length measured is already the length that fits.
 * @note Returns c->at rather than c->cap when it writes nothing, so a later call can still write.
 * @note A NULL c->text writes nothing, where verba_put would pass the NULL on to cellul.len.
 */
MMGR_INLINE size_t verba_put_clip(const VerbaCtx *c)
{
    if ((c->text == NULL) || (c->at >= c->cap))
    {
        return c->at;
    }

    const size_t room = (c->cap - c->at) - 1u;
    const size_t sl = MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = c->text, .cap = room);

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->out + c->at, .at = c->text, .size = sl);
    return c->at + sl;
}

/**
 * @brief Writes c->ch at c->at.
 *
 * @param[in] c Buffer, capacity, offset and the character [BORROWS].
 * @return      c->at plus one, or c->cap when there is no room.
 * @note The building block the escape and digit calls write through, one character at a time.
 */
MMGR_INLINE size_t verba_ch(const VerbaCtx *c)
{
    if (!verba_room(c, 1u))
    {
        return c->cap;
    }

    c->out[c->at] = c->ch;
    return c->at + 1u;
}

/**
 * @brief Writes c->val in base ten at c->at, right aligned in at least c->columns columns.
 *
 * @param[in] c Buffer, capacity, offset, the value and the column count [BORROWS].
 * @return      The offset past what was written, which is c->at when there was no room.
 * @note Pads on the left with spaces, where verba_uint pads with leading zeros.
 * @note Takes c->columns as a floor, so a value needing more digits than that widens the field.
 * @note Writes the digits from the last one back, taking c->val down by a factor of ten each time.
 */
MMGR_INLINE size_t verba_u64_clip(const VerbaCtx *c)
{
    uint64_t v = c->val;
    size_t digits = 1;

    while ((digits <= MMGR_VERBA_POW10_MAX) && (v >= mmgr_verba_pow10[digits]))
    {
        digits++;
    }

    const size_t width = (digits < c->columns) ? c->columns : digits;

    if (!verba_room(c, width))
    {
        return c->at;
    }

    for (size_t i = width - digits; i-- > 0;)
    {
        c->out[c->at + i] = ' ';
    }
    for (size_t i = width; i-- > (width - digits);)
    {
        c->out[c->at + i] = (char)('0' + (mmgr_word)(v % 10));
        v /= 10;
    }
    return c->at + width;
}

/**
 * @brief Writes c->val in base c->base at c->at, in at least c->min digits.
 *
 * @param[in] c Buffer, capacity, offset, the value, the base and the least digit count [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Base 16 and base 8 count and write by shifting; every other base counts and divides by ten.
 * @note A value inside 32 bits takes a separate loop that divides at 32 bits, where the last loop uses 64.
 * @note Raising the count to c->min before the room test is what pads the result with leading zeros.
 * @warning Any c->base other than 8 or 16 is written in base ten, whatever value it holds.
 */
MMGR_INLINE size_t verba_uint(const VerbaCtx *c)
{
    uint64_t v = c->val;
    const mmgr_word bits_per_digit = (c->base == 16) ? 4U : ((c->base == 8) ? 3U : 0U);
    const mmgr_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ULL << bits_per_digit) - 1U) : 0U;
    const mmgr_bool narrow = !power_of_two && (v <= 0xFFFFFFFFU);

    mmgr_word digits = 1;
    if (power_of_two)
    {
        uint64_t probe = v;

        while ((probe >>= bits_per_digit) != 0)
        {
            digits++;
        }
    }
    else
    {
        while ((digits <= MMGR_VERBA_POW10_MAX) && (v >= mmgr_verba_pow10[digits]))
        {
            digits++;
        }
    }

    if (digits < c->min)
    {
        digits = c->min;
    }
    if (!verba_room(c, digits))
    {
        return c->cap;
    }

    if (power_of_two)
    {
        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = mmgr_hex_lower[v & digit_mask];
            v >>= bits_per_digit;
        }
    }
    else if (narrow)
    {
        uint32_t v32 = (uint32_t)v;

        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = (char)('0' + (mmgr_word)(v32 % 10U));
            v32 /= 10U;
        }
    }
    else
    {
        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = (char)('0' + (mmgr_word)(v % 10));
            v /= 10;
        }
    }
    return c->at + digits;
}

/**
 * @brief Writes c->sval in base ten at c->at, with a leading minus when it is negative.
 *
 * @param[in] c Buffer, capacity, offset and the signed value [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Writes the sign through verba_ch, then hands the magnitude to verba_uint at base ten.
 * @note The magnitude is taken as -(sv + 1) plus one, which stays in range for the most negative value.
 */
MMGR_INLINE size_t verba_i64(const VerbaCtx *c)
{
    const int64_t sv = c->sval;
    size_t at = c->at;

    if (sv < 0)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                     .val = (sv < 0) ? ((uint64_t)(-(sv + 1)) + 1U) : (uint64_t)sv, .base = 10u, .min = 1u);
}

/**
 * @brief Writes c->text_len zero characters at c->at.
 *
 * @param[in] c Buffer, capacity, offset and the count in text_len [BORROWS].
 * @return      The offset past the zeros, or c->cap once one of them did not fit.
 * @note Takes the count from text_len rather than a count member of its own.
 * @note verba_g uses this for the run of zeros on either side of a point.
 */
MMGR_INLINE size_t verba_zeros(const VerbaCtx *c)
{
    size_t n = c->text_len;
    size_t at = c->at;

    while (n-- != 0u)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '0');
    }
    return at;
}

/**
 * @brief Writes c->text at c->at, replacing the four characters XML gives entities.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is c->at for a NULL c->text.
 * @note Replaces &amp; with &amp;amp;, &lt; with &amp;lt;, &gt; with &amp;gt; and the double quote with &amp;quot;.
 * @note The apostrophe is written as it stands, so this suits element text and double quoted attributes.
 * @note Walks to the terminator, so c->text is bounded by its own terminator rather than by c->cap.
 */
MMGR_INLINE size_t verba_xml(const VerbaCtx *c)
{
    size_t at = c->at;

    if (c->text == NULL)
    {
        return at;
    }

    for (const char *p = c->text; *p; p++)
    {
        const char *rep = NULL;

        switch (*p)
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
            at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = rep);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = *p);
        }
    }
    return at;
}

/**
 * @brief Writes c->text at c->at as a quoted JSON string, escaping what JSON requires.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past the closing quote, or c->cap once something did not fit.
 * @note Writes the opening and closing quotes itself, so the result is a complete JSON string.
 * @note The double quote and the backslash escape as themselves; the five control bytes in JSON_CTRL_ESC take a letter.
 * @note Every other byte below 0x20 is written as \\u00 followed by two lower case hexadecimal digits.
 * @note A NULL c->text writes an empty pair of quotes, where verba_xml writes nothing at all.
 * @note Walks to the terminator, so c->text is bounded by its own terminator rather than by c->cap.
 */
MMGR_INLINE size_t verba_json(const VerbaCtx *c)
{
    const char *const src = (c->text != NULL) ? c->text : "";
    size_t at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = "\"");

    for (const char *p = src; *p; p++)
    {
        const uint8_t ch = (uint8_t)*p;
        const char two = ((ch == '"') || (ch == '\\')) ? (char)ch : ((ch < 0x20U) ? JSON_CTRL_ESC[ch] : 0);

        if (two != 0)
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '\\');
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = two);
        }
        else if (ch < 0x20U)
        {
            at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "\\u00");
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                           .ch = mmgr_hex_lower[(ch >> 4) & 0xFU]);
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = mmgr_hex_lower[ch & 0xFU]);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (char)ch);
        }
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "\"");
}

/**
 * @brief Writes c->mant as c->digits characters, with a point after c->point_after of them.
 *
 * @param[in] c Buffer, capacity, offset, the digits and where the point goes [BORROWS].
 * @return      The offset past the last digit, or c->cap once one of them did not fit.
 * @note Starts the divisor at ten raised to c->digits minus one, so digits come out most significant first.
 * @note A c->point_after of 0 writes no point, since the point is only inserted at a non-zero index.
 * @warning c->digits must be 1 through 20, since the divisor is taken from mmgr_verba_pow10.
 */
MMGR_INLINE size_t verba_digits(const VerbaCtx *c)
{
    uint64_t left = c->mant;
    uint64_t div = mmgr_verba_pow10[c->digits - 1u];
    size_t at = c->at;

    for (uint8_t i = 0; i < c->digits; i++)
    {
        if ((i == c->point_after) && (i != 0))
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '.');
        }

        const uint64_t d = left / div;

        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (char)('0' + (mmgr_word)d));
        left -= d * div;
        div /= 10u;
    }
    return at;
}

/**
 * @brief Returns the bit pattern of c->real.
 *
 * @param[in] c The double to take apart [BORROWS].
 * @return      What fract.to_bits reported for c->real.
 * @note The only one of the four float helpers that reads c->real; the other three read c->bits.
 */
MMGR_INLINE mmgr_u64 verba_bits(const VerbaCtx *c)
{
    return MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);
}

/**
 * @brief Returns the biased exponent field of c->bits.
 *
 * @param[in] c The bit pattern to read [BORROWS].
 * @return      What fract.exp reported for c->bits.
 * @note A value of MMGR_DBL_EXP_ALL marks an infinity or a NaN, which is how the float calls test for one.
 */
MMGR_INLINE mmgr_u64 verba_exp(const VerbaCtx *c)
{
    return MMGR_CALL(fract.exp, FractioCfg, .bits = c->bits);
}

/**
 * @brief Returns the stored mantissa field of c->bits.
 *
 * @param[in] c The bit pattern to read [BORROWS].
 * @return      What fract.mant reported for c->bits.
 * @note The implied leading bit is not included, so the float calls put it back when the exponent is non-zero.
 */
MMGR_INLINE mmgr_u64 verba_mant(const VerbaCtx *c)
{
    return MMGR_CALL(fract.mant, FractioCfg, .bits = c->bits);
}

/**
 * @brief Returns the sign field of c->bits.
 *
 * @param[in] c The bit pattern to read [BORROWS].
 * @return      What fract.sign reported for c->bits, non-zero when the value is negative.
 * @note Reads the bit rather than comparing against zero, so a negative zero reports as negative.
 */
MMGR_INLINE mmgr_u64 verba_sign(const VerbaCtx *c)
{
    return MMGR_CALL(fract.sign, FractioCfg, .bits = c->bits);
}

/**
 * @brief Writes nan or inf at c->at, for a value whose exponent field is all ones.
 *
 * @param[in] c Buffer, capacity, offset and the bit pattern [BORROWS].
 * @return      The offset past what was written, or c->cap when it did not fit.
 * @note A non-zero mantissa gives nan, written without a sign whatever the sign bit holds.
 * @note A zero mantissa gives inf, with a leading minus when the sign bit is set.
 * @note Both are lower case and unquoted, so neither is valid JSON on its own.
 */
MMGR_INLINE size_t verba_non_finite(const VerbaCtx *c)
{
    if (MMGR_CALL(verba_mant, VerbaCtx, .bits = c->bits) != 0U)
    {
        return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = "nan");
    }

    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = c->bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "inf");
}

/**
 * @brief Writes c->real at c->at in the shorter of a plain or an exponential form, to c->sig digits.
 *
 * @param[in] c Buffer, capacity, offset, the value and the significant digit count [BORROWS].
 * @return      The offset past what was written, or c->cap once something did not fit.
 * @note A c->sig of 0 is taken as 1, and anything above MMGR_G_MAX_SIG is held there.
 * @note An exponent field of all ones goes to verba_non_finite, and a zero value writes a single 0.
 * @note The decimal exponent is first estimated by multiplying the binary one by 78913 and shifting right 18,
 *       which is 0.30103 to five places, then corrected by muto.scale_to_u64 in at most four passes.
 * @note Trailing zeros are dropped from the digits before any of the four forms is chosen.
 * @note The four forms are exponential, digits with trailing zeros, digits with the point inside, and 0. with
 *       leading zeros, picked on where the decimal exponent falls against the digit count.
 */
MMGR_INLINE size_t verba_g(const VerbaCtx *c)
{
    const mmgr_u64 bits = verba_bits(c);

    if (MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .bits = bits);
    }

    uint8_t sig = (c->sig == 0) ? 1u : c->sig;
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }

    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }

    const mmgr_u64 be = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);
    mmgr_u64 n = MMGR_CALL(verba_mant, VerbaCtx, .bits = bits);

    if ((be == 0U) && (n == 0U))
    {
        return MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '0');
    }

    mmgr_iword s = 1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS;
    if (be != 0U)
    {
        n |= 1ULL << MMGR_DBL_MANT_BITS;
        s = (mmgr_iword)(be - MMGR_DBL_BIAS - MMGR_DBL_MANT_BITS);
    }

    const mmgr_u64 limit = mmgr_verba_pow10[sig];

    mmgr_iword e = (mmgr_iword)(((mmgr_i64)(63 - MMGR_CALL(clz.lead, ClzCfg, .val = n) + s) * 78913) >> 18);
    mmgr_iword p = (mmgr_iword)((mmgr_iword)sig - 1 - e);
    mmgr_u64 mant = 0U;

    for (uint8_t guard = 0; guard < 4U; guard++)
    {
        mant = MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &n, .e2 = s, .ex = p, .above = 0U);
        if (mant >= limit)
        {
            e++;
            p--;
        }
        else if ((sig > 1U) && (mant < (limit / 10U)))
        {
            e--;
            p++;
        }
        else
        {
            break;
        }
    }

    uint8_t digits = sig;
    while ((digits > 1u) && ((mant % 10u) == 0u))
    {
        mant /= 10u;
        digits--;
    }

    if ((e < -4) || (e >= (mmgr_i32)sig))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                       .point_after = 1u);
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = 'e');
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (e < 0) ? '-' : '+');
        return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                         .val = (uint64_t)((e < 0) ? -e : e), .base = 10u, .min = 2u);
    }
    if (e >= ((mmgr_i32)digits - 1))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                       .point_after = 0u);
        return MMGR_CALL(verba_zeros, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                         .text_len = (size_t)(e - (mmgr_i32)digits + 1));
    }
    if (e >= 0)
    {
        return MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                         .point_after = (uint8_t)(e + 1));
    }

    at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "0.");
    at = MMGR_CALL(verba_zeros, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text_len = (size_t)(-e - 1));
    return MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                     .point_after = 0u);
}

/**
 * @brief Writes c->real at c->at with exactly c->decimals digits after the point.
 *
 * @param[in] c Buffer, capacity, offset, the value and the decimal count [BORROWS].
 * @return      The offset past what was written, or c->cap once something did not fit.
 * @note An exponent field of all ones goes to verba_non_finite; the sign is written before anything else.
 * @note A magnitude too large for 64 bits of integer part falls back to verba_g at ten significant digits.
 * @note c->decimals is held at MMGR_FIXED_MAX_DECIMALS, and a value of 0 writes no point at all.
 * @note The fraction is rounded by muto.scale_to_u64, given the low bit of the integer part so a tie goes to even.
 * @note A fraction that rounds up to the whole scale carries into the integer part and is written as zeros.
 */
MMGR_INLINE size_t verba_fixed(const VerbaCtx *c)
{
    const mmgr_u64 bits = verba_bits(c);
    const mmgr_u64 klass = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);

    if (klass == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .bits = bits);
    }

    double v = c->real;
    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
        v = -v;
    }

    if (klass >= (MMGR_DBL_BIAS + 64))
    {
        return MMGR_CALL(verba_g, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .real = v, .sig = 10u);
    }

    uint8_t decimals = c->decimals;
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

    at = MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .val = ip, .base = 10u, .min = 1u);

    if (decimals != 0u)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '.');
        at = MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .val = frac, .base = 10u,
                       .min = decimals);
    }
    return at;
}

/**
 * @brief Stores the terminator at c->at and reports the length.
 *
 * @param[in] c Buffer, capacity and the offset reached [BORROWS].
 * @return      c->at, or 0 when c->at already reached c->cap.
 * @note This is the only call that writes a terminator, so a buffer is not a string until it has run.
 * @note A return of 0 covers both an empty result and one that ran out of room; verba_ok tells them apart.
 */
MMGR_INLINE size_t verba_finish(const VerbaCtx *c)
{
    if (c->at >= c->cap)
    {
        return 0;
    }
    c->out[c->at] = '\0';
    return c->at;
}

/**
 * @brief Returns whether c->at is still below c->cap.
 *
 * @param[in] c Capacity and the offset reached [BORROWS].
 * @return      MMGR_TRUE while there is still room, MMGR_FALSE once a call returned c->cap.
 * @note Reads neither c->out nor any value member, so it touches no memory.
 */
MMGR_INLINE mmgr_bool verba_ok(const VerbaCtx *c)
{
    return (mmgr_bool)(c->at < c->cap);
}


/**
 * @brief Writes c->val in base ten, in at least c->min digits.
 *
 * @param[in,out] c Buffer, cursor, value and least digit count [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes the base at ten and forwards c->min, which is what separates it from verba_u32.
 */
MMGR_INLINE size_t verba_u32w(const VerbaCtx *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u,
                     .min = c->min);
}

/**
 * @brief Writes c->val in base sixteen, in at least c->min digits.
 *
 * @param[in,out] c Buffer, cursor, value and least digit count [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes the base at sixteen, so the digits come out lower case.
 */
MMGR_INLINE size_t verba_hex(const VerbaCtx *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 16u,
                     .min = c->min);
}

/**
 * @brief Writes c->val in base ten, with no padding.
 *
 * @param[in,out] c Buffer, cursor and value [BORROWS].
 * @return          The cursor past what was written.
 * @note Fixes both the base at ten and the least digit count at one, so c->min and c->base take no part.
 */
MMGR_INLINE size_t verba_u32(const VerbaCtx *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u,
                     .min = 1u);
}

/**
 * @brief Writes c->val in base ten, with no padding.
 *
 * @param[in,out] c Buffer, cursor and value [BORROWS].
 * @return          The cursor past what was written.
 * @note The same walk as verba_u32, since c->val is 64 bits either way. Both names exist so a caller
 *       reads the width it means at the call.
 */
MMGR_INLINE size_t verba_u64(const VerbaCtx *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u,
                     .min = 1u);
}

/**
 * @brief Returns whether c->real has its sign bit set.
 *
 * @param[in] c The value to test, as c->real [BORROWS].
 * @return      MMGR_TRUE when the sign bit is set.
 * @note Reads the bit rather than comparing against zero, so a negative zero returns MMGR_TRUE.
 */
MMGR_INLINE mmgr_bool verba_sign_bit(const VerbaCtx *c)
{
    // Explicit cast narrows the bit test into the mmgr_bool container
    return (mmgr_bool)(MMGR_CALL(verba_sign, VerbaCtx, .bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real)) !=
                       0U);
}

/**
 * @brief Returns whether c->real is an infinity.
 *
 * @param[in] c The value to test, as c->real [BORROWS].
 * @return      MMGR_TRUE for either infinity.
 * @note Wants the exponent field all ones and the mantissa zero, where verba_is_nan wants it non-zero.
 * @note Says nothing about the sign, so a negative infinity returns MMGR_TRUE too.
 */
MMGR_INLINE mmgr_bool verba_is_inf(const VerbaCtx *c)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);

    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) == 0U));
}

/**
 * @brief Returns whether c->real is a NaN.
 *
 * @param[in] c The value to test, as c->real [BORROWS].
 * @return      MMGR_TRUE for any NaN.
 * @note Wants the exponent field all ones and the mantissa non-zero, where verba_is_inf wants it zero.
 */
MMGR_INLINE mmgr_bool verba_is_nan(const VerbaCtx *c)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);

    // Explicit cast narrows the combined test into the mmgr_bool container
    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) != 0U));
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_verba_ and verba_ prefixes, which the two share.
 */
#define VERBA_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_verba_, verba_, VerbaCtx, VerbaCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in verba_scribo.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest. ok
 *       forwards cap and at alone, so it touches no memory, and the three classifiers forward real.
 * @note uint is the only entry that forwards c->base. u32w, hex, u32 and u64 each fix a base of their
 *       own in the backend above rather than at the call.
 */
VERBA_ENTRY(size_t, put_n, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text, .text_len = c->text_len)
VERBA_ENTRY(size_t, put, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text, .text_len = c->text_len)
VERBA_ENTRY(size_t, put_clip, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text)
VERBA_ENTRY(size_t, u64_clip, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .columns = c->columns)
VERBA_ENTRY(size_t, xml, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text)
VERBA_ENTRY(size_t, ch, .out = c->out, .cap = c->cap, .at = c->at, .ch = c->ch)
VERBA_ENTRY(size_t, uint, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = c->base, .min = c->min)
VERBA_ENTRY(size_t, u32w, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .min = c->min)
VERBA_ENTRY(size_t, hex, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .min = c->min)
VERBA_ENTRY(size_t, u32, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val)
VERBA_ENTRY(size_t, u64, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val)
VERBA_ENTRY(size_t, i64, .out = c->out, .cap = c->cap, .at = c->at, .sval = c->sval)
VERBA_ENTRY(size_t, g, .out = c->out, .cap = c->cap, .at = c->at, .real = c->real, .sig = c->sig)
VERBA_ENTRY(size_t, fixed, .out = c->out, .cap = c->cap, .at = c->at, .real = c->real, .decimals = c->decimals)
VERBA_ENTRY(size_t, json, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text)
VERBA_ENTRY(size_t, finish, .out = c->out, .cap = c->cap, .at = c->at)
VERBA_ENTRY(mmgr_bool, ok, .cap = c->cap, .at = c->at)
VERBA_ENTRY(mmgr_bool, sign_bit, .real = c->real)
VERBA_ENTRY(mmgr_bool, is_inf, .real = c->real)
VERBA_ENTRY(mmgr_bool, is_nan, .real = c->real)
