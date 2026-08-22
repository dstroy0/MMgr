// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CELLULARUM_LABORO_H
#define MMGR_CELLULARUM_LABORO_H

#include "verbum_scrutor/verbum_scrutor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file cellularum_laboro.h
 * @brief Bounded string work.
 *
 * Every entry takes a read cap saying how far it may read, and none of them scan past it. The cap
 * is not a length - a scan still stops at the terminator - it is a ceiling on a string that has
 * none.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*len)(const char *s, size_t nul_cap);
    size_t (*diff)(const char *a, const char *b, size_t read_cap, mmgr_bool ci);
    mmgr_bool (*eq)(const char *a, const char *b, size_t read_cap, mmgr_bool ci);
    mmgr_bool (*starts)(const char *s, const char *pre, size_t read_cap, mmgr_bool ci);
    const char *(*find)(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci);
    mmgr_bool (*has)(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci);
    const char *(*chr)(const char *s, size_t nul_cap, uint8_t c);
    size_t (*copy)(char *dst, const char *src, size_t dst_cap);
    int (*step_word)(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci, int end_wins);
    int (*step_byte)(unsigned char ca, unsigned char cb, mmgr_bool ci, int end_wins);
    mmgr_bool (*ws)(char c);
    mmgr_bool (*digit)(char c);
    long (*to_long)(const char *s, const char **end);
    unsigned long (*to_ulong)(const char *s, const char **end);
    double (*to_double)(const char *s, const char **end);
    float (*to_float)(const char *s, const char **end);
    mmgr_bool (*rd_str)(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
    mmgr_bool (*mpint_fixed)(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
} CellularumLaboroNs;
MMGR_NS_LAYOUT(CellularumLaboroNs, len, diff, eq, starts, find, has, chr, copy, step_word, step_byte, ws, digit,
               to_long, to_ulong, to_double, to_float, rd_str, mpint_fixed);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
size_t mmgr_cellul_len(const char *s, size_t nul_cap);
size_t mmgr_cellul_diff(const char *a, const char *b, size_t read_cap, mmgr_bool ci);
mmgr_bool mmgr_cellul_eq(const char *a, const char *b, size_t read_cap, mmgr_bool ci);
mmgr_bool mmgr_cellul_starts(const char *s, const char *pre, size_t read_cap, mmgr_bool ci);
const char *mmgr_cellul_find(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci);
mmgr_bool mmgr_cellul_has(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci);
const char *mmgr_cellul_chr(const char *s, size_t nul_cap, uint8_t c);
size_t mmgr_cellul_copy(char *dst, const char *src, size_t dst_cap);
int mmgr_cellul_step_word(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci, int end_wins);
int mmgr_cellul_step_byte(unsigned char ca, unsigned char cb, mmgr_bool ci, int end_wins);
mmgr_bool mmgr_cellul_ws(char c);
mmgr_bool mmgr_cellul_digit(char c);
long mmgr_cellul_to_long(const char *s, const char **end);
unsigned long mmgr_cellul_to_ulong(const char *s, const char **end);
double mmgr_cellul_to_double(const char *s, const char **end);
float mmgr_cellul_to_float(const char *s, const char **end);
mmgr_bool mmgr_cellul_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
mmgr_bool mmgr_cellul_mpint_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS CellularumLaboroNs cellul MMGR_UNUSED = {
    .len = mmgr_cellul_len,
    .diff = mmgr_cellul_diff,
    .eq = mmgr_cellul_eq,
    .starts = mmgr_cellul_starts,
    .find = mmgr_cellul_find,
    .has = mmgr_cellul_has,
    .chr = mmgr_cellul_chr,
    .copy = mmgr_cellul_copy,
    .step_word = mmgr_cellul_step_word,
    .step_byte = mmgr_cellul_step_byte,
    .ws = mmgr_cellul_ws,
    .digit = mmgr_cellul_digit,
    .to_long = mmgr_cellul_to_long,
    .to_ulong = mmgr_cellul_to_ulong,
    .to_double = mmgr_cellul_to_double,
    .to_float = mmgr_cellul_to_float,
    .rd_str = mmgr_cellul_rd_str,
    .mpint_fixed = mmgr_cellul_mpint_fixed,
};

MMGR_FINIS_DECLS

#endif
