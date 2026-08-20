// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_VERBA_SCRIBO_H
#define MMGR_VERBA_SCRIBO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define mmgr_verba_lit(b, s) mmgr_verba_put_n((b), (s), sizeof(s) - 1)

typedef struct
{
    char *p;
    size_t cap;
    size_t len;
    mmgr_bool ok;
} mmgr_verba;

typedef struct
{
    void (*put_n)(mmgr_verba *b, const char *s, size_t sl);
    void (*put)(mmgr_verba *b, const char *s);
    void (*put_clip)(mmgr_verba *b, const char *s);
    void (*u64_clip)(mmgr_verba *b, uint64_t v, uint8_t columns);
    void (*xml)(mmgr_verba *b, const char *s);
    void (*ch)(mmgr_verba *b, char c);
    void (*uint)(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits);
    void (*u32w)(mmgr_verba *b, uint32_t v, unsigned min_digits);
    void (*hex)(mmgr_verba *b, uint64_t v, unsigned min_digits);
    void (*u32)(mmgr_verba *b, uint32_t v);
    void (*u64)(mmgr_verba *b, uint64_t v);
    void (*i64)(mmgr_verba *b, int64_t v);
    mmgr_bool (*sign_bit)(double v);
    mmgr_bool (*is_inf)(double v);
    mmgr_bool (*is_nan)(double v);
    void (*g)(mmgr_verba *b, double v, unsigned sig);
    void (*fixed)(mmgr_verba *b, double v, unsigned decimals);
    void (*json)(mmgr_verba *b, const char *s);
    size_t (*finish)(mmgr_verba *b);
} VerbaScriboNs;

void mmgr_verba_put_n(mmgr_verba *b, const char *s, size_t sl);
void mmgr_verba_put(mmgr_verba *b, const char *s);
void mmgr_verba_put_clip(mmgr_verba *b, const char *s);
void mmgr_verba_u64_clip(mmgr_verba *b, uint64_t v, uint8_t columns);
void mmgr_verba_xml(mmgr_verba *b, const char *s);
void mmgr_verba_ch(mmgr_verba *b, char c);
void mmgr_verba_uint(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits);
void mmgr_verba_u32w(mmgr_verba *b, uint32_t v, unsigned min_digits);
void mmgr_verba_hex(mmgr_verba *b, uint64_t v, unsigned min_digits);
void mmgr_verba_u32(mmgr_verba *b, uint32_t v);
void mmgr_verba_u64(mmgr_verba *b, uint64_t v);
void mmgr_verba_i64(mmgr_verba *b, int64_t v);
mmgr_bool mmgr_signbit(double v);
mmgr_bool mmgr_isinf(double v);
mmgr_bool mmgr_isnan(double v);
void mmgr_verba_g(mmgr_verba *b, double v, unsigned sig);
void mmgr_verba_fixed(mmgr_verba *b, double v, unsigned decimals);
void mmgr_verba_json(mmgr_verba *b, const char *s);
size_t mmgr_verba_finish(mmgr_verba *b);

static const VerbaScriboNs verba __attribute__((unused)) = {.put_n = mmgr_verba_put_n,
                                                            .put = mmgr_verba_put,
                                                            .put_clip = mmgr_verba_put_clip,
                                                            .u64_clip = mmgr_verba_u64_clip,
                                                            .xml = mmgr_verba_xml,
                                                            .ch = mmgr_verba_ch,
                                                            .uint = mmgr_verba_uint,
                                                            .u32w = mmgr_verba_u32w,
                                                            .hex = mmgr_verba_hex,
                                                            .u32 = mmgr_verba_u32,
                                                            .u64 = mmgr_verba_u64,
                                                            .i64 = mmgr_verba_i64,
                                                            .sign_bit = mmgr_signbit,
                                                            .is_inf = mmgr_isinf,
                                                            .is_nan = mmgr_isnan,
                                                            .g = mmgr_verba_g,
                                                            .fixed = mmgr_verba_fixed,
                                                            .json = mmgr_verba_json,
                                                            .finish = mmgr_verba_finish};

MMGR_END_DECLS

#endif
