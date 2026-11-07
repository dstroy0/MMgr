/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file numeros_scribo_fuzz.c
 * @brief A libFuzzer harness over the spec writer: any spec of literals and fields with any values, against the
 *        same text written field by field through the verba writer each kind names, with its default width where
 *        none is given; a value missing, a kind that differs or a value left over answering 0 with the text before
 *        the write left whole; the appends against the same write after the text already there; and emit against
 *        the values written with their own widths.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "numeros_scribo/numeros_scribo.h"
#include "verba_scribo/verba_scribo.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_CAP_MOST 160u
#define FUZZ_FIELDS_MOST 12u
#define FUZZ_PAD 16u

static char s_out[FUZZ_CAP_MOST + FUZZ_PAD];
static char s_expected[FUZZ_CAP_MOST + FUZZ_PAD];
static char s_texts[FUZZ_FIELDS_MOST][24];
static char s_literals[FUZZ_FIELDS_MOST][16];

/** @brief The width a kind takes when its field or value gives none. */
static uint8_t default_width(mmgr_fk kind)
{
    switch (kind)
    {
    case MMGR_FK_U32:
    case MMGR_FK_U64:
    case MMGR_FK_I64:
    case MMGR_FK_HEX:
    case MMGR_FK_OCT:
        return 1u;
    case MMGR_FK_G:
        return 6u;
    default:
        return 0u;
    }
}

/** @brief One value written at @p at through the verba writer its kind names, at @p width. */
static size_t written_one(const mmgr_fval *value, uint8_t width, size_t cap, size_t at)
{
    const uint8_t taken = (width != 0u) ? width : default_width(value->kind);
    switch (value->kind)
    {
    case MMGR_FK_STR:
        return EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = s_expected, .cap = cap, .at = at,
                          .text = value->as.text);
    case MMGR_FK_U32:
        return EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .val = value->as.u32);
    case MMGR_FK_U64:
        return EMBED_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .val = value->as.u64);
    case MMGR_FK_I64:
        return EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .sval = value->as.i64);
    case MMGR_FK_DEC:
        return EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .val = value->as.u32, .min = taken);
    case MMGR_FK_HEX:
        return EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .val = value->as.u64, .min = taken);
    case MMGR_FK_OCT:
        return EMBED_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = s_expected, .cap = cap, .at = at,
                          .val = value->as.u64, .base = 8u, .min = taken);
    case MMGR_FK_G:
        return EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = s_expected, .cap = cap, .at = at,
                          .real = value->as.real, .sig = taken);
    case MMGR_FK_FIX:
        return EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = s_expected, .cap = cap, .at = at,
                          .real = value->as.real, .decimals = taken);
    case MMGR_FK_CH:
        return EMBED_CALL(verba_littera.ch, VerbaLitteraCfg, .out = s_expected, .cap = cap, .at = at,
                          .ch = value->as.character);
    case MMGR_FK_JSON:
        return EMBED_CALL(verba_textus.json, VerbaTextusCfg, .out = s_expected, .cap = cap, .at = at,
                          .text = value->as.text);
    case MMGR_FK_XML:
        return EMBED_CALL(verba_textus.xml, VerbaTextusCfg, .out = s_expected, .cap = cap, .at = at,
                          .text = value->as.text);
    default:
        return cap;
    }
}

static mmgr_fval drawn_value(FuzzInput *input, mmgr_fk kind, unsigned int slot)
{
    mmgr_fval value;
    memset(&value, 0, sizeof value);
    value.kind = kind;
    const uint64_t bits = fuzz_edge_word(input);
    switch (kind)
    {
    case MMGR_FK_STR:
    case MMGR_FK_JSON:
    case MMGR_FK_XML: {
        static const char alphabet[8] = {'a', '&', '"', '<', '\n', 'Q', '\\', ' '};
        const size_t bytes = fuzz_below(input, sizeof s_texts[0]);
        for (size_t at = 0u; at < bytes; at += 1u)
        {
            s_texts[slot][at] = alphabet[fuzz_byte(input) & 7u];
        }
        s_texts[slot][bytes] = '\0';
        value.as.text = s_texts[slot];
        break;
    }
    case MMGR_FK_U32:
    case MMGR_FK_DEC:
        value.as.u32 = (uint32_t)bits;
        break;
    case MMGR_FK_I64:
        value.as.i64 = (int64_t)bits;
        break;
    case MMGR_FK_G:
    case MMGR_FK_FIX:
        value.as.real = (double)(int64_t)(bits % 2000001u) / 64.0 - 15625.0;
        break;
    case MMGR_FK_CH:
        value.as.character = (char)(bits | 1u);
        break;
    default:
        value.as.u64 = bits;
        break;
    }
    value.width = (uint8_t)(fuzz_byte(input) % 12u);
    return value;
}

static mmgr_fk drawn_kind(FuzzInput *input)
{
    return (mmgr_fk)(MMGR_FK_STR + (fuzz_byte(input) % (MMGR_FK_XML - MMGR_FK_STR + 1u)));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        mmgr_field spec[FUZZ_FIELDS_MOST + 1u];
        mmgr_fval vals[FUZZ_FIELDS_MOST + 2u];
        const unsigned int fields = fuzz_byte(&input) % FUZZ_FIELDS_MOST;
        unsigned int values = 0u;

        for (unsigned int each = 0u; each < fields; each += 1u)
        {
            if ((fuzz_byte(&input) & 3u) == 0u)
            {
                const size_t bytes = fuzz_below(&input, sizeof s_literals[0]);
                for (size_t at = 0u; at < bytes; at += 1u)
                {
                    s_literals[each][at] = (char)('A' + (fuzz_byte(&input) % 26u));
                }
                spec[each] = (mmgr_field){MMGR_FK_LIT, 0u, (uint16_t)bytes, s_literals[each]};
            }
            else
            {
                const mmgr_fk kind = drawn_kind(&input);
                spec[each] = (mmgr_field){kind, (uint8_t)(fuzz_byte(&input) % 12u), 0u, NULL};
                vals[values] = drawn_value(&input, kind, each);
                values += 1u;
            }
        }
        spec[fields] = (mmgr_field){MMGR_FK_END, 0u, 0u, NULL};

        const size_t cap = fuzz_below(&input, FUZZ_CAP_MOST + 1u);
        const size_t start = (cap == 0u) ? 0u : fuzz_below(&input, cap);
        const uint8_t shape = fuzz_byte(&input);
        size_t handed = values;
        int matched = 1;
        if (((shape & 7u) == 1u) && (values != 0u))
        {
            handed = values - 1u;
            matched = 0;
        }
        else if ((shape & 7u) == 2u)
        {
            vals[values] = drawn_value(&input, drawn_kind(&input), FUZZ_FIELDS_MOST - 1u);
            handed = values + 1u;
            matched = 0;
        }
        else if (((shape & 7u) == 3u) && (values != 0u))
        {
            const unsigned int which = (unsigned int)fuzz_below(&input, values);
            const mmgr_fk other = drawn_kind(&input);
            if (other != vals[which].kind)
            {
                vals[which].kind = other;
                matched = 0;
            }
        }

        memset(s_out, 'p', sizeof s_out);
        memcpy(s_expected, s_out, sizeof s_out);
        for (size_t at = 0u; at < start; at += 1u)
        {
            s_out[at] = (char)('a' + (at % 26u));
            s_expected[at] = s_out[at];
        }
        if (cap != 0u)
        {
            s_out[start] = '\0';
            s_expected[start] = '\0';
        }
        const int appending = (shape & 8u) != 0u;
        const int threaded = (shape & 16u) != 0u;
        const size_t told = (appending && !threaded) ? 0u : start;

        size_t answered;
        if (appending)
        {
            answered = EMBED_CALL(numer.append, NumerosCfg, .out = s_out, .cap = cap, .at = told, .spec = spec,
                                  .vals = vals, .nvals = handed);
        }
        else
        {
            answered = EMBED_CALL(numer.build, NumerosCfg, .out = s_out, .cap = cap, .at = start, .spec = spec,
                                  .vals = vals, .nvals = handed);
        }

        if (cap == 0u)
        {
            fuzz_check(answered == 0u, "a write into no room answers 0");
            continue;
        }
        if (!matched)
        {
            fuzz_check(answered == 0u, "a spec and values that do not match answer 0");
            fuzz_check((s_out[start] == '\0') && (memcmp(s_out, s_expected, start) == 0),
                       "a refused write leaves the text before it whole");
            continue;
        }
        size_t at = start;
        unsigned int next = 0u;
        for (unsigned int each = 0u; each < fields; each += 1u)
        {
            if (spec[each].kind == MMGR_FK_LIT)
            {
                at = EMBED_CALL(verba_textus.put_n, VerbaTextusCfg, .out = s_expected, .cap = cap, .at = at,
                                .text = spec[each].literal, .text_len = spec[each].bytes);
            }
            else
            {
                at = written_one(&vals[next], spec[each].width, cap, at);
                next += 1u;
            }
        }
        const size_t finished = EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = s_expected, .cap = cap, .at = at);
        const size_t expected = (finished == 0u) ? 0u : finished;
        fuzz_check(answered == expected, "build answers the length the verba writers reach");
        if (expected != 0u)
        {
            fuzz_check(memcmp(s_out, s_expected, expected + 1u) == 0, "build writes what the verba writers write");
        }
        else
        {
            fuzz_check(s_out[start] == '\0', "a write that does not fit is terminated where it began");
        }

        if (!appending && (fields == values))
        {
            memset(s_out, 'p', sizeof s_out);
            const size_t emitted =
                EMBED_CALL(numer.emit, NumerosCfg, .out = s_out, .cap = cap, .at = 0u, .vals = vals, .nvals = values);
            at = 0u;
            for (unsigned int each = 0u; each < values; each += 1u)
            {
                at = written_one(&vals[each], vals[each].width, cap, at);
            }
            const size_t emit_finished =
                EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = s_expected, .cap = cap, .at = at);
            fuzz_check(emitted == emit_finished,
                       "emit answers the length the verba writers reach with each value's width");
            if (emit_finished != 0u)
            {
                fuzz_check(memcmp(s_out, s_expected, emit_finished + 1u) == 0,
                           "emit writes what the verba writers write");
            }
        }
    }
    return 0;
}
