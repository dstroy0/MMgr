#include "fractio/fractio.h"


MMGR_INLINE mmgr_u64 fract_sign(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

MMGR_INLINE mmgr_u64 fract_exp(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

MMGR_INLINE mmgr_u64 fract_mant(const FractioCfg *c)
{
    return c->bits & MMGR_DBL_MANT_MASK;
}

MMGR_INLINE mmgr_u64 fract_merge(const FractioCfg *c)
{
    return ((c->sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) |
           ((c->exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) | (c->mant & MMGR_DBL_MANT_MASK);
}

MMGR_INLINE double fract_from_bits(const FractioCfg *c)
{
    return c->v;
}

MMGR_INLINE mmgr_u64 fract_to_bits(const FractioCfg *c)
{
    return c->bits;
}


mmgr_u64 (mmgr_fract_sign)(const FractioCfg *c)
{
    return fract_sign(c);
}

mmgr_u64 (mmgr_fract_exp)(const FractioCfg *c)
{
    return fract_exp(c);
}

mmgr_u64 (mmgr_fract_mant)(const FractioCfg *c)
{
    return fract_mant(c);
}

mmgr_u64 (mmgr_fract_merge)(const FractioCfg *c)
{
    return fract_merge(c);
}

double (mmgr_fract_from_bits)(const FractioCfg *c)
{
    return fract_from_bits(c);
}

mmgr_u64 (mmgr_fract_to_bits)(const FractioCfg *c)
{
    return fract_to_bits(c);
}
