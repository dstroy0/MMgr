// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_MEMBUILD_H
#define MMGR_MEMBUILD_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define mmgr_sb_lit(b, s) mmgr_sb_put_n((b), (s), sizeof(s) - 1)

typedef struct
{
    char *p;
    size_t cap;
    size_t len;
    mmgr_bool ok;
} mmgr_sb;

typedef struct
{
    void (*put_n)(mmgr_sb *b, const char *s, size_t sl);
    void (*put)(mmgr_sb *b, const char *s);
    void (*put_clip)(mmgr_sb *b, const char *s);
    void (*u64_clip)(mmgr_sb *b, uint64_t v, uint8_t columns);
    void (*xml)(mmgr_sb *b, const char *s);
    void (*ch)(mmgr_sb *b, char c);
    void (*uint)(mmgr_sb *b, uint64_t v, unsigned base, unsigned min_digits);
    void (*u32w)(mmgr_sb *b, uint32_t v, unsigned min_digits);
    void (*hex)(mmgr_sb *b, uint64_t v, unsigned min_digits);
    void (*u32)(mmgr_sb *b, uint32_t v);
    void (*u64)(mmgr_sb *b, uint64_t v);
    void (*i64)(mmgr_sb *b, int64_t v);
    mmgr_bool (*sign_bit)(double v);
    mmgr_bool (*is_inf)(double v);
    mmgr_bool (*is_nan)(double v);
    void (*g)(mmgr_sb *b, double v, unsigned sig);
    void (*fixed)(mmgr_sb *b, double v, unsigned decimals);
    void (*json)(mmgr_sb *b, const char *s);
    size_t (*finish)(mmgr_sb *b);
} SbNs;

void mmgr_sb_put_n(mmgr_sb *b, const char *s, size_t sl);
void mmgr_sb_put(mmgr_sb *b, const char *s);
void mmgr_sb_put_clip(mmgr_sb *b, const char *s);
void mmgr_sb_u64_clip(mmgr_sb *b, uint64_t v, uint8_t columns);
void mmgr_sb_xml(mmgr_sb *b, const char *s);
void mmgr_sb_ch(mmgr_sb *b, char c);
void mmgr_sb_uint(mmgr_sb *b, uint64_t v, unsigned base, unsigned min_digits);
void mmgr_sb_u32w(mmgr_sb *b, uint32_t v, unsigned min_digits);
void mmgr_sb_hex(mmgr_sb *b, uint64_t v, unsigned min_digits);
void mmgr_sb_u32(mmgr_sb *b, uint32_t v);
void mmgr_sb_u64(mmgr_sb *b, uint64_t v);
void mmgr_sb_i64(mmgr_sb *b, int64_t v);
mmgr_bool mmgr_signbit(double v);
mmgr_bool mmgr_isinf(double v);
mmgr_bool mmgr_isnan(double v);
void mmgr_sb_g(mmgr_sb *b, double v, unsigned sig);
void mmgr_sb_fixed(mmgr_sb *b, double v, unsigned decimals);
void mmgr_sb_json(mmgr_sb *b, const char *s);
size_t mmgr_sb_finish(mmgr_sb *b);

static const SbNs Sb __attribute__((unused)) = {.put_n = mmgr_sb_put_n,
                                                .put = mmgr_sb_put,
                                                .put_clip = mmgr_sb_put_clip,
                                                .u64_clip = mmgr_sb_u64_clip,
                                                .xml = mmgr_sb_xml,
                                                .ch = mmgr_sb_ch,
                                                .uint = mmgr_sb_uint,
                                                .u32w = mmgr_sb_u32w,
                                                .hex = mmgr_sb_hex,
                                                .u32 = mmgr_sb_u32,
                                                .u64 = mmgr_sb_u64,
                                                .i64 = mmgr_sb_i64,
                                                .sign_bit = mmgr_signbit,
                                                .is_inf = mmgr_isinf,
                                                .is_nan = mmgr_isnan,
                                                .g = mmgr_sb_g,
                                                .fixed = mmgr_sb_fixed,
                                                .json = mmgr_sb_json,
                                                .finish = mmgr_sb_finish};

MMGR_END_DECLS

#endif
