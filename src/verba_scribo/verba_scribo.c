#include "verba_scribo/verba_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"
#include "fractio/fractio.h"
#include "proximus_operor/proximus_operor.h"
#include "transformo/transformo.h"

static const char mmgr_hex_lower[] = "0123456789abcdef";

static const char JSON_CTRL_ESC[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0,   0,   0,   0, 0,   0,   0, 0};

#define MMGR_VERBA_POW10_MAX 19u

static const uint64_t mmgr_verba_pow10[MMGR_VERBA_POW10_MAX + 1u] = {
    1ull,                 10ull,                 100ull,                 1000ull,
    10000ull,             100000ull,             1000000ull,             10000000ull,
    100000000ull,         1000000000ull,         10000000000ull,         100000000000ull,
    1000000000000ull,     10000000000000ull,     100000000000000ull,     1000000000000000ull,
    10000000000000000ull, 100000000000000000ull, 1000000000000000000ull, 10000000000000000000ull};

typedef struct
{
    char *out;
    size_t cap;
    size_t at;
    const char *text;
    size_t text_len;
    char ch;
    uint64_t val;
    int64_t sval;
    double real;
    uint8_t base;
    uint8_t min;
    uint8_t columns;
    uint8_t sig;
    uint8_t decimals;
    uint64_t mant;
    mmgr_u64 bits;
    uint8_t digits;
    uint8_t point_after;
} VerbaCtx;

MMGR_INLINE mmgr_bool verba_room(const VerbaCtx *c, size_t want)
{
    return (mmgr_bool)((c->at < c->cap) && (want <= ((c->cap - c->at) - 1u)));
}

MMGR_INLINE size_t verba_put_n(const VerbaCtx *c)
{
    if (!verba_room(c, c->text_len))
    {
        return c->cap;
    }

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->out + c->at, .at = c->text, .size = c->text_len);
    return c->at + c->text_len;
}

MMGR_INLINE size_t verba_put(const VerbaCtx *c)
{
    const size_t sl = MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = c->text, .cap = c->cap);

    return MMGR_CALL(verba_put_n, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text, .text_len = sl);
}

MMGR_INLINE size_t verba_put_clip(const VerbaCtx *c)
{
    if ((c->text == NULL) || (c->at >= c->cap))
    {
        return c->at;
    }

    const size_t room = (c->cap - c->at) - 1u;
    const size_t sl = MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = c->text, .cap = room);

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->out + c->at, .at = c->text, .size = sl);
    return c->at + sl;
}

MMGR_INLINE size_t verba_ch(const VerbaCtx *c)
{
    if (!verba_room(c, 1u))
    {
        return c->cap;
    }

    c->out[c->at] = c->ch;
    return c->at + 1u;
}

MMGR_INLINE size_t verba_u64_clip(const VerbaCtx *c)
{
    uint64_t v = c->val;
    size_t digits = 1;

    while ((digits <= MMGR_VERBA_POW10_MAX) && (v >= mmgr_verba_pow10[digits]))
    {
        digits++;
    }

    const size_t width = (digits < c->columns) ? c->columns : digits;

    if (!verba_room(c, width))
    {
        return c->at;
    }

    for (size_t i = width - digits; i-- > 0;)
    {
        c->out[c->at + i] = ' ';
    }
    for (size_t i = width; i-- > (width - digits);)
    {
        c->out[c->at + i] = (char)('0' + (mmgr_word)(v % 10));
        v /= 10;
    }
    return c->at + width;
}

MMGR_INLINE size_t verba_uint(const VerbaCtx *c)
{
    uint64_t v = c->val;
    const mmgr_word bits_per_digit = (c->base == 16) ? 4U : ((c->base == 8) ? 3U : 0U);
    const mmgr_bool power_of_two = bits_per_digit != 0;
    const uint64_t digit_mask = power_of_two ? ((1ULL << bits_per_digit) - 1U) : 0U;
    const mmgr_bool narrow = !power_of_two && (v <= 0xFFFFFFFFU);

    mmgr_word digits = 1;
    if (power_of_two)
    {
        uint64_t probe = v;

        while ((probe >>= bits_per_digit) != 0)
        {
            digits++;
        }
    }
    else
    {
        while ((digits <= MMGR_VERBA_POW10_MAX) && (v >= mmgr_verba_pow10[digits]))
        {
            digits++;
        }
    }

    if (digits < c->min)
    {
        digits = c->min;
    }
    if (!verba_room(c, digits))
    {
        return c->cap;
    }

    if (power_of_two)
    {
        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = mmgr_hex_lower[v & digit_mask];
            v >>= bits_per_digit;
        }
    }
    else if (narrow)
    {
        uint32_t v32 = (uint32_t)v;

        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = (char)('0' + (mmgr_word)(v32 % 10U));
            v32 /= 10U;
        }
    }
    else
    {
        for (mmgr_word i = digits; i-- > 0;)
        {
            c->out[c->at + i] = (char)('0' + (mmgr_word)(v % 10));
            v /= 10;
        }
    }
    return c->at + digits;
}

MMGR_INLINE size_t verba_i64(const VerbaCtx *c)
{
    const int64_t sv = c->sval;
    size_t at = c->at;

    if (sv < 0)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                     .val = (sv < 0) ? ((uint64_t)(-(sv + 1)) + 1U) : (uint64_t)sv, .base = 10u, .min = 1u);
}

MMGR_INLINE size_t verba_zeros(const VerbaCtx *c)
{
    size_t n = c->text_len;
    size_t at = c->at;

    while (n-- != 0u)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '0');
    }
    return at;
}

MMGR_INLINE size_t verba_xml(const VerbaCtx *c)
{
    size_t at = c->at;

    if (c->text == NULL)
    {
        return at;
    }

    for (const char *p = c->text; *p; p++)
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
            at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = rep);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = *p);
        }
    }
    return at;
}

MMGR_INLINE size_t verba_json(const VerbaCtx *c)
{
    const char *const src = (c->text != NULL) ? c->text : "";
    size_t at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = "\"");

    for (const char *p = src; *p; p++)
    {
        const uint8_t ch = (uint8_t)*p;
        const char two = ((ch == '"') || (ch == '\\')) ? (char)ch : ((ch < 0x20U) ? JSON_CTRL_ESC[ch] : 0);

        if (two != 0)
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '\\');
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = two);
        }
        else if (ch < 0x20U)
        {
            at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "\\u00");
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                           .ch = mmgr_hex_lower[(ch >> 4) & 0xFU]);
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = mmgr_hex_lower[ch & 0xFU]);
        }
        else
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (char)ch);
        }
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "\"");
}

MMGR_INLINE size_t verba_digits(const VerbaCtx *c)
{
    uint64_t left = c->mant;
    uint64_t div = mmgr_verba_pow10[c->digits - 1u];
    size_t at = c->at;

    for (uint8_t i = 0; i < c->digits; i++)
    {
        if ((i == c->point_after) && (i != 0))
        {
            at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '.');
        }

        const uint64_t d = left / div;

        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (char)('0' + (mmgr_word)d));
        left -= d * div;
        div /= 10u;
    }
    return at;
}

MMGR_INLINE mmgr_u64 verba_bits(const VerbaCtx *c)
{
    return MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);
}

MMGR_INLINE mmgr_u64 verba_exp(const VerbaCtx *c)
{
    return MMGR_CALL(fract.exp, FractioCfg, .bits = c->bits);
}

MMGR_INLINE mmgr_u64 verba_mant(const VerbaCtx *c)
{
    return MMGR_CALL(fract.mant, FractioCfg, .bits = c->bits);
}

MMGR_INLINE mmgr_u64 verba_sign(const VerbaCtx *c)
{
    return MMGR_CALL(fract.sign, FractioCfg, .bits = c->bits);
}

MMGR_INLINE size_t verba_non_finite(const VerbaCtx *c)
{
    if (MMGR_CALL(verba_mant, VerbaCtx, .bits = c->bits) != 0U)
    {
        return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = "nan");
    }

    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = c->bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }
    return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "inf");
}

MMGR_INLINE size_t verba_g(const VerbaCtx *c)
{
    const mmgr_u64 bits = verba_bits(c);

    if (MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .bits = bits);
    }

    uint8_t sig = (c->sig == 0) ? 1u : c->sig;
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }

    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
    }

    const mmgr_u64 be = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);
    mmgr_u64 n = MMGR_CALL(verba_mant, VerbaCtx, .bits = bits);

    if ((be == 0U) && (n == 0U))
    {
        return MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '0');
    }

    mmgr_iword s = 1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS;
    if (be != 0U)
    {
        n |= 1ULL << MMGR_DBL_MANT_BITS;
        s = (mmgr_iword)(be - MMGR_DBL_BIAS - MMGR_DBL_MANT_BITS);
    }

    const mmgr_u64 limit = mmgr_verba_pow10[sig];

    mmgr_iword e = (mmgr_iword)(((mmgr_i64)(63 - MMGR_CALL(clz.lead, ClzCfg, .val = n) + s) * 78913) >> 18);
    mmgr_iword p = (mmgr_iword)((mmgr_iword)sig - 1 - e);
    mmgr_u64 mant = 0U;

    for (uint8_t guard = 0; guard < 4U; guard++)
    {
        mant = MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &n, .e2 = s, .ex = p, .above = 0U);
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

    uint8_t digits = sig;
    while ((digits > 1u) && ((mant % 10u) == 0u))
    {
        mant /= 10u;
        digits--;
    }

    if ((e < -4) || (e >= (mmgr_i32)sig))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                       .point_after = 1u);
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = 'e');
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = (e < 0) ? '-' : '+');
        return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                         .val = (uint64_t)((e < 0) ? -e : e), .base = 10u, .min = 2u);
    }
    if (e >= ((mmgr_i32)digits - 1))
    {
        at = MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                       .point_after = 0u);
        return MMGR_CALL(verba_zeros, VerbaCtx, .out = c->out, .cap = c->cap, .at = at,
                         .text_len = (size_t)(e - (mmgr_i32)digits + 1));
    }
    if (e >= 0)
    {
        return MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                         .point_after = (uint8_t)(e + 1));
    }

    at = MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text = "0.");
    at = MMGR_CALL(verba_zeros, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .text_len = (size_t)(-e - 1));
    return MMGR_CALL(verba_digits, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .mant = mant, .digits = digits,
                     .point_after = 0u);
}

MMGR_INLINE size_t verba_fixed(const VerbaCtx *c)
{
    const mmgr_u64 bits = verba_bits(c);
    const mmgr_u64 klass = MMGR_CALL(verba_exp, VerbaCtx, .bits = bits);

    if (klass == MMGR_DBL_EXP_ALL)
    {
        return MMGR_CALL(verba_non_finite, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .bits = bits);
    }

    double v = c->real;
    size_t at = c->at;

    if (MMGR_CALL(verba_sign, VerbaCtx, .bits = bits) != 0U)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '-');
        v = -v;
    }

    if (klass >= (MMGR_DBL_BIAS + 64))
    {
        return MMGR_CALL(verba_g, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .real = v, .sig = 10u);
    }

    uint8_t decimals = c->decimals;
    if (decimals > MMGR_FIXED_MAX_DECIMALS)
    {
        decimals = MMGR_FIXED_MAX_DECIMALS;
    }

    const mmgr_u64 scale = mmgr_verba_pow10[decimals];

    mmgr_u64 mant = MMGR_CALL(verba_mant, VerbaCtx, .bits = bits);
    mmgr_iword exp2 = 1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS;

    if (klass != 0U)
    {
        mant |= 1ULL << MMGR_DBL_MANT_BITS;
        exp2 = (mmgr_iword)(klass - MMGR_DBL_BIAS - MMGR_DBL_MANT_BITS);
    }

    mmgr_u64 ip = 0U;
    mmgr_u64 rem = 0U;

    if (exp2 >= 0)
    {
        ip = mant << (mmgr_word)exp2;
    }
    else
    {
        const mmgr_word shift = (mmgr_word)(-exp2);

        if (shift < 64U)
        {
            ip = mant >> shift;
            rem = mant - (ip << shift);
        }
        else
        {
            rem = mant;
        }
    }

    mmgr_u64 frac = MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &rem, .e2 = exp2, .ex = (mmgr_iword)decimals,
                              .above = (decimals == 0U) ? (mmgr_word)(ip & 1U) : 0U);

    if (frac >= scale)
    {
        ip++;
        frac = 0U;
    }

    at = MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .val = ip, .base = 10u, .min = 1u);

    if (decimals != 0u)
    {
        at = MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .ch = '.');
        at = MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = at, .val = frac, .base = 10u,
                       .min = decimals);
    }
    return at;
}

MMGR_INLINE size_t verba_finish(const VerbaCtx *c)
{
    if (c->at >= c->cap)
    {
        return 0;
    }
    c->out[c->at] = '\0';
    return c->at;
}

MMGR_INLINE mmgr_bool verba_ok(const VerbaCtx *c)
{
    return (mmgr_bool)(c->at < c->cap);
}

size_t mmgr_verba_put_n(const VerbaCfg *c)
{
    return MMGR_CALL(verba_put_n, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text, .text_len = c->text_len);
}

size_t mmgr_verba_put(const VerbaCfg *c)
{
    return MMGR_CALL(verba_put, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text);
}

size_t mmgr_verba_put_clip(const VerbaCfg *c)
{
    return MMGR_CALL(verba_put_clip, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text);
}

size_t mmgr_verba_u64_clip(const VerbaCfg *c)
{
    return MMGR_CALL(verba_u64_clip, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val,
                     .columns = c->columns);
}

size_t mmgr_verba_xml(const VerbaCfg *c)
{
    return MMGR_CALL(verba_xml, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text);
}

size_t mmgr_verba_ch(const VerbaCfg *c)
{
    return MMGR_CALL(verba_ch, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .ch = c->ch);
}

size_t mmgr_verba_uint(const VerbaCfg *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = c->base,
                     .min = c->min);
}

size_t mmgr_verba_u32w(const VerbaCfg *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u,
                     .min = c->min);
}

size_t mmgr_verba_hex(const VerbaCfg *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 16u,
                     .min = c->min);
}

size_t mmgr_verba_u32(const VerbaCfg *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u, .min = 1u);
}

size_t mmgr_verba_u64(const VerbaCfg *c)
{
    return MMGR_CALL(verba_uint, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .val = c->val, .base = 10u, .min = 1u);
}

size_t mmgr_verba_i64(const VerbaCfg *c)
{
    return MMGR_CALL(verba_i64, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .sval = c->sval);
}

size_t mmgr_verba_g(const VerbaCfg *c)
{
    return MMGR_CALL(verba_g, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .real = c->real, .sig = c->sig);
}

size_t mmgr_verba_fixed(const VerbaCfg *c)
{
    return MMGR_CALL(verba_fixed, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .real = c->real, .decimals = c->decimals);
}

size_t mmgr_verba_json(const VerbaCfg *c)
{
    return MMGR_CALL(verba_json, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at, .text = c->text);
}

size_t mmgr_verba_finish(const VerbaCfg *c)
{
    return MMGR_CALL(verba_finish, VerbaCtx, .out = c->out, .cap = c->cap, .at = c->at);
}

mmgr_bool mmgr_verba_ok(const VerbaCfg *c)
{
    return MMGR_CALL(verba_ok, VerbaCtx, .cap = c->cap, .at = c->at);
}

mmgr_bool mmgr_verba_sign_bit(const VerbaCfg *c)
{
    return (mmgr_bool)(MMGR_CALL(verba_sign, VerbaCtx, .bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real)) != 0U);
}

mmgr_bool mmgr_verba_is_inf(const VerbaCfg *c)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);

    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) == 0U));
}

mmgr_bool mmgr_verba_is_nan(const VerbaCfg *c)
{
    const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = c->real);

    return (mmgr_bool)((MMGR_CALL(verba_exp, VerbaCtx, .bits = bits) == MMGR_DBL_EXP_ALL) &&
                       (MMGR_CALL(verba_mant, VerbaCtx, .bits = bits) != 0U));
}
