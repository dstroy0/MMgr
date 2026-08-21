// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @file mmgr_config.h
 * @brief Every compile time knob. Include this, not mmgr_types.h.
 *
 * Nothing here is discovered at run time. Widths, tenant sizes and feature gates are all constants,
 * which is what lets a bounded scan resolve to a fixed number of loads.
 */

/**
 * @brief Machine word width. Derived from UINTPTR_MAX unless a build pins it.
 *
 * Pass -DMMGR_WORD_BITS=16 to model a narrower machine. Everything follows from it.
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
 * @brief Slot index width.
 *
 * 32 is more slots than any statically sized pool here can hold, so it is the default wherever the
 * register can carry it. On a 16-bit machine it cannot, and an index wider than the register is
 * spilled arithmetic on every use.
 */
#ifndef MMGR_INDEX_BITS
#if MMGR_WORD_BITS < 32
#define MMGR_INDEX_BITS MMGR_WORD_BITS
#else
#define MMGR_INDEX_BITS 32
#endif
#endif

#include "config/mmgr_types.h"

/**
 * @brief Scan width. Always the word width, and not overridable.
 *
 * It was a separate knob once and that was wrong. Setting it below MMGR_WORD_BITS asks the machine
 * to operate on less than a register, which is never faster: the same cache line moves, the same
 * load port is used, C promotes to int anyway, and fewer lanes are processed for it. To model a
 * narrower machine, narrow the machine.
 */
#ifdef MMGR_SWAR_BITS
#undef MMGR_SWAR_BITS
#endif
#define MMGR_SWAR_BITS MMGR_WORD_BITS

/** @brief Contract check. Compiles to nothing unless a build supplies one. */
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif

/** @brief Compile the debug checks in. */
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif

/** @brief How many workers may hold a tenant at once. */
#ifndef MMGR_WORKER_COUNT
#define MMGR_WORKER_COUNT 1
#endif
/** @brief Slot used by a borrow with no worker behind it. */
#ifndef MMGR_GHOST_WORKER_SLOT
#define MMGR_GHOST_WORKER_SLOT (MMGR_WORKER_COUNT)
#endif

/** @brief Whether a platform context id is needed at all. */
#if (MMGR_WORKER_COUNT != 1) || MMGR_DEBUG_CHECKS
#define MMGR_NEEDS_CONTEXT_ID 1
#else
#define MMGR_NEEDS_CONTEXT_ID 0
#endif

#if MMGR_NEEDS_CONTEXT_ID
uintptr_t mmgr_platform_context_id(void);
#endif

/** @brief Size of one plaintext tenant. */
#ifndef MMGR_PLAINTEXT_CONFIN_SIZE
#define MMGR_PLAINTEXT_CONFIN_SIZE 4096u
#endif
/** @brief Size of one secure tenant. */
#ifndef MMGR_SECURE_CONFIN_SIZE
#define MMGR_SECURE_CONFIN_SIZE 4096u
#endif

/**
 * @brief The largest single tenant, and so the worst case every bounded scan plans for.
 *
 * One slot, not the whole buffer. A custodia dimensions its store as
 * mem[MMGR_REG_POOL_SLOTS][MMGR_PLAINTEXT_CONFIN_SIZE], and a string lives inside one tenant's
 * confinium. The buffer is that times the slot count and no string ever spans it.
 */
#ifndef MMGR_CONFIN_MAX
#if MMGR_PLAINTEXT_CONFIN_SIZE >= MMGR_SECURE_CONFIN_SIZE
#define MMGR_CONFIN_MAX ((size_t)MMGR_PLAINTEXT_CONFIN_SIZE)
#else
#define MMGR_CONFIN_MAX ((size_t)MMGR_SECURE_CONFIN_SIZE)
#endif
#endif

/** @brief Build the DMA module. */
#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
/** @brief Build the external pool module. */
#ifndef MMGR_ENABLE_PSRAM_POOL
#define MMGR_ENABLE_PSRAM_POOL 0
#endif

/** @brief DMA channel count and buffer size. */
#if MMGR_ENABLE_DMA
#ifndef MMGR_DMA_CHANNELS
#define MMGR_DMA_CHANNELS 2
#endif
#ifndef MMGR_DMA_BUF_SIZE
#define MMGR_DMA_BUF_SIZE 256
#endif
#endif

#endif
