// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_NUMEROS_SCRIBO_H
#define MMGR_NUMEROS_SCRIBO_H

#include "verba_scribo/verba_scribo.h"

typedef enum
{
    MMGR_FK_END = 0,
    MMGR_FK_LIT,
    MMGR_FK_STR,
    MMGR_FK_U32,
    MMGR_FK_U64,
    MMGR_FK_I64,
    MMGR_FK_DEC,
    MMGR_FK_HEX,
    MMGR_FK_OCT,
    MMGR_FK_G,
    MMGR_FK_FIX,
    MMGR_FK_CH,
    MMGR_FK_JSON,
    MMGR_FK_XML,
} mmgr_fk;

typedef struct mmgr_field
{
    uint8_t kind;
    uint8_t width;
    uint16_t len;
    const char *lit;
} mmgr_field;

#define MMGR_STR {MMGR_FK_STR, 0, 0, NULL}
#define MMGR_U32 {MMGR_FK_U32, 0, 0, NULL}
#define MMGR_U64 {MMGR_FK_U64, 0, 0, NULL}
#define MMGR_I64 {MMGR_FK_I64, 0, 0, NULL}
#define MMGR_CH {MMGR_FK_CH, 0, 0, NULL}
#define MMGR_JSON {MMGR_FK_JSON, 0, 0, NULL}
#define MMGR_XML {MMGR_FK_XML, 0, 0, NULL}
#define MMGR_END {MMGR_FK_END, 0, 0, NULL}

typedef struct mmgr_fval
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
} mmgr_fval;

#define MMGR_VSTR(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_STR,                                                                                                   \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }
#define MMGR_VU32(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_U32,                                                                                                   \
        {                                                                                                              \
            .u32 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VU64(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_U64,                                                                                                   \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VI64(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_I64,                                                                                                   \
        {                                                                                                              \
            .i64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VDEC(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_DEC,                                                                                                   \
        {                                                                                                              \
            .u32 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VHEX(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_HEX,                                                                                                   \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VOCT(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_OCT,                                                                                                   \
        {                                                                                                              \
            .u64 = (x)                                                                                                 \
        }                                                                                                              \
    }
#define MMGR_VG(x)                                                                                                     \
    {                                                                                                                  \
        MMGR_FK_G,                                                                                                     \
        {                                                                                                              \
            .d = (x)                                                                                                   \
        }                                                                                                              \
    }
#define MMGR_VFIX(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_FIX,                                                                                                   \
        {                                                                                                              \
            .d = (x)                                                                                                   \
        }                                                                                                              \
    }
#define MMGR_VCH(x)                                                                                                    \
    {                                                                                                                  \
        MMGR_FK_CH,                                                                                                    \
        {                                                                                                              \
            .c = (x)                                                                                                   \
        }                                                                                                              \
    }
#define MMGR_VJSON(x)                                                                                                  \
    {                                                                                                                  \
        MMGR_FK_JSON,                                                                                                  \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }
#define MMGR_VXML(x)                                                                                                   \
    {                                                                                                                  \
        MMGR_FK_XML,                                                                                                   \
        {                                                                                                              \
            .s = (x)                                                                                                   \
        }                                                                                                              \
    }

size_t mmgr_numer_build(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);

size_t mmgr_numer_append(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);

typedef struct
{
    size_t (*build)(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);
    size_t (*append)(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);
} NumerosScriboNs;

static const NumerosScriboNs numer __attribute__((unused)) = {mmgr_numer_build, mmgr_numer_append};

#endif
