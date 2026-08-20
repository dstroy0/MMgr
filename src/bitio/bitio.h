// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BITIO_H
#define MMGR_BITIO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

typedef struct
{
    uint8_t *out;
    size_t cap;
    size_t cnt;
    uint32_t acc;
    int nbits;
    mmgr_bool overflow;
} mmgr_bit_writer;

typedef struct
{
    void (*put)(mmgr_bit_writer *w, uint32_t bits, int n);
    void (*align)(mmgr_bit_writer *w);
} BitwNs;

void mmgr_bitw_put(mmgr_bit_writer *w, uint32_t bits, int n);
void mmgr_bitw_align(mmgr_bit_writer *w);

static const BitwNs bitw __attribute__((unused)) = {.put = mmgr_bitw_put, .align = mmgr_bitw_align};

MMGR_END_DECLS

#endif
