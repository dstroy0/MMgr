// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/membuild/membuild.h"
#include "mmgr/float_bits/float_bits.h"
#include "mmgr/protostr/protostr.h"
#include "mmgr/rawmemcpy/rawmemcpy.h"
#include "shared/hex/hex.h"

#define PROTOCORE_G_WORK_BITS 58u

void protocore_sb_put_n(protocore_sb *b, const char *s, size_t sl)
{
    if (!b->ok)
    {
        return;
    }
    if (b->len + sl >= b->cap)
    {
        b->ok = PROTO_FALSE;
        return;
    }

    proto_raw_read(b->p + b->len, s, sl);
    b->len += sl;
}

void protocore_sb_put(protocore_sb *b, const char *s)
{
    if (!b->ok)
    {
        return;
    }
    protocore_sb_put_n(b, s, str.len(s, b->cap));
}

void protocore_sb_put_clip(protocore_sb *b, const char *s)
{
    if (!b->ok || !s || b->len + 1 >= b->cap)
    {
        return;
    }
    size_t room = b->cap - b->len - 1;
    size_t sl = str.len(s, room);

    proto_raw_read(b->p + b->len, s, sl);
    b->len += sl;
}

void protocore_sb_u64_clip(protocore_sb *b, uint64_t v, uint8_t columns)
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

void protocore_sb_xml(protocore_sb *b, const char *s)
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
            protocore_sb_put(b, rep);
        }
        else
        {
            if (b->len + 1 >= b->cap)
            {
                b->ok = PROTO_FALSE;
                return;
            }
            b->p[b->len] = *s;
            b->len++;
        }
    }
}

void protocore_sb_ch(protocore_sb *b, char c)
{
    if (!b->ok)
    {
        return;
    }
    if (b->len + 1 >= b->cap)
    {
        b->ok = PROTO_FALSE;
        return;
    }
    b->p[b->len++] = c;
}

void protocore_sb_uint(protocore_sb *b, uint64_t v, unsigned base, unsigned min_digits)
{
    if (!b->ok)
    {
        return;
    }

    const unsigned bits_per_digit = (base == 16) ? 4u : (base == 8) ? 3u : 0u;
    const proto_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ull << bits_per_digit) - 1u) : 0u;

    const proto_bool narrow = !power_of_two && v <= 0xFFFFFFFFu;

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
        while (p32 >= 10u)
        {
            p32 /= 10u;
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
        b->ok = PROTO_FALSE;
        return;
    }
    if (power_of_two)
    {
        for (unsigned i = digits; i-- > 0;)
        {
            b->p[b->len + i] = PROTOCORE_HEX.lower[v & digit_mask];
            v >>= bits_per_digit;
        }
    }
    else if (narrow)
    {
        uint32_t v32 = (uint32_t)v;
        for (unsigned i = digits; i-- > 0;)
        {
            b->p[b->len + i] = (char)('0' + (unsigned)(v32 % 10u));
            v32 /= 10u;
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

void protocore_sb_u32w(protocore_sb *b, uint32_t v, unsigned min_digits)
{
    protocore_sb_uint(b, v, 10, min_digits);
}

void protocore_sb_hex(protocore_sb *b, uint64_t v, unsigned min_digits)
{
    protocore_sb_uint(b, v, 16, min_digits);
}

void protocore_sb_u32(protocore_sb *b, uint32_t v)
{
    protocore_sb_uint(b, v, 10, 1);
}

void protocore_sb_u64(protocore_sb *b, uint64_t v)
{
    protocore_sb_uint(b, v, 10, 1);
}

void protocore_sb_i64(protocore_sb *b, int64_t v)
{

    uint64_t mag = (v < 0) ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
    if (v < 0)
    {
        protocore_sb_ch(b, '-');
    }
    protocore_sb_uint(b, mag, 10, 1);
}

proto_bool protocore_signbit(double v)
{
    return proto_dbl_sign(v) != 0u;
}

proto_bool protocore_isinf(double v)
{
    return proto_dbl_exp(v) == 0x7FFu && proto_dbl_mant(v) == 0u;
}

proto_bool protocore_isnan(double v)
{
    return proto_dbl_exp(v) == 0x7FFu && proto_dbl_mant(v) != 0u;
}

static void sb_digits(protocore_sb *b, uint64_t mant, unsigned digits, unsigned point_after)
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
            protocore_sb_ch(b, '.');
        }
        protocore_sb_ch(b, (char)('0' + (unsigned)((mant / div) % 10)));
        div /= 10;
    }
}

static void g_renorm(proto_u64 *n, proto_i32 *s)
{
    if (*n == 0u)
    {
        return;
    }
    while (*n >= (1ull << PROTOCORE_G_WORK_BITS))
    {
        *n >>= 1;
        *s += 1;
    }
    while (*n < (1ull << (PROTOCORE_G_WORK_BITS - 1u)))
    {
        *n <<= 1;
        *s -= 1;
    }
}

static void g_mul10(proto_u64 *n, proto_i32 *s)
{
    *n *= 10u;
    g_renorm(n, s);
}

static void g_div10(proto_u64 *n, proto_i32 *s)
{
    *n <<= 4;
    *s -= 4;
    *n /= 10u;
    g_renorm(n, s);
}

static proto_u64 g_round(proto_u64 n, proto_i32 s)
{
    if (s >= 0)
    {
        return n << (unsigned)s;
    }
    unsigned sh = (unsigned)(-s);
    if (sh >= 64u)
    {
        return 0u;
    }
    proto_u64 r = n >> sh;
    proto_u64 rem = n - (r << sh);
    proto_u64 half = 1ull << (sh - 1u);
    if (rem > half || (rem == half && (r & 1u) != 0u))
    {
        r++;
    }
    return r;
}

void protocore_sb_g(protocore_sb *b, double v, unsigned sig)
{
    if (!b->ok)
    {
        return;
    }
    if (sig == 0)
    {
        sig = 1;
    }
    if (protocore_isnan(v))
    {
        protocore_sb_put(b, "nan");
        return;
    }

    if (protocore_signbit(v))
    {
        protocore_sb_ch(b, '-');
        v = -v;
    }
    if (protocore_isinf(v))
    {
        protocore_sb_put(b, "inf");
        return;
    }
    proto_u64 be = proto_dbl_exp(v);
    proto_u64 n = proto_dbl_mant(v);
    if (be == 0u && n == 0u)
    {
        protocore_sb_ch(b, '0');
        return;
    }

    proto_i32 s = 1 - PROTO_DBL_BIAS - (proto_i32)PROTO_DBL_MANT_BITS;
    if (be != 0u)
    {
        n |= 1ull << PROTO_DBL_MANT_BITS;
        s = (proto_i32)be - PROTO_DBL_BIAS - (proto_i32)PROTO_DBL_MANT_BITS;
    }
    g_renorm(&n, &s);

    proto_u64 limit = 1u;
    for (unsigned i = 0; i < sig; i++)
    {
        limit *= 10u;
    }

    int e = (int)(((int64_t)((int)PROTOCORE_G_WORK_BITS - 1 + s) * 78913) >> 18);

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

    proto_u64 mant = g_round(n, s);
    for (unsigned guard = 0; guard < 4u; guard++)
    {
        if (mant >= limit)
        {
            g_div10(&n, &s);
            e++;
        }
        else if (sig > 1u && mant < limit / 10u)
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
        protocore_sb_ch(b, 'e');
        protocore_sb_ch(b, e < 0 ? '-' : '+');
        unsigned mag = (unsigned)(e < 0 ? -e : e);
        protocore_sb_u32w(b, mag, 2);
        return;
    }
    if (e >= (int)digits - 1)
    {
        sb_digits(b, mant, digits, 0);
        for (int i = 0; i < e - (int)digits + 1; i++)
        {
            protocore_sb_ch(b, '0');
        }
        return;
    }
    if (e >= 0)
    {
        sb_digits(b, mant, digits, (unsigned)e + 1);
        return;
    }
    protocore_sb_put(b, "0.");
    for (int i = 0; i < -e - 1; i++)
    {
        protocore_sb_ch(b, '0');
    }
    sb_digits(b, mant, digits, 0);
}

void protocore_sb_fixed(protocore_sb *b, double v, unsigned decimals)
{
    if (!b->ok)
    {
        return;
    }
    if (protocore_isnan(v))
    {
        protocore_sb_put(b, "nan");
        return;
    }

    if (protocore_signbit(v))
    {
        protocore_sb_ch(b, '-');
        v = -v;
    }
    if (protocore_isinf(v))
    {
        protocore_sb_put(b, "inf");
        return;
    }

    if (proto_dbl_exp(v) >= (PROTO_DBL_BIAS + 64))
    {
        protocore_sb_g(b, v, 10);
        return;
    }
    if (decimals > 18u)
    {
        decimals = 18u;
    }
    proto_u64 scale = 1u;
    for (unsigned i = 0; i < decimals; i++)
    {
        scale *= 10u;
    }

    proto_u64 mant = proto_dbl_mant(v);
    proto_u64 be = proto_dbl_exp(v);
    proto_i32 exp2 = 1 - PROTO_DBL_BIAS - (proto_i32)PROTO_DBL_MANT_BITS;
    if (be != 0u)
    {
        mant |= 1ull << PROTO_DBL_MANT_BITS;
        exp2 = (proto_i32)be - PROTO_DBL_BIAS - (proto_i32)PROTO_DBL_MANT_BITS;
    }

    proto_u64 ip = 0u;
    proto_u64 frac = 0u;
    if (exp2 >= 0)
    {
        ip = mant << (unsigned)exp2;
    }
    else
    {
        unsigned shift = (unsigned)(-exp2);
        proto_u64 num = mant;
        if (shift < 64u)
        {
            ip = mant >> shift;
            num = mant - (ip << shift);
        }

        if (shift > 60u)
        {
            num >>= (shift - 60u);
            shift = 60u;
        }
        const proto_u64 den = 1ull << shift;
        for (unsigned i = 0; i < decimals; i++)
        {
            num *= 10u;
            proto_u64 digit = num >> shift;
            num -= digit << shift;
            frac = frac * 10u + digit;
        }

        proto_u64 twice = num * 2u;
        if (twice > den || (twice == den && (frac & 1u) != 0u))
        {
            frac++;
        }
    }
    if (frac >= scale)
    {
        ip++;
        frac = 0u;
    }
    protocore_sb_u64(b, ip);
    if (decimals)
    {
        protocore_sb_ch(b, '.');
        protocore_sb_uint(b, frac, 10, decimals);
    }
}

static const char JSON_CTRL_ESC[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0};

void protocore_sb_json(protocore_sb *b, const char *s)
{
    static const char HEX[] = "0123456789abcdef";

    if (!b->ok)
    {
        return;
    }
    protocore_sb_put(b, "\"");

    for (const char *p = s ? s : ""; *p; p++)
    {
        const unsigned char c = (unsigned char)*p;
        const char two = (c == '"' || c == '\\') ? (char)c : (c < 0x20u ? JSON_CTRL_ESC[c] : 0);
        if (two)
        {
            if (b->len + 2 >= b->cap)
            {
                b->ok = PROTO_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = two;
        }
        else if (c < 0x20u)
        {
            if (b->len + 6 >= b->cap)
            {
                b->ok = PROTO_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = 'u';
            b->p[b->len++] = '0';
            b->p[b->len++] = '0';
            b->p[b->len++] = HEX[(c >> 4) & 0xFu];
            b->p[b->len++] = HEX[c & 0xFu];
        }
        else if (b->len + 1 < b->cap)
        {
            b->p[b->len++] = (char)c;
        }
        else
        {
            b->ok = PROTO_FALSE;
        }
    }
    protocore_sb_put(b, "\"");
}

size_t protocore_sb_finish(protocore_sb *b)
{

    if (!b->ok || b->cap == 0)
    {
        return 0;
    }
    b->p[b->len] = '\0';
    return b->len;
}
