#include "fractio/fractio.h"

mmgr_u64 mmgr_fract_sign(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT;
}

mmgr_u64 mmgr_fract_exp(const FractioCfg *c)
{
    return (c->bits & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS;
}

mmgr_u64 mmgr_fract_mant(const FractioCfg *c)
{
    return c->bits & MMGR_DBL_MANT_MASK;
}

mmgr_u64 mmgr_fract_merge(const FractioCfg *c)
{
    return ((c->sign & MMGR_DBL_SIGN_ONE) << MMGR_DBL_SIGN_SHIFT) |
           ((c->exp & MMGR_DBL_EXP_ALL) << MMGR_DBL_MANT_BITS) | (c->mant & MMGR_DBL_MANT_MASK);
}

double mmgr_fract_from_bits(const FractioCfg *c)
{
    return c->v;
}

mmgr_u64 mmgr_fract_to_bits(const FractioCfg *c)
{
    return c->bits;
}
