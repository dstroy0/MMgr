// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "mmgr/proximus_operor/proximus_operor.h"

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
} MemoriaOperorNs;

void mmgr_memor_cpy(void *dst, const void *src, size_t n);
void mmgr_memor_move(void *dst, const void *src, size_t n);
int mmgr_memor_cmp(const void *a, const void *b, size_t n);
const void *mmgr_memor_chr(const void *p, size_t n, uint8_t c);
void mmgr_memor_set(void *dst, unsigned char v, size_t n);
void mmgr_memor_zero(void *dst, size_t n);

static const MemoriaOperorNs memor __attribute__((unused)) = {mmgr_memor_cpy, mmgr_memor_move, mmgr_memor_cmp,
                                                              mmgr_memor_chr, mmgr_memor_set,  mmgr_memor_zero};

MMGR_END_DECLS

#endif
