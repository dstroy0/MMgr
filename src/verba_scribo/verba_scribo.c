// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "verba_scribo/verba_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "fractio/fractio.h"
#include "transformo/transformo.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @file verba_scribo.c
 * @brief Text building over caller memory. Overflow latches; the buffer is always terminated.
 */


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

void mmgr_verba_g(mmgr_verba *b, double v, unsigned sig)
{
    if (!b->ok)
    {
        return;
    }
    /* One look, and the necessary question first: is this a number at all. An exponent field of
     * all ones is an infinity or a nan and nothing else is either, so a single compare turns both
     * away before anything else is decided. */
    mmgr_u64 be = mmgr_fract_exp(v);

    if (be == MMGR_DBL_EXP_ALL)
    {
        if (mmgr_fract_mant(v) != 0U)
        {
            mmgr_verba_put(b, "nan");
            return;
        }
        if (mmgr_fract_sign(v) != 0U)
        {
            mmgr_verba_ch(b, '-');
        }
        mmgr_verba_put(b, "inf");
        return;
    }

    /* A number from here, so a digit count is worth having an opinion about. */
    if (sig == 0)
    {
        sig = 1;
    }
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }
    if (mmgr_fract_sign(v) != 0U)
    {
        mmgr_verba_ch(b, '-');
        v = -v;
    }
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
    mmgr_u64 limit = 1U;
    for (unsigned i = 0; i < sig; i++)
    {
        limit *= 10U;
    }

    /* Where the decimal point wants to be. The value is near two to the bit length less one plus
       the binary exponent, and 78913 over two to the eighteenth is log ten of two, so this lands on
       the right power of ten or one either side of it. The fit below settles which. */
    int e = (int)(((int64_t)(63 - mmgr_muto_clz(n) + s) * 78913) >> 18);
    int p = (int)sig - 1 - e;

    /* One call site on purpose. The scale is inlined wherever it is written, so a second one here
       would be a second copy of the whole engine in this translation unit.

       The counter is the backstop, not the exit - the loop leaves through the break. The estimate
       is never more than one power of ten out, so one correction is all it ever takes. */
    mmgr_u64 mant = 0U;
    for (unsigned guard = 0; guard < 4U; guard++) /* GCOVR_EXCL_BR_LINE */
    {
        mant = mmgr_muto_scale_to_u64(n, s, p, 0U);
        if (mant >= limit)
        {
            e++;
            p--;
        }
        else if ((sig > 1U) && (mant < (limit / 10U)))
        {
            e--;
            p++;
        }
        else
        {
            break;
        }
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
    /* Same order as g: the one look, then the rejection, then the work. Negating the value
     * afterwards flips the sign bit and leaves the exponent alone, so this reading stays good. */
    const mmgr_u64 klass = mmgr_fract_exp(v);

    if (klass == MMGR_DBL_EXP_ALL)
    {
        if (mmgr_fract_mant(v) != 0U)
        {
            mmgr_verba_put(b, "nan");
            return;
        }
        if (mmgr_fract_sign(v) != 0U)
        {
            mmgr_verba_ch(b, '-');
        }
        mmgr_verba_put(b, "inf");
        return;
    }

    if (mmgr_fract_sign(v) != 0U)
    {
        mmgr_verba_ch(b, '-');
        v = -v;
    }

    /* Past what a 64 bit integer part can hold there are no fixed digits to write, so it goes to
     * the exponent form instead. */
    if (klass >= (MMGR_DBL_BIAS + 64))
    {
        mmgr_verba_g(b, v, 10);
        return;
    }
    if (decimals > MMGR_FIXED_MAX_DECIMALS)
    {
        decimals = MMGR_FIXED_MAX_DECIMALS;
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

    /* The whole part, and the bits left over that are not part of it. Both exact: one is a shift
     * of the mantissa and the other is what the shift left behind. */
    mmgr_u64 ip = 0U;
    mmgr_u64 rem = 0U;

    if (exp2 >= 0)
    {
        ip = mant << (unsigned)exp2;
    }
    else
    {
        const unsigned shift = (unsigned)(-exp2);

        if (shift < 64U)
        {
            ip = mant >> shift;
            rem = mant - (ip << shift);
        }
        else
        {
            rem = mant; /* the value is below one, so all of it is the remainder */
        }
    }

    /* The digits after the point are the remainder scaled by ten to the decimals and rounded, and
     * that is the engine's job rather than something to open code with shifts. It was open coded,
     * and it took the scale off the remainder only when the shift was under 64 and then shifted a
     * word by more than its width, which C does not define. */
    mmgr_u64 frac = mmgr_muto_scale_to_u64(rem, exp2, (int)decimals,
                                           (decimals == 0U) ? (unsigned)(ip & 1U) : 0U);

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
