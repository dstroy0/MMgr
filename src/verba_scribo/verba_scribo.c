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
 *
 * Every entry below takes one parameter, a pointer to VerbaCtx. A builder and whatever is being
 * appended to it are one append, so they are one context.
 *
 * The latch is what makes deferring the check correct rather than merely convenient: a formatted
 * number is as long as it turns out to be, so unlike a field on the wire its length is not
 * something the caller had. See @ref ref_error_handling.
 */

static const char mmgr_hex_lower[] = "0123456789abcdef";

/** @brief One append: the builder, and what is going into it. */
typedef struct
{
    mmgr_verba *b; /**< The builder. */

    /* what is being appended */
    const char *s;    /**< A string. */
    size_t sl;        /**< Its length, when the caller had one. */
    char c;           /**< One character. */
    uint64_t v;       /**< An integer. */
    int64_t sv;       /**< A signed one. */
    double d;         /**< A double. */
    unsigned base;    /**< Radix, for uint. */
    unsigned min;     /**< Least digits, for uint. */
    uint8_t columns;  /**< Least columns, for the clipping integer. */
    unsigned sig;     /**< Significant digits, for g. */
    unsigned decimals;/**< Digits after the point, for fixed. */

    /* the digit run being laid down */
    uint64_t mant;        /**< The digits. */
    unsigned digits;      /**< How many. */
    unsigned point_after; /**< Where the point goes. Zero means no point. */
} VerbaCtx;

/**
 * @brief Append @c sl bytes of @c s.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_put_n(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok)
    {
        return;
    }
    if ((b->len + c->sl) >= b->cap)
    {
        b->ok = MMGR_FALSE;
        return;
    }

    proxim.read(b->p + b->len, c->s, c->sl);
    b->len += c->sl;
}

/**
 * @brief Append @c s, up to its terminator.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_put(VerbaCtx *c)
{
    if (!c->b->ok)
    {
        return;
    }
    c->sl = cellul.len(c->s, c->b->cap);
    verba_put_n(c);
}

/**
 * @brief Append as much of @c s as fits, without latching.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_put_clip(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok || (c->s == NULL) || ((b->len + 1u) >= b->cap))
    {
        return;
    }

    const size_t room = b->cap - b->len - 1u;
    const size_t sl = cellul.len(c->s, room);

    proxim.read(b->p + b->len, c->s, sl);
    b->len += sl;
}

/**
 * @brief Append one character.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_ch(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok)
    {
        return;
    }
    if ((b->len + 1u) >= b->cap)
    {
        b->ok = MMGR_FALSE;
        return;
    }
    b->p[b->len++] = c->c;
}

/**
 * @brief Append @c v right aligned in at least @c columns, without latching.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_u64_clip(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok)
    {
        return;
    }

    uint64_t v = c->v;
    uint64_t probe = v;
    size_t digits = 1;
    while (probe >= 10)
    {
        probe /= 10;
        digits++;
    }

    const size_t width = (digits < c->columns) ? c->columns : digits;
    if ((b->len + width) >= b->cap)
    {
        return;
    }
    for (size_t i = width - digits; i-- > 0;)
    {
        b->p[b->len + i] = ' ';
    }
    for (size_t i = width; i-- > (width - digits);)
    {
        b->p[b->len + i] = (char)('0' + (unsigned)(v % 10));
        v /= 10;
    }
    b->len += width;
}

/**
 * @brief Append @c v in @c base, padded to at least @c min digits.
 * @param c In/out. The append.
 *
 * A power of two radix shifts, a narrow decimal divides in 32 bits, and a wide one in 64. The
 * three are separate because a 64 bit divide is the expensive case and most values are not it.
 */
MMGR_INLINE void verba_uint(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok)
    {
        return;
    }

    uint64_t v = c->v;
    const unsigned bits_per_digit = (c->base == 16) ? 4U : ((c->base == 8) ? 3U : 0U);
    const mmgr_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ULL << bits_per_digit) - 1U) : 0U;
    const mmgr_bool narrow = !power_of_two && (v <= 0xFFFFFFFFU);

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

    if (digits < c->min)
    {
        digits = c->min;
    }
    if ((b->len + digits) >= b->cap)
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

/**
 * @brief Append a signed integer.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_i64(VerbaCtx *c)
{
    const int64_t sv = c->sv;

    if (sv < 0)
    {
        c->c = '-';
        verba_ch(c);
    }
    c->v = (sv < 0) ? ((uint64_t)(-(sv + 1)) + 1U) : (uint64_t)sv;
    c->base = 10;
    c->min = 1;
    verba_uint(c);
}

/**
 * @brief Append @c s with the four XML metacharacters replaced.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_xml(VerbaCtx *c)
{
    mmgr_verba *b = c->b;

    if (!b->ok || (c->s == NULL))
    {
        return;
    }

    for (const char *p = c->s; *p; p++)
    {
        const char *rep = NULL;
        switch (*p)
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
        if (rep != NULL)
        {
            c->s = rep;
            verba_put(c);
            c->s = p;
        }
        else
        {
            if ((b->len + 1u) >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len] = *p;
            b->len++;
        }
    }
}

static const char JSON_CTRL_ESC[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0};

/**
 * @brief Append @c s as a quoted JSON string.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_json(VerbaCtx *c)
{
    static const char HEX[] = "0123456789abcdef";
    mmgr_verba *b = c->b;

    if (!b->ok)
    {
        return;
    }

    const char *src = (c->s != NULL) ? c->s : "";

    c->s = "\"";
    verba_put(c);

    for (const char *p = src; *p; p++)
    {
        const unsigned char ch = (unsigned char)*p;
        const char two = ((ch == '"') || (ch == '\\')) ? (char)ch : ((ch < 0x20U) ? JSON_CTRL_ESC[ch] : 0);

        if (two != 0)
        {
            if ((b->len + 2u) >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = two;
        }
        else if (ch < 0x20U)
        {
            if ((b->len + 6u) >= b->cap)
            {
                b->ok = MMGR_FALSE;
                return;
            }
            b->p[b->len++] = '\\';
            b->p[b->len++] = 'u';
            b->p[b->len++] = '0';
            b->p[b->len++] = '0';
            b->p[b->len++] = HEX[(ch >> 4) & 0xFU];
            b->p[b->len++] = HEX[ch & 0xFU];
        }
        else if ((b->len + 1u) < b->cap)
        {
            b->p[b->len++] = (char)ch;
        }
        else
        {
            b->ok = MMGR_FALSE;
        }
    }

    c->s = "\"";
    verba_put(c);
}

/**
 * @brief Append @c digits digits of @c mant, with a point after @c point_after of them.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_digits(VerbaCtx *c)
{
    uint64_t div = 1;

    for (unsigned i = 1; i < c->digits; i++)
    {
        div *= 10;
    }
    for (unsigned i = 0; i < c->digits; i++)
    {
        if ((i == c->point_after) && (i != 0))
        {
            c->c = '.';
            verba_ch(c);
        }
        c->c = (char)('0' + (unsigned)((c->mant / div) % 10));
        verba_ch(c);
        div /= 10;
    }
}

/**
 * @brief Append the name of a value that is not a number.
 * @param c In/out. The append.
 * @return MMGR_TRUE if it was one, so the caller stops.
 *
 * An exponent field of all ones is an infinity or a nan and nothing else is either, so a single
 * compare turns both away before anything else is decided.
 */
MMGR_INLINE mmgr_bool verba_non_finite(VerbaCtx *c)
{
    if (fract.exp(c->d) != MMGR_DBL_EXP_ALL)
    {
        return MMGR_FALSE;
    }

    if (fract.mant(c->d) != 0U)
    {
        c->s = "nan";
        verba_put(c);
        return MMGR_TRUE;
    }
    if (fract.sign(c->d) != 0U)
    {
        c->c = '-';
        verba_ch(c);
    }
    c->s = "inf";
    verba_put(c);
    return MMGR_TRUE;
}

/**
 * @brief Append the value in the shorter of fixed and exponent form.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_g(VerbaCtx *c)
{
    if (!c->b->ok)
    {
        return;
    }
    if (verba_non_finite(c))
    {
        return;
    }

    /* A number from here, so a digit count is worth having an opinion about. */
    unsigned sig = (c->sig == 0) ? 1u : c->sig;
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }

    double v = c->d;
    if (fract.sign(v) != 0U)
    {
        c->c = '-';
        verba_ch(c);
        v = -v;
    }

    const mmgr_u64 be = fract.exp(v);
    mmgr_u64 n = fract.mant(v);
    if ((be == 0U) && (n == 0U))
    {
        c->c = '0';
        verba_ch(c);
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
    int e = (int)(((int64_t)(63 - muto.clz(n) + s) * 78913) >> 18);
    int p = (int)sig - 1 - e;

    /* One call site on purpose. The scale is a call now rather than an inline body, but a second
       site is still a second frame set up for the same work.

       The counter is the backstop, not the exit - the loop leaves through the break. The estimate
       is never more than one power of ten out, so one correction is all it ever takes. */
    mmgr_u64 mant = 0U;
    for (unsigned guard = 0; guard < 4U; guard++) /* GCOVR_EXCL_BR_LINE */
    {
        mant = muto.scale_to_u64(n, s, p, 0U);
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
    while ((digits > 1) && ((mant % 10) == 0))
    {
        mant /= 10;
        digits--;
    }

    c->mant = mant;
    c->digits = digits;

    if ((e < -4) || (e >= (int)sig))
    {
        c->point_after = 1;
        verba_digits(c);
        c->c = 'e';
        verba_ch(c);
        c->c = (e < 0) ? '-' : '+';
        verba_ch(c);
        c->v = (uint64_t)((e < 0) ? -e : e);
        c->base = 10;
        c->min = 2;
        verba_uint(c);
        return;
    }
    if (e >= ((int)digits - 1))
    {
        c->point_after = 0;
        verba_digits(c);
        for (int i = 0; i < (e - (int)digits + 1); i++)
        {
            c->c = '0';
            verba_ch(c);
        }
        return;
    }
    if (e >= 0)
    {
        c->point_after = (unsigned)e + 1u;
        verba_digits(c);
        return;
    }

    c->s = "0.";
    verba_put(c);
    for (int i = 0; i < (-e - 1); i++)
    {
        c->c = '0';
        verba_ch(c);
    }
    c->mant = mant;
    c->digits = digits;
    c->point_after = 0;
    verba_digits(c);
}

/**
 * @brief Append the value with exactly @c decimals digits after the point.
 * @param c In/out. The append.
 */
MMGR_INLINE void verba_fixed(VerbaCtx *c)
{
    if (!c->b->ok)
    {
        return;
    }

    /* Same order as g: the one look, then the rejection, then the work. Negating the value
     * afterwards flips the sign bit and leaves the exponent alone, so this reading stays good. */
    const mmgr_u64 klass = fract.exp(c->d);
    if (verba_non_finite(c))
    {
        return;
    }

    double v = c->d;
    if (fract.sign(v) != 0U)
    {
        c->c = '-';
        verba_ch(c);
        v = -v;
    }

    /* Past what a 64 bit integer part can hold there are no fixed digits to write, so it goes to
     * the exponent form instead. */
    if (klass >= (MMGR_DBL_BIAS + 64))
    {
        c->d = v;
        c->sig = 10;
        verba_g(c);
        return;
    }

    unsigned decimals = c->decimals;
    if (decimals > MMGR_FIXED_MAX_DECIMALS)
    {
        decimals = MMGR_FIXED_MAX_DECIMALS;
    }

    mmgr_u64 scale = 1U;
    for (unsigned i = 0; i < decimals; i++)
    {
        scale *= 10U;
    }

    mmgr_u64 mant = fract.mant(v);
    const mmgr_u64 be = fract.exp(v);
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
    mmgr_u64 frac = muto.scale_to_u64(rem, exp2, (int)decimals, (decimals == 0U) ? (unsigned)(ip & 1U) : 0U);

    if (frac >= scale)
    {
        ip++;
        frac = 0U;
    }

    c->v = ip;
    c->base = 10;
    c->min = 1;
    verba_uint(c);

    if (decimals != 0u)
    {
        c->c = '.';
        verba_ch(c);
        c->v = frac;
        c->base = 10;
        c->min = decimals;
        verba_uint(c);
    }
}

/**
 * @brief Terminate the buffer and say how much is in it.
 * @param b The builder.
 * @return Byte count, or zero if anything failed to fit.
 *
 * No context. There is nothing being appended, so there is no argument list to group.
 */
MMGR_INLINE size_t verba_finish(mmgr_verba *b)
{
    if (!b->ok || (b->cap == 0))
    {
        return 0;
    }
    b->p[b->len] = '\0';
    return b->len;
}

void mmgr_verba_put_n(mmgr_verba *b, const char *s, size_t sl)
{
    MMGR_CALL(verba_put_n, VerbaCtx, .b = b, .s = s, .sl = sl);
}

void mmgr_verba_put(mmgr_verba *b, const char *s)
{
    MMGR_CALL(verba_put, VerbaCtx, .b = b, .s = s);
}

void mmgr_verba_put_clip(mmgr_verba *b, const char *s)
{
    MMGR_CALL(verba_put_clip, VerbaCtx, .b = b, .s = s);
}

void mmgr_verba_u64_clip(mmgr_verba *b, uint64_t v, uint8_t columns)
{
    MMGR_CALL(verba_u64_clip, VerbaCtx, .b = b, .v = v, .columns = columns);
}

void mmgr_verba_xml(mmgr_verba *b, const char *s)
{
    MMGR_CALL(verba_xml, VerbaCtx, .b = b, .s = s);
}

void mmgr_verba_ch(mmgr_verba *b, char c)
{
    MMGR_CALL(verba_ch, VerbaCtx, .b = b, .c = c);
}

void mmgr_verba_uint(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits)
{
    MMGR_CALL(verba_uint, VerbaCtx, .b = b, .v = v, .base = base, .min = min_digits);
}

void mmgr_verba_u32w(mmgr_verba *b, uint32_t v, unsigned min_digits)
{
    MMGR_CALL(verba_uint, VerbaCtx, .b = b, .v = v, .base = 10, .min = min_digits);
}

void mmgr_verba_hex(mmgr_verba *b, uint64_t v, unsigned min_digits)
{
    MMGR_CALL(verba_uint, VerbaCtx, .b = b, .v = v, .base = 16, .min = min_digits);
}

void mmgr_verba_u32(mmgr_verba *b, uint32_t v)
{
    MMGR_CALL(verba_uint, VerbaCtx, .b = b, .v = v, .base = 10, .min = 1);
}

void mmgr_verba_u64(mmgr_verba *b, uint64_t v)
{
    MMGR_CALL(verba_uint, VerbaCtx, .b = b, .v = v, .base = 10, .min = 1);
}

void mmgr_verba_i64(mmgr_verba *b, int64_t v)
{
    MMGR_CALL(verba_i64, VerbaCtx, .b = b, .sv = v);
}

void mmgr_verba_g(mmgr_verba *b, double v, unsigned sig)
{
    MMGR_CALL(verba_g, VerbaCtx, .b = b, .d = v, .sig = sig);
}

void mmgr_verba_fixed(mmgr_verba *b, double v, unsigned decimals)
{
    MMGR_CALL(verba_fixed, VerbaCtx, .b = b, .d = v, .decimals = decimals);
}

void mmgr_verba_json(mmgr_verba *b, const char *s)
{
    MMGR_CALL(verba_json, VerbaCtx, .b = b, .s = s);
}

size_t mmgr_verba_finish(mmgr_verba *b)
{
    return verba_finish(b);
}

mmgr_bool mmgr_signbit(double v)
{
    return fract.sign(v) != 0U;
}

mmgr_bool mmgr_isinf(double v)
{
    return (fract.exp(v) == 0x7FFU) && (fract.mant(v) == 0U);
}

mmgr_bool mmgr_isnan(double v)
{
    return (fract.exp(v) == 0x7FFU) && (fract.mant(v) != 0U);
}
