/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file cellularum_laboro_fuzz.c
 * @brief A libFuzzer harness over the string walks. The walks len, diff, eq, starts, find, has, chr, copy, ws, digit
 *        and the two step compares run on text of any bytes. The text has any case and terminator place, at every
 *        offset and cap. Each runs
 *        against a byte loop of what the header states. The four conversions run against a byte loop for the
 *        integers and the C library's strtod for the floating values. Every buffer extends past its cap by the
 *        word the header allows the walks to read there.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "cellularum_laboro/cellularum_laboro.h"

#include "fuzz_input.h"

#include <stdlib.h>
#include <string.h>

#define FUZZ_BYTES 96u
#define FUZZ_PAD 16u

static char s_src[FUZZ_BYTES + FUZZ_PAD];
static char s_other[FUZZ_BYTES + FUZZ_PAD];
static char s_dst[FUZZ_BYTES + FUZZ_PAD];

static uint8_t folded(uint8_t byte, int ci)
{
    return (ci && (byte >= 'A') && (byte <= 'Z')) ? (uint8_t)(byte + 32u) : byte;
}

/** @brief A text of @p bytes drawn from a small alphabet: matches and case pairs are common. */
static void drawn_text(FuzzInput *input, char *text, size_t bytes)
{
    static const char alphabet[16] = {'a', 'b', 'A', 'B', 'c', 'C', '0', '9', ' ', '\t', '-', '+', '.', 'e', '\0', 'z'};
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        const uint8_t pick = fuzz_byte(input);
        text[at] = ((pick & 0x80u) != 0u) ? (char)pick : alphabet[pick & 15u];
    }
}

static size_t bounded_len(const char *text, size_t cap)
{
    size_t at = 0u;
    while ((at < cap) && (text[at] != '\0'))
    {
        at += 1u;
    }
    return at;
}

/** @brief The agreement walk eq and starts share: left's terminator before the first difference. */
static int agreed(const char *left, const char *right, size_t cap, int ci, int end_wins)
{
    for (size_t at = 0u; at < cap; at += 1u)
    {
        const uint8_t one = (uint8_t)left[at];
        const uint8_t two = (uint8_t)right[at];
        if (folded(one, ci) != folded(two, ci))
        {
            return (one == 0u) && end_wins;
        }
        if (one == 0u)
        {
            return 1;
        }
    }
    return end_wins;
}

static int step_of(uint8_t left, uint8_t right, int ci, int end_wins)
{
    if (folded(left, ci) != folded(right, ci))
    {
        return ((left == 0u) && end_wins) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (left == 0u) ? MMGR_SWAR_YES : MMGR_SWAR_GO;
}

static void walks(FuzzInput *input)
{
    const size_t cap = fuzz_below(input, FUZZ_BYTES + 1u);
    const int ci = (fuzz_byte(input) & 1u) != 0u;

    drawn_text(input, s_src, FUZZ_BYTES + FUZZ_PAD);
    memcpy(s_other, s_src, sizeof s_other);
    drawn_text(input, s_other + fuzz_below(input, FUZZ_BYTES), fuzz_below(input, 4u));
    for (size_t at = 0u; at < FUZZ_BYTES; at += 1u)
    {
        if ((fuzz_byte(input) & 7u) == 0u)
        {
            s_other[at] = (char)(((s_other[at] >= 'a') && (s_other[at] <= 'z')) ? (s_other[at] - 32) : s_other[at]);
        }
    }

    const size_t at = fuzz_below(input, cap + 1u);
    fuzz_check(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_src, .cap = cap, .at = at) ==
                   bounded_len(s_src + at, cap - at),
               "len counts the bytes before the terminator within the cap");

    size_t differs = cap;
    for (size_t step = 0u; step < cap; step += 1u)
    {
        if (folded((uint8_t)s_src[step], ci) != folded((uint8_t)s_other[step], ci))
        {
            differs = step;
            break;
        }
    }
    fuzz_check(EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = s_src, .other = s_other, .cap = cap,
                          .ci = (embed_bool)ci) == differs,
               "diff answers the first differing offset");
    fuzz_check((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_src, .other = s_other, .cap = cap,
                           .ci = (embed_bool)ci) != 0) == agreed(s_src, s_other, cap, ci, 0),
               "eq is both ending together with no difference");
    fuzz_check((EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_src, .other = s_other, .cap = cap,
                           .ci = (embed_bool)ci) != 0) == agreed(s_other, s_src, cap, ci, 1),
               "starts is other's terminator before the first difference");

    const uint8_t byte = fuzz_byte(input);
    const size_t hay = bounded_len(s_src, cap);
    const char *expected_chr = NULL;
    if (byte == 0u)
    {
        expected_chr = s_src + hay;
    }
    else
    {
        for (size_t step = 0u; step < hay; step += 1u)
        {
            if ((uint8_t)s_src[step] == byte)
            {
                expected_chr = s_src + step;
                break;
            }
        }
    }
    fuzz_check(EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = s_src, .cap = cap, .byte = byte) == expected_chr,
               "chr finds the first byte before the terminator");

    const size_t write_cap = fuzz_below(input, FUZZ_BYTES + 1u);
    memset(s_dst, 0x5A, sizeof s_dst);
    const size_t copied = EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = s_src, .dst = s_dst, .cap = write_cap);
    if (write_cap == 0u)
    {
        fuzz_check((copied == 0u) && ((uint8_t)s_dst[0] == 0x5Au), "a copy of cap 0 writes nothing");
    }
    else
    {
        const size_t expected = bounded_len(s_src, write_cap - 1u);
        fuzz_check(copied == expected, "copy counts the bytes it copied");
        fuzz_check((memcmp(s_dst, s_src, expected) == 0) && (s_dst[expected] == '\0'), "copy copies and terminates");
        fuzz_check((uint8_t)s_dst[expected + 1u] == 0x5Au, "copy writes nothing past its terminator");
    }

    const char probe = s_src[at];
    fuzz_check((EMBED_CALL(cellul.ws, CatenaFinitaCfg, .src = s_src, .at = at) != 0) ==
                   ((probe == ' ') || ((probe >= 9) && (probe <= 13))),
               "ws is the six whitespace bytes");
    fuzz_check((EMBED_CALL(cellul.digit, CatenaFinitaCfg, .src = s_src, .at = at) != 0) ==
                   ((probe >= '0') && (probe <= '9')),
               "digit is the ten decimal digits");
}

static void finds(FuzzInput *input)
{
    const size_t cap = fuzz_below(input, FUZZ_BYTES + 1u);
    const int ci = (fuzz_byte(input) & 1u) != 0u;
    const size_t needle_cap = fuzz_below(input, 24u);
    const int stated_len = (fuzz_byte(input) & 1u) != 0u;

    drawn_text(input, s_src, FUZZ_BYTES + FUZZ_PAD);
    memset(s_other, 0, sizeof s_other);
    if (((fuzz_byte(input) & 1u) != 0u) && (cap > 0u))
    {
        const size_t from = fuzz_below(input, cap);
        const size_t span = fuzz_below(input, needle_cap + 1u);
        for (size_t step = 0u; (step < span) && (from + step < FUZZ_BYTES); step += 1u)
        {
            s_other[step] = s_src[from + step];
        }
    }
    else
    {
        drawn_text(input, s_other, needle_cap);
    }
    size_t needle_len = bounded_len(s_other, needle_cap);
    const size_t told = stated_len ? needle_len : 0u;

    const size_t hay = bounded_len(s_src, cap);
    const char *expected = NULL;
    if (needle_len == 0u)
    {
        expected = s_src;
    }
    else
    {
        for (size_t start = 0u; (start + needle_len <= hay) && (expected == NULL); start += 1u)
        {
            size_t step = 0u;
            while ((step < needle_len) &&
                   (folded((uint8_t)s_src[start + step], ci) == folded((uint8_t)s_other[step], ci)))
            {
                step += 1u;
            }
            if (step == needle_len)
            {
                expected = s_src + start;
            }
        }
    }
    const char *const found = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = s_src, .cap = cap, .other = s_other,
                                         .other_cap = needle_cap, .other_len = told, .ci = (embed_bool)ci);
    fuzz_check(found == expected, "find answers the first match before the haystack's terminator");
    fuzz_check((EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = s_src, .cap = cap, .other = s_other,
                           .other_cap = needle_cap, .other_len = told, .ci = (embed_bool)ci) != 0) ==
                   (expected != NULL),
               "has is whether find matches");
}

static void steps(FuzzInput *input)
{
    const int ci = (fuzz_byte(input) & 1u) != 0u;
    const int end_wins = (fuzz_byte(input) & 1u) != 0u;
    uint8_t left[sizeof(embed_word)];
    uint8_t right[sizeof(embed_word)];
    drawn_text(input, (char *)left, sizeof left);
    memcpy(right, left, sizeof right);
    drawn_text(input, (char *)right + fuzz_below(input, sizeof right), 1u);

    fuzz_check(EMBED_CALL(cellul.step_byte, VerboProgrediorCfg, .byte_left = left[0], .byte_right = right[0],
                          .ci = (embed_bool)ci,
                          .end_wins = (embed_bool)end_wins) == step_of(left[0], right[0], ci, end_wins),
               "step_byte answers go, yes or no as the byte walk does");

    embed_word word_left;
    embed_word word_right;
    memcpy(&word_left, left, sizeof word_left);
    memcpy(&word_right, right, sizeof word_right);
    int expected = MMGR_SWAR_GO;
    for (size_t at_lane = 0u; (at_lane < sizeof left) && (expected == MMGR_SWAR_GO); at_lane += 1u)
    {
        expected = step_of(left[at_lane], right[at_lane], ci, end_wins);
    }
    fuzz_check(EMBED_CALL(cellul.step_word, VerboProgrediorCfg, .word_left = word_left, .word_right = word_right,
                          .ci = (embed_bool)ci, .end_wins = (embed_bool)end_wins) == expected,
               "step_word answers what the first lane that is not go answers");
}

static void conversions(FuzzInput *input)
{
    char text[48];
    const size_t bytes = fuzz_below(input, sizeof text);
    // Decimal text only: strtod also reads a hexadecimal float, which the conversions do not, and so no x comes out
    static const char alphabet[16] = {'0', '1', '5', '9', '0', '7', ' ', '\t', '-', '+', '.', 'e', 'E', ',', '3', '8'};
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        text[at] = alphabet[fuzz_byte(input) & 15u];
    }
    text[bytes] = '\0';

    size_t at = 0u;
    while ((text[at] == ' ') || ((text[at] >= 9) && (text[at] <= 13)))
    {
        at += 1u;
    }
    const size_t signed_at = at;
    int negative = 0;
    if ((text[at] == '+') || (text[at] == '-'))
    {
        negative = text[at] == '-';
        at += 1u;
    }
    embed_word accumulated = 0u;
    size_t digits_end = at;
    while ((text[digits_end] >= '0') && (text[digits_end] <= '9'))
    {
        accumulated = accumulated * 10u + (embed_word)(text[digits_end] - '0');
        digits_end += 1u;
    }
    const int any = digits_end != at;

    const char *end = NULL;
    const embed_iword value = EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = text, .end = &end);
    fuzz_check(value == (embed_iword)(negative ? (embed_word)(0u - accumulated) : accumulated),
               "to_long accumulates the digits and negates on a minus");
    fuzz_check(end == (any ? text + digits_end : text), "to_long ends past the digits, or at the start without one");

    size_t unsigned_at = signed_at + ((text[signed_at] == '+') ? 1u : 0u);
    embed_word unsigned_value = 0u;
    size_t unsigned_end = unsigned_at;
    while ((text[unsigned_end] >= '0') && (text[unsigned_end] <= '9'))
    {
        unsigned_value = unsigned_value * 10u + (embed_word)(text[unsigned_end] - '0');
        unsigned_end += 1u;
    }
    end = NULL;
    fuzz_check(EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = text, .end = &end) == unsigned_value,
               "to_ulong accumulates the digits after an optional plus");
    fuzz_check(end == ((unsigned_end != unsigned_at) ? text + unsigned_end : text),
               "to_ulong ends past the digits, or at the start without one");

    char *library_end = NULL;
    const double library = strtod(text, &library_end);
    end = NULL;
    const double converted = EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = text, .end = &end);
    if (memcmp(&converted, &library, sizeof converted) != 0)
    {
        fprintf(stderr, "to_double \"%s\" gave %a, strtod %a\n", text, converted, library);
    }
    fuzz_check(memcmp(&converted, &library, sizeof converted) == 0, "to_double is strtod's value");
    if (end != library_end)
    {
        fprintf(stderr, "to_double \"%s\" ended at %d, strtod at %d\n", text, (int)(end - text),
                (int)(library_end - text));
    }
    fuzz_check(end == library_end, "to_double ends where strtod does");
    const float narrowed = (float)library;
    const float converted_float = EMBED_CALL(cellul.to_float, TransfiguroCfg, .src = text);
    fuzz_check(memcmp(&converted_float, &narrowed, sizeof narrowed) == 0, "to_float is strtod's value narrowed");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        switch (fuzz_byte(&input) % 4u)
        {
        case 0:
            walks(&input);
            break;
        case 1:
            finds(&input);
            break;
        case 2:
            steps(&input);
            break;
        default:
            conversions(&input);
            break;
        }
    }
    return 0;
}
