// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_FLOAT_BITS_H
#define PROTOCORE_FLOAT_BITS_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#define PROTO_DBL_SIGN_MASK 0x8000000000000000ull
#define PROTO_DBL_EXP_MASK 0x7FF0000000000000ull
#define PROTO_DBL_MANT_MASK 0x000FFFFFFFFFFFFFull
#define PROTO_DBL_SIGN_SHIFT 63u
#define PROTO_DBL_MANT_BITS 52u
#define PROTO_DBL_SIGN_ONE 0x1ull
#define PROTO_DBL_EXP_ALL 0x7FFull
#define PROTO_DBL_BIAS 1023

typedef struct
{
    proto_u64 (*sign)(double v);
    proto_u64 (*exp)(double v);
    proto_u64 (*mant)(double v);
    proto_u64 (*merge)(proto_u64 sign, proto_u64 exp, proto_u64 mant);
    double (*from_bits)(proto_u64 bits);
} DblNs;

proto_u64 proto_dbl_sign(double v);
proto_u64 proto_dbl_exp(double v);
proto_u64 proto_dbl_mant(double v);
proto_u64 proto_dbl_merge(proto_u64 sign, proto_u64 exp, proto_u64 mant);
double proto_dbl_from_bits(proto_u64 bits);

static const DblNs dbl
    __attribute__((unused)) = {proto_dbl_sign, proto_dbl_exp, proto_dbl_mant, proto_dbl_merge, proto_dbl_from_bits};

PROTOCORE_END_DECLS

#endif
