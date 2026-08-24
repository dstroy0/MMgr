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
    const mmgr_word wa;
    const mmgr_word wb;
    const uint8_t ca;
    const uint8_t cb;
    const mmgr_bool ci;
    const mmgr_iword end_wins;
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
    mmgr_iword (*step_word)(const VerboProgrediorCfg *c);
    mmgr_iword (*step_byte)(const VerboProgrediorCfg *c);
    mmgr_iword (*to_long)(const TransfiguroCfg *c);
    mmgr_word (*to_ulong)(const TransfiguroCfg *c);
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
mmgr_iword mmgr_cellul_step_word(const VerboProgrediorCfg *c);
mmgr_iword mmgr_cellul_step_byte(const VerboProgrediorCfg *c);
mmgr_iword mmgr_cellul_to_long(const TransfiguroCfg *c);
mmgr_word mmgr_cellul_to_ulong(const TransfiguroCfg *c);
double mmgr_cellul_to_double(const TransfiguroCfg *c);
float mmgr_cellul_to_float(const TransfiguroCfg *c);
mmgr_bool mmgr_cellul_mpint_fixed(const TransfiguroCfg *c);

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
