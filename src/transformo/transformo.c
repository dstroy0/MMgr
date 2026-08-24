#include "transformo/transformo.h"
#include "clz/clz.h"


#define MMGR_MUTO_EXACT_POW10 22

static const double mmgr_muto_ten[MMGR_MUTO_EXACT_POW10 + 1] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                                1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                                1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

typedef struct
{
        mmgr_u64 *mant;     char digit;         mmgr_iword e2;      mmgr_iword ex;      mmgr_iword dropped; mmgr_word above;     mmgr_bool neg;
        mmgr_u64 hi;
    mmgr_u64 lo;
    mmgr_iword fe2;
    mmgr_iword rest;
        mmgr_u64 a;
    mmgr_u64 b;
    mmgr_u64 phi;
    mmgr_u64 plo;

    const MmgrPow5 *pow; } MutoCtx;

MMGR_INLINE mmgr_bool muto_take(const MutoCtx *c)
{
    if (*c->mant > MMGR_MUTO_MANT_MAX)
    {
        return MMGR_FALSE;
    }
    *c->mant = (*c->mant * 10u) + (mmgr_u64)(c->digit - '0');
    return MMGR_TRUE;
}

MMGR_INLINE void muto_mul(MutoCtx *c)
{
    const mmgr_u64 half = (mmgr_u64)0xFFFFFFFFu;
    const mmgr_u64 a0 = c->a & half;
    const mmgr_u64 a1 = c->a >> 32;
    const mmgr_u64 b0 = c->b & half;
    const mmgr_u64 b1 = c->b >> 32;

    const mmgr_u64 p00 = a0 * b0;
    const mmgr_u64 p01 = a0 * b1;
    const mmgr_u64 p10 = a1 * b0;
    const mmgr_u64 p11 = a1 * b1;
    const mmgr_u64 mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    c->plo = (p00 & half) | (mid << 32);
    c->phi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

MMGR_INLINE void muto_norm(MutoCtx *c)
{
    if (c->hi == 0u)
    {
        if (c->lo == 0u)
        {
            return;
        }
        c->hi = c->lo;
        c->lo = 0u;
        c->fe2 -= 64;
    }

    const mmgr_iword n = MMGR_CALL(clz.lead, ClzCfg, .val = c->hi);
    if (n != 0)
    {
        c->hi = (c->hi << n) | (c->lo >> (64 - n));
        c->lo <<= n;
        c->fe2 -= n;
    }
}

MMGR_INLINE void muto_mul_pow5(MutoCtx *c)
{
    const mmgr_u64 fhi = c->hi;
    const mmgr_u64 flo = c->lo;
    const mmgr_u64 ghi = c->pow->hi;
    const mmgr_u64 glo = c->pow->lo;

    c->a = fhi;
    c->b = ghi;
    muto_mul(c);
    const mmgr_u64 hh_h = c->phi;
    const mmgr_u64 hh_l = c->plo;

    c->a = fhi;
    c->b = glo;
    muto_mul(c);
    const mmgr_u64 hl_h = c->phi;
    const mmgr_u64 hl_l = c->plo;

    c->a = flo;
    c->b = ghi;
    muto_mul(c);
    const mmgr_u64 lh_h = c->phi;
    const mmgr_u64 lh_l = c->plo;

    c->a = flo;
    c->b = glo;
    muto_mul(c);
    const mmgr_u64 ll_h = c->phi;
    const mmgr_u64 ll_l = c->plo;

        mmgr_u64 carry = 0u;
    mmgr_u64 col1 = ll_h + hl_l;
    carry += (col1 < ll_h) ? 1u : 0u;
    const mmgr_u64 col1b = col1 + lh_l;
    carry += (col1b < col1) ? 1u : 0u;
    col1 = col1b;

    mmgr_u64 col2 = hh_l + hl_h;
    mmgr_u64 carry2 = (col2 < hh_l) ? 1u : 0u;
    const mmgr_u64 col2b = col2 + lh_h;
    carry2 += (col2b < col2) ? 1u : 0u;
    col2 = col2b + carry;
    carry2 += (col2 < col2b) ? 1u : 0u;

    if ((ll_l != 0u) || (col1 != 0u))
    {
        c->rest = 1;
    }
    c->hi = hh_h + carry2;
    c->lo = col2;
    c->fe2 = (mmgr_iword)(c->fe2 + c->pow->e2 + 128);
    muto_norm(c);
}

MMGR_INLINE void muto_apply_pow10(MutoCtx *c)
{
    const mmgr_iword k = (c->ex < 0) ? (mmgr_iword)(-c->ex) : c->ex;

    for (mmgr_iword i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            c->pow = (c->ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];
            muto_mul_pow5(c);
        }
    }
    c->fe2 += c->ex; }

MMGR_INLINE void muto_seat(MutoCtx *c)
{
    c->hi = *c->mant;
    c->lo = 0u;
    c->fe2 = c->e2 - 64;
    c->rest = c->dropped;
    muto_norm(c);
}

MMGR_INLINE double muto_round(const MutoCtx *c)
{
        if ((c->hi | c->lo) == 0u)
    {
        return c->neg ? -0.0 : 0.0;
    }

    mmgr_u64 mant = c->hi >> 11;
    mmgr_u64 half = (c->hi >> 10) & 1u;
    mmgr_u64 rest = (mmgr_u64)c->rest | ((c->lo != 0u) ? 1u : 0u) | (((c->hi & 0x3FFu) != 0u) ? 1u : 0u);
    mmgr_iword be = c->fe2 + 75 + (mmgr_iword)MMGR_DBL_MANT_BITS + MMGR_DBL_BIAS;

    if (be <= 0)
    {
                mmgr_iword shift = 1 - be;
        if (shift > 60)
        {
            return c->neg ? -0.0 : 0.0;
        }
        while (shift-- > 0)
        {
            rest |= half;
            half = mant & 1u;
            mant >>= 1;
        }
        be = 0;
    }

    if ((half != 0u) && ((rest != 0u) || ((mant & 1u) != 0u)))
    {
        mant += 1u;
        if ((mant >> 53) != 0u)
        {
            mant >>= 1;
            be += 1;
        }
        else if ((be == 0) && ((mant >> MMGR_DBL_MANT_BITS) != 0u))
        {
            be = 1;         }
    }

    if (be >= (mmgr_iword)MMGR_DBL_EXP_ALL)
    {
        const double big = 1.0e308 * 10.0;
        return c->neg ? -big : big;
    }
    const mmgr_u64 bits = MMGR_CALL(fract.merge, FractioCfg, .sign = (mmgr_u64)(c->neg ? MMGR_DBL_SIGN_ONE : 0u),
                                    .exp = (mmgr_u64)be, .mant = mant & MMGR_DBL_MANT_MASK);

    return MMGR_CALL(fract.from_bits, FractioCfg, .bits = bits);
}

MMGR_INLINE mmgr_u64 muto_to_u64(const MutoCtx *c)
{
    if ((c->hi | c->lo) == 0u)
    {
        return 0u;
    }

    const mmgr_iword k = -(c->fe2);

    if (k > 128)
    {
        return 0u;     }
    if (k < 64)
    {
        return ~(mmgr_u64)0;     }

    const mmgr_word j = (mmgr_word)(k - 64);
    const mmgr_u64 low_mask = (j == 0u) ? 0u : (((mmgr_u64)1 << (j - 1u)) - 1u);
    mmgr_u64 whole;
    mmgr_u64 half;
    mmgr_u64 rest = (c->rest != 0) ? 1u : 0u;

    if (j == 0u)
    {
        whole = c->hi;
        half = (c->lo >> 63) & 1u;
        rest |= ((c->lo & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
    }
    else if (j < 64u)
    {
        whole = c->hi >> j;
        half = (c->hi >> (j - 1u)) & 1u;
        rest |= ((c->hi & low_mask) != 0u) ? 1u : 0u;
        rest |= (c->lo != 0u) ? 1u : 0u;
    }
    else
    {
        whole = 0u;         half = c->hi >> 63;
        rest |= ((c->hi & (((mmgr_u64)1 << 63) - 1u)) != 0u) ? 1u : 0u;
        rest |= (c->lo != 0u) ? 1u : 0u;
    }

        const mmgr_word odd = ((mmgr_word)(whole & 1u)) ^ (c->above & 1u);

    if ((half != 0u) && ((rest != 0u) || (odd != 0u)))
    {
        whole += 1u;
    }
    return whole;
}

MMGR_INLINE double muto_scale(MutoCtx *c)
{
    if (*c->mant == 0u)
    {
        return c->neg ? -0.0 : 0.0;
    }

        if ((c->dropped == 0) && (*c->mant < ((mmgr_u64)1 << 53)) && (c->ex >= -MMGR_MUTO_EXACT_POW10) &&
        (c->ex <= MMGR_MUTO_EXACT_POW10))
    {
        double v = (double)*c->mant;

        if (c->ex > 0)
        {
            v *= mmgr_muto_ten[c->ex];
        }
        else if (c->ex < 0)
        {
            v /= mmgr_muto_ten[-c->ex];
        }
        return c->neg ? -v : v;
    }
    if (c->ex > MMGR_POW5_MAX)
    {
        const double big = 1.0e308 * 10.0;
        return c->neg ? -big : big;
    }
    if (c->ex < -MMGR_POW5_MAX)
    {
        return c->neg ? -0.0 : 0.0;
    }

    muto_seat(c);
    muto_apply_pow10(c);
    return muto_round(c);
}

MMGR_INLINE mmgr_u64 muto_scale_to_u64(MutoCtx *c)
{
    if (*c->mant == 0u)
    {
        return 0u;
    }

    muto_seat(c);
    muto_apply_pow10(c);
    return muto_to_u64(c);
}


mmgr_bool mmgr_muto_take(const TransformoCfg *c)
{
    return MMGR_CALL(muto_take, MutoCtx, .mant = c->mant, .digit = c->digit);
}

double mmgr_muto_scale(const TransformoCfg *c)
{
    return MMGR_CALL(muto_scale, MutoCtx, .mant = c->mant, .ex = c->ex, .dropped = c->rest, .neg = c->neg);
}

mmgr_u64 mmgr_muto_scale_to_u64(const TransformoCfg *c)
{
    return MMGR_CALL(muto_scale_to_u64, MutoCtx, .mant = c->mant, .e2 = c->e2, .ex = c->ex, .above = c->above);
}
