// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/float_bits/float_bits.h"
#include "mmgr/rawmemcpy/rawmemcpy.h"

proto_u64 proto_dbl_sign(double v)
{
    return (proto_raw_u64(&v) & PROTO_DBL_SIGN_MASK) >> PROTO_DBL_SIGN_SHIFT;
}

proto_u64 proto_dbl_exp(double v)
{
    return (proto_raw_u64(&v) & PROTO_DBL_EXP_MASK) >> PROTO_DBL_MANT_BITS;
}

proto_u64 proto_dbl_mant(double v)
{
    return proto_raw_u64(&v) & PROTO_DBL_MANT_MASK;
}

proto_u64 proto_dbl_merge(proto_u64 sign, proto_u64 exp, proto_u64 mant)
{
    return ((sign & PROTO_DBL_SIGN_ONE) << PROTO_DBL_SIGN_SHIFT) | ((exp & PROTO_DBL_EXP_ALL) << PROTO_DBL_MANT_BITS) |
           (mant & PROTO_DBL_MANT_MASK);
}

double proto_dbl_from_bits(proto_u64 bits)
{
    double v = 0.0;
    proto_raw_read(&v, &bits, sizeof(v));
    return v;
}
