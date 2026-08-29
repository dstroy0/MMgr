/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Typed loads and stores: the word width, the arguments, and the proxim dispatch table.
 *
 * @note The load, put, load16 through put64 and read entries take any address; the al_ entries need an aligned one.
 */
#ifndef MMGR_PROXIMUS_OPEROR_H
#define MMGR_PROXIMUS_OPEROR_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Expands to MMGR_ALIGN(1) MMGR_ALIAS.
 *
 * @note Applied to a typedef, it drops the type's alignment to one byte and lets it alias other types.
 * @warning Either half expands to nothing where its attribute is unavailable, leaving the type's own alignment.
 */
#define MMGR_RAW MMGR_ALIGN(1) MMGR_ALIAS

/**
 * @brief Bytes in one raw word: 8 at MMGR_WORD_BITS 64 or more, 4 at 32 or more, 2 otherwise.
 *
 * @note Sets the stride of the word loop in proxim_read, and the granularity proxim_head aligns to.
 * @note memoria_operor uses it the same way, for the whole-word part of its copies and fills.
 */
#if MMGR_WORD_BITS >= 64
#define MMGR_RAW_WORD 8
#elif MMGR_WORD_BITS >= 32
#define MMGR_RAW_WORD 4
#else
#define MMGR_RAW_WORD 2
#endif

/**
 * @brief The unsigned type MMGR_RAW_WORD bytes wide: uint64_t at 8, uint32_t at 4, uint16_t otherwise.
 *
 * @note The type the load, put, al_load and al_put entries carry one word in.
 */
#if MMGR_RAW_WORD >= 8
typedef uint64_t mmgr_migro_word;
#elif MMGR_RAW_WORD >= 4
typedef uint32_t mmgr_migro_word;
#else

typedef uint16_t mmgr_migro_word;
#endif

/**
 * @brief Arguments for the proxim calls.
 *
 * @note The loads read at; the stores read dst and val; read is the only entry that reads size.
 */
typedef struct
{
    void *const dst;      /**< Destination for the stores and for read [BORROWS]. */
    const void *const at; /**< Source for the loads and for read [BORROWS]. */
    const uint64_t val;   /**< Value the stores write, taken from its low bytes. */
    const size_t size;    /**< Bytes the read entry copies. */
} ProximusCfg;

/**
 * @brief Type of the proxim dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the thirteen members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The four al_ members need an aligned address; the other nine take any address.
 */
typedef struct
{
    uint16_t (*load16)(const ProximusCfg *args);         /**< Reads two bytes. */
    uint32_t (*load32)(const ProximusCfg *args);         /**< Reads four bytes. */
    uint64_t (*load64)(const ProximusCfg *args);         /**< Reads eight bytes. */
    void (*put16)(const ProximusCfg *args);              /**< Writes two bytes. */
    void (*put32)(const ProximusCfg *args);              /**< Writes four bytes. */
    void (*put64)(const ProximusCfg *args);              /**< Writes eight bytes. */
    mmgr_migro_word (*load)(const ProximusCfg *args);    /**< Reads MMGR_RAW_WORD bytes. */
    void (*put)(const ProximusCfg *args);                /**< Writes MMGR_RAW_WORD bytes. */
    mmgr_migro_word (*al_load)(const ProximusCfg *args); /**< Reads MMGR_RAW_WORD bytes from an aligned address. */
    void (*al_put)(const ProximusCfg *args);             /**< Writes MMGR_RAW_WORD bytes to an aligned address. */
    uint64_t (*al_load64)(const ProximusCfg *args);      /**< Reads eight bytes from an aligned address. */
    void (*al_put64)(const ProximusCfg *args);           /**< Writes eight bytes to an aligned address. */
    void (*read)(const ProximusCfg *args);               /**< Copies size bytes, aligning the destination first. */
} ProximusOperorNs;
MMGR_NS_LAYOUT(ProximusOperorNs, load16, load32, load64, put16, put32, put64, load, put, al_load, al_put, al_load64,
               al_put64, read);

/**
 * @brief Reads two bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The two bytes as a uint16_t.
 * @warning args->at must be readable for two bytes; any alignment will do.
 */
uint16_t mmgr_proxim_load16(const ProximusCfg *args);

/**
 * @brief Reads four bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The four bytes as a uint32_t.
 * @warning args->at must be readable for four bytes; any alignment will do.
 */
uint32_t mmgr_proxim_load32(const ProximusCfg *args);

/**
 * @brief Reads eight bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The eight bytes as a uint64_t.
 * @warning args->at must be readable for eight bytes; any alignment will do.
 */
uint64_t mmgr_proxim_load64(const ProximusCfg *args);

/**
 * @brief Writes the low two bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note The upper six bytes of args->val take no part.
 * @warning args->dst must be writable for two bytes; any alignment will do.
 */
void mmgr_proxim_put16(const ProximusCfg *args);

/**
 * @brief Writes the low four bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note The upper four bytes of args->val take no part.
 * @warning args->dst must be writable for four bytes; any alignment will do.
 */
void mmgr_proxim_put32(const ProximusCfg *args);

/**
 * @brief Writes all eight bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @warning args->dst must be writable for eight bytes; any alignment will do.
 */
void mmgr_proxim_put64(const ProximusCfg *args);

/**
 * @brief Reads MMGR_RAW_WORD bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The bytes as an mmgr_migro_word.
 * @warning args->at must be readable for MMGR_RAW_WORD bytes; any alignment will do.
 */
mmgr_migro_word mmgr_proxim_load(const ProximusCfg *args);

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @warning args->dst must be writable for MMGR_RAW_WORD bytes; any alignment will do.
 */
void mmgr_proxim_put(const ProximusCfg *args);

/**
 * @brief Reads MMGR_RAW_WORD bytes from an aligned args->at, in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The bytes as an mmgr_migro_word.
 * @note Reaches the same bytes as mmgr_proxim_load, through a type that keeps mmgr_migro_word's alignment.
 * @warning args->at must be readable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
mmgr_migro_word mmgr_aequus_load(const ProximusCfg *args);

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of args->val to an aligned args->dst.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Reaches the same bytes as mmgr_proxim_put, through a type that keeps mmgr_migro_word's alignment.
 * @warning args->dst must be writable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
void mmgr_aequus_put(const ProximusCfg *args);

/**
 * @brief Reads eight bytes from an aligned args->at, in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The eight bytes as a uint64_t.
 * @note Reaches the same bytes as mmgr_proxim_load64, through a type that keeps uint64_t's alignment.
 * @warning args->at must be readable for eight bytes and aligned for a uint64_t.
 */
uint64_t mmgr_aequus_load64(const ProximusCfg *args);

/**
 * @brief Writes all eight bytes of args->val to an aligned args->dst.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Reaches the same bytes as mmgr_proxim_put64, through a type that keeps uint64_t's alignment.
 * @warning args->dst must be writable for eight bytes and aligned for a uint64_t.
 */
void mmgr_aequus_put64(const ProximusCfg *args);

/**
 * @brief Copies args->size bytes from args->at to args->dst.
 *
 * @param[in] args Destination, source and count [BORROWS].
 * @note Copies bytes until args->dst reaches an MMGR_RAW_WORD boundary, then whole words, then the odd bytes left.
 * @note This is the only entry that reads args->size.
 * @warning Copies forward, so an args->dst above args->at within one region would read bytes it has already written.
 */
void mmgr_proxim_read(const ProximusCfg *args);

/**
 * @brief Dispatch table instance named proxim.
 *
 * @note The nine unaligned members call the mmgr_proxim_ functions; the four al_ members call the mmgr_aequus_ ones.
 */
MMGR_NS ProximusOperorNs proxim MMGR_UNUSED = {
    .load16 = mmgr_proxim_load16,
    .load32 = mmgr_proxim_load32,
    .load64 = mmgr_proxim_load64,
    .put16 = mmgr_proxim_put16,
    .put32 = mmgr_proxim_put32,
    .put64 = mmgr_proxim_put64,
    .load = mmgr_proxim_load,
    .put = mmgr_proxim_put,
    .al_load = mmgr_aequus_load,
    .al_put = mmgr_aequus_put,
    .al_load64 = mmgr_aequus_load64,
    .al_put64 = mmgr_aequus_put64,
    .read = mmgr_proxim_read,
};

MMGR_FINIS_DECLS

#endif
