// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TYPES_H
#define MMGR_TYPES_H

// The vocabulary this library declares in. Nothing here is a policy or a tunable - those live in
// mmgr_config.h, which includes this file after it has settled the widths below.
//
// Two headers and no more. stdint gives the exact-width integers, stddef gives size_t and NULL.
// assert.h is deliberately NOT here: it is needed only for the `static_assert` spelling, and
// _Static_assert is a keyword that costs no include at all. See MMGR_STATIC_ASSERT.
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------------------------
// Compile-time assertion
// ---------------------------------------------------------------------------------------------
// C11 has the _Static_assert keyword; the friendlier `static_assert` spelling is a macro that only
// exists once <assert.h> is included. C23 promoted static_assert to a keyword, and C++ has had it
// as one since C++11. Spelling it through this macro keeps the include list at two headers on every
// dialect this library claims to build under.
#if defined(__cplusplus)
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MMGR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

// ---------------------------------------------------------------------------------------------
// Linkage
// ---------------------------------------------------------------------------------------------
#ifdef __cplusplus
#define MMGR_BEGIN_DECLS                                                                                               \
    extern "C"                                                                                                         \
    {
#define MMGR_END_DECLS }
#else
#define MMGR_BEGIN_DECLS
#define MMGR_END_DECLS
#endif

// ---------------------------------------------------------------------------------------------
// Fixed-width scalars
// ---------------------------------------------------------------------------------------------
typedef uint8_t mmgr_u8;
typedef uint16_t mmgr_u16;
typedef uint32_t mmgr_u32;
typedef uint64_t mmgr_u64;
typedef int8_t mmgr_i8;
typedef int16_t mmgr_i16;
typedef int32_t mmgr_i32;
typedef int64_t mmgr_i64;

#ifdef __cplusplus
typedef bool mmgr_bool;
#else
typedef _Bool mmgr_bool;
#endif
#define MMGR_TRUE ((mmgr_bool)1)
#define MMGR_FALSE ((mmgr_bool)0)

// ---------------------------------------------------------------------------------------------
// Compiler attributes
// ---------------------------------------------------------------------------------------------
// Each of these is a thing C has no portable spelling for, which is the whole reason it is a macro
// rather than plain code. On a compiler that lacks the attribute the macro is empty and the code
// still compiles - correctly, just without the guarantee. Where the guarantee is load-bearing there
// is an MMGR_STATIC_ASSERT below that fails rather than letting it pass silently.
#if defined(__GNUC__) || defined(__clang__)
#define MMGR_ENUM_PACKED __attribute__((packed))
#define MMGR_ALIGN(n) __attribute__((aligned(n)))
#define MMGR_ALIAS __attribute__((may_alias))
#define MMGR_UNUSED __attribute__((unused))
#else
#define MMGR_ENUM_PACKED
#define MMGR_ALIGN(n)
#define MMGR_ALIAS
#define MMGR_UNUSED
#endif

#ifndef MMGR_INLINE
#if defined(__GNUC__)
#define MMGR_INLINE static inline __attribute__((always_inline))
#else
#define MMGR_INLINE static inline
#endif
#endif

// ---------------------------------------------------------------------------------------------
// Machine widths
// ---------------------------------------------------------------------------------------------
// MMGR_WORD_BITS and MMGR_INDEX_BITS are settled by mmgr_config.h before this header is read.
// Included on its own this header has no way to know them, so it says so rather than silently
// picking one: a wrong word width is not a compile error, it is a scan that reads past the end of a
// register on the target and passes every host test.
#if !defined(MMGR_WORD_BITS) || !defined(MMGR_INDEX_BITS)
#error "mmgr_types.h is not a standalone header - include mmgr_config.h, which sets the widths first"
#endif

#if MMGR_WORD_BITS == 64
typedef mmgr_u64 mmgr_word;
#elif MMGR_WORD_BITS == 32
typedef mmgr_u32 mmgr_word;
#elif MMGR_WORD_BITS == 16
typedef mmgr_u16 mmgr_word;
#else
#error "MMGR_WORD_BITS must be 16, 32 or 64 - see mmgr_config.h"
#endif

#if MMGR_INDEX_BITS == 32
typedef mmgr_u32 mmgr_idx;
#elif MMGR_INDEX_BITS == 16
typedef mmgr_u16 mmgr_idx;
#else
#error "MMGR_INDEX_BITS must be 16 or 32 - see mmgr_config.h"
#endif

// ---------------------------------------------------------------------------------------------
// What the target has to honour
// ---------------------------------------------------------------------------------------------
// stdint promises exact widths for the uintN_t family, so these look tautological. They are not:
// they are what turns "this target cannot host this library" into a diagnostic at the top of the
// build instead of a wrong answer somewhere in the middle of it.
MMGR_STATIC_ASSERT(sizeof(mmgr_u8) == 1, "mmgr_u8 must be exactly 8 bits: this target has no 8-bit type");
MMGR_STATIC_ASSERT(sizeof(mmgr_u16) * 8u == 16u, "mmgr_u16 must be exactly 16 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u32) * 8u == 32u, "mmgr_u32 must be exactly 32 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) * 8u == 64u, "mmgr_u64 must be exactly 64 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_word) * 8u == MMGR_WORD_BITS, "mmgr_word must be exactly MMGR_WORD_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) * 8u == MMGR_INDEX_BITS, "mmgr_idx must be exactly MMGR_INDEX_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) <= sizeof(mmgr_word), "an index must fit the register it is carried in");

// Every packed enum in this library is sized on the assumption that the attribute is honoured. If
// it is not, each one silently grows to int and every struct holding one changes size - which
// changes every offset carved out of a borrow. Probed once, here, rather than discovered as a
// corrupt region at run time.
typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENUM_PROBE_MIN = 0,
    MMGR_ENUM_PROBE_MAX = 255,
} MmgrEnumProbe;
MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
                   "MMGR_ENUM_PACKED is not honoured here, so no enum keeps its declared width and every "
                   "borrow offset computed from a struct containing one is wrong (TI: pass --small_enum)");

#endif // MMGR_TYPES_H
