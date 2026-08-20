// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_BITIO_H
#define PROTOCORE_BITIO_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

typedef struct
{
    uint8_t *out;
    size_t cap;
    size_t cnt;
    uint32_t acc;
    int nbits;
    proto_bool overflow;
} protocore_bit_writer;

typedef struct
{
    void (*put)(protocore_bit_writer *w, uint32_t bits, int n);
    void (*align)(protocore_bit_writer *w);
} BitwNs;

void protocore_bitw_put(protocore_bit_writer *w, uint32_t bits, int n);
void protocore_bitw_align(protocore_bit_writer *w);

static const BitwNs bitw __attribute__((unused)) = {.put = protocore_bitw_put, .align = protocore_bitw_align};

PROTOCORE_END_DECLS

#endif
