/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file verba_linea.c
 * @brief A line written into a caller's buffer through verba's own entries, and passed on as it fills.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note Every write comes down to mmgr_line_put_n, which places each piece through verba_textus.put_n
 *       and passes the line to its flush between pieces. A number goes into a buffer of its own
 *       first, and so its digits cross a flush the way a text does.
 * @note Every write ends with the store of the terminator, one store it pays for on every call. A
 *       written line's buffer is always a terminated text.
 */
#include "verba_scribo/verba_linea.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"
#include "transformo/transformo.h"
#include "verba_scribo/verba_scribo.h"

/**
 * @brief Bytes a number occupies before it goes on a line: its max digits at 255 and more.
 */
#define LINEA_DIGITS 280u

/**
 * @brief The max digits or columns a number entry can take, the max a byte of verba's arguments holds.
 */
#define LINEA_LEAST_MAX 255u

/**
 * @brief Spaces mmgr_line_put_columns writes in one run.
 */
#define LINEA_SPACES 32u

/**
 * @brief The run of spaces mmgr_line_put_columns pads with, and its terminator.
 *
 * @note EMBED_PASS_MEMORY gives each pass of nvcc its own copy, and so a kernel reads the run from device memory.
 */
static EMBED_PASS_MEMORY const char s_linea_spaces[LINEA_SPACES + 1u] = "                                ";

/**
 * @brief Stores the terminator: at at while the line has room, in its last byte once a write did not fit.
 *
 * @param[in] line The line [BORROWS].
 * @note A line of no bytes has nowhere to store one.
 */
EMBED_HOST_DEVICE EMBED_INLINE void linea_terminated(const VerbaLinea *line)
{
    if (line->capacity > 0u)
    {
        line->out[(line->at < line->capacity) ? line->at : (line->capacity - 1u)] = '\0';
    }
}

/**
 * @brief Marks the line full, what it holds terminated in its last byte.
 *
 * @param[in,out] line The line [BORROWS].
 */
EMBED_HOST_DEVICE EMBED_INLINE void linea_filled(VerbaLinea *line)
{
    line->at = line->capacity;
    linea_terminated(line);
}

/**
 * @brief Puts the written digits of a number on the line.
 *
 * @param[in,out] line    The line [BORROWS].
 * @param[in]     digits  The number's own buffer, LINEA_DIGITS bytes [BORROWS].
 * @param[in]     written Where verba's write ended. LINEA_DIGITS holds the widest number, which
 *                        LINEA_LEAST_MAX bounds, and so it always fit.
 */
EMBED_HOST_DEVICE EMBED_INLINE void linea_number(VerbaLinea *line, const char *digits, size_t written)
{
    (void)mmgr_line_put_n(line, digits, written);
}

/**
 * @brief Holds a count of digits or columns at LINEA_LEAST_MAX.
 *
 * @param[in] count The count asked for.
 * @return          count, or LINEA_LEAST_MAX where it is more.
 */
EMBED_HOST_DEVICE EMBED_INLINE uint8_t linea_least(unsigned int count)
{
    // Explicit cast narrows a count already clamped at 255 into the byte verba reads it from
    return (uint8_t)((count < LINEA_LEAST_MAX) ? count : LINEA_LEAST_MAX);
}

EMBED_HOST_DEVICE VerbaLinea mmgr_line_over(char *out, size_t capacity)
{
    const VerbaLinea line = {out, (out != NULL) ? capacity : 0u, 0u, NULL, NULL, 0u};
    return line;
}

EMBED_HOST_DEVICE embed_bool mmgr_line_flush(VerbaLinea *line)
{
    if ((line->flush == NULL) || (line->at >= line->capacity))
    {
        return EMBED_FALSE;
    }
    const int passed = line->flush(line->context, line->out, line->at);

    if (passed == 0)
    {
        linea_filled(line);
        return EMBED_FALSE;
    }
    line->flushed += line->at;
    line->at = 0u;
    linea_terminated(line);
    return EMBED_TRUE;
}

EMBED_HOST_DEVICE size_t mmgr_line_put_n(VerbaLinea *line, const char *text, size_t length)
{
    // A line of fewer than two bytes holds none past its terminator, and a flush could never empty it into room
    if ((length > 0u) && (line->capacity < 2u))
    {
        linea_filled(line);
        return 0u;
    }
    size_t done = 0u;

    while (done < length)
    {
        // One byte of the line stays reserved for the terminator, as verba holds it
        const size_t room = (line->at < line->capacity) ? (line->capacity - line->at - 1u) : 0u;
        const size_t left = length - done;
        const size_t piece = (left < room) ? left : room;

        line->at = EMBED_CALL(verba_textus.put_n, VerbaTextusCfg, .out = line->out, .cap = line->capacity,
                              .at = line->at, .text = text + done, .text_len = piece);
        done += piece;
        if ((done < length) && (line->flush == NULL))
        {
            linea_filled(line);
            return done;
        }
        if ((done < length) && !mmgr_line_flush(line))
        {
            return done;
        }
    }
    linea_terminated(line);
    return done;
}

/**
 * @brief Writes a terminated text a piece the size of the room left at a time.
 *
 * @param[in,out] line The line [BORROWS].
 * @param[in]     text Terminated text [BORROWS].
 * @return             Bytes of text the line took, passed on included.
 * @note The measure of each piece goes as far as one byte past what the room takes. So the read of the
 *       text goes no further than its terminator or the room, whichever comes first. It needs no
 *       bound of its own.
 * @note A piece that reaches the room's last byte goes on a line with a flush and goes out before the
 *       measure of the next. On a line with none it fills the line, what fits written.
 */
EMBED_HOST_DEVICE EMBED_INLINE size_t linea_streamed(VerbaLinea *line, const char *text)
{
    size_t done = 0u;

    for (;;)
    {
        const size_t room = (line->at < line->capacity) ? (line->capacity - line->at) : 0u;

        if (room == 0u)
        {
            return done;
        }
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text + done, .cap = room);

        if (length < room)
        {
            return done + mmgr_line_put_n(line, text + done, length);
        }
        // The rest runs to the room's last byte or past it, and that byte is the terminator's
        if ((line->flush == NULL) || (line->capacity < 2u))
        {
            return done + mmgr_line_put_n(line, text + done, room);
        }
        (void)mmgr_line_put_n(line, text + done, room - 1u);
        done += room - 1u;
        if (!mmgr_line_flush(line))
        {
            return done;
        }
    }
}

EMBED_HOST_DEVICE size_t mmgr_line_put(VerbaLinea *line, const char *text)
{
    return linea_streamed(line, text);
}

EMBED_HOST_DEVICE size_t mmgr_line_put_max(VerbaLinea *line, const char *text, size_t max)
{
    return mmgr_line_put_n(line, text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = max));
}

/**
 * @brief Writes spaces, LINEA_SPACES at a time.
 *
 * @param[in,out] line  The line [BORROWS].
 * @param[in]     count Spaces to write.
 */
EMBED_HOST_DEVICE EMBED_INLINE void linea_spaced(VerbaLinea *line, size_t count)
{
    for (size_t pad = count; pad > 0u;)
    {
        const size_t run = (pad < LINEA_SPACES) ? pad : LINEA_SPACES;

        (void)mmgr_line_put_n(line, s_linea_spaces, run);
        pad -= run;
    }
}

EMBED_HOST_DEVICE void mmgr_line_put_columns(VerbaLinea *line, const char *text, unsigned int columns)
{
    const size_t length = linea_streamed(line, text);

    linea_spaced(line, (length < columns) ? (columns - length) : 0u);
}

EMBED_HOST_DEVICE void mmgr_line_put_right(VerbaLinea *line, const char *text, unsigned int columns)
{
    // The measure stops at columns bytes, as a longer text gets no spaces
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = columns);

    linea_spaced(line, (length < columns) ? (columns - length) : 0u);
    (void)linea_streamed(line, text);
}

EMBED_HOST_DEVICE void mmgr_line_ch(VerbaLinea *line, char character)
{
    (void)mmgr_line_put_n(line, &character, 1u);
}

EMBED_HOST_DEVICE void mmgr_line_decimal(VerbaLinea *line, uint64_t value, unsigned int digits_least)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .min = linea_least(digits_least)));
}

EMBED_HOST_DEVICE void mmgr_line_decimal_columns(VerbaLinea *line, uint64_t value, unsigned int columns)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .columns = linea_least(columns)));
}

EMBED_HOST_DEVICE void mmgr_line_decimal_signed(VerbaLinea *line, int64_t value)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .sval = value));
}

EMBED_HOST_DEVICE void mmgr_line_hex(VerbaLinea *line, uint64_t value, unsigned int digits_least)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .min = linea_least(digits_least)));
}

/**
 * @brief Reports whether value times ten to the power scale is an exact tie, an odd integer q over two.
 *
 * @param[in]  value  The value.
 * @param[in]  scale  The power of ten that the round applies before it takes the integer.
 * @param[out] halves q, modulo 2 to the power 64, where the value is a tie [BORROWS].
 * @return            EMBED_TRUE where the value is a tie.
 * @note A finite double is an odd mantissa m times 2 to the power e. Times ten to the scale, it is m times five to
 *       the scale, times 2 to the power e + scale. That is q over two where e + scale is -1 and q is an integer. For
 *       a scale below 0, q is an integer where five to the minus scale divides m.
 * @note The product wraps at 2 to the power 64, which 4 divides, and so q modulo 4 is right for any scale.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool linea_tie(double value, int64_t scale, uint64_t *halves)
{
    const union {
        double real;
        uint64_t bits;
    } pun = {value};
    const uint64_t biased = (pun.bits >> 52u) & 0x7FFu;
    uint64_t mantissa = pun.bits & ((1ULL << 52u) - 1u);
    int64_t exponent = -1074;

    if (biased == 0x7FFu)
    {
        return EMBED_FALSE;
    }
    if (biased != 0u)
    {
        mantissa |= 1ULL << 52u;
        // Explicit cast takes the eleven bits of the biased exponent, which fit any signed width
        exponent = (int64_t)biased - 1075;
    }
    if (mantissa == 0u)
    {
        return EMBED_FALSE;
    }
    while ((mantissa & 1u) == 0u)
    {
        mantissa >>= 1u;
        exponent += 1;
    }
    if ((exponent + scale) != -1)
    {
        return EMBED_FALSE;
    }
    // A mantissa below 2^53 holds no power of five past 5^22, and so a larger power never divides it
    for (int64_t fives = scale; fives < 0; fives += 1)
    {
        if ((mantissa % 5u) != 0u)
        {
            return EMBED_FALSE;
        }
        mantissa /= 5u;
    }
    for (int64_t fives = 0; fives < scale; fives += 1)
    {
        mantissa *= 5u;
    }
    *halves = mantissa;
    return EMBED_TRUE;
}

/**
 * @brief Reports whether a round up in magnitude of value times ten to the power scale, from an exact tie, gives an
 *        odd last digit.
 *
 * @param[in] value The value.
 * @param[in] scale The power of ten that the round applies before it takes the integer.
 * @return          EMBED_TRUE where the even digit is the one below.
 * @note The round up gives (q + 1) / 2, which is odd where q is 1 modulo 4.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool linea_tie_odd(double value, int64_t scale)
{
    uint64_t halves = 0u;

    return (linea_tie(value, scale, &halves) && ((halves & 3u) == 1u)) ? EMBED_TRUE : EMBED_FALSE;
}

/**
 * @brief The integer nearest value times ten to the power cursor, an exact tie to the even one.
 *
 * @param[in] value    The value, finite and above zero.
 * @param[in] mantissa Its mantissa, with the implicit bit [BORROWS].
 * @param[in] binary   Its binary exponent.
 * @param[in] cursor   The power of ten.
 * @return             The rounded integer.
 * @note muto.scale_to_u64 scales with a bound on its bits, and so its round of a tie is not a guide. A tie takes its
 *       digits from q itself.
 * @note q is twice the digits plus one. The power estimate is never too high, and it is one short only for a value
 *       below about twice a power of ten. So the digits stay below about 2 times 10 to the 18 at MMGR_G_MAX_SIG,
 *       and q stays below about 4 times 10 to the 18. It never wraps at 2 to the 64.
 */
EMBED_HOST_DEVICE EMBED_INLINE uint64_t linea_scaled(double value, embed_u64 *mantissa, embed_iword binary, embed_iword cursor)
{
    const uint64_t scaled = EMBED_CALL(muto.scale_to_u64, TransformoCfg, .mant = mantissa, .e2 = binary, .ex = cursor);
    uint64_t halves = 0u;

    if (!linea_tie(value, (int64_t)cursor, &halves))
    {
        return scaled;
    }
    const uint64_t below = (halves - 1u) / 2u;

    return ((below & 1u) == 0u) ? below : (below + 1u);
}

EMBED_HOST_DEVICE void mmgr_line_fixed(VerbaLinea *line, double value, unsigned int decimals, unsigned int columns, embed_bool plus)
{
    char digits[LINEA_DIGITS];
    const unsigned int places = (decimals < MMGR_FIXED_MAX_DECIMALS) ? decimals : MMGR_FIXED_MAX_DECIMALS;
    const embed_bool plus_written =
        (plus != EMBED_FALSE) && !EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = value);

    if (plus_written)
    {
        digits[0] = '+';
    }
    // LINEA_DIGITS holds a sign, the twenty digits of 64 bits, a point and MMGR_FIXED_MAX_DECIMALS, and so it fits
    const size_t written = EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = digits, .cap = sizeof digits,
                                      .at = plus_written ? 1u : 0u, .real = value, .decimals = linea_least(places));

    // An odd digit is 1 at least, and so the step down to the even one borrows from no digit before it
    if (linea_tie_odd(value, (int64_t)places))
    {
        // Explicit cast narrows a digit of '1' to '9' less one, which a char holds
        digits[written - 1u] = (char)(digits[written - 1u] - 1);
    }
    const size_t wide = linea_least(columns);

    linea_spaced(line, (written < wide) ? (wide - written) : 0u);
    linea_number(line, digits, written);
}

/**
 * @brief The significant digits of a finite, nonzero magnitude, and the power of ten of the first.
 *
 * @param[in]  value       The value, finite and not zero.
 * @param[in]  significant Digits to keep, 1 to MMGR_G_MAX_SIG.
 * @param[out] power       The power of ten of the first digit [BORROWS].
 * @return                 The digits as an integer of significant digits, rounded as printf rounds them.
 * @note The scale and the search for the power are those of mmgr_verba_g. A round up to ten to the significant
 *       raises the power by one and scales again. An exact tie rounds to the even digit, through linea_scaled.
 */
EMBED_HOST_DEVICE EMBED_INLINE uint64_t linea_significant(double value, unsigned int significant, int64_t *power)
{
    const union {
        double real;
        uint64_t bits;
    } pun = {value};
    const uint64_t biased = (pun.bits >> 52u) & 0x7FFu;
    embed_u64 mantissa = pun.bits & ((1ULL << 52u) - 1u);
    embed_iword binary = -1074;

    if (biased != 0u)
    {
        mantissa |= 1ULL << 52u;
        // Explicit cast takes the eleven bits of the biased exponent, which fit any signed width
        binary = (embed_iword)biased - 1075;
    }
    uint64_t limit = 1u;

    for (unsigned int place = 0u; place < significant; place += 1u)
    {
        limit *= 10u;
    }
    // The power estimate is the binary one times 78913, shifted right 18, a power that is never too high
    embed_iword decimal =
        (embed_iword)(((embed_i64)(63 - EMBED_CALL(clz.lead, ClzCfg, .val = mantissa) + binary) * 78913) >> 18);
    // Explicit casts take a count of at max MMGR_G_MAX_SIG into the signed word of the scale, and the difference
    // back from int. The power of a double is at max 309 either way, and so the difference fits 16 bits
    embed_iword cursor = (embed_iword)((embed_iword)significant - 1 - decimal);
    uint64_t digits = linea_scaled(value, &mantissa, binary, cursor);

    while (digits >= limit)
    {
        decimal += 1;
        cursor -= 1;
        digits = linea_scaled(value, &mantissa, binary, cursor);
    }
    *power = (int64_t)decimal;
    return digits;
}

/**
 * @brief Writes the sign of a value, and an infinity or a NaN whole.
 *
 * @param[in,out] written  The line over the text of the value [BORROWS].
 * @param[in]     value    The value.
 * @param[in]     negative Whether the sign bit of the value is set.
 * @param[in]     plus     Whether a value of no sign gets a plus sign.
 * @return                 EMBED_TRUE where the value is an infinity or a NaN, and the text holds it whole.
 * @note An infinity or a NaN goes out as mmgr_verba_fixed writes it, with no sign of its own here. Its text starts
 *       at the start of the line, over the sign.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool linea_signed(VerbaLinea *written, double value, embed_bool negative, embed_bool plus)
{
    if ((negative != EMBED_FALSE) || (plus != EMBED_FALSE))
    {
        mmgr_line_ch(written, (negative != EMBED_FALSE) ? '-' : '+');
    }
    if ((EMBED_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = value) == EMBED_FALSE) &&
        (EMBED_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = value) == EMBED_FALSE))
    {
        return EMBED_FALSE;
    }
    written->at = EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = written->out, .cap = written->capacity,
                             .at = 0u, .real = value, .decimals = 0u);
    return EMBED_TRUE;
}

/**
 * @brief Finishes the text of a value and writes it right aligned in its columns.
 *
 * @param[in,out] line    The line to write to [BORROWS].
 * @param[in,out] written The line over the text of the value [BORROWS].
 * @param[in]     columns The fewest columns, held at LINEA_LEAST_MAX.
 */
EMBED_HOST_DEVICE EMBED_INLINE void linea_aligned(VerbaLinea *line, VerbaLinea *written, unsigned int columns)
{
    const size_t length = mmgr_line_finish(written);
    const size_t wide = linea_least(columns);

    linea_spaced(line, (length < wide) ? (wide - length) : 0u);
    (void)mmgr_line_put_n(line, written->out, length);
}

EMBED_HOST_DEVICE void mmgr_line_exponent(VerbaLinea *line, double value, unsigned int decimals, unsigned int columns, embed_bool plus)
{
    char text[LINEA_DIGITS];
    const unsigned int significant = ((decimals + 1u) < MMGR_G_MAX_SIG) ? (decimals + 1u) : MMGR_G_MAX_SIG;
    const embed_bool negative = EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = value);
    const double magnitude = (negative != EMBED_FALSE) ? -value : value;
    VerbaLinea written = mmgr_line_over(text, sizeof text);

    if (linea_signed(&written, value, negative, plus) == EMBED_FALSE)
    {
        int64_t power = 0;
        // A finite magnitude of no sign is 0 where it is at max 0
        const uint64_t digits = (magnitude <= 0.0) ? 0u : linea_significant(magnitude, significant, &power);
        char figures[LINEA_DIGITS];
        const size_t figured = EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = figures, .cap = sizeof figures,
                                          .val = digits, .min = linea_least(significant));

        (void)mmgr_line_put_n(&written, figures, 1u);
        if (significant > 1u)
        {
            mmgr_line_ch(&written, '.');
            (void)mmgr_line_put_n(&written, figures + 1u, figured - 1u);
        }
        mmgr_line_ch(&written, 'e');
        mmgr_line_ch(&written, (power < 0) ? '-' : '+');
        // Explicit cast takes the magnitude of a power of at max 324, which an unsigned word holds
        mmgr_line_decimal(&written, (uint64_t)((power < 0) ? -power : power), 2u);
    }
    linea_aligned(line, &written, columns);
}

EMBED_HOST_DEVICE void mmgr_line_general(VerbaLinea *line, double value, unsigned int significant, unsigned int columns, embed_bool plus)
{
    char text[LINEA_DIGITS];
    const unsigned int kept =
        (significant == 0u) ? 1u : ((significant < MMGR_G_MAX_SIG) ? significant : MMGR_G_MAX_SIG);
    const embed_bool negative = EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = value);
    const double magnitude = (negative != EMBED_FALSE) ? -value : value;
    VerbaLinea written = mmgr_line_over(text, sizeof text);

    if (linea_signed(&written, value, negative, plus) != EMBED_FALSE)
    {
        // linea_signed wrote the infinity or the NaN whole
    }
    else if (magnitude <= 0.0)
    {
        // A finite magnitude of no sign is 0 where it is at max 0, and the general form writes it as one zero
        mmgr_line_ch(&written, '0');
    }
    else
    {
        int64_t power = 0;
        const uint64_t digits = linea_significant(magnitude, kept, &power);
        char figures[LINEA_DIGITS];
        const size_t figured = EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = figures, .cap = sizeof figures,
                                          .val = digits, .min = linea_least(kept));
        size_t used = figured;

        // The general form drops the zeros at the end of the digits, and a point with no digit after it
        while ((used > 1u) && (figures[used - 1u] == '0'))
        {
            used -= 1u;
        }
        // Explicit cast widens a count of at max MMGR_G_MAX_SIG to the signed width of the power
        if ((power < -4) || (power >= (int64_t)kept))
        {
            (void)mmgr_line_put_n(&written, figures, 1u);
            if (used > 1u)
            {
                mmgr_line_ch(&written, '.');
                (void)mmgr_line_put_n(&written, figures + 1u, used - 1u);
            }
            mmgr_line_ch(&written, 'e');
            mmgr_line_ch(&written, (power < 0) ? '-' : '+');
            // Explicit cast takes the magnitude of a power of at max 324, which an unsigned word holds
            mmgr_line_decimal(&written, (uint64_t)((power < 0) ? -power : power), 2u);
        }
        else if (power >= 0)
        {
            // Explicit cast takes a power below the count of digits, which a size_t holds
            const size_t whole = (size_t)power + 1u;

            (void)mmgr_line_put_n(&written, figures, whole);
            if (used > whole)
            {
                mmgr_line_ch(&written, '.');
                (void)mmgr_line_put_n(&written, figures + whole, used - whole);
            }
        }
        else
        {
            mmgr_line_put(&written, "0.");
            for (int64_t zero = power + 1; zero < 0; zero += 1)
            {
                mmgr_line_ch(&written, '0');
            }
            (void)mmgr_line_put_n(&written, figures, used);
        }
    }
    linea_aligned(line, &written, columns);
}

EMBED_HOST_DEVICE size_t mmgr_line_finish(VerbaLinea *line)
{
    return EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = line->out, .cap = line->capacity, .at = line->at);
}

EMBED_HOST_DEVICE embed_bool mmgr_line_ok(const VerbaLinea *line)
{
    return EMBED_CALL(verba_finis.ok, VerbaFinisCfg, .cap = line->capacity, .at = line->at);
}
