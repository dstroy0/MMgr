#ifndef MMGR_FRACTIO_H
#define MMGR_FRACTIO_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS


#define MMGR_DBL_SIGN_MASK 0x8000000000000000ull
#define MMGR_DBL_EXP_MASK 0x7FF0000000000000ull
#define MMGR_DBL_MANT_MASK 0x000FFFFFFFFFFFFFull
#define MMGR_DBL_SIGN_SHIFT 63u
#define MMGR_DBL_MANT_BITS 52u
#define MMGR_DBL_EXP_BITS 11u
#define MMGR_DBL_SIGN_ONE 0x1ull
#define MMGR_DBL_EXP_ALL 0x7FFull
#define MMGR_DBL_BIAS 1023

#define MMGR_DBL_BITS 64u
#define MMGR_DBL_BYTES (MMGR_DBL_BITS / 8u)
#define MMGR_DBL_WORDS ((MMGR_DBL_BITS + (MMGR_WORD_BITS - 1u)) / MMGR_WORD_BITS)

MMGR_STATIC_ASSERT(sizeof(double) * 8u == MMGR_DBL_BITS,
                   "double is not 64 bits on this target, so every field position in this file is wrong "
                   "- a build where double means float cannot use it");
MMGR_STATIC_ASSERT(MMGR_DBL_WORDS *MMGR_WORD_BITS == MMGR_DBL_BITS,
                   "a double is not a whole number of words on this target");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) == sizeof(double), "the bit pattern of a double does not fit mmgr_u64");

MMGR_STATIC_ASSERT(1u + MMGR_DBL_EXP_BITS + MMGR_DBL_MANT_BITS == MMGR_DBL_BITS,
                   "the three fields do not add up to the width of the value");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK | MMGR_DBL_EXP_MASK | MMGR_DBL_MANT_MASK) == 0xFFFFFFFFFFFFFFFFull,
                   "the three field masks leave a gap");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK & MMGR_DBL_EXP_MASK) == 0u && (MMGR_DBL_EXP_MASK & MMGR_DBL_MANT_MASK) == 0u &&
                       (MMGR_DBL_SIGN_MASK & MMGR_DBL_MANT_MASK) == 0u,
                   "the three field masks overlap");
MMGR_STATIC_ASSERT(MMGR_DBL_SIGN_MASK == (MMGR_DBL_SIGN_ONE << MMGR_DBL_SIGN_SHIFT),
                   "the sign mask and the sign shift disagree about where the sign is");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_MASK == (MMGR_DBL_EXP_ALL << MMGR_DBL_MANT_BITS),
                   "the exponent mask and the exponent width disagree");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_ALL == ((1u << MMGR_DBL_EXP_BITS) - 1u), "the exponent does not fill its field");
MMGR_STATIC_ASSERT(MMGR_DBL_BIAS == ((1 << (MMGR_DBL_EXP_BITS - 1u)) - 1), "the bias is not the one binary64 uses");

#define MMGR_DBL_SCALE_MAX ((int)(MMGR_DBL_EXP_ALL - 1u) - MMGR_DBL_BIAS - (int)MMGR_DBL_MANT_BITS)
#define MMGR_DBL_SCALE_MIN (1 - MMGR_DBL_BIAS - (int)MMGR_DBL_MANT_BITS)

MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MAX == 971, "the largest scale a finite double can carry is not what it was");
MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MIN == -1074, "the smallest scale a subnormal can carry is not what it was");

typedef struct
{
    union {
        const double v;         const mmgr_u64 bits;  };
    const mmgr_u64 sign;    const mmgr_u64 exp;     const mmgr_u64 mant;  } FractioCfg;

typedef struct
{
    mmgr_u64 (*sign)(const FractioCfg *c);
    mmgr_u64 (*exp)(const FractioCfg *c);
    mmgr_u64 (*mant)(const FractioCfg *c);
    mmgr_u64 (*merge)(const FractioCfg *c);
    double (*from_bits)(const FractioCfg *c);
    mmgr_u64 (*to_bits)(const FractioCfg *c);
} FractioNs;
MMGR_NS_LAYOUT(FractioNs, sign, exp, mant, merge, from_bits, to_bits);

mmgr_u64 mmgr_fract_sign(const FractioCfg *c);
mmgr_u64 mmgr_fract_exp(const FractioCfg *c);
mmgr_u64 mmgr_fract_mant(const FractioCfg *c);
mmgr_u64 mmgr_fract_merge(const FractioCfg *c);
double mmgr_fract_from_bits(const FractioCfg *c);
mmgr_u64 mmgr_fract_to_bits(const FractioCfg *c);

#define MMGR_FRACT_IS_DOUBLE(x_) ((void)_Generic((x_), double: 0))
#define MMGR_FRACT_IS_BITS(x_) ((void)_Generic((x_), mmgr_u64: 0))

#define mmgr_fract_sign(bits_) (MMGR_FRACT_IS_BITS(bits_), fract.sign(&(FractioCfg){.bits = (bits_)}))
#define mmgr_fract_exp(bits_) (MMGR_FRACT_IS_BITS(bits_), fract.exp(&(FractioCfg){.bits = (bits_)}))
#define mmgr_fract_mant(bits_) (MMGR_FRACT_IS_BITS(bits_), fract.mant(&(FractioCfg){.bits = (bits_)}))

#define mmgr_fract_merge(sign_, exp_, mant_)                                                                           \
    (MMGR_FRACT_IS_BITS(sign_), MMGR_FRACT_IS_BITS(exp_), MMGR_FRACT_IS_BITS(mant_),                                   \
     fract.merge(&(FractioCfg){.sign = (sign_), .exp = (exp_), .mant = (mant_)}))

#define mmgr_fract_from_bits(bits_) (MMGR_FRACT_IS_BITS(bits_), fract.from_bits(&(FractioCfg){.bits = (bits_)}))
#define mmgr_fract_to_bits(v_) (MMGR_FRACT_IS_DOUBLE(v_), fract.to_bits(&(FractioCfg){.v = (v_)}))

MMGR_NS FractioNs fract MMGR_UNUSED = {
    .sign = mmgr_fract_sign,
    .exp = mmgr_fract_exp,
    .mant = mmgr_fract_mant,
    .merge = mmgr_fract_merge,
    .from_bits = mmgr_fract_from_bits,
    .to_bits = mmgr_fract_to_bits,
};

MMGR_FINIS_DECLS

#endif
