/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_types.h
 * @brief The MMgr scalar types, and the assertions that pin their widths.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note Every width this library reasons about is decided here and proved here. A module takes the
 *       type rather than a platform spelling, so a build at another width changes one file.
 * @warning Not standalone. mmgr_config.h must set MMGR_WORD_BITS and MMGR_INDEX_BITS first, and the
 *          #error below fires when it has not. An editor opening this file on its own trips that
 *          #error, leaves mmgr_word undeclared, and reports the assertions below as errors. That is
 *          the missing include, not a defect in the assertions.
 */
#ifndef MMGR_TYPES_H
#define MMGR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @brief The eight-bit unsigned integer, which is the byte every scan and copy walks.
 *
 * @note Pinned below, and the only one of the eight whose assertion can fail on a real target. A
 *       part with no eight-bit type has no byte to walk.
 */
typedef uint8_t mmgr_u8;

/** @brief The sixteen-bit unsigned integer. Pinned below. */
typedef uint16_t mmgr_u16;

/** @brief The thirty-two-bit unsigned integer. Pinned below. */
typedef uint32_t mmgr_u32;

/** @brief The sixty-four-bit unsigned integer. Pinned below. */
typedef uint64_t mmgr_u64;

/**
 * @brief The eight-bit signed integer.
 *
 * @note The four signed aliases carry no assertion of their own. Each stdint type they alias is
 *       already exact by the standard, and the unsigned four are pinned because the word and the
 *       index are built out of them.
 */
typedef int8_t mmgr_i8;

/** @brief The sixteen-bit signed integer. Carries no assertion, as mmgr_i8 describes. */
typedef int16_t mmgr_i16;

/** @brief The thirty-two-bit signed integer. Carries no assertion, as mmgr_i8 describes. */
typedef int32_t mmgr_i32;

/** @brief The sixty-four-bit signed integer. Carries no assertion, as mmgr_i8 describes. */
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
 * @brief True, cast so it carries mmgr_bool rather than int.
 *
 * @note The cast is what keeps a comparison against it in the boolean type. An unparenthesized 1
 *       would promote and compare as int, which reads the same and is a different expression.
 */
#define MMGR_TRUE ((mmgr_bool)1)

/**
 * @brief False, cast so it carries mmgr_bool rather than int.
 *
 * @note The counterpart to MMGR_TRUE, cast for the same reason.
 */
#define MMGR_FALSE ((mmgr_bool)0)

/**
 * @brief Stops the build when this header is reached without the widths already chosen.
 *
 * @note mmgr_config.h sets both, then includes this file.
 */
#if !defined(MMGR_WORD_BITS) || !defined(MMGR_INDEX_BITS)
#error "mmgr_types.h is not a standalone header - include mmgr_config.h, which sets the widths first"
#endif

#if MMGR_WORD_BITS == 64

/**
 * @brief The unsigned word, MMGR_WORD_BITS wide, which every SWAR lane operation runs in.
 *
 * @note The register the library reasons in. A lane mask, a zero test and a byte count are all built
 *       to this width, so changing MMGR_WORD_BITS changes what a single operation covers.
 * @note Declared on each of the three arms below. The 64-bit arm carries this block because it is
 *       the one the compiler meets first.
 * @warning A MMGR_WORD_BITS that is not 16, 32 or 64 raises #error rather than falling through to a
 *          default width.
 */
typedef mmgr_u64 mmgr_word;

/**
 * @brief The signed word, the same register as mmgr_word.
 *
 * @note Carries a result that has gone negative, which an unsigned word would wrap instead. The
 *       assertion below holds it to mmgr_word's size, so the two are interchangeable as storage.
 */
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

#if MMGR_INDEX_BITS == 32

/**
 * @brief The index type, MMGR_INDEX_BITS wide, carrying every offset and length.
 *
 * @note Separate from the word because an offset does not need the register's full width. A 64-bit
 *       host carrying 32-bit offsets halves what a descriptor costs, and nothing this library bounds
 *       reaches four gigabytes.
 * @note Declared on both arms below. The 32-bit arm carries this block, being the one the compiler
 *       meets first.
 * @note The assertion below pins mmgr_idx no wider than mmgr_word, since an index that outgrew the
 *       register it is carried in could not be held.
 * @warning A MMGR_INDEX_BITS that is not 16 or 32 raises #error rather than falling through.
 */
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
 * @note Every offset this library computes is derived from these widths at compile time rather than
 *       measured at run time, so a width that is not what the code assumed has to fail the build.
 *       These eight are where that failure happens.
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
 * @warning A failure means MMGR_ENUM_PACKED expanded to nothing. The assertion message states the
 *          consequence.
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
