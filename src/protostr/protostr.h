// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_PROTOSTR_H
#define PROTOCORE_PROTOSTR_H

#include "mmgr/swar/swar.h"

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

typedef struct
{
    size_t (*len)(const char *s, size_t nul_cap);
    size_t (*diff)(const char *a, const char *b, size_t read_cap, proto_bool ci);
    proto_bool (*eq)(const char *a, const char *b, size_t read_cap, proto_bool ci);
    proto_bool (*starts)(const char *s, const char *pre, size_t read_cap, proto_bool ci);
    const char *(*find)(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, proto_bool ci);
    proto_bool (*has)(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, proto_bool ci);
    size_t (*copy)(char *dst, const char *src, size_t dst_cap);
    int (*step_word)(protocore_swar_word wa, protocore_swar_word wb, proto_bool ci, int end_wins);
    int (*step_byte)(unsigned char ca, unsigned char cb, proto_bool ci, int end_wins);
    proto_bool (*ws)(char c);
    proto_bool (*digit)(char c);
    long (*to_long)(const char *s, const char **end);
    unsigned long (*to_ulong)(const char *s, const char **end);
    double (*to_double)(const char *s, const char **end);
    float (*to_float)(const char *s, const char **end);
} StrNs;

extern const StrNs str;

PROTOCORE_END_DECLS

#endif
