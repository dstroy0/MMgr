// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_PROTOMEM_H
#define PROTOCORE_PROTOMEM_H

#include "mmgr/rawmemcpy/rawmemcpy.h"

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

typedef struct
{
    void (*cpy)(void *dst, const void *src, size_t n);
    void (*move)(void *dst, const void *src, size_t n);
    int (*cmp)(const void *a, const void *b, size_t n);
    const void *(*chr)(const void *p, size_t n, uint8_t c);
    void (*set)(void *dst, unsigned char v, size_t n);
    void (*zero)(void *dst, size_t n);
} MemNs;

void protocore_mem_cpy(void *dst, const void *src, size_t n);
void protocore_mem_move(void *dst, const void *src, size_t n);
int protocore_mem_cmp(const void *a, const void *b, size_t n);
const void *protocore_mem_chr(const void *p, size_t n, uint8_t c);
void protocore_mem_set(void *dst, unsigned char v, size_t n);
void protocore_mem_zero(void *dst, size_t n);

static const MemNs mem __attribute__((unused)) = {protocore_mem_cpy, protocore_mem_move, protocore_mem_cmp,
                                                  protocore_mem_chr, protocore_mem_set,  protocore_mem_zero};

PROTOCORE_END_DECLS

#endif
