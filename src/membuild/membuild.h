// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_MEMBUILD_H
#define PROTOCORE_MEMBUILD_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#define protocore_sb_lit(b, s) protocore_sb_put_n((b), (s), sizeof(s) - 1)

typedef struct
{
    char *p;
    size_t cap;
    size_t len;
    proto_bool ok;
} protocore_sb;

typedef struct
{
    void (*put_n)(protocore_sb *b, const char *s, size_t sl);
    void (*put)(protocore_sb *b, const char *s);
    void (*put_clip)(protocore_sb *b, const char *s);
    void (*u64_clip)(protocore_sb *b, uint64_t v, uint8_t columns);
    void (*xml)(protocore_sb *b, const char *s);
    void (*ch)(protocore_sb *b, char c);
    void (*uint)(protocore_sb *b, uint64_t v, unsigned base, unsigned min_digits);
    void (*u32w)(protocore_sb *b, uint32_t v, unsigned min_digits);
    void (*hex)(protocore_sb *b, uint64_t v, unsigned min_digits);
    void (*u32)(protocore_sb *b, uint32_t v);
    void (*u64)(protocore_sb *b, uint64_t v);
    void (*i64)(protocore_sb *b, int64_t v);
    proto_bool (*sign_bit)(double v);
    proto_bool (*is_inf)(double v);
    proto_bool (*is_nan)(double v);
    void (*g)(protocore_sb *b, double v, unsigned sig);
    void (*fixed)(protocore_sb *b, double v, unsigned decimals);
    void (*json)(protocore_sb *b, const char *s);
    size_t (*finish)(protocore_sb *b);
} SbNs;

void protocore_sb_put_n(protocore_sb *b, const char *s, size_t sl);
void protocore_sb_put(protocore_sb *b, const char *s);
void protocore_sb_put_clip(protocore_sb *b, const char *s);
void protocore_sb_u64_clip(protocore_sb *b, uint64_t v, uint8_t columns);
void protocore_sb_xml(protocore_sb *b, const char *s);
void protocore_sb_ch(protocore_sb *b, char c);
void protocore_sb_uint(protocore_sb *b, uint64_t v, unsigned base, unsigned min_digits);
void protocore_sb_u32w(protocore_sb *b, uint32_t v, unsigned min_digits);
void protocore_sb_hex(protocore_sb *b, uint64_t v, unsigned min_digits);
void protocore_sb_u32(protocore_sb *b, uint32_t v);
void protocore_sb_u64(protocore_sb *b, uint64_t v);
void protocore_sb_i64(protocore_sb *b, int64_t v);
proto_bool protocore_signbit(double v);
proto_bool protocore_isinf(double v);
proto_bool protocore_isnan(double v);
void protocore_sb_g(protocore_sb *b, double v, unsigned sig);
void protocore_sb_fixed(protocore_sb *b, double v, unsigned decimals);
void protocore_sb_json(protocore_sb *b, const char *s);
size_t protocore_sb_finish(protocore_sb *b);

static const SbNs Sb __attribute__((unused)) = {.put_n = protocore_sb_put_n,
                                                .put = protocore_sb_put,
                                                .put_clip = protocore_sb_put_clip,
                                                .u64_clip = protocore_sb_u64_clip,
                                                .xml = protocore_sb_xml,
                                                .ch = protocore_sb_ch,
                                                .uint = protocore_sb_uint,
                                                .u32w = protocore_sb_u32w,
                                                .hex = protocore_sb_hex,
                                                .u32 = protocore_sb_u32,
                                                .u64 = protocore_sb_u64,
                                                .i64 = protocore_sb_i64,
                                                .sign_bit = protocore_signbit,
                                                .is_inf = protocore_isinf,
                                                .is_nan = protocore_isnan,
                                                .g = protocore_sb_g,
                                                .fixed = protocore_sb_fixed,
                                                .json = protocore_sb_json,
                                                .finish = protocore_sb_finish};

PROTOCORE_END_DECLS

#endif
