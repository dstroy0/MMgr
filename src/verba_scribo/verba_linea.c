/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file verba_linea.c
 * @brief A line written into a caller's buffer through verba's own entries, and handed on as it fills.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note Every write comes down to mmgr_line_put_n, which places each piece through verba_textus.put_n
 *       and hands the line to its flush between pieces. A number is written into a buffer of its own
 *       first, and so its digits cross a flush the way a text does.
 * @note Every write ends by storing the terminator, one store it pays for on every call, and so a
 *       written line's buffer is always a terminated text.
 */
#include "verba_scribo/verba_linea.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "verba_scribo/verba_scribo.h"

/**
 * @brief Bytes a number is written into before it goes on a line: its most digits at 255 and more.
 */
#define LINEA_DIGITS 280u

/**
 * @brief The most digits or columns a number entry is asked for, the most a byte of verba's arguments holds.
 */
#define LINEA_LEAST_MOST 255u

/**
 * @brief Spaces mmgr_line_put_columns writes in one run.
 */
#define LINEA_SPACES 32u

/**
 * @brief The run of spaces mmgr_line_put_columns pads with, and its terminator.
 */
static const char s_linea_spaces[LINEA_SPACES + 1u] = "                                ";

/**
 * @brief Stores the terminator: at at while the line has room, in its last byte once a write did not fit.
 *
 * @param[in] line The line [BORROWS].
 * @note A line of no bytes has nowhere to store one.
 */
EMBED_INLINE void linea_terminated(const VerbaLinea *line)
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
EMBED_INLINE void linea_filled(VerbaLinea *line)
{
    line->at = line->capacity;
    linea_terminated(line);
}

/**
 * @brief Puts the written digits of a number on the line.
 *
 * @param[in,out] line    The line [BORROWS].
 * @param[in]     digits  The number's own buffer, LINEA_DIGITS bytes [BORROWS].
 * @param[in]     written Where verba's writing ended. LINEA_DIGITS holds the widest number, which
 *                        LINEA_LEAST_MOST bounds, and so it always fit.
 */
EMBED_INLINE void linea_number(VerbaLinea *line, const char *digits, size_t written)
{
    (void)mmgr_line_put_n(line, digits, written);
}

/**
 * @brief Holds a count of digits or columns at LINEA_LEAST_MOST.
 *
 * @param[in] count The count asked for.
 * @return          count, or LINEA_LEAST_MOST where it is more.
 */
EMBED_INLINE uint8_t linea_least(unsigned int count)
{
    // Explicit cast narrows a count already clamped at 255 into the byte verba reads it from
    return (uint8_t)((count < LINEA_LEAST_MOST) ? count : LINEA_LEAST_MOST);
}

VerbaLinea mmgr_line_over(char *out, size_t capacity)
{
    const VerbaLinea line = {out, (out != NULL) ? capacity : 0u, 0u, NULL, NULL, 0u};
    return line;
}

embed_bool mmgr_line_flush(VerbaLinea *line)
{
    if ((line->flush == NULL) || (line->at >= line->capacity))
    {
        return EMBED_FALSE;
    }
    const int handed = line->flush(line->context, line->out, line->at);

    if (handed == 0)
    {
        linea_filled(line);
        return EMBED_FALSE;
    }
    line->flushed += line->at;
    line->at = 0u;
    linea_terminated(line);
    return EMBED_TRUE;
}

size_t mmgr_line_put_n(VerbaLinea *line, const char *text, size_t length)
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
        // A byte of the line is reserved for the terminator, as verba holds it
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
 * @return             Bytes of text the line took, handed on included.
 * @note Each piece is measured as far as one byte past what the room takes, and so the text is read no
 *       further than its terminator or the room, whichever comes first, and needs no bound of its own.
 * @note A piece reaching the room's last byte goes on a line with a flush and is handed on before the
 *       next is measured. On a line with none it fills the line, what fits written.
 */
EMBED_INLINE size_t linea_streamed(VerbaLinea *line, const char *text)
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

size_t mmgr_line_put(VerbaLinea *line, const char *text)
{
    return linea_streamed(line, text);
}

size_t mmgr_line_put_most(VerbaLinea *line, const char *text, size_t most)
{
    return mmgr_line_put_n(line, text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = most));
}

void mmgr_line_put_columns(VerbaLinea *line, const char *text, unsigned int columns)
{
    const size_t length = linea_streamed(line, text);

    for (size_t pad = (length < columns) ? (columns - length) : 0u; pad > 0u;)
    {
        const size_t run = (pad < LINEA_SPACES) ? pad : LINEA_SPACES;

        (void)mmgr_line_put_n(line, s_linea_spaces, run);
        pad -= run;
    }
}

void mmgr_line_ch(VerbaLinea *line, char character)
{
    (void)mmgr_line_put_n(line, &character, 1u);
}

void mmgr_line_decimal(VerbaLinea *line, uint64_t value, unsigned int digits_least)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .min = linea_least(digits_least)));
}

void mmgr_line_decimal_columns(VerbaLinea *line, uint64_t value, unsigned int columns)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .columns = linea_least(columns)));
}

void mmgr_line_decimal_signed(VerbaLinea *line, int64_t value)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .sval = value));
}

void mmgr_line_hex(VerbaLinea *line, uint64_t value, unsigned int digits_least)
{
    char digits[LINEA_DIGITS];

    linea_number(line, digits,
                 EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = digits, .cap = sizeof digits, .val = value,
                            .min = linea_least(digits_least)));
}

size_t mmgr_line_finish(VerbaLinea *line)
{
    return EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = line->out, .cap = line->capacity, .at = line->at);
}

embed_bool mmgr_line_ok(const VerbaLinea *line)
{
    return EMBED_CALL(verba_finis.ok, VerbaFinisCfg, .cap = line->capacity, .at = line->at);
}
