// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_RAWMEMCPY_H
#define PROTOCORE_RAWMEMCPY_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#define PROTO_RAW __attribute__((aligned(1), may_alias))

#define PROTO_ALIGN(n) __attribute__((aligned(n)))

#define PROTO_ALIAS __attribute__((may_alias))

#if PROTO_WORD_BITS >= 64
#define PROTO_RAW_WORD 8
#elif PROTO_WORD_BITS >= 32
#define PROTO_RAW_WORD 4
#else
#define PROTO_RAW_WORD 2
#endif

#define PROTO_MV_BITS (PROTO_RAW_WORD * 8u)

typedef uint16_t proto_raw_u16_t PROTO_RAW;
typedef uint32_t proto_raw_u32_t PROTO_RAW;
typedef uint64_t proto_raw_u64_t PROTO_RAW;

typedef uint16_t proto_al_u16_t PROTO_ALIAS;
typedef uint32_t proto_al_u32_t PROTO_ALIAS;
typedef uint64_t proto_al_u64_t PROTO_ALIAS;

#if PROTO_RAW_WORD >= 8
typedef uint64_t proto_mv_word;
#elif PROTO_RAW_WORD >= 4
typedef uint32_t proto_mv_word;
#else
typedef uint16_t proto_mv_word;
#endif

typedef struct
{
    uint16_t (*u16)(const void *p);
    uint32_t (*u32)(const void *p);
    uint64_t (*u64)(const void *p);
    uint64_t (*load)(const void *p, size_t n);
    void (*put_u16)(void *p, uint16_t v);
    void (*put_u32)(void *p, uint32_t v);
    void (*put_u64)(void *p, uint64_t v);
    uint64_t (*al_load)(const void *p, size_t n);
    void (*al_put_u16)(void *p, uint16_t v);
    void (*al_put_u32)(void *p, uint32_t v);
    void (*al_put_u64)(void *p, uint64_t v);
    proto_mv_word (*mv_load)(const unsigned char *p);
    void (*mv_put)(unsigned char *p, proto_mv_word v);
    void (*read)(void *dst, const void *p, size_t sz);
} RawNs;

PROTOCORE_INLINE uint16_t proto_raw_u16(const void *p)
{
    return *(const proto_raw_u16_t *)p;
}

PROTOCORE_INLINE uint32_t proto_raw_u32(const void *p)
{
    return *(const proto_raw_u32_t *)p;
}

PROTOCORE_INLINE uint64_t proto_raw_u64(const void *p)
{
    return *(const proto_raw_u64_t *)p;
}

PROTOCORE_INLINE uint64_t proto_raw_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return proto_raw_u16(p);
    case 4:
        return proto_raw_u32(p);
    case 8:
        return proto_raw_u64(p);
    default:
        return 0;
    }
}

PROTOCORE_INLINE void proto_raw_put_u16(void *p, uint16_t v)
{
    *(proto_raw_u16_t *)p = v;
}

PROTOCORE_INLINE void proto_raw_put_u32(void *p, uint32_t v)
{
    *(proto_raw_u32_t *)p = v;
}

PROTOCORE_INLINE void proto_raw_put_u64(void *p, uint64_t v)
{
    *(proto_raw_u64_t *)p = v;
}

PROTOCORE_INLINE uint64_t proto_al_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return *(const proto_al_u16_t *)p;
    case 4:
        return *(const proto_al_u32_t *)p;
    case 8:
        return *(const proto_al_u64_t *)p;
    default:
        return 0;
    }
}

PROTOCORE_INLINE void proto_al_put_u16(void *p, uint16_t v)
{
    *(proto_al_u16_t *)p = v;
}

PROTOCORE_INLINE void proto_al_put_u32(void *p, uint32_t v)
{
    *(proto_al_u32_t *)p = v;
}

PROTOCORE_INLINE void proto_al_put_u64(void *p, uint64_t v)
{
    *(proto_al_u64_t *)p = v;
}

PROTOCORE_INLINE proto_mv_word proto_mv_load(const unsigned char *p)
{
    return (proto_mv_word)proto_al_load(p, PROTO_RAW_WORD);
}

PROTOCORE_INLINE void proto_mv_put(unsigned char *p, proto_mv_word v)
{
#if PROTO_RAW_WORD >= 8
    proto_al_put_u64(p, (uint64_t)v);
#elif PROTO_RAW_WORD >= 4
    proto_al_put_u32(p, (uint32_t)v);
#else
    proto_al_put_u16(p, (uint16_t)v);
#endif
}

void proto_raw_read(void *dst, const void *p, size_t sz);

static const RawNs raw __attribute__((unused)) = {.u16 = proto_raw_u16,
                                                  .u32 = proto_raw_u32,
                                                  .u64 = proto_raw_u64,
                                                  .load = proto_raw_load,
                                                  .put_u16 = proto_raw_put_u16,
                                                  .put_u32 = proto_raw_put_u32,
                                                  .put_u64 = proto_raw_put_u64,
                                                  .al_load = proto_al_load,
                                                  .al_put_u16 = proto_al_put_u16,
                                                  .al_put_u32 = proto_al_put_u32,
                                                  .al_put_u64 = proto_al_put_u64,
                                                  .mv_load = proto_mv_load,
                                                  .mv_put = proto_mv_put,
                                                  .read = proto_raw_read};

PROTOCORE_END_DECLS

#endif
