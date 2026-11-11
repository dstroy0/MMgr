/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file verba_scribo_fuzz.c
 * @brief A libFuzzer harness over the writers. Every text, character and integer writer runs at every offset and
 *        cap against a model. The model writes a piece whole while a byte stays for the terminator and answers
 *        the cap once one does not fit. The integers run against snprintf, and fixed against the value rounded
 *        half away from zero in a wide integer. The g writer runs against the value rounded to its digits as strtod reads
 *        both back. The three predicates run against the bits, finish and ok against the offset, and the line
 *        writer has its own section.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "verba_scribo/verba_linea.h"
#include "verba_scribo/verba_scribo.h"

#include "fuzz_input.h"
#include "fuzz_wide.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_CAP_MOST 96u
#define FUZZ_PAD 16u

static char s_out[FUZZ_CAP_MOST + FUZZ_PAD];
static char s_expected[FUZZ_CAP_MOST + FUZZ_PAD];

/** @brief The model's writer: a buffer, its cap and the offset reached. */
typedef struct
{
    size_t cap;
    size_t at;
} ModelWriter;

static int model_room(const ModelWriter *writer, size_t want)
{
    return (writer->at < writer->cap) && (want <= writer->cap - writer->at - 1u);
}

/** @brief A piece written whole while it fits, or the cap answered. */
static void model_piece(ModelWriter *writer, const char *piece, size_t bytes)
{
    if (!model_room(writer, bytes))
    {
        writer->at = writer->cap;
        return;
    }
    memcpy(s_expected + writer->at, piece, bytes);
    writer->at += bytes;
}

static void drawn_text(FuzzInput *input, char *text, size_t bytes)
{
    static const char alphabet[16] = {'a',  'Z',    '&',    '<',  '>', '"', '\\',   '\n',
                                      '\t', '\x01', '\x1f', '\'', ' ', '7', '\x7f', 'q'};
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        const uint8_t pick = fuzz_byte(input);
        text[at] = ((pick & 0x80u) != 0u) ? (char)(pick | 1u) : alphabet[pick & 15u];
    }
    text[bytes] = '\0';
}

static void compare_written(const ModelWriter *writer, size_t answered, const char *what)
{
    fuzz_check(answered == writer->at, what);
    if (writer->at < writer->cap)
    {
        fuzz_check(memcmp(s_out, s_expected, writer->at) == 0, what);
    }
}

static void texts(FuzzInput *input)
{
    char text[40 + FUZZ_PAD];
    memset(text, 'x', sizeof text);
    const size_t bytes = fuzz_below(input, 40u);
    drawn_text(input, text, bytes);
    const size_t cap = fuzz_below(input, FUZZ_CAP_MOST + 1u);
    const size_t at = fuzz_below(input, cap + 2u);
    ModelWriter writer = {cap, at};

    memset(s_out, '.', sizeof s_out);
    memcpy(s_expected, s_out, sizeof s_out);
    switch (fuzz_byte(input) % 6u)
    {
    case 0:
        model_piece(&writer, text, bytes);
        compare_written(&writer,
                        EMBED_CALL(verba_textus.put_n, VerbaTextusCfg, .out = s_out, .cap = cap, .at = at, .text = text,
                                   .text_len = bytes),
                        "put_n writes the run whole or answers the cap");
        break;
    case 1:
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = s_out, .cap = cap, .at = at, .text = text),
                        "put writes the string whole or answers the cap");
        break;
    case 2: {
        const size_t room = (at < cap) ? (cap - at - 1u) : 0u;
        const size_t fits = (strlen(text) < room) ? strlen(text) : room;
        if (at < cap)
        {
            memcpy(s_expected + at, text, fits);
        }
        writer.at = (at < cap) ? (at + fits) : at;
        compare_written(
            &writer,
            EMBED_CALL(verba_textus.put_clip, VerbaTextusCfg, .out = s_out, .cap = cap, .at = at, .text = text),
            "put_clip writes what fits and answers past it");
        break;
    }
    case 3:
        for (size_t step = 0u; step < bytes; step += 1u)
        {
            switch (text[step])
            {
            case '&':
                model_piece(&writer, "&amp;", 5u);
                break;
            case '<':
                model_piece(&writer, "&lt;", 4u);
                break;
            case '>':
                model_piece(&writer, "&gt;", 4u);
                break;
            case '"':
                model_piece(&writer, "&quot;", 6u);
                break;
            default:
                model_piece(&writer, text + step, 1u);
                break;
            }
        }
        compare_written(&writer,
                        EMBED_CALL(verba_textus.xml, VerbaTextusCfg, .out = s_out, .cap = cap, .at = at, .text = text),
                        "xml writes the four entities and every other byte as it stands");
        break;
    case 4:
        model_piece(&writer, "\"", 1u);
        for (size_t step = 0u; step < bytes; step += 1u)
        {
            const uint8_t byte = (uint8_t)text[step];
            char escape[8];
            if ((byte == '"') || (byte == '\\'))
            {
                escape[0] = '\\';
                escape[1] = (char)byte;
                model_piece(&writer, escape, 1u);
                model_piece(&writer, escape + 1u, 1u);
            }
            else if ((byte == 8u) || (byte == 9u) || (byte == 10u) || (byte == 12u) || (byte == 13u))
            {
                static const char letters[14] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r'};
                escape[0] = '\\';
                escape[1] = letters[byte];
                model_piece(&writer, escape, 1u);
                model_piece(&writer, escape + 1u, 1u);
            }
            else if (byte < 0x20u)
            {
                snprintf(escape, sizeof escape, "%02x", byte);
                model_piece(&writer, "\\u00", 4u);
                model_piece(&writer, escape, 1u);
                model_piece(&writer, escape + 1u, 1u);
            }
            else
            {
                model_piece(&writer, text + step, 1u);
            }
        }
        model_piece(&writer, "\"", 1u);
        compare_written(&writer,
                        EMBED_CALL(verba_textus.json, VerbaTextusCfg, .out = s_out, .cap = cap, .at = at, .text = text),
                        "json quotes the text and escapes what JSON escapes");
        break;
    default:
        model_piece(&writer, text, (bytes != 0u) ? 1u : 0u);
        if (bytes != 0u)
        {
            compare_written(
                &writer,
                EMBED_CALL(verba_littera.ch, VerbaLitteraCfg, .out = s_out, .cap = cap, .at = at, .ch = text[0]),
                "ch writes one byte or answers the cap");
        }
        break;
    }
    fuzz_check((cap == 0u) || (s_out[cap - 1u] == '.'), "no writer touches the last byte, which the terminator takes");
}

static void integers(FuzzInput *input)
{
    const uint64_t val = fuzz_edge_word(input);
    const int64_t sval = (int64_t)fuzz_edge_word(input);
    const uint8_t base = fuzz_byte(input);
    const uint8_t min = (uint8_t)(fuzz_byte(input) % 24u);
    const uint8_t columns = (uint8_t)(fuzz_byte(input) % 30u);
    const size_t cap = fuzz_below(input, FUZZ_CAP_MOST + 1u);
    const size_t at = fuzz_below(input, cap + 2u);
    char text[64];
    ModelWriter writer = {cap, at};

    memset(s_out, '.', sizeof s_out);
    memcpy(s_expected, s_out, sizeof s_out);
    switch (fuzz_byte(input) % 7u)
    {
    case 0:
        snprintf(text, sizeof text, (base == 8u) ? "%0*llo" : ((base == 16u) ? "%0*llx" : "%0*llu"),
                 (min == 0u) ? 1 : (int)min, (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .val = val,
                                   .base = base, .min = min),
                        "uint writes the value in its base padded to min digits");
        break;
    case 1:
        snprintf(text, sizeof text, "%0*llu", (min == 0u) ? 1 : (int)min, (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .val = val,
                                   .base = 16u, .min = min),
                        "u32w writes base ten padded to min digits");
        break;
    case 2:
        snprintf(text, sizeof text, "%0*llx", (min == 0u) ? 1 : (int)min, (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .val = val,
                                   .base = 8u, .min = min),
                        "hex writes lower case base sixteen padded to min digits");
        break;
    case 3:
        snprintf(text, sizeof text, "%llu", (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .val = val,
                                   .base = 16u, .min = 9u),
                        "u32 writes base ten unpadded");
        break;
    case 4:
        snprintf(text, sizeof text, "%llu", (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .val = val),
                        "u64 writes base ten unpadded");
        break;
    case 5:
        snprintf(text, sizeof text, "%lld", (long long)sval);
        model_piece(&writer, text, strlen(text));
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at, .sval = sval,
                                   .val = val),
                        "i64 writes the signed value in base ten");
        break;
    default:
        snprintf(text, sizeof text, "%*llu", (int)columns, (unsigned long long)val);
        model_piece(&writer, text, strlen(text));
        if (writer.at == cap)
        {
            writer.at = at;
        }
        compare_written(&writer,
                        EMBED_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = s_out, .cap = cap, .at = at,
                                   .val = val, .columns = columns),
                        "u64_clip writes the value right aligned, or answers where it began");
        break;
    }
}

static double drawn_real(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    switch (kind & 3u)
    {
    case 0: {
        const int64_t whole = (int64_t)(fuzz_word(input) % 2000001u) - 1000000;
        return (double)whole / (double)(1u + (fuzz_byte(input) % 200u));
    }
    case 1:
        return ldexp((double)(fuzz_word(input) >> 11), (int)(fuzz_byte(input) % 140u) - 120);
    default: {
        uint64_t bits = fuzz_edge_word(input);
        double real;
        memcpy(&real, &bits, sizeof real);
        return real;
    }
    }
}

/** @brief The text of @p real to @p decimals places, a half rounded away from zero, computed exactly. */
static void exact_fixed(double real, unsigned int decimals, char *text, size_t room)
{
    int exponent = 0;
    const double fraction = frexp(fabs(real), &exponent);
    const uint64_t mant = (uint64_t)ldexp(fraction, 53);
    const int shift = exponent - 53;
    Wide numerator = wide_of(mant);
    for (unsigned int step = 0u; step < decimals; step += 1u)
    {
        wide_times(&numerator, 10u);
    }
    Wide denominator = wide_of(1u);
    if (shift >= 0)
    {
        wide_shift_up(&numerator, (unsigned int)shift);
    }
    else
    {
        wide_shift_up(&denominator, (unsigned int)-shift);
    }
    wide_times(&numerator, 2u);
    wide_plus(&numerator, &denominator);
    wide_times(&denominator, 2u);
    Wide rounded = wide_quotient(&numerator, &denominator);

    char digits[400];
    size_t count = 0u;
    while (wide_bits(&rounded) != 0u)
    {
        Wide quotient = rounded;
        wide_over(&quotient, 10u);
        Wide back = quotient;
        wide_times(&back, 10u);
        Wide remainder = rounded;
        wide_minus(&remainder, &back);
        digits[count] = (char)('0' + remainder.limb[0]);
        count += 1u;
        rounded = quotient;
    }
    while (count <= decimals)
    {
        digits[count] = '0';
        count += 1u;
    }
    size_t at = 0u;
    // The sign goes ahead of the magnitude whatever the magnitude rounds to, as printf's %.*f writes it
    if (signbit(real))
    {
        text[at] = '-';
        at += 1u;
    }
    for (size_t step = count; step > 0u && at + 2u < room; step -= 1u)
    {
        if ((step == decimals) && (decimals != 0u))
        {
            text[at] = '.';
            at += 1u;
        }
        text[at] = digits[step - 1u];
        at += 1u;
    }
    text[at] = '\0';
}

static void reals(FuzzInput *input)
{
    const double real = drawn_real(input);
    uint64_t bits;
    memcpy(&bits, &real, sizeof bits);
    const int nan = ((bits >> 52) & 0x7FFu) == 0x7FFu && (bits & 0x000FFFFFFFFFFFFFull) != 0u;
    const int inf = ((bits >> 52) & 0x7FFu) == 0x7FFu && (bits & 0x000FFFFFFFFFFFFFull) == 0u;

    fuzz_check((EMBED_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = real) != 0) == ((bits >> 63) != 0u),
               "sign_bit reads the sign bit");
    fuzz_check((EMBED_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = real) != 0) == inf,
               "is_inf is an all-ones exponent, zero mantissa");
    fuzz_check((EMBED_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = real) != 0) == nan,
               "is_nan is an all-ones exponent, nonzero mantissa");

    memset(s_out, 0, sizeof s_out);
    if ((fuzz_byte(input) & 1u) == 0u)
    {
        const uint8_t decimals = (uint8_t)(fuzz_byte(input) % 22u);
        const unsigned int kept = (decimals > MMGR_FIXED_MAX_DECIMALS) ? MMGR_FIXED_MAX_DECIMALS : decimals;
        const size_t end = EMBED_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = s_out, .cap = FUZZ_CAP_MOST,
                                      .real = real, .decimals = decimals);
        if (nan || inf || (fabs(real) >= 18446744073709551616.0))
        {
            return;
        }
        char expected[FUZZ_CAP_MOST];
        exact_fixed(real, kept, expected, sizeof expected);
        if (strlen(expected) + 1u > FUZZ_CAP_MOST)
        {
            return;
        }
        s_out[(end < FUZZ_CAP_MOST) ? end : 0u] = '\0';
        if (strcmp(s_out, expected) != 0)
        {
            fprintf(stderr, "fixed %a to %u places wrote \"%s\", exact \"%s\"\n", real, kept, s_out, expected);
        }
        fuzz_check(strcmp(s_out, expected) == 0, "fixed is the value rounded half away from zero");
        return;
    }
    const uint8_t sig = (uint8_t)(fuzz_byte(input) % 22u);
    const unsigned int kept = (sig == 0u) ? 1u : ((sig > MMGR_G_MAX_SIG) ? MMGR_G_MAX_SIG : sig);
    const size_t end =
        EMBED_CALL(verba_fractio.g, VerbaFractioCfg, .out = s_out, .cap = FUZZ_CAP_MOST, .real = real, .sig = sig);
    fuzz_check(end < FUZZ_CAP_MOST, "g fits a double in the buffer");
    s_out[end] = '\0';
    if (nan)
    {
        fuzz_check(strstr(s_out, "nan") != NULL, "g writes nan for a NaN");
        return;
    }
    char rounded[64];
    snprintf(rounded, sizeof rounded, "%.*e", (int)kept - 1, real);
    const double read_back = strtod(s_out, NULL);
    double expected = strtod(rounded, NULL);
    // A value exactly on a half at the digit kept may round either way, as the header states no rule for g.
    // The value one step away from zero rounds the half away, and either answer is the value rounded to its digits
    char exact[96];
    snprintf(exact, sizeof exact, "%.*e", (int)kept + 40, real);
    const char *const beyond = strchr(exact, '.') != NULL ? strchr(exact, '.') + kept : NULL;
    if ((beyond != NULL) && (beyond[0] == '5') && (strspn(beyond + 1, "0") == strcspn(beyond + 1, "e")))
    {
        char away[64];
        snprintf(away, sizeof away, "%.*e", (int)kept - 1, nextafter(real, copysign(INFINITY, real)));
        const double away_value = strtod(away, NULL);
        if (memcmp(&read_back, &away_value, sizeof read_back) == 0)
        {
            expected = away_value;
        }
    }
    if (memcmp(&read_back, &expected, sizeof read_back) != 0)
    {
        fprintf(stderr, "g %a to %u digits wrote \"%s\", the rounded value is \"%s\"\n", real, kept, s_out, rounded);
    }
    fuzz_check(memcmp(&read_back, &expected, sizeof read_back) == 0, "g reads back as the value rounded to its digits");
}

static void ends(FuzzInput *input)
{
    const size_t cap = fuzz_below(input, FUZZ_CAP_MOST + 1u);
    const size_t at = fuzz_below(input, cap + 2u);
    memset(s_out, '.', sizeof s_out);
    const size_t length = EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = s_out, .cap = cap, .at = at);
    fuzz_check(length == ((at < cap) ? at : 0u), "finish answers the offset, or 0 past the cap");
    fuzz_check((at < cap) ? (s_out[at] == '\0') : (s_out[at] == '.'),
               "finish stores the terminator only within the cap");
    fuzz_check((EMBED_CALL(verba_finis.ok, VerbaFinisCfg, .cap = cap, .at = at) != 0) == (at < cap),
               "ok is the offset below the cap");
}

/** @brief Where a line's flush hands its bytes: what it took, and how many more it takes before it refuses. */
typedef struct
{
    char taken[1024];
    size_t bytes;
    size_t budget;
} LineSink;

static int line_sink(void *context, const char *bytes, size_t size)
{
    LineSink *const sink = (LineSink *)context;
    if ((size > sink->budget) || (sink->bytes + size > sizeof sink->taken))
    {
        return 0;
    }
    memcpy(sink->taken + sink->bytes, bytes, size);
    sink->bytes += size;
    sink->budget -= size;
    return 1;
}

/**
 * @brief Any sequence of line writes against the stream of what each write formats. With a flush that takes
 *        everything, the bytes handed on and the bytes the line holds are the stream. With no flush, or one that
 *        refuses, they are the stream as far as the last write that went whole. A line that ran out of room
 *        stands full.
 */
static void verba_linea_fuzzed(FuzzInput *input)
{
    static char stream[2048];
    static char buffer[FUZZ_CAP_MOST + FUZZ_PAD];
    static LineSink sink;
    const size_t capacity = fuzz_below(input, 40u);
    const uint8_t mode = (uint8_t)(fuzz_byte(input) % 3u);
    size_t streamed = 0u;
    size_t whole = 0u;
    int lost = 0;

    memset(buffer, '.', sizeof buffer);
    sink.bytes = 0u;
    sink.budget = (mode == 2u) ? fuzz_below(input, 200u) : SIZE_MAX;
    VerbaLinea line = mmgr_line_over((fuzz_byte(input) & 15u) == 0u ? NULL : buffer, capacity);
    const int refused_take = line.out == NULL;
    if (mode != 0u)
    {
        line.flush = line_sink;
        line.context = &sink;
    }

    const unsigned int count = fuzz_byte(input) % 16u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        char piece[320];
        char text[40];
        size_t piece_bytes = 0u;
        size_t took = SIZE_MAX;
        const uint64_t value = fuzz_edge_word(input);
        const unsigned int least = fuzz_byte(input) % 30u;
        const size_t text_bytes = fuzz_below(input, sizeof text);
        drawn_text(input, text, text_bytes);
        switch (fuzz_byte(input) % 9u)
        {
        case 0:
            took = mmgr_line_put_n(&line, text, text_bytes);
            memcpy(piece, text, text_bytes);
            piece_bytes = text_bytes;
            break;
        case 1:
            took = mmgr_line_put(&line, text);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%s", text);
            break;
        case 2: {
            const size_t most = fuzz_below(input, sizeof text);
            took = mmgr_line_put_most(&line, text, most);
            piece_bytes = (strlen(text) < most) ? strlen(text) : most;
            memcpy(piece, text, piece_bytes);
            break;
        }
        case 3:
            mmgr_line_put_columns(&line, text, least);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%-*s", (int)least, text);
            break;
        case 4:
            mmgr_line_ch(&line, (char)(text_bytes != 0u ? text[0] : 'k'));
            piece[0] = (char)(text_bytes != 0u ? text[0] : 'k');
            piece_bytes = 1u;
            break;
        case 5:
            mmgr_line_decimal(&line, value, least);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%0*llu", (least == 0u) ? 1 : (int)least,
                                           (unsigned long long)value);
            break;
        case 6:
            mmgr_line_decimal_columns(&line, value, least);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%*llu", (int)least, (unsigned long long)value);
            break;
        case 7:
            mmgr_line_decimal_signed(&line, (int64_t)value);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%lld", (long long)(int64_t)value);
            break;
        default:
            mmgr_line_hex(&line, value, least);
            piece_bytes = (size_t)snprintf(piece, sizeof piece, "%0*llx", (least == 0u) ? 1 : (int)least,
                                           (unsigned long long)value);
            break;
        }
        memcpy(stream + streamed, piece, piece_bytes);
        streamed += piece_bytes;
        if (!lost && (sink.bytes + ((line.at < line.capacity) ? line.at : 0u) == streamed) && mmgr_line_ok(&line))
        {
            whole = streamed;
        }
        else
        {
            lost = 1;
        }
        if ((mode == 1u) && !refused_take && (capacity >= 2u))
        {
            fuzz_check(mmgr_line_ok(&line) != 0, "a line that flushes everything never runs out of room");
            fuzz_check((took == SIZE_MAX) || (took == piece_bytes), "a line that flushes everything takes every byte");
        }
        if (streamed > sizeof stream - 400u)
        {
            break;
        }
    }

    const size_t held = (line.at < line.capacity) ? line.at : ((line.capacity != 0u) ? line.capacity - 1u : 0u);
    if (!refused_take && (held != 0u))
    {
        fuzz_check(sink.bytes + held <= streamed, "a line holds no more than was written");
    }
    fuzz_check(memcmp(sink.taken, stream, (sink.bytes < whole) ? sink.bytes : whole) == 0,
               "the bytes handed on are the stream in order");
    if (!refused_take && (sink.bytes < whole))
    {
        fuzz_check(memcmp(buffer, stream + sink.bytes, whole - sink.bytes) == 0,
                   "the line holds the rest of the stream");
    }
    const size_t length = mmgr_line_finish(&line);
    if (mmgr_line_ok(&line))
    {
        fuzz_check(!refused_take || (capacity == 0u), "a line over a refused take is never ok");
        fuzz_check(length == line.at, "finish answers the length a line holds");
        if (!refused_take && (line.capacity != 0u))
        {
            fuzz_check(buffer[line.at] == '\0', "finish terminates the line");
        }
    }
    else
    {
        fuzz_check(length == 0u, "finish answers 0 for a line that ran out of room");
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        switch (fuzz_byte(&input) % 5u)
        {
        case 0:
            texts(&input);
            break;
        case 1:
            integers(&input);
            break;
        case 2:
            reals(&input);
            break;
        case 3:
            ends(&input);
            break;
        default:
            verba_linea_fuzzed(&input);
            break;
        }
    }
    return 0;
}
