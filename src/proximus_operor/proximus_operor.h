// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_PROXIMUS_OPEROR_H
#define MMGR_PROXIMUS_OPEROR_H

#include "config/mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file proximus_operor.h
 * @brief Loads and stores, aligned and not.
 *
 * The proxim entries read at any alignment; the aequus entries require it. On every target measured
 * these are the same single instruction, so the scans do not peel to a boundary to reach the second
 * set - alignment buys a property nothing here needs.
 */

/** @brief A type readable at any alignment. For a caller's punned pointer, not for our own use -
 *         the library's loads are byte assembly and need no exemption. */
#define MMGR_RAW MMGR_ALIGN(1) MMGR_ALIAS

/** @brief Bytes in a bulk move word. */
#if MMGR_WORD_BITS >= 64
#define MMGR_RAW_WORD 8
#elif MMGR_WORD_BITS >= 32
#define MMGR_RAW_WORD 4
#else
#define MMGR_RAW_WORD 2
#endif

/** @brief Bits in a bulk move word. */
#define MMGR_MV_BITS (MMGR_RAW_WORD * 8u)

/** @brief Unaligned-readable integer types. */
typedef uint16_t mmgr_proxim_u16_t MMGR_RAW;
typedef uint32_t mmgr_proxim_u32_t MMGR_RAW;
typedef uint64_t mmgr_proxim_u64_t MMGR_RAW;

/** @brief Aligned integer types, exempt from strict aliasing. */
typedef uint16_t mmgr_aequus_u16_t MMGR_ALIAS;
typedef uint32_t mmgr_aequus_u32_t MMGR_ALIAS;
typedef uint64_t mmgr_aequus_u64_t MMGR_ALIAS;

/** @brief The bulk move carrier. */
#if MMGR_RAW_WORD >= 8
typedef uint64_t mmgr_migro_word;
#elif MMGR_RAW_WORD >= 4
typedef uint32_t mmgr_migro_word;
#else
typedef uint16_t mmgr_migro_word;
#endif

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
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
    mmgr_migro_word (*mv_load)(const unsigned char *p);
    void (*mv_put)(unsigned char *p, mmgr_migro_word v);
    void (*read)(void *dst, const void *p, size_t sz);
} ProximusOperorNs;
MMGR_NS_LAYOUT(ProximusOperorNs, u16, u32, u64, load, put_u16, put_u32, put_u64, al_load, al_put_u16, al_put_u32,
               al_put_u64, mv_load, mv_put, read);

/**
 * @brief Read a uint16 at any alignment.
 * @param p Source.
 * @return The value.
 */
MMGR_INLINE uint16_t mmgr_proxim_u16(const void *p)
{
    return *(const mmgr_proxim_u16_t *)p;
}

/**
 * @brief Read a uint32 at any alignment.
 * @param p Source.
 * @return The value.
 */
MMGR_INLINE uint32_t mmgr_proxim_u32(const void *p)
{
    return *(const mmgr_proxim_u32_t *)p;
}

/**
 * @brief Read a uint64 at any alignment.
 * @param p Source.
 * @return The value.
 */
MMGR_INLINE uint64_t mmgr_proxim_u64(const void *p)
{
    return *(const mmgr_proxim_u64_t *)p;
}

/**
 * @brief Read @p n bytes at any alignment.
 * @param p Source.
 * @param n 1, 2, 4 or 8. Anything else reads nothing.
 * @return The value, zero extended.
 */
MMGR_INLINE uint64_t mmgr_proxim_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return mmgr_proxim_u16(p);
    case 4:
        return mmgr_proxim_u32(p);
    case 8:
        return mmgr_proxim_u64(p);
    /* GCOVR_EXCL_START - unreachable from inside the library: every internal call passes
       MMGR_SWAR_BYTES or MMGR_RAW_WORD, which are 2, 4 or 8 on every target. Kept because these are
       public entries and a caller can pass anything. */
    default:
        return 0;
        /* GCOVR_EXCL_STOP */
    }
}

/**
 * @brief Write a uint16 at any alignment.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_proxim_put_u16(void *p, uint16_t v)
{
    *(mmgr_proxim_u16_t *)p = v;
}

/**
 * @brief Write a uint32 at any alignment.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_proxim_put_u32(void *p, uint32_t v)
{
    *(mmgr_proxim_u32_t *)p = v;
}

/**
 * @brief Write a uint64 at any alignment.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_proxim_put_u64(void *p, uint64_t v)
{
    *(mmgr_proxim_u64_t *)p = v;
}

/**
 * @brief Read @p n bytes from an aligned address.
 * @param p Source, aligned to @p n.
 * @param n 1, 2, 4 or 8. Anything else reads nothing.
 * @return The value, zero extended.
 */
MMGR_INLINE uint64_t mmgr_aequus_load(const void *p, size_t n)
{
    switch (n)
    {
    case 1:
        return *(const unsigned char *)p;
    case 2:
        return *(const mmgr_aequus_u16_t *)p;
    case 4:
        return *(const mmgr_aequus_u32_t *)p;
    case 8:
        return *(const mmgr_aequus_u64_t *)p;
    /* GCOVR_EXCL_START - unreachable from inside the library: every internal call passes
       MMGR_SWAR_BYTES or MMGR_RAW_WORD, which are 2, 4 or 8 on every target. Kept because these are
       public entries and a caller can pass anything. */
    default:
        return 0;
        /* GCOVR_EXCL_STOP */
    }
}

/**
 * @brief Write a uint16 to an aligned address.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_aequus_put_u16(void *p, uint16_t v)
{
    *(mmgr_aequus_u16_t *)p = v;
}

/**
 * @brief Write a uint32 to an aligned address.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_aequus_put_u32(void *p, uint32_t v)
{
    *(mmgr_aequus_u32_t *)p = v;
}

/**
 * @brief Write a uint64 to an aligned address.
 * @param p Destination.
 * @param v Value.
 */
MMGR_INLINE void mmgr_aequus_put_u64(void *p, uint64_t v)
{
    *(mmgr_aequus_u64_t *)p = v;
}

/**
 * @brief Read one bulk move word.
 * @param p Source, aligned.
 * @return The word.
 */
MMGR_INLINE mmgr_migro_word mmgr_migro_load(const unsigned char *p)
{
    return (mmgr_migro_word)mmgr_aequus_load(p, MMGR_RAW_WORD);
}

/**
 * @brief Write one bulk move word.
 * @param p Destination, aligned.
 * @param v Value.
 */
MMGR_INLINE void mmgr_migro_put(unsigned char *p, mmgr_migro_word v)
{
#if MMGR_RAW_WORD >= 8
    mmgr_aequus_put_u64(p, (uint64_t)v);
#elif MMGR_RAW_WORD >= 4
    mmgr_aequus_put_u32(p, (uint32_t)v);
#else
    mmgr_aequus_put_u16(p, (uint16_t)v);
#endif
}

/**
 * @brief Copy @p sz bytes.
 * @param dst Destination.
 * @param p Source.
 * @param sz Byte count.
 */
void mmgr_proxim_read(void *dst, const void *p, size_t sz);

/** @brief Module namespace. */
MMGR_NS ProximusOperorNs proxim MMGR_UNUSED = {.u16 = mmgr_proxim_u16,
                                               .u32 = mmgr_proxim_u32,
                                               .u64 = mmgr_proxim_u64,
                                               .load = mmgr_proxim_load,
                                               .put_u16 = mmgr_proxim_put_u16,
                                               .put_u32 = mmgr_proxim_put_u32,
                                               .put_u64 = mmgr_proxim_put_u64,
                                               .al_load = mmgr_aequus_load,
                                               .al_put_u16 = mmgr_aequus_put_u16,
                                               .al_put_u32 = mmgr_aequus_put_u32,
                                               .al_put_u64 = mmgr_aequus_put_u64,
                                               .mv_load = mmgr_migro_load,
                                               .mv_put = mmgr_migro_put,
                                               .read = mmgr_proxim_read};

MMGR_END_DECLS

#endif
