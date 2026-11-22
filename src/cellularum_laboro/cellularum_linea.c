/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file cellularum_linea.c
 * @brief The calls of cellul with plain arguments. Each is a compound literal of the members that its call reads.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note Each entry is one call of cellul, and it does no other work. The members that the entry does not name are zero,
 * and the call that the entry makes does not read them. A zero ci is the compare with no fold.
 */
#include "cellularum_laboro/cellularum_linea.h"
#include "cellularum_laboro/cellularum_laboro.h"

size_t mmgr_text_length(const char *text, size_t cap)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = cap);
}

embed_bool mmgr_text_equal(const char *left, const char *right, size_t cap)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = left, .other = right, .cap = cap, .ci = EMBED_FALSE);
}

int mmgr_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    // the shorter text and its terminator, or cap where neither ends inside it
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .other = right, .cap = reach, .ci = EMBED_FALSE);

    if (differs >= reach)
    {
        return 0;
    }
    // Explicit casts read each byte as unsigned, as strcmp orders them, and widen it to int
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

embed_bool mmgr_text_starts(const char *text, const char *prefix, size_t cap)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .other = prefix, .cap = cap, .ci = EMBED_FALSE);
}

const char *mmgr_text_char(const char *text, uint8_t value, size_t cap)
{
    return EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = cap, .byte = value);
}

const char *mmgr_text_find(const char *haystack, const char *needle, size_t cap)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = haystack, .cap = cap, .other = needle, .other_cap = cap,
                      .ci = EMBED_FALSE);
}

size_t mmgr_text_copy(char *dst, const char *src, size_t cap)
{
    return EMBED_CALL(cellul.copy, CatenaFinitaCfg, .dst = dst, .src = src, .cap = cap);
}

embed_word mmgr_text_unsigned(const char *text, const char **end)
{
    return EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = text, .end = end);
}

embed_word mmgr_text_unsigned_in(const char *text, const char **end, unsigned int base)
{
    return EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = text, .end = end, .base = base);
}

size_t mmgr_text_span(const char *text, const char *set, size_t cap)
{
    return EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = cap, .other = set);
}

size_t mmgr_text_span_not(const char *text, const char *set, size_t cap)
{
    return EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = cap, .other = set);
}

const char *mmgr_text_char_last(const char *text, uint8_t value, size_t cap)
{
    return EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = text, .cap = cap, .byte = value);
}

embed_bool mmgr_text_class(uint8_t value, MmgrAsciiClass type)
{
    return EMBED_CALL(ascii.in, AsciiCfg, .type = type, .byte = value);
}

uint8_t mmgr_text_lower(uint8_t value)
{
    return EMBED_CALL(ascii.lower, AsciiCfg, .byte = value);
}

uint8_t mmgr_text_upper(uint8_t value)
{
    return EMBED_CALL(ascii.upper, AsciiCfg, .byte = value);
}
