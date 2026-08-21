// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CELLULARUM_LABORO_H
#define MMGR_CELLULARUM_LABORO_H

#include "verbum_scrutor/verbum_scrutor.h"

#include "config/mmgr_config.h"

MMGR_BEGIN_DECLS

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
} CellularumLaboroNs;
MMGR_NS_LAYOUT(CellularumLaboroNs, len, diff, eq, starts, find, has, chr, copy, step_word, step_byte, ws, digit,
               to_long, to_ulong, to_double, to_float);

/** @brief Module namespace. */
extern const CellularumLaboroNs cellul;

MMGR_END_DECLS

#endif
