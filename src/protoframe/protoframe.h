// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_PROTOFRAME_H
#define PROTOCORE_PROTOFRAME_H

#include "mmgr/membuild/membuild.h"

typedef enum
{
    PROTOCORE_FK_END = 0,
    PROTOCORE_FK_LIT,
    PROTOCORE_FK_STR,
    PROTOCORE_FK_U32,
    PROTOCORE_FK_U64,
    PROTOCORE_FK_I64,
    PROTOCORE_FK_DEC,
    PROTOCORE_FK_HEX,
    PROTOCORE_FK_OCT,
    PROTOCORE_FK_G,
    PROTOCORE_FK_FIX,
    PROTOCORE_FK_CH,
    PROTOCORE_FK_JSON,
    PROTOCORE_FK_XML,
} protocore_fk;

typedef struct protocore_field
{
    uint8_t kind;
    uint8_t width;
    uint16_t len;
    const char *lit;
} protocore_field;

#define PROTOCORE_STR {PROTOCORE_FK_STR, 0, 0, NULL}
#define PROTOCORE_U32 {PROTOCORE_FK_U32, 0, 0, NULL}
#define PROTOCORE_U64 {PROTOCORE_FK_U64, 0, 0, NULL}
#define PROTOCORE_I64 {PROTOCORE_FK_I64, 0, 0, NULL}
#define PROTOCORE_CH {PROTOCORE_FK_CH, 0, 0, NULL}
#define PROTOCORE_JSON {PROTOCORE_FK_JSON, 0, 0, NULL}
#define PROTOCORE_XML {PROTOCORE_FK_XML, 0, 0, NULL}
#define PROTOCORE_END {PROTOCORE_FK_END, 0, 0, NULL}

typedef struct protocore_fval
{
    uint8_t kind;
    union {
        const char *s;
        uint32_t u32;
        uint64_t u64;
        int64_t i64;
        double d;
        char c;
    } as;
} protocore_fval;

#define PROTOCORE_VSTR(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_STR,                                                                                              \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }
#define PROTOCORE_VU32(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_U32,                                                                                              \
        {                                                                                                              \
            .u32 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VU64(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_U64,                                                                                              \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VI64(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_I64,                                                                                              \
        {                                                                                                              \
            .i64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VDEC(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_DEC,                                                                                              \
        {                                                                                                              \
            .u32 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VHEX(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_HEX,                                                                                              \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VOCT(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_OCT,                                                                                              \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define PROTOCORE_VG(x)                                                                                                \
    {                                                                                                                  \
        PROTOCORE_FK_G,                                                                                                \
        {                                                                                                              \
            .d = (x)                                                                                                   \
        }                                                                                                              \
    }
#define PROTOCORE_VFIX(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_FIX,                                                                                              \
        {                                                                                                              \
            .d = (x)                                                                                                   \
        }                                                                                                              \
    }
#define PROTOCORE_VCH(x)                                                                                               \
    {                                                                                                                  \
        PROTOCORE_FK_CH,                                                                                               \
        {                                                                                                              \
            .c = (x)                                                                                                   \
        }                                                                                                              \
    }
#define PROTOCORE_VJSON(x)                                                                                             \
    {                                                                                                                  \
        PROTOCORE_FK_JSON,                                                                                             \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }
#define PROTOCORE_VXML(x)                                                                                              \
    {                                                                                                                  \
        PROTOCORE_FK_XML,                                                                                              \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }

size_t protocore_frame_build(char *out, size_t cap, const protocore_field *spec, const protocore_fval *v, size_t nv);

size_t protocore_frame_append(char *out, size_t cap, const protocore_field *spec, const protocore_fval *v, size_t nv);

typedef struct
{
    size_t (*build)(char *out, size_t cap, const protocore_field *spec, const protocore_fval *v, size_t nv);
    size_t (*append)(char *out, size_t cap, const protocore_field *spec, const protocore_fval *v, size_t nv);
} FrameNs;

static const FrameNs frame __attribute__((unused)) = {protocore_frame_build, protocore_frame_append};

#endif
