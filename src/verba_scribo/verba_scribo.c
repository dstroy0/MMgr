// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "verba_scribo/verba_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "fractio/fractio.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @file verba_scribo.c
 * @brief Text building over caller memory. Overflow latches; the buffer is always terminated.
 */

#define MMGR_G_WORK_BITS 58U

static const char mmgr_hex_lower[] = "0123456789abcdef";

void mmgr_verba_put_n(mmgr_verba *b, const char *s, size_t sl)
{
    if (!b->ok)
    {
        return;
    }
    if (b->len + sl >= b->cap)
    {
        b->ok = MMGR_FALSE;
        return;
    }

    mmgr_proxim_read(b->p + b->len, s, sl);
    b->len += sl;
}

void mmgr_verba_put(mmgr_verba *b, const char *s)
{
    if (!b->ok)
    {
        return;
    }
    mmgr_verba_put_n(b, s, cellul.len(s, b->cap));
}

void mmgr_verba_put_clip(mmgr_verba *b, const char *s)
{
    if (!b->ok || !s || b->len + 1 >= b->cap)
    {
        return;
    }
    size_t room = b->cap - b->len - 1;
    size_t sl = cellul.len(s, room);

    mmgr_proxim_read(b->p + b->len, s, sl);
    b->len += sl;
}

void mmgr_verba_u64_clip(mmgr_verba *b, uint64_t v, uint8_t columns)
{
    if (!b->ok)
    {
        return;
    }
    uint64_t probe = v;
    size_t digits = 1;
    while (probe >= 10)
    {
        probe /= 10;
        digits++;
    }
    size_t width = (digits < columns) ? columns : digits;
    if (b->len + width >= b->cap)
    {
        return;
    }
    for (size_t i = width - digits; i-- > 0;)
    {
        b->p[b->len + i] = ' ';
    }
    for (size_t i = width; i-- > width - digits;)
    {
        b->p[b->len + i] = (char)('0' + (unsigned)(v % 10));
        v /= 10;
    }
    b->len += width;
}

void mmgr_verba_xml(mmgr_verba *b, const char *s)
{
    if (!b->ok || !s)
    {
        return;
    }
    for (; *s; s++)
    {
        const char *rep = NULL;
        switch (*s)
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
        if (rep)
        {
            mmgr_verba_put(b, rep);
        }
        else
        {
            if (b->len + 1 >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len] = *s;
            b->len++;
        }
    }
}

void mmgr_verba_ch(mmgr_verba *b, char c)
{
    if (!b->ok)
    {
        return;
    }
    if (b->len + 1 >= b->cap)
    {
        b->ok = MMGR_FALSE;
        return;
    }
    b->p[b->len++] = c;
}

void mmgr_verba_uint(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits)
{
    if (!b->ok)
    {
        return;
    }

    const unsigned bits_per_digit = (base == 16) ? 4U : (base == 8) ? 3U : 0U;
    const mmgr_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ULL << bits_per_digit) - 1U) : 0U;

    const mmgr_bool narrow = !power_of_two && v <= 0xFFFFFFFFU;

    uint64_t probe = v;
    unsigned digits = 1;
    if (power_of_two)
    {
        while ((probe >>= bits_per_digit) != 0)
        {
            digits++;
        }
    }
    else if (narrow)
    {
        uint32_t p32 = (uint32_t)v;
        while (p32 >= 10U)
        {
            p32 /= 10U;
            digits++;
        }
    }
    else
    {
        while (probe >= 10)
        {
            probe /= 10;
            digits++;
        }
    }
    if (digits < min_digits)
    {
        digits = min_digits;
    }
    if (b->len + digits >= b->cap)
    {
        b->ok = MMGR_FALSE;
        return;
    }
    if (power_of_two)
    {
        for (unsigned i = digits; i-- > 0;)
        {
            b->p[b->len + i] = mmgr_hex_lower[v & digit_mask];
            v >>= bits_per_digit;
        }
    }
    else if (narrow)
    {
        uint32_t v32 = (uint32_t)v;
        for (unsigned i = digits; i-- > 0;)
        {
            b->p[b->len + i] = (char)('0' + (unsigned)(v32 % 10U));
            v32 /= 10U;
        }
    }
    else
    {
        for (unsigned i = digits; i-- > 0;)
        {
            b->p[b->len + i] = (char)('0' + (unsigned)(v % 10));
            v /= 10;
        }
    }
    b->len += digits;
}

void mmgr_verba_u32w(mmgr_verba *b, uint32_t v, unsigned min_digits)
{
    mmgr_verba_uint(b, v, 10, min_digits);
}

void mmgr_verba_hex(mmgr_verba *b, uint64_t v, unsigned min_digits)
{
    mmgr_verba_uint(b, v, 16, min_digits);
}

void mmgr_verba_u32(mmgr_verba *b, uint32_t v)
{
    mmgr_verba_uint(b, v, 10, 1);
}

void mmgr_verba_u64(mmgr_verba *b, uint64_t v)
{
    mmgr_verba_uint(b, v, 10, 1);
}

void mmgr_verba_i64(mmgr_verba *b, int64_t v)
{

    uint64_t mag = (v < 0) ? (uint64_t)(-(v + 1)) + 1U : (uint64_t)v;
    if (v < 0)
    {
        mmgr_verba_ch(b, '-');
    }
    mmgr_verba_uint(b, mag, 10, 1);
}

mmgr_bool mmgr_signbit(double v)
{
    return mmgr_fract_sign(v) != 0U;
}

mmgr_bool mmgr_isinf(double v)
{
    return mmgr_fract_exp(v) == 0x7FFU && mmgr_fract_mant(v) == 0U;
}

mmgr_bool mmgr_isnan(double v)
{
    return mmgr_fract_exp(v) == 0x7FFU && mmgr_fract_mant(v) != 0U;
}

/**
 * @brief Append @p digits decimal digits of @p mant, with a point after @p point_after of them.
 * @param b Builder.
 * @param mant Mantissa.
 * @param digits How many digits to emit.
 * @param point_after Where the decimal point goes. Zero means no point.
 */
static void sb_digits(mmgr_verba *b, uint64_t mant, unsigned digits, unsigned point_after)
{
    uint64_t div = 1;
    for (unsigned i = 1; i < digits; i++)
    {
        div *= 10;
    }
    for (unsigned i = 0; i < digits; i++)
    {
        if (i == point_after && i != 0)
        {
            mmgr_verba_ch(b, '.');
        }
        mmgr_verba_ch(b, (char)('0' + (unsigned)((mant / div) % 10)));
        div /= 10;
    }
}

/**
 * @brief Renormalize a mantissa and scale pair so the mantissa sits in the working range.
 * @param n In/out. Mantissa.
 * @param s In/out. Binary scale.
 *
 * The pair is a fixed point value, not a double. Doing the decimal conversion in integers is what
 * keeps this module free of <math.h> and of any rounding the FPU would apply on the way.
 */
static void g_renorm(mmgr_u64 *n, mmgr_i32 *s)
{
    /* GCOVR_EXCL_START - mmgr_verba_g answers a zero before it gets here, and the two callers that
       loop through this multiply by ten or shift up four and divide by ten, neither of which can
       turn a mantissa in the working range into nothing. Kept because a renormalize that spins on
       zero would not come back. */
    if (*n == 0U)
    {
        return;
    }
    /* GCOVR_EXCL_STOP */
    while (*n >= (1ULL << MMGR_G_WORK_BITS))
    {
        *n >>= 1;
        *s += 1;
    }
    while (*n < (1ULL << (MMGR_G_WORK_BITS - 1U)))
    {
        *n <<= 1;
        *s -= 1;
    }
}

/**
 * @brief Multiply the pair by ten.
 * @param n In/out. Mantissa.
 * @param s In/out. Binary scale.
 */
static void g_mul10(mmgr_u64 *n, mmgr_i32 *s)
{
    *n *= 10U;
    g_renorm(n, s);
}

/**
 * @brief Divide the pair by ten.
 * @param n In/out. Mantissa.
 * @param s In/out. Binary scale.
 *
 * Shifts up by four before dividing so the division keeps its low bits.
 */
static void g_div10(mmgr_u64 *n, mmgr_i32 *s)
{
    *n <<= 4;
    *s -= 4;
    *n /= 10U;
    g_renorm(n, s);
}

/**
 * @brief Collapse the pair to an integer, rounding.
 * @param n Mantissa.
 * @param s Binary scale.
 * @return The integer.
 */
static mmgr_u64 g_round(mmgr_u64 n, mmgr_i32 s)
{
    if (s >= 0)
    {
        return n << (unsigned)s;
    }
    unsigned sh = (unsigned)(-s);
    /* GCOVR_EXCL_START - the digit fit leaves the scale near 3.32 times the precision less the
       working width, which bottoms out around -58 at one significant digit. Swept over 20 million
       value and precision pairs without reaching -64. Kept because a shift of the full width is
       undefined and this is the only thing standing between it and the caller. */
    if (sh >= 64U)
    {
        return 0U;
    }
    /* GCOVR_EXCL_STOP */
    mmgr_u64 r = n >> sh;
    mmgr_u64 rem = n - (r << sh);
    mmgr_u64 half = 1ULL << (sh - 1U);
    if (rem > half || (rem == half && (r & 1U) != 0U))
    {
        r++;
    }
    return r;
}

void mmgr_verba_g(mmgr_verba *b, double v, unsigned sig)
{
    if (!b->ok)
    {
        return;
    }
    if (sig == 0)
    {
        sig = 1;
    }
    if (mmgr_isnan(v))
    {
        mmgr_verba_put(b, "nan");
        return;
    }

    if (mmgr_signbit(v))
    {
        mmgr_verba_ch(b, '-');
        v = -v;
    }
    if (mmgr_isinf(v))
    {
        mmgr_verba_put(b, "inf");
        return;
    }
    mmgr_u64 be = mmgr_fract_exp(v);
    mmgr_u64 n = mmgr_fract_mant(v);
    if (be == 0U && n == 0U)
    {
        mmgr_verba_ch(b, '0');
        return;
    }

    mmgr_i32 s = 1 - MMGR_DBL_BIAS - (mmgr_i32)MMGR_DBL_MANT_BITS;
    if (be != 0U)
    {
        n |= 1ULL << MMGR_DBL_MANT_BITS;
        s = (mmgr_i32)be - MMGR_DBL_BIAS - (mmgr_i32)MMGR_DBL_MANT_BITS;
    }
    g_renorm(&n, &s);

    mmgr_u64 limit = 1U;
    for (unsigned i = 0; i < sig; i++)
    {
        limit *= 10U;
    }

    int e = (int)(((int64_t)((int)MMGR_G_WORK_BITS - 1 + s) * 78913) >> 18);

    int p = (int)sig - 1 - e;
    while (p > 0)
    {
        g_mul10(&n, &s);
        p--;
    }
    while (p < 0)
    {
        g_div10(&n, &s);
        p++;
    }

    mmgr_u64 mant = g_round(n, s);
    for (unsigned guard = 0; guard < 4U; guard++)
    {
        if (mant >= limit)
        {
            g_div10(&n, &s);
            e++;
        }
        else if (sig > 1U && mant < limit / 10U)
        {
            g_mul10(&n, &s);
            e--;
        }
        else
        {
            break;
        }
        mant = g_round(n, s);
    }

    unsigned digits = sig;
    while (digits > 1 && mant % 10 == 0)
    {
        mant /= 10;
        digits--;
    }

    if (e < -4 || e >= (int)sig)
    {
        sb_digits(b, mant, digits, 1);
        mmgr_verba_ch(b, 'e');
        mmgr_verba_ch(b, e < 0 ? '-' : '+');
        unsigned mag = (unsigned)(e < 0 ? -e : e);
        mmgr_verba_u32w(b, mag, 2);
        return;
    }
    if (e >= (int)digits - 1)
    {
        sb_digits(b, mant, digits, 0);
        for (int i = 0; i < e - (int)digits + 1; i++)
        {
            mmgr_verba_ch(b, '0');
        }
        return;
    }
    if (e >= 0)
    {
        sb_digits(b, mant, digits, (unsigned)e + 1);
        return;
    }
    mmgr_verba_put(b, "0.");
    for (int i = 0; i < -e - 1; i++)
    {
        mmgr_verba_ch(b, '0');
    }
    sb_digits(b, mant, digits, 0);
}

void mmgr_verba_fixed(mmgr_verba *b, double v, unsigned decimals)
{
    if (!b->ok)
    {
        return;
    }
    if (mmgr_isnan(v))
    {
        mmgr_verba_put(b, "nan");
        return;
    }

    if (mmgr_signbit(v))
    {
        mmgr_verba_ch(b, '-');
        v = -v;
    }
    if (mmgr_isinf(v))
    {
        mmgr_verba_put(b, "inf");
        return;
    }

    if (mmgr_fract_exp(v) >= (MMGR_DBL_BIAS + 64))
    {
        mmgr_verba_g(b, v, 10);
        return;
    }
    if (decimals > 18U)
    {
        decimals = 18U;
    }
    mmgr_u64 scale = 1U;
    for (unsigned i = 0; i < decimals; i++)
    {
        scale *= 10U;
    }

    mmgr_u64 mant = mmgr_fract_mant(v);
    mmgr_u64 be = mmgr_fract_exp(v);
    mmgr_i32 exp2 = 1 - MMGR_DBL_BIAS - (mmgr_i32)MMGR_DBL_MANT_BITS;
    if (be != 0U)
    {
        mant |= 1ULL << MMGR_DBL_MANT_BITS;
        exp2 = (mmgr_i32)be - MMGR_DBL_BIAS - (mmgr_i32)MMGR_DBL_MANT_BITS;
    }

    mmgr_u64 ip = 0U;
    mmgr_u64 frac = 0U;
    if (exp2 >= 0)
    {
        ip = mant << (unsigned)exp2;
    }
    else
    {
        unsigned shift = (unsigned)(-exp2);
        mmgr_u64 num = mant;
        if (shift < 64U)
        {
            ip = mant >> shift;
            num = mant - (ip << shift);
        }

        if (shift > 60U)
        {
            num >>= (shift - 60U);
            shift = 60U;
        }
        const mmgr_u64 den = 1ULL << shift;
        for (unsigned i = 0; i < decimals; i++)
        {
            num *= 10U;
            mmgr_u64 digit = num >> shift;
            num -= digit << shift;
            frac = frac * 10U + digit;
        }

        mmgr_u64 twice = num * 2U;
        if (twice > den || (twice == den && (frac & 1U) != 0U))
        {
            frac++;
        }
    }
    if (frac >= scale)
    {
        ip++;
        frac = 0U;
    }
    mmgr_verba_u64(b, ip);
    if (decimals)
    {
        mmgr_verba_ch(b, '.');
        mmgr_verba_uint(b, frac, 10, decimals);
    }
}

static const char JSON_CTRL_ESC[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0};

void mmgr_verba_json(mmgr_verba *b, const char *s)
{
    static const char HEX[] = "0123456789abcdef";

    if (!b->ok)
    {
        return;
    }
    mmgr_verba_put(b, "\"");

    for (const char *p = s ? s : ""; *p; p++)
    {
        const unsigned char c = (unsigned char)*p;
        const char two = (c == '"' || c == '\\') ? (char)c : (c < 0x20U ? JSON_CTRL_ESC[c] : 0);
        if (two)
        {
            if (b->len + 2 >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = two;
        }
        else if (c < 0x20U)
        {
            if (b->len + 6 >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = 'u';
            b->p[b->len++] = '0';
            b->p[b->len++] = '0';
            b->p[b->len++] = HEX[(c >> 4) & 0xFU];
            b->p[b->len++] = HEX[c & 0xFU];
        }
        else if (b->len + 1 < b->cap)
        {
            b->p[b->len++] = (char)c;
        }
        else
        {
            b->ok = MMGR_FALSE;
        }
    }
    mmgr_verba_put(b, "\"");
}

size_t mmgr_verba_finish(mmgr_verba *b)
{

    if (!b->ok || b->cap == 0)
    {
        return 0;
    }
    b->p[b->len] = '\0';
    return b->len;
}
