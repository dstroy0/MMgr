#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "device_bench.h"

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"

static volatile uint64_t g_bits = 0x0000123456789000ull;
static volatile mmgr_word g_mask = (mmgr_word)0x8080008000800080ull;
static volatile mmgr_word g_word = (mmgr_word)0x6162630061626300ull;

static volatile unsigned g_scrut_pick = 0u;

#define CAP 4096u

static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_a[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_b[CAP];

static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_c[CAP];

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

static mmgr_bool eq_unaligned(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool eq_aligned(const char *a, const char *b, size_t cap)
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

            return (mmgr_bool)(a[at] == b[at]);
        }
        at++;
    }

    const size_t full = lead + (((cap - lead) / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES);

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
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

static mmgr_bool agree_entry_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u) || (wa != wb))
        {
            return (mmgr_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool agree_inline_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if ((((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u) || (wa != wb))
        {
            return (mmgr_bool)(wa == wb);
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool agree_reordered(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool agree_filter_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
        {
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return MMGR_TRUE;
            }
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_word part_loads(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    mmgr_word seen = 0u;
    size_t at = 0u;

    while (at != full)
    {
        seen |= MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        seen |= MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        at += MMGR_SWAR_BYTES;
    }
    return seen;
}

static mmgr_bool part_compare(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool part_zero(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool part_marked(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t block = 4u * MMGR_SWAR_BYTES;

    const size_t blocks = (full / block) * block;
    size_t at = 0u;

    while (at != blocks)
    {
        mmgr_word ends = 0u;
        mmgr_word diff = 0u;

        for (size_t k = 0; k < block; k += MMGR_SWAR_BYTES)
        {
            const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at + k);
            const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at + k);

            ends |= MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
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
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool part_onetest(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (((wa ^ wb) | ((wa - MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH)) != 0u)
        {
            if (wa != wb)
            {
                return MMGR_FALSE;
            }
            if (((wa - MMGR_SWAR_ONES) & ~wa & MMGR_VERBUM_SCRUTOR_HIGH) != 0u)
            {
                return MMGR_TRUE;
            }

        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool part_pipelined(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    if (full == 0u)
    {
        return MMGR_TRUE;
    }

    mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a);
    mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b);

    while (at + MMGR_SWAR_BYTES != full)
    {
        const mmgr_word na = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at + MMGR_SWAR_BYTES);
        const mmgr_word nb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at + MMGR_SWAR_BYTES);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        wa = na;
        wb = nb;
        at += MMGR_SWAR_BYTES;
    }

    if (wa != wb)
    {
        return MMGR_FALSE;
    }
    return MMGR_TRUE;
}

static mmgr_bool part_two_pass(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t pair = 2u * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while ((full - at) >= pair)
    {
        const mmgr_word a0 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word b0 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);
        const mmgr_word a1 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at + MMGR_SWAR_BYTES);
        const mmgr_word b1 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at + MMGR_SWAR_BYTES);
        const mmgr_word z0 = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = a0);
        const mmgr_word z1 = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = a1);

        if (a0 != b0)
        {
            return MMGR_FALSE;
        }
        if (z0 != 0u)
        {
            return MMGR_TRUE;
        }
        if (a1 != b1)
        {
            return MMGR_FALSE;
        }
        if (z1 != 0u)
        {
            return MMGR_TRUE;
        }
        at += pair;
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return MMGR_FALSE;
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_word arm_diff_lanes(mmgr_word d)
{
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d);
}

static mmgr_bool arm_at(mmgr_word wa, mmgr_word wb, mmgr_bool end_wins)
{
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
    const mmgr_word x = arm_diff_lanes(wa ^ wb);
    const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
    const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

    return (mmgr_bool)(end_wins ? (lz <= lx) : (lz < lx));
}

static mmgr_bool arm_all_inline(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, MMGR_FALSE);
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (mmgr_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (mmgr_bool)(lz < lx);
        }
    }
    return MMGR_FALSE;
}

static mmgr_bool arm_slow(const char *a, const char *b, size_t at, size_t full, size_t rest)
{
    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = arm_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (mmgr_bool)(lz < lx);
        }
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x = arm_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;

            return (mmgr_bool)(lz < lx);
        }
    }
    return MMGR_FALSE;
}

static mmgr_bool arm_slow_out(const char *a, const char *b, size_t cap)
{
    const size_t full = (cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = cap - full;
    size_t at = 0u;

    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = b + at);

        if (wa != wb)
        {
            return arm_at(wa, wb, MMGR_FALSE);
        }
        if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) != 0u)
        {
            return MMGR_TRUE;
        }
        at += MMGR_SWAR_BYTES;
    }
    return arm_slow(a, b, at, full, rest);
}

static size_t lane_first_trail(mmgr_word m)
{
    if (m == 0u)
    {
        return MMGR_SWAR_BYTES;
    }

    return (size_t)((mmgr_word)MMGR_CALL(clz.trail, ClzCfg, .val = (mmgr_u64)m) >> 3u);
}

static size_t lane_first_builtin(mmgr_word m)
{
    if (m == 0u)
    {
        return MMGR_SWAR_BYTES;
    }
#if MMGR_HAS_BUILTIN(__builtin_ctzll)

    return (size_t)((unsigned)__builtin_ctzll((unsigned long long)m) >> 3u);
#else
    return MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
#endif
}

static mmgr_word scrut_span(unsigned pick)
{
    mmgr_word seen = 0u;

    switch (pick)
    {
    case 0u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s);
        }
        break;
    case 1u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.eq, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s, .byte = (uint8_t)'a');
        }
        break;
    case 2u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.ge, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s, .byte = (uint8_t)'a');
        }
        break;
    case 3u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.le, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s, .byte = (uint8_t)'z');
        }
        break;
    case 4u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.alpha, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s);
        }
        break;
    case 5u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.any_digit, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s);
        }
        break;
    case 6u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = g_word ^ (mmgr_word)s);
        }
        break;
    case 7u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (mmgr_word)MMGR_CALL(lane.count, ScrutLaneCfg, .mask = (g_word ^ (mmgr_word)s) & g_mask);
        }
        break;
    case 8u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (mmgr_word)MMGR_CALL(lane.first, ScrutLaneCfg, .mask = (g_word ^ (mmgr_word)s) & g_mask);
        }
        break;
    case 9u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= (mmgr_word)MMGR_CALL(lane.last, ScrutLaneCfg, .mask = (g_word ^ (mmgr_word)s) & g_mask);
        }
        break;
    case 10u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(mask.spread, ScrutMaskCfg, .mask = (g_word ^ (mmgr_word)s) & g_mask);
        }
        break;
    case 11u:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = (size_t)(s & 7u));
        }
        break;
    default:
        for (unsigned s = 0; s < 64u; s++)
        {
            seen ^= MMGR_CALL(word.fold_lower, ScrutWordCfg, .word = g_word ^ (mmgr_word)s);
        }
        break;
    }
    return seen;
}

typedef mmgr_word bench_word_t MMGR_ALIAS;

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
            const mmgr_word w = MMGR_CALL(word.load_al, ScrutWordCfg, .at = src + at);

            if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) != 0u)
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
        found += MMGR_CALL(cellul.ws, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
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
        found += MMGR_CALL(cellul.digit, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .at = at) ? 1u : 0u;
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

        found += MMGR_CALL(ascii.in, AsciiCfg, .kind = kind, .byte = (uint8_t)(byte ^ g_byte_bias)) ? 1u : 0u;
    }
    return found;
}

typedef struct
{
    const char *src;
    const char *other;
    size_t cap;
} BenchEqCtx;

static mmgr_bool eq_via_ctx(const BenchEqCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
}

static mmgr_bool eq_via_ctx_hoisted(const BenchEqCtx *args)
{
    const char *const a = args->src;
    const char *const b = args->other;
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = a + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = b + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {

            return (mmgr_bool)((z != 0u) && (wa == wb));
        }
        at += MMGR_SWAR_BYTES;
    }
    return MMGR_TRUE;
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

MMGR_FLATTEN static size_t len_flat(const char *s, size_t cap)
{
    return MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = cap);
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

            DBENCH_AB("len", iters, n, DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(strnlen(g_a, n + 1u)));

            DBENCH_AB(
                "chr", iters, n,
                DBENCH_KEEP(MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .byte = (uint8_t)'z')),
                DBENCH_KEEP(strchr(g_a, 'z')));

            DBENCH_AB("cmp", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n)),
                      DBENCH_KEEP(memcmp(g_a, g_b, n)));

            DBENCH_AB("find", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_needle,
                                            .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_needle)));

            DBENCH_AB("find_hot", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_hot,
                                            .other_cap = NLEN + 1u, .other_len = NLEN)),
                      DBENCH_KEEP(strstr(g_a, g_hot)));

            DBENCH_AB("eq", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(strcmp(g_a, g_b) == 0));

            DBENCH_AB("starts", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u,
                                            .other_cap = n + 1u)),
                      DBENCH_KEEP(strncmp(g_a, g_b, n) == 0));

            DBENCH_AB("copy", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP((strncpy(g_c, g_a, n), g_c[n] = '\0', (uintptr_t)g_c)));

            g_cp_dst = g_c;
            g_cp_src = g_a;
            DBENCH_AB("copy_one", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = g_c, .src = g_a, .cap = n + 1u)),
                      DBENCH_KEEP(copy_single(g_cp_dst, g_cp_src, n + 1u)));

            DBENCH_AB("has", iters, n,
                      DBENCH_KEEP(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_needle,
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
                      DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                      DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

            {
                const BenchEqCtx ctx = {.src = g_a, .other = g_b, .cap = n + 1u};

                DBENCH_AB("eq_ctx", iters, n, DBENCH_KEEP(eq_via_ctx(&ctx)),
                          DBENCH_KEEP(eq_unaligned(g_a, g_b, n + 1u)));

                DBENCH_AB("eq_hoist", iters, n,
                          DBENCH_KEEP(MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n + 1u)),
                          DBENCH_KEEP(eq_via_ctx_hoisted(&ctx)));
            }
        }

        {
            const uint32_t iters = 5000u;

            DBENCH_AB("to_long", iters, 7u, DBENCH_KEEP(MMGR_CALL(cellul.to_long, TransfiguroCfg, .src = g_int)),
                      DBENCH_KEEP(strtol(g_int, NULL, 10)));

            DBENCH_AB("to_ulong", iters, 19u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_ulong, TransfiguroCfg, .src = g_int_wide)),
                      DBENCH_KEEP(strtoul(g_int_wide, NULL, 10)));

            DBENCH_AB("to_double", iters, 16u, DBENCH_KEEP(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = g_real)),
                      DBENCH_KEEP(strtod(g_real, NULL)));

            DBENCH_AB("to_double_exp", iters, 23u,
                      DBENCH_KEEP(MMGR_CALL(cellul.to_double, TransfiguroCfg, .src = g_real_exp)),
                      DBENCH_KEEP(strtod(g_real_exp, NULL)));

            DBENCH_AB("to_float", iters, 16u, DBENCH_KEEP(MMGR_CALL(cellul.to_float, TransfiguroCfg, .src = g_real)),
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
                mmgr_word acc = 0u;

                DBENCH_AB("lane_first2", 2000u, 64u, ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (mmgr_word)MMGR_CALL(lane.first, ScrutLaneCfg,
                                                              .mask = (g_word ^ (mmgr_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }),
                          ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (mmgr_word)lane_first_trail((g_word ^ (mmgr_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }));

                DBENCH_AB("lane_first3", 2000u, 64u, ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (mmgr_word)MMGR_CALL(lane.first, ScrutLaneCfg,
                                                              .mask = (g_word ^ (mmgr_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }),
                          ({
                              acc = 0u;
                              for (unsigned s = 0; s < 64u; s++)
                              {
                                  acc ^= (mmgr_word)lane_first_builtin((g_word ^ (mmgr_word)s) & g_mask);
                              }
                              DBENCH_KEEP(acc);
                          }));
            }

            DBENCH_AB("clz_lead", iters, 8u, DBENCH_KEEP(MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)g_bits)),
                      DBENCH_KEEP(__builtin_clzll(g_bits)));

            DBENCH_AB("clz_trail", iters, 8u, DBENCH_KEEP(MMGR_CALL(clz.trail, ClzCfg, .val = (mmgr_u64)g_bits)),
                      DBENCH_KEEP(__builtin_ctzll(g_bits)));

            DBENCH_AB("lane_count", iters, 8u, DBENCH_KEEP(MMGR_CALL(lane.count, ScrutLaneCfg, .mask = g_mask)),
                      DBENCH_KEEP(__builtin_popcountll((unsigned long long)g_mask)));

            DBENCH_AB(
                "lane_first", iters, 8u, DBENCH_KEEP(MMGR_CALL(lane.first, ScrutLaneCfg, .mask = g_mask)),
                DBENCH_KEEP((g_mask == 0u) ? MMGR_SWAR_BYTES : (__builtin_ctzll((unsigned long long)g_mask) / 8u)));

            DBENCH_AB("word_load", iters, 8u, DBENCH_KEEP(MMGR_CALL(word.load, ScrutWordCfg, .at = g_a + 1)),
                      DBENCH_KEEP(MMGR_CALL(word.load_al, ScrutWordCfg, .at = g_a)));

            DBENCH_AB(
                "lane_zero", iters, 8u, DBENCH_KEEP(MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = g_word)),
                DBENCH_KEEP((g_word - (mmgr_word)0x0101010101010101ull) & ~g_word & (mmgr_word)0x8080808080808080ull));
        }

        {
            const uint32_t iters = 5000u;

            DBENCH_AB("soft_divmul", iters, 8u, DBENCH_KEEP((double)g_scale_mant / 1e14),
                      DBENCH_KEEP((double)g_scale_mant * 1e-14));
        }

        fill(8u);
        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_a));

        DBENCH_OP("floor_call", 20000u, DBENCH_KEEP(strnlen(g_a, 1u)));

        DBENCH_OP("dispatch_len8", 20000u, DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = 9u)));
        DBENCH_OP("direct_len8", 20000u, DBENCH_KEEP(mmgr_cellul_len(&(CatenaFinitaCfg){.src = g_a, .cap = 9u})));

        DBENCH_OP("flat_len8", 20000u, DBENCH_KEEP(len_flat(g_a, 9u)));
        fill(64u);
        DBENCH_OP("flat_len64", 20000u, DBENCH_KEEP(len_flat(g_a, 65u)));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("cellularum")
