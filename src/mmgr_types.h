// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TYPES_H
#define MMGR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "mmgr_compiler_directives.h"

/**
 * @file mmgr_types.h
 * @brief Fixed width types, and the two widths everything else derives from.
 *
 * Not standalone. mmgr_config.h sets MMGR_WORD_BITS and MMGR_INDEX_BITS first.
 */

/** @brief Fixed width integers. */
typedef uint8_t mmgr_u8;
typedef uint16_t mmgr_u16;
typedef uint32_t mmgr_u32;
typedef uint64_t mmgr_u64;
typedef int8_t mmgr_i8;
typedef int16_t mmgr_i16;
typedef int32_t mmgr_i32;
typedef int64_t mmgr_i64;

/** @brief Boolean. */
#ifdef __cplusplus
typedef bool mmgr_bool;
#else
typedef _Bool mmgr_bool;
#endif
/** @brief Boolean constants. */
#define MMGR_TRUE ((mmgr_bool)1)
#define MMGR_FALSE ((mmgr_bool)0)

#if !defined(MMGR_WORD_BITS) || !defined(MMGR_INDEX_BITS)
#error "mmgr_types.h is not a standalone header - include mmgr_config.h, which sets the widths first"
#endif

/** @brief The machine word. Every SWAR lane count follows it. */
#if MMGR_WORD_BITS == 64
typedef mmgr_u64 mmgr_word;
#elif MMGR_WORD_BITS == 32
typedef mmgr_u32 mmgr_word;
#elif MMGR_WORD_BITS == 16
typedef mmgr_u16 mmgr_word;
#else
#error "MMGR_WORD_BITS must be 16, 32 or 64 - see mmgr_config.h"
#endif

/** @brief A slot index. Never wider than the register that carries it. */
#if MMGR_INDEX_BITS == 32
typedef mmgr_u32 mmgr_idx;
#elif MMGR_INDEX_BITS == 16
typedef mmgr_u16 mmgr_idx;
#else
#error "MMGR_INDEX_BITS must be 16 or 32 - see mmgr_config.h"
#endif

MMGR_STATIC_ASSERT(sizeof(mmgr_u8) == 1, "mmgr_u8 must be exactly 8 bits: this target has no 8-bit type");
MMGR_STATIC_ASSERT(sizeof(mmgr_u16) * 8u == 16u, "mmgr_u16 must be exactly 16 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u32) * 8u == 32u, "mmgr_u32 must be exactly 32 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) * 8u == 64u, "mmgr_u64 must be exactly 64 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_word) * 8u == MMGR_WORD_BITS, "mmgr_word must be exactly MMGR_WORD_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) * 8u == MMGR_INDEX_BITS, "mmgr_idx must be exactly MMGR_INDEX_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) <= sizeof(mmgr_word), "an index must fit the register it is carried in");

/**
 * @brief Probe that proves MMGR_ENUM_PACKED was honored.
 *
 * An enum that silently becomes an int moves every field after it in every struct, and the library
 * addresses borrows by offset. Nothing at the use site can detect that, so it is asserted here.
 */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENUM_PROBE_MIN = 0,
    MMGR_ENUM_PROBE_MAX = 255,
} MmgrEnumProbe;
MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
                   "MMGR_ENUM_PACKED is not honored here, so no enum keeps its declared width and every "
                   "borrow offset computed from a struct containing one is wrong (TI: pass --small_enum)");

#endif
