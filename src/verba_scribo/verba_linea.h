/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file verba_linea.h
 * @brief A line written into a caller's buffer and handed on as it fills, one field at a time.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note Every entry takes the line and the value it writes as plain arguments, and therefore a C++ translation
 *       unit calls them as they are. This header includes mmgr.h alone and defines no table, which
 *       keeps it free of the designated initializers verba_scribo.h carries.
 * @note A write that does not fit a line with no flush leaves at at capacity, and every later write
 *       then finds no room. A line with a flush hands what it holds to the flush as it fills and
 *       goes on.
 * @note mmgr_line_finish stores the terminator and reports the length. Nothing before it terminates
 *       the buffer.
 */
#ifndef MMGR_VERBA_LINEA_H
#define MMGR_VERBA_LINEA_H

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Where a line hands the size bytes at bytes it holds once it is full.
 *
 * @param[in] context The line's context [BORROWS].
 * @param[in] bytes   What the line holds [BORROWS].
 * @param[in] size    Bytes at bytes.
 * @return            1 where the bytes went, MMGR_LINE_GROWN where the line was moved into a larger
 *                    room that holds them still, 0 where they did not go.
 */
typedef int (*VerbaLineaFlush)(void *context, const char *bytes, size_t size);

/**
 * @brief A flush's answer where it moved the line into a larger room, what the line held kept at the
 *        same offsets.
 */
#define MMGR_LINE_GROWN 2

/**
 * @brief A line written into out, at bytes so far, and handed to flush as it fills.
 *
 * @note A byte of out is reserved for the terminator. A write that did not fit leaves at at
 *       capacity.
 * @note flushed counts the bytes the line has handed on. A flush answering MMGR_LINE_GROWN hands
 *       nothing on and adds nothing to it.
 * @warning out must be writable for capacity bytes.
 */
typedef struct
{
    char *out;             /**< The line's buffer [BORROWS]. */
    size_t capacity;       /**< Bytes in out. */
    size_t at;             /**< Bytes written, and capacity once a write did not fit. */
    VerbaLineaFlush flush; /**< Where the line hands what it holds, or NULL for a line that fills. */
    void *context;         /**< Handed to flush with every call [BORROWS]. */
    size_t flushed;        /**< Bytes handed to flush so far. */
} VerbaLinea;

/**
 * @brief Hands what the line holds to its flush and empties it.
 *
 * @param[in,out] line The line [BORROWS].
 * @return             EMBED_TRUE where the flush took the bytes or grew the line, EMBED_FALSE where
 *                     the line has no flush, is full, or its flush refused.
 * @note A flush that refuses leaves the line full.
 */
embed_bool mmgr_line_flush(VerbaLinea *line);

/**
 * @brief Writes length bytes of text.
 *
 * @param[in,out] line   The line [BORROWS].
 * @param[in]     text   Bytes to write [BORROWS].
 * @param[in]     length Bytes of text.
 * @note A text past the room left is handed on in pieces where the line has a flush, and fills the
 *       line where it has none.
 * @note A line of fewer than two bytes holds none past its terminator, and so any text fills it.
 * @warning text must be readable for length bytes.
 */
void mmgr_line_put_n(VerbaLinea *line, const char *text, size_t length);

/**
 * @brief Writes a terminated text.
 *
 * @param[in,out] line The line [BORROWS].
 * @param[in]     text Terminated text to write [BORROWS].
 * @note The text is read a piece the size of the room left at a time, never past its terminator, and therefore a
 *       line with a flush takes a text of any length and needs no bound on it. A line with none is
 *       filled by a text longer than its room.
 */
void mmgr_line_put(VerbaLinea *line, const char *text);

/**
 * @brief Writes a terminated text, at most most bytes of it.
 *
 * @param[in,out] line The line [BORROWS].
 * @param[in]     text Text to write [BORROWS].
 * @param[in]     most The most bytes of text read.
 */
void mmgr_line_put_most(VerbaLinea *line, const char *text, size_t most);

/**
 * @brief Writes a terminated text and spaces after it out to columns.
 *
 * @param[in,out] line    The line [BORROWS].
 * @param[in]     text    Terminated text to write [BORROWS].
 * @param[in]     columns Bytes the text and its spaces fill. A longer text gets no spaces.
 * @note The text is read as mmgr_line_put reads it.
 */
void mmgr_line_put_columns(VerbaLinea *line, const char *text, unsigned int columns);

/**
 * @brief Writes one character.
 *
 * @param[in,out] line      The line [BORROWS].
 * @param[in]     character The character.
 */
void mmgr_line_ch(VerbaLinea *line, char character);

/**
 * @brief Writes value in base ten, padded on the left with zeros to digits_least digits.
 *
 * @param[in,out] line         The line [BORROWS].
 * @param[in]     value        The value.
 * @param[in]     digits_least Fewest digits written, clamped at 255.
 */
void mmgr_line_decimal(VerbaLinea *line, uint64_t value, unsigned int digits_least);

/**
 * @brief Writes value in base ten, right aligned in columns with spaces before it.
 *
 * @param[in,out] line    The line [BORROWS].
 * @param[in]     value   The value.
 * @param[in]     columns Bytes the value and its spaces fill, clamped at 255.
 */
void mmgr_line_decimal_columns(VerbaLinea *line, uint64_t value, unsigned int columns);

/**
 * @brief Writes a signed value in base ten, a minus sign before a negative one.
 *
 * @param[in,out] line  The line [BORROWS].
 * @param[in]     value The value. INT64_MIN is written whole.
 */
void mmgr_line_decimal_signed(VerbaLinea *line, int64_t value);

/**
 * @brief Writes value in lower case base sixteen, padded on the left with zeros to digits_least digits.
 *
 * @param[in,out] line         The line [BORROWS].
 * @param[in]     value        The value.
 * @param[in]     digits_least Fewest digits written, clamped at 255.
 */
void mmgr_line_hex(VerbaLinea *line, uint64_t value, unsigned int digits_least);

/**
 * @brief Stores the terminator.
 *
 * @param[in,out] line The line [BORROWS].
 * @return             The length the line holds, or 0 where it is full.
 */
size_t mmgr_line_finish(VerbaLinea *line);

/**
 * @brief Reports whether every write fit.
 *
 * @param[in] line The line [BORROWS].
 * @return         EMBED_TRUE while the line has room, EMBED_FALSE once a write did not fit.
 */
embed_bool mmgr_line_ok(const VerbaLinea *line);

EMBED_END_DECLS

#endif
