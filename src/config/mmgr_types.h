/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_types.h
 * @brief The MMgr scalar types, and the assertions that pin their widths.
 *
 * @warning Not standalone: mmgr_config.h must set MMGR_WORD_BITS and MMGR_INDEX_BITS first.
 */
#ifndef MMGR_TYPES_H
#define MMGR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @brief Fixed-width integers under MMgr names.
 *
 * @note The assertions further down pin mmgr_u8 through mmgr_u64. The signed four carry none of
 *       their own.
 */
typedef uint8_t mmgr_u8;
typedef uint16_t mmgr_u16;
typedef uint32_t mmgr_u32;
typedef uint64_t mmgr_u64;
typedef int8_t mmgr_i8;
typedef int16_t mmgr_i16;
typedef int32_t mmgr_i32;
typedef int64_t mmgr_i64;

/**
 * @brief Boolean carrier: bool under C++, _Bool otherwise.
 */
#ifdef __cplusplus
typedef bool mmgr_bool;
#else

typedef _Bool mmgr_bool;
#endif

/**
 * @brief The two mmgr_bool values, each cast so it carries the boolean type rather than int.
 */
#define MMGR_TRUE ((mmgr_bool)1)
#define MMGR_FALSE ((mmgr_bool)0)

/**
 * @brief Stops the build when this header is reached without the widths already chosen.
 *
 * @note mmgr_config.h sets both, then includes this file.
 */
#if !defined(MMGR_WORD_BITS) || !defined(MMGR_INDEX_BITS)
#error "mmgr_types.h is not a standalone header - include mmgr_config.h, which sets the widths first"
#endif

/**
 * @brief The unsigned and signed word types, both MMGR_WORD_BITS wide.
 *
 * @note mmgr_word carries every SWAR lane operation; mmgr_iword carries the signed results.
 * @warning A MMGR_WORD_BITS that is not 16, 32 or 64 raises #error.
 */
#if MMGR_WORD_BITS == 64

typedef mmgr_u64 mmgr_word;

typedef mmgr_i64 mmgr_iword;
#elif MMGR_WORD_BITS == 32
typedef mmgr_u32 mmgr_word;
typedef mmgr_i32 mmgr_iword;
#elif MMGR_WORD_BITS == 16
typedef mmgr_u16 mmgr_word;
typedef mmgr_i16 mmgr_iword;
#else
#error "MMGR_WORD_BITS must be 16, 32 or 64 - see mmgr_config.h"
#endif

/**
 * @brief The index type, MMGR_INDEX_BITS wide, used for every offset and length.
 *
 * @note The assertion below pins mmgr_idx no wider than mmgr_word.
 * @warning A MMGR_INDEX_BITS that is not 16 or 32 raises #error.
 */
#if MMGR_INDEX_BITS == 32

typedef mmgr_u32 mmgr_idx;
#elif MMGR_INDEX_BITS == 16
typedef mmgr_u16 mmgr_idx;
#else
#error "MMGR_INDEX_BITS must be 16 or 32 - see mmgr_config.h"
#endif

/**
 * @brief Pins the four unsigned widths, both word types and mmgr_idx.
 *
 * @note Six compare a type's size against a declared width. The other two compare two types, holding
 *       mmgr_iword to the same register as mmgr_word and mmgr_idx no wider than it.
 */
MMGR_STATIC_ASSERT(sizeof(mmgr_u8) == 1, "mmgr_u8 must be exactly 8 bits: this target has no 8-bit type");
MMGR_STATIC_ASSERT(sizeof(mmgr_u16) * 8u == 16u, "mmgr_u16 must be exactly 16 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u32) * 8u == 32u, "mmgr_u32 must be exactly 32 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) * 8u == 64u, "mmgr_u64 must be exactly 64 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_word) * 8u == MMGR_WORD_BITS, "mmgr_word must be exactly MMGR_WORD_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_iword) == sizeof(mmgr_word), "the signed word must be the same register as the word");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) * 8u == MMGR_INDEX_BITS, "mmgr_idx must be exactly MMGR_INDEX_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) <= sizeof(mmgr_word), "an index must fit the register it is carried in");

/**
 * @brief A one-byte enum used only to prove MMGR_ENUM_PACKED reaches the compiler.
 *
 * @note Its range needs a single byte, so the assertion below fails exactly when packing is ignored.
 * @warning A failure means MMGR_ENUM_PACKED expanded to nothing; the assertion message states the consequence.
 */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENUM_PROBE_MIN = 0,   /**< Low end of the probe range. */
    MMGR_ENUM_PROBE_MAX = 255, /**< High end, the largest value one byte holds. */
} MmgrEnumProbe;
MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
                   "MMGR_ENUM_PACKED is not honored here, so no enum keeps its declared width and every "
                   "borrow offset computed from a struct containing one is wrong (TI: pass --small_enum)");

#endif
