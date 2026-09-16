#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "device_bench.h"

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Threshold the two lane comparison arms are given, read out of a volatile.
 *
 * @note Volatile so neither arm can fold its broadcast into a constant. A folded broadcast measures
 *       a multiply that the shipping code performs and the candidate would too.
 */
static volatile uint8_t g_scrut_threshold = 0x7Au;

/**
 * @brief Word the letter test arms are given, read out of a volatile.
 *
 * @note Volatile so the whole test cannot be hoisted out of the timing loop. Both arms take the word
 *       and nothing else, and a loop invariant one would let the optimiser compute each result once
 *       and measure an empty loop.
 * @note The lanes alternate a letter with a byte at or above 0x80, which is the shape the two arms
 *       disagree about.
 */
static volatile embed_word g_scrut_word = (embed_word)0xFF7AFF7Aul;

/**
 * @brief The seven bit lane comparison the module ships, written out here for the A and B arms.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against.
 * @return         A lane mask holding the lanes at or above byte, for lanes under 0x80.
 * @note The same three operations scrut_ge performs, copied so both arms sit in this translation
 *       unit and neither is reached through a call the other avoids.
 */
static inline embed_word bench_ge_seven_bit(embed_word word, uint8_t byte)
{
    return ((word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * byte) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief The eight bit candidate, correct over the whole 0 to 255 range.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against.
 * @return         A lane mask holding the lanes at or above byte.
 * @note Splits each lane at its high bit: a lane is at or above the byte when its high bit is set
 *       and the byte's is not, or when the two agree and the low seven bits are at or above.
 * @note Proved against a byte-at-a-time reference over all 65536 pairs in
 *       test/accuracy/test_verbum_scrutor_accuracy before it was brought here.
 */
/**
 * @brief The seven bit at-or-below the module ships, written out here for the arms below.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against.
 * @return         A lane mask holding the lanes at or below byte, for lanes under 0x80.
 */
static inline embed_word bench_le_seven_bit(embed_word word, uint8_t byte)
{
    return ((MMGR_SWAR_ONES * byte | MMGR_VERBUM_SCRUTOR_HIGH) - word) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Candidate at-or-above: the shipping subtraction with the word's own high bits or-ed in.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against, under 0x80.
 * @return         A lane mask holding the lanes at or above byte, over the whole 0 to 255 range.
 * @note A lane at or above 0x80 is above any threshold under 0x80, and its own high bit carries
 *       that. No lane can borrow out, so the neighbor is untouched either way.
 * @note Proved against a byte-at-a-time reference over every lane value and every threshold under
 *       0x80 in test/accuracy/test_verbum_scrutor_accuracy before it was brought here.
 */
static inline embed_word bench_ge_or_high(embed_word word, uint8_t byte)
{
    return (word & MMGR_VERBUM_SCRUTOR_HIGH) |
           (((word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * byte) & MMGR_VERBUM_SCRUTOR_HIGH);
}

/**
 * @brief Candidate at-or-below: the shipping subtraction with the word narrowed and its high lanes
 *        removed.
 *
 * @param[in] word Lanes to compare.
 * @param[in] byte Byte every lane is compared against, under 0x80.
 * @return         A lane mask holding the lanes at or below byte, over the whole 0 to 255 range.
 * @note The mirror of bench_ge_or_high and not symmetric with it. A lane at or above 0x80 is never
 *       at or below a threshold under 0x80, so those lanes come out of the result.
 * @note Proved over the same range as bench_ge_or_high before it was brought here.
 */
static inline embed_word bench_le_mask_high(embed_word word, uint8_t byte)
{
    return ~word & ((((MMGR_SWAR_ONES * byte) | MMGR_VERBUM_SCRUTOR_HIGH) - (word & ~MMGR_VERBUM_SCRUTOR_HIGH)) &
                    MMGR_VERBUM_SCRUTOR_HIGH);
}

/**
 * @brief The letter test the module ships.
 *
 * @param[in] word Lanes to test.
 * @return         A lane mask holding the lanes that carry an ASCII letter.
 * @note The same operations scrut_alpha performs, copied so both arms sit in this translation unit.
 */
static inline embed_word bench_alpha_current(embed_word word)
{
    const embed_word lowered = word | (MMGR_SWAR_ONES * 0x20u);

    return bench_ge_seven_bit(lowered, (uint8_t)'a') & bench_le_seven_bit(lowered, (uint8_t)'z') & ~lowered;
}

/**
 * @brief The same letter test with the lanes narrowed to seven bits before the range compares.
 *
 * @param[in] word Lanes to test.
 * @return         A lane mask holding the lanes that carry an ASCII letter.
 * @note One and more than bench_alpha_current. It puts the lanes in the quadrant where the shipping
 *       comparison is exact, which the accuracy suite measures at 0 wrong out of 16384 pairs.
 * @note The closing and is against the unmasked word, so a lane that carried the high bit is still
 *       thrown out and no byte at 0x80 or above can pass as a letter.
 */
static inline embed_word bench_alpha_narrowed(embed_word word)
{
    const embed_word lowered = word | (MMGR_SWAR_ONES * 0x20u);
    const embed_word in_range = lowered & ~MMGR_VERBUM_SCRUTOR_HIGH;

    return bench_ge_seven_bit(in_range, (uint8_t)'a') & bench_le_seven_bit(in_range, (uint8_t)'z') & ~lowered;
}

/**
 * @brief The same test again, taking the narrowing mask from MMGR_SWAR_LOW7.
 *
 * @param[in] word Lanes to test.
 * @return         A lane mask holding the lanes that carry an ASCII letter.
 * @note The header names MMGR_SWAR_LOW7 the complement of MMGR_VERBUM_SCRUTOR_HIGH, so this is the
 *       same mask as bench_alpha_narrowed reaches by complementing. What differs is the width the
 *       complement is taken at: LOW7 is built from MMGR_SWAR_ONES and stays an embed_word, where
 *       ~MMGR_VERBUM_SCRUTOR_HIGH promotes to int first and is narrowed on the way back.
 * @note alpha_masks_agree is what says the two produce the same mask. Without that the row below
 *       would be timing two different functions.
 */
static inline embed_word bench_alpha_low7(embed_word word)
{
    const embed_word lowered = word | (MMGR_SWAR_ONES * 0x20u);
    const embed_word in_range = lowered & MMGR_SWAR_LOW7;

    return bench_ge_seven_bit(in_range, (uint8_t)'a') & bench_le_seven_bit(in_range, (uint8_t)'z') & ~lowered;
}

/**
 * @brief The same test again, keeping the complement and stating the narrowing with a cast.
 *
 * @param[in] word Lanes to test.
 * @return         A lane mask holding the lanes that carry an ASCII letter.
 * @note The third form the two above are priced against. It leaves the expression exactly as the
 *       module writes it and only names the width the result is taken at, where bench_alpha_low7
 *       reaches the same mask through a different constant.
 */
static inline embed_word bench_alpha_cast(embed_word word)
{
    const embed_word lowered = word | (MMGR_SWAR_ONES * 0x20u);
    // Explicit cast takes the complement back to the embed_word the lanes are held at. The value is
    // the low seven bits of every lane either way
    const embed_word in_range = lowered & (embed_word)~MMGR_VERBUM_SCRUTOR_HIGH;

    return bench_ge_seven_bit(in_range, (uint8_t)'a') & bench_le_seven_bit(in_range, (uint8_t)'z') & ~lowered;
}

/**
 * @brief Counts the words the two narrowing forms disagree on.
 *
 * @return The disagreement count, which is expected to be zero.
 * @note Walks a fixed recurrence so a failure reproduces, and covers every lane pattern the walk
 *       reaches rather than the one word the timing row uses.
 */
static uint32_t alpha_masks_agree(void)
{
    uint32_t bad = 0u;
    embed_word walk = 1u;

    for (unsigned step = 0; step < 8192u; step++)
    {
        const embed_word shipped = bench_alpha_narrowed(walk);

        if ((shipped != bench_alpha_low7(walk)) || (shipped != bench_alpha_cast(walk)))
        {
            bad++;
        }
        // Explicit cast narrows the recurrence to the word the lanes are read from
        walk = (embed_word)((walk * 6364136223846793005ull) + 1442695040888963407ull);
    }
    return bad;
}

static volatile uint64_t g_bits = 0x0000123456789000ull;
static volatile embed_word g_mask = (embed_word)0x8080008000800080ull;
static volatile embed_word g_word = (embed_word)0x6162630061626300ull;

static volatile unsigned g_scrut_pick = 0u;

#define CAP 4096u

static EMBED_ALIGN(MMGR_ALIGN_BYTES) char g_a[CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) char g_b[CAP];

static EMBED_ALIGN(MMGR_ALIGN_BYTES) char g_c[CAP];

static const char *const g_needle = "qx";

static const char *const g_hot = "ao";
#define NLEN 2u

static const char *volatile g_int = "1234567";
static const char *volatile g_int_wide = "9223372036854775807";
static const char *volatile g_real = "3.14159265358979";
static const char *volatile g_real_exp = "1.7976931348623157e+308";

static volatile uint64_t g_scale_mant = 314159265358979ull;

static size_t head_bytes(const char *p, size_t cap)
{

    const size_t off = (size_t)((uintptr_t)p & (uintptr_t)(MMGR_SWAR_BYTES - 1u));
    const size_t need = (off == 0u) ? 0u : (MMGR_SWAR_BYTES - off);

    return (need > cap) ? cap : need;
}

static embed_bool eq_unaligned(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (embed_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool eq_aligned(const char *a, const char *b, size_t cap)
{

    const size_t skew_a = (size_t)((uintptr_t)a & (uintptr_t)(MMGR_SWAR_BYTES - 1u));
    const size_t skew_b = (size_t)((uintptr_t)b & (uintptr_t)(MMGR_SWAR_BYTES - 1u));

    if (skew_a != skew_b)
    {
        return eq_unaligned(a, b, cap);
    }

    const size_t lead = head_bytes(a, cap);
    size_t at = 0u;

    while (at != lead)
    {
        if ((a[at] != b[at]) || (a[at] == '\0'))
        {

            return (embed_bool)(a[at] == b[at]);
        }
        at++;
    }

    const size_t full = lead + (((cap - lead) / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES);

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (embed_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static volatile unsigned g_byte_bias = 0u;

static volatile int g_class = 0;

static uint32_t ascii_span_libc_digit(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {

        found += isdigit((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

static uint32_t ascii_span_libc_alpha(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {

        found += isalpha((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

static uint32_t ascii_span_libc_space(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {

        found += isspace((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

static uint32_t ascii_span_libc_print(void)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {

        found += isprint((int)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

static embed_bool agree_entry_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u) || (wa != wb))
        {
            return (embed_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool agree_inline_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u) || (wa != wb))
        {
            return (embed_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool agree_reordered(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool agree_filter_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return EMBED_TRUE;
            }
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_word part_loads(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    embed_word seen = 0u;
    size_t at = 0u;

    while (at != full)
    {
        seen |= EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        seen |= EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        at += MMGR_SWAR_BYTES;
    }
    return seen;
}

static embed_bool part_compare(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool part_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool part_marked(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t block = 4u * MMGR_SWAR_BYTES;

    const size_t blocks = (full / block) * block;
    size_t at = 0u;

    while (at != blocks)
    {
        embed_word ends = 0u;
        embed_word diff = 0u;

        for (size_t k = 0; k < block; k += MMGR_SWAR_BYTES)
        {
            const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at + k);
            const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at + k);

            ends |= EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
            diff |= wa ^ wb;
        }

        if ((ends | diff) != 0u)
        {
            break;
        }
        at += block;
    }

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool part_onetest(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (((wa ^ wb) | ((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH)) != 0u)
        {
            if (wa != wb)
            {
                return EMBED_FALSE;
            }
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return EMBED_TRUE;
            }
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool part_pipelined(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    if (full == 0u)
    {
        return EMBED_TRUE;
    }

    embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a);
    embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b);

    while (at + MMGR_SWAR_BYTES != full)
    {
        const embed_word na = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at + MMGR_SWAR_BYTES);
        const embed_word nb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at + MMGR_SWAR_BYTES);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        wa = na;
        wb = nb;
        at += MMGR_SWAR_BYTES;
    }

    if (wa != wb)
    {
        return EMBED_FALSE;
    }
    return EMBED_TRUE;
}

static embed_bool part_two_pass(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t pair = 2u * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while ((full - at) >= pair)
    {
        const embed_word a0 = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word b0 = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        const embed_word a1 = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at + MMGR_SWAR_BYTES);
        const embed_word b1 = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at + MMGR_SWAR_BYTES);
        const embed_word z0 = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = a0);
        const embed_word z1 = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = a1);

        if (a0 != b0)
        {
            return EMBED_FALSE;
        }
        if (z0 != 0u)
        {
            return EMBED_TRUE;
        }
        if (a1 != b1)
        {
            return EMBED_FALSE;
        }
        if (z1 != 0u)
        {
            return EMBED_TRUE;
        }
        at += pair;
    }

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return EMBED_FALSE;
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_word arm_diff_lanes(embed_word d)
{
    return MMGR_VERBUM_SCRUTOR_HIGH & ~EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = d);
}

static embed_bool arm_at(embed_word wa, embed_word wb, embed_bool end_wins)
{
    const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
    const embed_word x = arm_diff_lanes(wa ^ wb);
    const size_t lz = (z != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
    const size_t lx = (x != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

    return (embed_bool)(end_wins ? (lz <= lx) : (lz < lx));
}

static embed_bool arm_all_inline(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    const embed_bool level =
        (embed_bool)(((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, EMBED_FALSE);
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const embed_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (embed_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const embed_word keep = EMBED_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const embed_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (embed_bool)(lz < lx);
        }
    }
    return EMBED_FALSE;
}

static embed_bool arm_slow(const char *a, const char *b, size_t at, size_t full, size_t rest)
{
    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const embed_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (embed_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const embed_word keep = EMBED_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const embed_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? EMBED_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (embed_bool)(lz < lx);
        }
    }
    return EMBED_FALSE;
}

static embed_bool arm_slow_out(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    const embed_bool level =
        (embed_bool)(((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const embed_word wa = EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, EMBED_FALSE);
        }
        if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return EMBED_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return arm_slow(a, b, at, full, rest);
}

static size_t lane_first_trail(embed_word m)
{
    if (m == 0u)
    {
        return MMGR_SWAR_BYTES;
    }

    return (size_t)((embed_word)EMBED_CALL(clz.trail, ClzCfg, .val = (embed_u64)m) >> 3u);
}

static size_t lane_first_builtin(embed_word m)
{
    if (m == 0u)
    {
        return MMGR_SWAR_BYTES;
    }
#if EMBED_HAS_BUILTIN(__builtin_ctzll)

    return (size_t)((unsigned)__builtin_ctzll((unsigned long long)m) >> 3u);
#else
    return EMBED_CALL(lane.first, ScrutLaneCfg, .mask = m);
#endif
}

static embed_word scrut_span(unsigned pick)
{
    embed_word seen = 0u;

    switch (pick)
    {
    case 0u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = g_word ^ (embed_word)s);
        }
        break;
    case 1u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.eq, ScrutLaneCfg, .word = g_word ^ (embed_word)s, .byte = (uint8_t)'a');
        }
        break;
    case 2u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.ge, ScrutLaneCfg, .word = g_word ^ (embed_word)s, .byte = (uint8_t)'a');
        }
        break;
    case 3u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.le, ScrutLaneCfg, .word = g_word ^ (embed_word)s, .byte = (uint8_t)'z');
        }
        break;
    case 4u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.alpha, ScrutLaneCfg, .word = g_word ^ (embed_word)s);
        }
        break;
    case 5u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.any_digit, ScrutLaneCfg, .word = g_word ^ (embed_word)s);
        }
        break;
    case 6u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(lane.any_upper, ScrutLaneCfg, .word = g_word ^ (embed_word)s);
        }
        break;
    case 7u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (embed_word)EMBED_CALL(lane.count, ScrutLaneCfg, .mask = (g_word ^ (embed_word)s) & g_mask);
        }
        break;
    case 8u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (embed_word)EMBED_CALL(lane.first, ScrutLaneCfg, .mask = (g_word ^ (embed_word)s) & g_mask);
        }
        break;
    case 9u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (embed_word)EMBED_CALL(lane.last, ScrutLaneCfg, .mask = (g_word ^ (embed_word)s) & g_mask);
        }
        break;
    case 10u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(mask.spread, ScrutMaskCfg, .mask = (g_word ^ (embed_word)s) & g_mask);
        }
        break;
    case 11u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = (size_t)(s & 7u));
        }
        break;
    default:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= EMBED_CALL(word.fold_lower, ScrutWordCfg, .word = g_word ^ (embed_word)s);
        }
        break;
    }
    return seen;
}

typedef embed_word bench_word_t EMBED_ALIAS;

static char *volatile g_cp_dst;
static const char *volatile g_cp_src;

static size_t copy_single(char *dst, const char *src, size_t cap)
{
    if (cap == 0u)
    {
        return 0u;
    }

    const size_t limit = cap - 1u;
    size_t at = 0u;

    if (((((uintptr_t)dst) | ((uintptr_t)src)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u)
    {
        const size_t full = (limit / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;

        while (at != full)
        {
            const embed_word w = EMBED_CALL(word.load_al, ScrutWordCfg, .at = src + at);

            if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = w) != 0u)
            {
                break;
            }

            *(bench_word_t *)(dst + at) = w;
            at += MMGR_SWAR_BYTES;
        }
    }

    while ((at != limit) && (src[at] != '\0'))
    {
        dst[at] = src[at];
        at += 1u;
    }
    dst[at] = '\0';
    return at;
}

static uint32_t ws_span_chain(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        const char ch = g_a[at];

        found +=
            ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r') || (ch == '\f') || (ch == '\v')) ? 1u : 0u;
    }
    return found;
}

static uint32_t ws_span_range(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {

        const unsigned ch = (unsigned)(unsigned char)g_a[at];

        found += (((ch - 9u) <= 4u) || (ch == 32u)) ? 1u : 0u;
    }
    return found;
}

static uint32_t ws_span_mmgr(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        found += EMBED_CALL(cellul.ws, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
    }
    return found;
}

static uint32_t ws_span_libc(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {

        found += isspace((int)(unsigned char)g_a[at]) ? 1u : 0u;
    }
    return found;
}

static uint32_t digit_span_mmgr(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {
        found += EMBED_CALL(cellul.digit, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
    }
    return found;
}

static uint32_t digit_span_libc(size_t n)
{
    uint32_t found = 0u;

    for (size_t at = 0; at < n; at++)
    {

        found += isdigit((int)(unsigned char)g_a[at]) ? 1u : 0u;
    }
    return found;
}

static uint32_t ascii_span_mmgr(MmgrAsciiClass kind)
{
    uint32_t found = 0u;

    for (unsigned byte = 0; byte < 256u; byte++)
    {

        found += EMBED_CALL(ascii.in, AsciiCfg, .kind = kind, .byte = (uint8_t)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

typedef struct
{
    const char *src;
    const char *other;
    size_t cap;
} BenchEqCtx;

static embed_bool eq_via_ctx(const BenchEqCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (embed_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static embed_bool eq_via_ctx_hoisted(const BenchEqCtx *args)
{
    const char *const a = args->src;
    const char *const b = args->other;
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const embed_word wa = EMBED_CALL(word.load, ScrutWordCfg, .at = a + at);
        const embed_word wb = EMBED_CALL(word.load, ScrutWordCfg, .at = b + at);
        const embed_word z = EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (embed_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return EMBED_TRUE;
}

static void fill(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        g_a[i] = (char)('a' + (i % 15));
        g_b[i] = (char)('a' + (i % 15));
    }
    g_a[n] = '\0';
    g_b[n] = '\0';
}

EMBED_FLATTEN static size_t len_flat(const char *s, size_t cap)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = cap);
}

void dbench_run(void)
{
    static const size_t lens[] = {8u, 16u, 32u, 64u, 128u, 512u, 2048u};

    for (;;)
    {
        DBENCH_BANNER("cellularum vs libc");

        for (unsigned li = 0; li < (sizeof lens / sizeof lens[0]); li++)
        {
            const size_t n = lens[li];
            const uint32_t iters = (n <= 64u) ? 20000u : ((n <= 512u) ? 4000u : 1000u);

            fill(n);

            DBENCH_AB("len", iters, n, DBENCH_KEEP(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(strnlen(g_a, n + 1u)));

            DBENCH_AB(
                "chr", iters, n,
                DBENCH_KEEP(EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .byte = (uint8_t)'z')),
                DBENCH_KEEP(strchr(g_a, 'z')));

            DBENCH_AB("cmp", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n)),
                      DBENCH_KEEP(memcmp(g_a, g_b, n)));

            DBENCH_AB("find", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_needle,
                                             .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_needle)));

            DBENCH_AB("find_hot", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_hot,
                                             .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_hot)));

            DBENCH_AB("eq", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(strcmp(g_a, g_b) == 0));

            DBENCH_AB("starts", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u,
                                             .other_cap = n + 1u)),
                      DBENCH_KEEP(strncmp(g_a, g_b, n) == 0));

            DBENCH_AB("copy", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP((strncpy(g_c, g_a, n), g_c[n] = '\0', (uintptr_t)g_c)));

            g_cp_dst = g_c;
            g_cp_src = g_a;
            DBENCH_AB("copy_one", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(copy_single(g_cp_dst, g_cp_src, n + 1u)));

            DBENCH_AB("has", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_needle,
                                             .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_needle) != NULL));

            DBENCH_AB("ws_scan", iters, n, DBENCH_KEEP(ws_span_mmgr(n)), DBENCH_KEEP(ws_span_libc(n)));

            DBENCH_AB("ws_range", iters, n, DBENCH_KEEP(ws_span_chain(n)), DBENCH_KEEP(ws_span_range(n)));

            DBENCH_AB("digit_scan", iters, n, DBENCH_KEEP(digit_span_mmgr(n)), DBENCH_KEEP(digit_span_libc(n)));

            DBENCH_AB("eq_align", iters, n, DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)),
                      DBENCH_KEEP(eq_aligned(g_a, g_b, n + 1u)));

            g_cp_src = g_a;
            g_cp_dst = (char *)g_b;
            DBENCH_AB("eq_zero", iters, n, DBENCH_KEEP(agree_entry_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_inline_zero(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("eq_order", iters, n, DBENCH_KEEP(agree_inline_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_reordered(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("eq_filter", iters, n, DBENCH_KEEP(agree_reordered(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(agree_filter_zero(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("eq_cold", iters, n, DBENCH_KEEP(arm_all_inline(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(arm_slow_out(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_cmp", iters, n, DBENCH_KEEP(part_loads(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_compare(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_zero", iters, n, DBENCH_KEEP(part_compare(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_mark", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_marked(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_one", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_onetest(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_pipe", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_pipelined(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("part_two", iters, n, DBENCH_KEEP(part_zero(g_cp_src, g_cp_dst, n + 1u)),
                      DBENCH_KEEP(part_two_pass(g_cp_src, g_cp_dst, n + 1u)));

            DBENCH_AB("eq_entry", iters, n,
                      DBENCH_KEEP(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

            {
                const BenchEqCtx ctx = {.src = g_a, .other = g_b, .cap = n + 1u};

                DBENCH_AB("eq_ctx", iters, n, DBENCH_KEEP(eq_via_ctx(&ctx)),
                          DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

                DBENCH_AB("eq_hoist", iters, n,
                          DBENCH_KEEP(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                          DBENCH_KEEP(eq_via_ctx_hoisted(&ctx)));
            }
        }

        {
            const uint32_t iters = 5000u;

            DBENCH_AB("to_long", iters, 7u, DBENCH_KEEP(EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = g_int)),
                      DBENCH_KEEP(strtol(g_int, NULL, 10)));

            DBENCH_AB("to_ulong", iters, 19u,
                      DBENCH_KEEP(EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = g_int_wide)),
                      DBENCH_KEEP(strtoul(g_int_wide, NULL, 10)));

            DBENCH_AB("to_double", iters, 16u, DBENCH_KEEP(EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = g_real)),
                      DBENCH_KEEP(strtod(g_real, NULL)));

            DBENCH_AB("to_double_exp", iters, 23u,
                      DBENCH_KEEP(EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = g_real_exp)),
                      DBENCH_KEEP(strtod(g_real_exp, NULL)));

            DBENCH_AB("to_float", iters, 16u, DBENCH_KEEP(EMBED_CALL(cellul.to_float, TransfiguroCfg, .src = g_real)),
                      DBENCH_KEEP(strtof(g_real, NULL)));
        }

        {
            const uint32_t iters = 2000u;

            g_class = (int)MMGR_ASCII_NUM;
            DBENCH_AB("ascii_num", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_digit()));

            g_class = (int)MMGR_ASCII_ALPHA;
            DBENCH_AB("ascii_alpha", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_alpha()));

            g_class = (int)MMGR_ASCII_SPACE;
            DBENCH_AB("ascii_space", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_space()));

            g_class = (int)MMGR_ASCII_PRINT;
            DBENCH_AB("ascii_print", iters, 256u, DBENCH_KEEP(ascii_span_mmgr((MmgrAsciiClass)g_class)),
                      DBENCH_KEEP(ascii_span_libc_print()));
        }

        {
            const uint32_t iters = 20000u;

            {
                static const char *const named[] = {"has_zero", "eq",    "ge",   "le",     "alpha", "digit", "upper",
                                                    "count",    "first", "last", "spread", "below", "fold"};

                for (unsigned pick = 0; pick < 13u; pick++)
                {
                    g_scrut_pick = pick;
                    DBENCH_OP(named[pick], 2000u, DBENCH_KEEP(scrut_span(g_scrut_pick)));
                }
            }

            {
                embed_word acc = 0u;

                DBENCH_AB("lane_first2", 2000u, 64u, ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (embed_word)EMBED_CALL(lane.first, ScrutLaneCfg,
                                                                .mask = (g_word ^ (embed_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }),
                          ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (embed_word)lane_first_trail((g_word ^ (embed_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }));

                DBENCH_AB("lane_first3", 2000u, 64u, ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (embed_word)EMBED_CALL(lane.first, ScrutLaneCfg,
                                                                .mask = (g_word ^ (embed_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }),
                          ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (embed_word)lane_first_builtin((g_word ^ (embed_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }));
            }

            DBENCH_AB("clz_lead", iters, 8u, DBENCH_KEEP(EMBED_CALL(clz.lead, ClzCfg, .val = (embed_u64)g_bits)),
                      DBENCH_KEEP(__builtin_clzll(g_bits)));

            DBENCH_AB("clz_trail", iters, 8u, DBENCH_KEEP(EMBED_CALL(clz.trail, ClzCfg, .val = (embed_u64)g_bits)),
                      DBENCH_KEEP(__builtin_ctzll(g_bits)));

            DBENCH_AB("lane_count", iters, 8u, DBENCH_KEEP(EMBED_CALL(lane.count, ScrutLaneCfg, .mask = g_mask)),
                      DBENCH_KEEP(__builtin_popcountll((unsigned long long)g_mask)));

            DBENCH_AB(
                "lane_first", iters, 8u, DBENCH_KEEP(EMBED_CALL(lane.first, ScrutLaneCfg, .mask = g_mask)),
                DBENCH_KEEP((g_mask == 0u) ? MMGR_SWAR_BYTES : (__builtin_ctzll((unsigned long long)g_mask) / 8u)));

            DBENCH_AB("word_load", iters, 8u, DBENCH_KEEP(EMBED_CALL(word.load, ScrutWordCfg, .at = g_a + 1)),
                      DBENCH_KEEP(EMBED_CALL(word.load_al, ScrutWordCfg, .at = g_a)));

            DBENCH_AB("lane_zero", iters, 8u, DBENCH_KEEP(EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = g_word)),
                      DBENCH_KEEP((g_word - (embed_word)0x0101010101010101ull) & ~g_word &
                                  (embed_word)0x8080808080808080ull));
        }

        {
            const uint32_t iters = 5000u;

            DBENCH_AB("soft_divmul", iters, 8u, DBENCH_KEEP((double)g_scale_mant / 1e14),
                      DBENCH_KEEP((double)g_scale_mant * 1e-14));
        }

        // The letter test the module ships against the same test with its lanes narrowed first. Both
        // arms are static inline in this translation unit over the same volatile word, so neither is
        // privileged by a call the other pays or an inlining the other does not get, and neither can
        // be hoisted out of the loop.
        DBENCH_AB("alpha_now_vs_narrow", 20000u, 4u, DBENCH_KEEP(bench_alpha_current(g_scrut_word)),
                  DBENCH_KEEP(bench_alpha_narrowed(g_scrut_word)));

        // The shipping narrowing against the same mask taken from MMGR_SWAR_LOW7. The two are the
        // same value, so this row prices the width the complement is taken at and nothing else.
        DBENCH_PRINTF("DB alpha_mask_check disagreements=%u\n", (unsigned)alpha_masks_agree());

        DBENCH_AB("alpha_notHIGH_vs_LOW7", 20000u, 4u, DBENCH_KEEP(bench_alpha_narrowed(g_scrut_word)),
                  DBENCH_KEEP(bench_alpha_low7(g_scrut_word)));

        DBENCH_AB("alpha_notHIGH_vs_cast", 20000u, 4u, DBENCH_KEEP(bench_alpha_narrowed(g_scrut_word)),
                  DBENCH_KEEP(bench_alpha_cast(g_scrut_word)));

        // The two candidates against each other, so the pair is priced without the shipping form in
        // between and a one cycle move on one part can be read against a second row rather than alone
        DBENCH_AB("alpha_LOW7_vs_cast", 20000u, 4u, DBENCH_KEEP(bench_alpha_low7(g_scrut_word)),
                  DBENCH_KEEP(bench_alpha_cast(g_scrut_word)));

        // The shipping comparisons against the two candidates that answer over the whole byte range.
        // Same shape as the row above: both arms static inline here, both operands out of volatiles
        DBENCH_AB("ge_now_vs_or_high", 20000u, 4u, DBENCH_KEEP(bench_ge_seven_bit(g_scrut_word, g_scrut_threshold)),
                  DBENCH_KEEP(bench_ge_or_high(g_scrut_word, g_scrut_threshold)));

        DBENCH_AB("le_now_vs_mask_high", 20000u, 4u, DBENCH_KEEP(bench_le_seven_bit(g_scrut_word, g_scrut_threshold)),
                  DBENCH_KEEP(bench_le_mask_high(g_scrut_word, g_scrut_threshold)));

        fill(8u);
        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_a));

        DBENCH_OP("floor_call", 20000u, DBENCH_KEEP(strnlen(g_a, 1u)));

        DBENCH_OP("dispatch_len8", 20000u, DBENCH_KEEP(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = 9u)));
        DBENCH_OP("direct_len8", 20000u, DBENCH_KEEP(mmgr_cellul_len(&(CatenaFinitaCfg){.src = g_a, .cap = 9u})));

        DBENCH_OP("flat_len8", 20000u, DBENCH_KEEP(len_flat(g_a, 9u)));
        fill(64u);
        DBENCH_OP("flat_len64", 20000u, DBENCH_KEEP(len_flat(g_a, 65u)));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("cellularum")
