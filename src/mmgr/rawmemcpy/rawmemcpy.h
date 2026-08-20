// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_RAWMEMCPY_H
#define MMGR_RAWMEMCPY_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_RAW __attribute__((aligned(1), may_alias))

#define MMGR_ALIGN(n) __attribute__((aligned(n)))

#define MMGR_ALIAS __attribute__((may_alias))

#if MMGR_WORD_BITS >= 64
#define MMGR_RAW_WORD 8
#elif MMGR_WORD_BITS >= 32
#define MMGR_RAW_WORD 4
#else
#define MMGR_RAW_WORD 2
#endif

#define MMGR_MV_BITS (MMGR_RAW_WORD * 8u)

typedef uint16_t mmgr_raw_u16_t MMGR_RAW;
typedef uint32_t mmgr_raw_u32_t MMGR_RAW;
typedef uint64_t mmgr_raw_u64_t MMGR_RAW;

typedef uint16_t mmgr_al_u16_t MMGR_ALIAS;
typedef uint32_t mmgr_al_u32_t MMGR_ALIAS;
typedef uint64_t mmgr_al_u64_t MMGR_ALIAS;

#if MMGR_RAW_WORD >= 8
typedef uint64_t mmgr_mv_word;
#elif MMGR_RAW_WORD >= 4
typedef uint32_t mmgr_mv_word;
#else
typedef uint16_t mmgr_mv_word;
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
    mmgr_mv_word (*mv_load)(const unsigned char *p);
    void (*mv_put)(unsigned char *p, mmgr_mv_word v);
    void (*read)(void *dst, const void *p, size_t sz);
} RawNs;

MMGR_INLINE uint16_t mmgr_raw_u16(const void *p)
{
    return *(const mmgr_raw_u16_t *)p;
}

MMGR_INLINE uint32_t mmgr_raw_u32(const void *p)
{
    return *(const mmgr_raw_u32_t *)p;
}

MMGR_INLINE uint64_t mmgr_raw_u64(const void *p)
{
    return *(const mmgr_raw_u64_t *)p;
}

MMGR_INLINE uint64_t mmgr_raw_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return mmgr_raw_u16(p);
    case 4:
        return mmgr_raw_u32(p);
    case 8:
        return mmgr_raw_u64(p);
    default:
        return 0;
    }
}

MMGR_INLINE void mmgr_raw_put_u16(void *p, uint16_t v)
{
    *(mmgr_raw_u16_t *)p = v;
}

MMGR_INLINE void mmgr_raw_put_u32(void *p, uint32_t v)
{
    *(mmgr_raw_u32_t *)p = v;
}

MMGR_INLINE void mmgr_raw_put_u64(void *p, uint64_t v)
{
    *(mmgr_raw_u64_t *)p = v;
}

MMGR_INLINE uint64_t mmgr_al_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return *(const mmgr_al_u16_t *)p;
    case 4:
        return *(const mmgr_al_u32_t *)p;
    case 8:
        return *(const mmgr_al_u64_t *)p;
    default:
        return 0;
    }
}

MMGR_INLINE void mmgr_al_put_u16(void *p, uint16_t v)
{
    *(mmgr_al_u16_t *)p = v;
}

MMGR_INLINE void mmgr_al_put_u32(void *p, uint32_t v)
{
    *(mmgr_al_u32_t *)p = v;
}

MMGR_INLINE void mmgr_al_put_u64(void *p, uint64_t v)
{
    *(mmgr_al_u64_t *)p = v;
}

MMGR_INLINE mmgr_mv_word mmgr_mv_load(const unsigned char *p)
{
    return (mmgr_mv_word)mmgr_al_load(p, MMGR_RAW_WORD);
}

MMGR_INLINE void mmgr_mv_put(unsigned char *p, mmgr_mv_word v)
{
#if MMGR_RAW_WORD >= 8
    mmgr_al_put_u64(p, (uint64_t)v);
#elif MMGR_RAW_WORD >= 4
    mmgr_al_put_u32(p, (uint32_t)v);
#else
    mmgr_al_put_u16(p, (uint16_t)v);
#endif
}

void mmgr_raw_read(void *dst, const void *p, size_t sz);

static const RawNs raw __attribute__((unused)) = {.u16 = mmgr_raw_u16,
                                                  .u32 = mmgr_raw_u32,
                                                  .u64 = mmgr_raw_u64,
                                                  .load = mmgr_raw_load,
                                                  .put_u16 = mmgr_raw_put_u16,
                                                  .put_u32 = mmgr_raw_put_u32,
                                                  .put_u64 = mmgr_raw_put_u64,
                                                  .al_load = mmgr_al_load,
                                                  .al_put_u16 = mmgr_al_put_u16,
                                                  .al_put_u32 = mmgr_al_put_u32,
                                                  .al_put_u64 = mmgr_al_put_u64,
                                                  .mv_load = mmgr_mv_load,
                                                  .mv_put = mmgr_mv_put,
                                                  .read = mmgr_raw_read};

MMGR_END_DECLS

#endif
