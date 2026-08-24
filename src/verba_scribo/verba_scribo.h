#ifndef MMGR_VERBA_SCRIBO_H
#define MMGR_VERBA_SCRIBO_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

#define MMGR_G_MAX_SIG 18u

#define MMGR_FIXED_MAX_DECIMALS 18u

typedef struct
{
    char *const out;
    const size_t cap;
    const size_t at;
    const char *const text;
    const size_t text_len;
    const char ch;
    const uint64_t val;
    const int64_t sval;
    const double real;
    const uint8_t base;
    const uint8_t min;
    const uint8_t columns;
    const uint8_t sig;
    const uint8_t decimals;
} VerbaCfg;

typedef struct
{
    size_t (*put_n)(const VerbaCfg *c);
    size_t (*put)(const VerbaCfg *c);
    size_t (*put_clip)(const VerbaCfg *c);
    size_t (*u64_clip)(const VerbaCfg *c);
    size_t (*xml)(const VerbaCfg *c);
    size_t (*ch)(const VerbaCfg *c);
    size_t (*uint)(const VerbaCfg *c);
    size_t (*u32w)(const VerbaCfg *c);
    size_t (*hex)(const VerbaCfg *c);
    size_t (*u32)(const VerbaCfg *c);
    size_t (*u64)(const VerbaCfg *c);
    size_t (*i64)(const VerbaCfg *c);
    size_t (*g)(const VerbaCfg *c);
    size_t (*fixed)(const VerbaCfg *c);
    size_t (*json)(const VerbaCfg *c);
    size_t (*finish)(const VerbaCfg *c);
    mmgr_bool (*ok)(const VerbaCfg *c);
    mmgr_bool (*sign_bit)(const VerbaCfg *c);
    mmgr_bool (*is_inf)(const VerbaCfg *c);
    mmgr_bool (*is_nan)(const VerbaCfg *c);
} VerbaScriboNs;
MMGR_NS_LAYOUT(VerbaScriboNs, put_n, put, put_clip, u64_clip, xml, ch, uint, u32w, hex, u32, u64, i64, g, fixed, json,
               finish, ok, sign_bit, is_inf, is_nan);

size_t mmgr_verba_put_n(const VerbaCfg *c);
size_t mmgr_verba_put(const VerbaCfg *c);
size_t mmgr_verba_put_clip(const VerbaCfg *c);
size_t mmgr_verba_u64_clip(const VerbaCfg *c);
size_t mmgr_verba_xml(const VerbaCfg *c);
size_t mmgr_verba_ch(const VerbaCfg *c);
size_t mmgr_verba_uint(const VerbaCfg *c);
size_t mmgr_verba_u32w(const VerbaCfg *c);
size_t mmgr_verba_hex(const VerbaCfg *c);
size_t mmgr_verba_u32(const VerbaCfg *c);
size_t mmgr_verba_u64(const VerbaCfg *c);
size_t mmgr_verba_i64(const VerbaCfg *c);
size_t mmgr_verba_g(const VerbaCfg *c);
size_t mmgr_verba_fixed(const VerbaCfg *c);
size_t mmgr_verba_json(const VerbaCfg *c);
size_t mmgr_verba_finish(const VerbaCfg *c);
mmgr_bool mmgr_verba_ok(const VerbaCfg *c);
mmgr_bool mmgr_verba_sign_bit(const VerbaCfg *c);
mmgr_bool mmgr_verba_is_inf(const VerbaCfg *c);
mmgr_bool mmgr_verba_is_nan(const VerbaCfg *c);

MMGR_NS VerbaScriboNs verba MMGR_UNUSED = {
    .put_n = mmgr_verba_put_n,
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
    .g = mmgr_verba_g,
    .fixed = mmgr_verba_fixed,
    .json = mmgr_verba_json,
    .finish = mmgr_verba_finish,
    .ok = mmgr_verba_ok,
    .sign_bit = mmgr_verba_sign_bit,
    .is_inf = mmgr_verba_is_inf,
    .is_nan = mmgr_verba_is_nan,
};

MMGR_FINIS_DECLS

#endif
