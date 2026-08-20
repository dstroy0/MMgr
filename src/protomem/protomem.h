// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_PROTOMEM_H
#define MMGR_PROTOMEM_H

#include "mmgr/rawmemcpy/rawmemcpy.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

typedef struct
{
    void (*cpy)(void *dst, const void *src, size_t n);
    void (*move)(void *dst, const void *src, size_t n);
    int (*cmp)(const void *a, const void *b, size_t n);
    const void *(*chr)(const void *p, size_t n, uint8_t c);
    void (*set)(void *dst, unsigned char v, size_t n);
    void (*zero)(void *dst, size_t n);
} MemNs;

void mmgr_mem_cpy(void *dst, const void *src, size_t n);
void mmgr_mem_move(void *dst, const void *src, size_t n);
int mmgr_mem_cmp(const void *a, const void *b, size_t n);
const void *mmgr_mem_chr(const void *p, size_t n, uint8_t c);
void mmgr_mem_set(void *dst, unsigned char v, size_t n);
void mmgr_mem_zero(void *dst, size_t n);

static const MemNs mem
    __attribute__((unused)) = {mmgr_mem_cpy, mmgr_mem_move, mmgr_mem_cmp, mmgr_mem_chr, mmgr_mem_set, mmgr_mem_zero};

MMGR_END_DECLS

#endif
