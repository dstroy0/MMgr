/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Build-time settings: widths, region sizes, feature switches and the region carving macros.
 *
 * @note The tunables are guarded by #ifndef so a build may set them first; MMGR_SWAR_BITS is the exception.
 */
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @brief Width in bits of mmgr_word, the register every SWAR lane operation runs in.
 *
 * @note Derived from UINTPTR_MAX as 64, 32 or 16 when the build does not set it.
 * @warning A target whose UINTPTR_MAX matches none of the three raises #error; pass -DMMGR_WORD_BITS.
 */
#ifndef MMGR_WORD_BITS
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define MMGR_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define MMGR_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define MMGR_WORD_BITS 16
#else
#error "cannot derive MMGR_WORD_BITS from UINTPTR_MAX on this target - pass -DMMGR_WORD_BITS=16|32|64"
#endif
#endif

/**
 * @brief Width in bits of mmgr_idx, the type every offset and length is carried in.
 *
 * @note Follows MMGR_WORD_BITS below 32, and is capped at 32 above that.
 * @warning mmgr_types.h accepts only 16 or 32, and asserts mmgr_idx fits inside mmgr_word.
 */
#ifndef MMGR_INDEX_BITS
#if MMGR_WORD_BITS < 32
#define MMGR_INDEX_BITS MMGR_WORD_BITS
#else
#define MMGR_INDEX_BITS 32
#endif
#endif

/**
 * @brief Alignment applied to every carved region, and the granularity each pool must be a multiple of.
 *
 * @note MMGR_CARCER_CHECK asserts each pool is a whole number of these and at least two of them.
 */
#ifndef MMGR_ALIGN_BYTES

#define MMGR_ALIGN_BYTES 16u
#endif

#include "config/mmgr_types.h"

/**
 * @brief Width in bits of one SWAR word, always equal to MMGR_WORD_BITS.
 *
 * @warning Any earlier definition is discarded, so a build cannot set the two widths apart.
 */
#ifdef MMGR_SWAR_BITS
#undef MMGR_SWAR_BITS
#endif
#define MMGR_SWAR_BITS MMGR_WORD_BITS

/**
 * @brief Set to 1 to enable the library's debug-only checks.
 *
 * @note Declared ahead of MMGR_ASSERT because it selects which of the two the assert becomes.
 */
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif

/**
 * @brief Runtime assertion hook: a trap under MMGR_DEBUG_CHECKS, inert otherwise.
 *
 * @param[in] cond Condition the caller expects to hold.
 * @param[in] msg  String literal describing the expectation.
 * @note An expectation the library asserts is one a correct caller cannot break, so the shipping
 *       form pays nothing for it: it expands to a sizeof, which type checks cond and never evaluates
 *       it. The checks build evaluates it instead and stops on the spot, so a caller that broke one
 *       fails a test rather than carrying on with the damage done.
 * @note A build may define its own before including this header, and neither form below is then used.
 *       A target with no stderr and no abort wants that.
 * @warning With MMGR_DEBUG_CHECKS at 0, a failed expectation produces no diagnostic and no trap. It
 *          is not a runtime check and cannot be read as one.
 */
#ifndef MMGR_ASSERT
#if MMGR_DEBUG_CHECKS
#include <stdio.h>
#include <stdlib.h>
// fflush(NULL) before the trap, or a harness that buffers its progress on stdout loses every line of
// it to the abort and reports the failure with nothing naming which case reached it
#define MMGR_ASSERT(cond, msg)                                                                                         \
    ((cond) ? (void)0                                                                                                  \
            : (void)(fprintf(stderr, "MMGR_ASSERT failed: %s\n  %s:%d\n", (msg), __FILE__, __LINE__), fflush(NULL),    \
                     abort()))
#else
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif
#endif

/**
 * @brief Bytes in the largest plaintext confinium this build will declare.
 *
 * @note Allocates nothing and sizes no pool. A pool's extent is an argument to mmgr_carcer_init, and
 *       nothing in carceribus reads this. What it does is feed MMGR_CARCER_MAX below, which is a
 *       bound other modules size their worst case against - so it is a statement of intent about the
 *       regions you are going to declare, and it wants raising if you declare a bigger one.
 */
#ifndef MMGR_PLAINTEXT_CONFIN_SIZE
#define MMGR_PLAINTEXT_CONFIN_SIZE 4096u
#endif

/**
 * @brief Bytes in the largest secure confinium this build will declare.
 *
 * @note The same kind of number as MMGR_PLAINTEXT_CONFIN_SIZE, and it sizes no pool either.
 */
#ifndef MMGR_SECURE_CONFIN_SIZE
#define MMGR_SECURE_CONFIN_SIZE 4096u
#endif

/**
 * @brief Bytes in the larger of the two confinia, bounding the worst case a scan or a read must cover.
 *
 * @note verbum_scrutor.h derives MMGR_SCAN_MAX_WORDS from this, and mmgr_string_shim.h uses it as MMGR_STR_MAX.
 * @note The reason the bound exists is that these modules size a worst case at compile time rather
 *       than testing a length at run time. A string cannot be longer than the confinium holding it,
 *       so the confinium is the cap, and the scanner can be told how many words that is before it
 *       ever runs.
 * @warning Declare a region larger than both knobs and this bound is under-stated. The static asserts
 *          in verbum_scrutor.h check that the word count covers this value, not that this value
 *          covers your regions, which nothing here can see.
 */
#ifndef MMGR_CARCER_MAX
#if MMGR_PLAINTEXT_CONFIN_SIZE >= MMGR_SECURE_CONFIN_SIZE
#define MMGR_CARCER_MAX ((size_t)MMGR_PLAINTEXT_CONFIN_SIZE)
#else
#define MMGR_CARCER_MAX ((size_t)MMGR_SECURE_CONFIN_SIZE)
#endif
#endif

/**
 * @brief Set to 1 to build the memoriam_praetereo DMA path.
 *
 * @note mmgr.h includes memoriam_praetereo.h only when this is set, and it gates MMGR_PRAET_CHANNELS below.
 */
#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
/**
 * @brief Set to 1 to build the confinium_externum external memory path.
 *
 * @note mmgr.h includes confinium_externum.h only when this is set.
 */
#ifndef MMGR_ENABLE_EXTRAM

#define MMGR_ENABLE_EXTRAM 0
#endif

/**
 * @brief Set to 1 to build the time-based ring behavior.
 *
 * @note MMGR_RING_ATTACH_US exists only when this is set.
 */
#ifndef MMGR_ENABLE_CLOCK
#define MMGR_ENABLE_CLOCK 0
#endif

/**
 * @brief Microseconds a ring waits before attaching.
 *
 * @warning Defined only when MMGR_ENABLE_CLOCK is set.
 */
#if MMGR_ENABLE_CLOCK
#ifndef MMGR_RING_ATTACH_US
#define MMGR_RING_ATTACH_US 100u
#endif
#endif

/**
 * @brief DMA channels memoriam_praetereo carries, and the bytes each one buffers.
 *
 * @warning Both are defined only when MMGR_ENABLE_DMA is set.
 */
#if MMGR_ENABLE_DMA
#ifndef MMGR_PRAET_CHANNELS

#define MMGR_PRAET_CHANNELS 2
#endif
#ifndef MMGR_PRAET_BUF_SIZE

#define MMGR_PRAET_BUF_SIZE 256
#endif
#endif

/**
 * @brief Fails the build unless x_ has type size_t.
 *
 * @param[in] x_ Expression whose type is checked.
 * @note Evaluates to void; the _Generic association list admits size_t only.
 */
#define MMGR_MEMOR_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))

/**
 * @brief Fails the build unless x_ has type uint8_t.
 *
 * @param[in] x_ Expression whose type is checked.
 * @note Evaluates to void; the _Generic association list admits uint8_t only.
 */
#define MMGR_MEMOR_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

/**
 * @brief Defines a value-returning entry point that forwards an argument pack.
 *
 * @param PREFIX     The public entry point prefix (e.g., mmgr_infin_)
 * @param BACKEND    The backend function prefix (e.g., infin_)
 * @param CTX_TYPE   The context structure type (e.g., InfinCtx)
 * @param CFG_TYPE   The config structure type (e.g., InfinCfg)
 * @param RET_TYPE   Return type of the function
 * @param NAME       The core name of the function
 * @param ...        The variadic argument pack/fields to forward
 */
#define GENERIC_ENTRY(PREFIX, BACKEND, CTX_TYPE, CFG_TYPE, RET_TYPE, NAME, ...)                                        \
    RET_TYPE PREFIX##NAME(const CFG_TYPE *args)                                                                        \
    {                                                                                                                  \
        return MMGR_CALL(BACKEND##NAME, CTX_TYPE, __VA_ARGS__);                                                        \
    }

/**
 * @brief Defines a void entry point that forwards an argument pack.
 */
#define GENERIC_ENTRY_V(PREFIX, BACKEND, CTX_TYPE, CFG_TYPE, NAME, ...)                                                \
    void PREFIX##NAME(const CFG_TYPE *args)                                                                            \
    {                                                                                                                  \
        MMGR_CALL(BACKEND##NAME, CTX_TYPE, __VA_ARGS__);                                                               \
    }

#endif
