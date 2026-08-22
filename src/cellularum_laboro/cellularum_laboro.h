// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CELLULARUM_LABORO_H
#define MMGR_CELLULARUM_LABORO_H

#include "verbum_scrutor/verbum_scrutor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

#ifndef MMGR_SIEVE_ROWS
#define MMGR_SIEVE_ROWS 1u
#endif

typedef struct
{
    const char *const s;
    const size_t cap;
    const char *const t;
    const size_t t_cap;
    char *const dst;
    const size_t at;
    const uint8_t byte;
    const mmgr_bool ci;
    const uint8_t **const out;
    uint32_t *const slen;
} CatenaFinitaCfg;

typedef struct
{
    const mmgr_scrut_word wa;
    const mmgr_scrut_word wb;
    const unsigned char ca;
    const unsigned char cb;
    const mmgr_bool ci;
    const int end_wins;
} VerboProgrediorCfg;

typedef struct
{
    const char *const s;
    const char **const end;
    const uint8_t *const m;
    const uint32_t mlen;
    uint8_t *const field;
    const size_t fieldlen;
} TransfiguroCfg;

typedef struct
{
    CatenaFinitaCfg (*init)(const CatenaFinitaCfg *c);
    size_t (*len)(const CatenaFinitaCfg *c);
    size_t (*diff)(const CatenaFinitaCfg *c);
    mmgr_bool (*eq)(const CatenaFinitaCfg *c);
    mmgr_bool (*starts)(const CatenaFinitaCfg *c);
    const char *(*find)(const CatenaFinitaCfg *c);
    mmgr_bool (*has)(const CatenaFinitaCfg *c);
    const char *(*chr)(const CatenaFinitaCfg *c);
    size_t (*copy)(const CatenaFinitaCfg *c);
    mmgr_bool (*ws)(const CatenaFinitaCfg *c);
    mmgr_bool (*digit)(const CatenaFinitaCfg *c);
    mmgr_bool (*rd_str)(const CatenaFinitaCfg *c);
    int (*step_word)(const VerboProgrediorCfg *c);
    int (*step_byte)(const VerboProgrediorCfg *c);
    long (*to_long)(const TransfiguroCfg *c);
    unsigned long (*to_ulong)(const TransfiguroCfg *c);
    double (*to_double)(const TransfiguroCfg *c);
    float (*to_float)(const TransfiguroCfg *c);
    mmgr_bool (*mpint_fixed)(const TransfiguroCfg *c);
} CellularumLaboroNs;
MMGR_NS_LAYOUT(CellularumLaboroNs, init, len, diff, eq, starts, find, has, chr, copy, ws, digit, rd_str, step_word,
               step_byte, to_long, to_ulong, to_double, to_float, mpint_fixed);

CatenaFinitaCfg mmgr_cellul_init(const CatenaFinitaCfg *c);
size_t mmgr_cellul_len(const CatenaFinitaCfg *c);
size_t mmgr_cellul_diff(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_eq(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_starts(const CatenaFinitaCfg *c);
const char *mmgr_cellul_find(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_has(const CatenaFinitaCfg *c);
const char *mmgr_cellul_chr(const CatenaFinitaCfg *c);
size_t mmgr_cellul_copy(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_ws(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_digit(const CatenaFinitaCfg *c);
mmgr_bool mmgr_cellul_rd_str(const CatenaFinitaCfg *c);
int mmgr_cellul_step_word(const VerboProgrediorCfg *c);
int mmgr_cellul_step_byte(const VerboProgrediorCfg *c);
long mmgr_cellul_to_long(const TransfiguroCfg *c);
unsigned long mmgr_cellul_to_ulong(const TransfiguroCfg *c);
double mmgr_cellul_to_double(const TransfiguroCfg *c);
float mmgr_cellul_to_float(const TransfiguroCfg *c);
mmgr_bool mmgr_cellul_mpint_fixed(const TransfiguroCfg *c);

#define MMGR_CELLUL_IS_STR(x_) ((void)_Generic((x_), const char *: 0, char *: 0))
#define MMGR_CELLUL_IS_WSTR(x_) ((void)_Generic((x_), char *: 0))
#define MMGR_CELLUL_IS_BYTES(x_) ((void)_Generic((x_), const uint8_t *: 0, uint8_t *: 0))
#define MMGR_CELLUL_IS_WBYTES(x_) ((void)_Generic((x_), uint8_t *: 0))
#define MMGR_CELLUL_IS_SIZE(x_)                                                                                        \
    ((void)_Generic((x_), size_t: 0, int: 0, unsigned: 0, long: 0, unsigned long: 0, unsigned char: 0))
#define MMGR_CELLUL_IS_CHAR(x_) ((void)_Generic((x_), char: 0, int: 0, unsigned char: 0))
#define MMGR_CELLUL_IS_WORD(x_) ((void)_Generic((x_), mmgr_scrut_word: 0))
#define MMGR_CELLUL_IS_BYTE(x_) ((void)_Generic((x_), unsigned char: 0, int: 0, unsigned: 0))
#define MMGR_CELLUL_IS_BOOL(x_) ((void)_Generic((x_), mmgr_bool: 0, int: 0, unsigned: 0))

#define mmgr_cellul_init(s_, cap_, t_, tcap_, dst_, ci_)                                                               \
    (MMGR_CELLUL_IS_STR(s_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_STR(t_), MMGR_CELLUL_IS_SIZE(tcap_),            \
     MMGR_CELLUL_IS_WSTR(dst_), MMGR_CELLUL_IS_BOOL(ci_),                                                              \
     cellul.init(&(CatenaFinitaCfg){                                                                                   \
         .s = (s_), .cap = (cap_), .t = (t_), .t_cap = (tcap_), .dst = (dst_), .ci = (ci_)}))

#define mmgr_cellul_len(s_, cap_)                                                                                      \
    (MMGR_CELLUL_IS_STR(s_), MMGR_CELLUL_IS_SIZE(cap_),                                                                \
     cellul.len(&(CatenaFinitaCfg){.s = (s_), .cap = (cap_)}))

#define mmgr_cellul_chr(s_, cap_, byte_)                                                                               \
    (MMGR_CELLUL_IS_STR(s_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_BYTE(byte_),                                    \
     cellul.chr(&(CatenaFinitaCfg){.s = (s_), .cap = (cap_), .byte = (uint8_t)(byte_)}))

#define mmgr_cellul_diff(a_, b_, cap_, ci_)                                                                            \
    (MMGR_CELLUL_IS_STR(a_), MMGR_CELLUL_IS_STR(b_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_BOOL(ci_),              \
     cellul.diff(&(CatenaFinitaCfg){.s = (a_), .t = (b_), .cap = (cap_), .ci = (ci_)}))

#define mmgr_cellul_eq(a_, b_, cap_, ci_)                                                                              \
    (MMGR_CELLUL_IS_STR(a_), MMGR_CELLUL_IS_STR(b_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_BOOL(ci_),              \
     cellul.eq(&(CatenaFinitaCfg){.s = (a_), .t = (b_), .cap = (cap_), .ci = (ci_)}))

#define mmgr_cellul_starts(s_, pre_, cap_, ci_)                                                                        \
    (MMGR_CELLUL_IS_STR(s_), MMGR_CELLUL_IS_STR(pre_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_BOOL(ci_),            \
     cellul.starts(&(CatenaFinitaCfg){.s = (s_), .t = (pre_), .cap = (cap_), .ci = (ci_)}))

#define mmgr_cellul_find(hay_, cap_, needle_, ncap_, ci_)                                                              \
    (MMGR_CELLUL_IS_STR(hay_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_STR(needle_), MMGR_CELLUL_IS_SIZE(ncap_),     \
     MMGR_CELLUL_IS_BOOL(ci_),                                                                                         \
     cellul.find(&(CatenaFinitaCfg){                                                                                   \
         .s = (hay_), .cap = (cap_), .t = (needle_), .t_cap = (ncap_), .ci = (ci_)}))

#define mmgr_cellul_has(hay_, cap_, needle_, ncap_, ci_)                                                               \
    (MMGR_CELLUL_IS_STR(hay_), MMGR_CELLUL_IS_SIZE(cap_), MMGR_CELLUL_IS_STR(needle_), MMGR_CELLUL_IS_SIZE(ncap_),     \
     MMGR_CELLUL_IS_BOOL(ci_),                                                                                         \
     cellul.has(&(CatenaFinitaCfg){                                                                                    \
         .s = (hay_), .cap = (cap_), .t = (needle_), .t_cap = (ncap_), .ci = (ci_)}))

#define mmgr_cellul_copy(dst_, src_, cap_)                                                                             \
    (MMGR_CELLUL_IS_WSTR(dst_), MMGR_CELLUL_IS_STR(src_), MMGR_CELLUL_IS_SIZE(cap_),                                   \
     cellul.copy(&(CatenaFinitaCfg){.dst = (dst_), .s = (src_), .cap = (cap_)}))

#define mmgr_cellul_ws(ch_)                                                                                            \
    (MMGR_CELLUL_IS_CHAR(ch_), cellul.ws(&(CatenaFinitaCfg){.s = &(char){(char)(ch_)}, .cap = 1u}))

#define mmgr_cellul_digit(ch_)                                                                                         \
    (MMGR_CELLUL_IS_CHAR(ch_), cellul.digit(&(CatenaFinitaCfg){.s = &(char){(char)(ch_)}, .cap = 1u}))

#define mmgr_cellul_rd_str(buf_, len_, at_, out_, slen_)                                                               \
    (MMGR_CELLUL_IS_BYTES(buf_), MMGR_CELLUL_IS_SIZE(len_), MMGR_CELLUL_IS_SIZE(at_), MMGR_CELLUL_IS_BYTES(out_),      \
     MMGR_CELLUL_IS_SIZE(slen_),                                                                                       \
     cellul.rd_str(&(CatenaFinitaCfg){                                                                                 \
         .s = (const char *)(buf_), .cap = (len_), .at = (at_), .out = &(out_), .slen = &(slen_)}))

#define mmgr_cellul_step_word(wa_, wb_, ci_, endwins_)                                                                 \
    (MMGR_CELLUL_IS_WORD(wa_), MMGR_CELLUL_IS_WORD(wb_), MMGR_CELLUL_IS_BOOL(ci_), MMGR_CELLUL_IS_SIZE(endwins_),      \
     cellul.step_word(&(VerboProgrediorCfg){.wa = (wa_), .wb = (wb_), .ci = (ci_), .end_wins = (endwins_)}))

#define mmgr_cellul_step_byte(ca_, cb_, ci_, endwins_)                                                                 \
    (MMGR_CELLUL_IS_BYTE(ca_), MMGR_CELLUL_IS_BYTE(cb_), MMGR_CELLUL_IS_BOOL(ci_), MMGR_CELLUL_IS_SIZE(endwins_),      \
     cellul.step_byte(&(VerboProgrediorCfg){                                                                           \
         .ca = (unsigned char)(ca_), .cb = (unsigned char)(cb_), .ci = (ci_), .end_wins = (endwins_)}))

#define mmgr_cellul_to_long(s_, end_)                                                                                  \
    (MMGR_CELLUL_IS_STR(s_), cellul.to_long(&(TransfiguroCfg){.s = (s_), .end = &(end_)}))

#define mmgr_cellul_to_ulong(s_, end_)                                                                                 \
    (MMGR_CELLUL_IS_STR(s_), cellul.to_ulong(&(TransfiguroCfg){.s = (s_), .end = &(end_)}))

#define mmgr_cellul_to_double(s_, end_)                                                                                \
    (MMGR_CELLUL_IS_STR(s_), cellul.to_double(&(TransfiguroCfg){.s = (s_), .end = &(end_)}))

#define mmgr_cellul_to_float(s_, end_)                                                                                 \
    (MMGR_CELLUL_IS_STR(s_), cellul.to_float(&(TransfiguroCfg){.s = (s_), .end = &(end_)}))

#define mmgr_cellul_mpint_fixed(m_, mlen_, field_, fieldlen_)                                                          \
    (MMGR_CELLUL_IS_BYTES(m_), MMGR_CELLUL_IS_SIZE(mlen_), MMGR_CELLUL_IS_WBYTES(field_),                              \
     MMGR_CELLUL_IS_SIZE(fieldlen_),                                                                                   \
     cellul.mpint_fixed(&(TransfiguroCfg){                                                                             \
         .m = (m_), .mlen = (uint32_t)(mlen_), .field = (field_), .fieldlen = (fieldlen_)}))

MMGR_NS CellularumLaboroNs cellul MMGR_UNUSED = {
    .init = mmgr_cellul_init,
    .len = mmgr_cellul_len,
    .diff = mmgr_cellul_diff,
    .eq = mmgr_cellul_eq,
    .starts = mmgr_cellul_starts,
    .find = mmgr_cellul_find,
    .has = mmgr_cellul_has,
    .chr = mmgr_cellul_chr,
    .copy = mmgr_cellul_copy,
    .ws = mmgr_cellul_ws,
    .digit = mmgr_cellul_digit,
    .rd_str = mmgr_cellul_rd_str,
    .step_word = mmgr_cellul_step_word,
    .step_byte = mmgr_cellul_step_byte,
    .to_long = mmgr_cellul_to_long,
    .to_ulong = mmgr_cellul_to_ulong,
    .to_double = mmgr_cellul_to_double,
    .to_float = mmgr_cellul_to_float,
    .mpint_fixed = mmgr_cellul_mpint_fixed,
};

MMGR_FINIS_DECLS

#endif
