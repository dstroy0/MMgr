// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

// The entry point: every module includes this and nothing above it. It settles the widths, then
// pulls in the vocabulary, then states the tunables.
//
// Everything here is `#ifndef`-guarded, so any of it can be overridden from the build with -D
// without editing this file. That is the intended way to retarget: the defaults describe the
// machine the compiler says it is building for, not a machine anyone picked.

// ---------------------------------------------------------------------------------------------
// Machine widths - settled BEFORE mmgr_types.h, which types mmgr_word and mmgr_idx from them
// ---------------------------------------------------------------------------------------------
// Derived from the target rather than hardcoded. A word is the register the SWAR paths scan in, so
// the right default is the one the machine actually has: hardcoding 64 gives a 32-bit MCU a word
// twice its register width, and hardcoding 32 leaves half of every host register unused. UINTPTR_MAX
// is the portable way to ask, and it is a preprocessor-visible constant so this works in an #if.
//
// stdint is included here rather than relying on mmgr_types.h, because the answer is needed before
// that header is read.
#include <stdint.h>

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

// An index addresses a slot, not memory, and 32 bits is more slots than any deterministic,
// statically sized pool in this library can hold. Narrowing it to 16 is a real saving on a small
// target and is why it is a separate knob from the word.
#ifndef MMGR_INDEX_BITS
#define MMGR_INDEX_BITS 32
#endif

#include "mmgr_types.h"

// ---------------------------------------------------------------------------------------------
// SWAR
// ---------------------------------------------------------------------------------------------
// The width the ingestion path scans at. It defaults to the machine word because that is what SWAR
// is for - the point is to answer for MMGR_SWAR_BITS/8 bytes in one register operation. Set it
// lower only to model a narrower machine on a wider host, which is how the 16- and 32-bit lanes get
// exercised in host CI without a cross-compiler.
#ifndef MMGR_SWAR_BITS
#define MMGR_SWAR_BITS MMGR_WORD_BITS
#endif

MMGR_STATIC_ASSERT(MMGR_SWAR_BITS == 16 || MMGR_SWAR_BITS == 32 || MMGR_SWAR_BITS == 64,
                   "MMGR_SWAR_BITS must be 16, 32 or 64");
MMGR_STATIC_ASSERT(MMGR_SWAR_BITS <= MMGR_WORD_BITS,
                   "MMGR_SWAR_BITS exceeds MMGR_WORD_BITS - a lane wider than the register it is scanned in "
                   "does not degrade, it reads the wrong bytes");

// ---------------------------------------------------------------------------------------------
// Byte order
// ---------------------------------------------------------------------------------------------
// Asked of the compiler rather than declared. Every compiler this library targets defines
// __BYTE_ORDER__; the fallback is little-endian because every target it has actually been built for
// is, and a wrong guess here shows up immediately in the endian module's own tests.
#ifndef MMGR_HW_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MMGR_HW_BIG_ENDIAN 1
#else
#define MMGR_HW_BIG_ENDIAN 0
#endif
#endif

// ---------------------------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------------------------
// Every runtime check in this library goes through MMGR_ASSERT, and it does nothing by default.
//
// That default is what keeps the include list at stddef/stdint/stdatomic: <assert.h> is not
// reachable from here unless a build asks for it. Point it wherever the target wants - assert(), a
// trap instruction, a logger - by defining it before this header or with -D:
//
//     -DMMGR_ASSERT(cond,msg)='do{ if(!(cond)) my_trap(msg); }while(0)'
//
// The `(void)sizeof(...)` form is not a no-op in one important respect: the condition is still
// type-checked, so a check that stops compiling because the code around it changed is caught here
// rather than the day someone turns assertions on.
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif

// Whether the library keeps the state its debug checks need - notably the per-slot owner record
// that backs the single-owner check. Off by default: with MMGR_ASSERT a no-op that state is written
// and never read, which is exactly the kind of overhead this library exists not to have.
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif

#if MMGR_DEBUG_CHECKS
// Supplied by the integrator, not by this library: it has no idea what a "context" is on the target
// - a FreeRTOS task handle, a pthread_t, a core id, or 0 on something single-threaded.
//
// It backs the single-owner check, which records which context first borrowed a pool slot and
// MMGR_ASSERTs that every later borrow of that slot comes from the same one. Declared only under
// MMGR_DEBUG_CHECKS, so a release build neither calls it nor requires anyone to have written it.
#include <stdint.h>
uintptr_t mmgr_platform_context_id(void);
#endif

// ---------------------------------------------------------------------------------------------
// Pools
// ---------------------------------------------------------------------------------------------
// One worker unless a build says otherwise. The ghost slot is the index one past the last real
// worker: it is where a borrow taken outside any worker context is recorded, so the single-owner
// check has somewhere to put it rather than aliasing worker 0.
#ifndef MMGR_WORKER_COUNT
#define MMGR_WORKER_COUNT 1
#endif
#ifndef MMGR_GHOST_WORKER_SLOT
#define MMGR_GHOST_WORKER_SLOT (MMGR_WORKER_COUNT)
#endif

// Arena sizes.
//
// ProtoCore derived these by summing one work-area constant per protocol it implements - HTTP,
// TLS, SSH, SNMP and the rest. None of those exist here and this library has no opinion about what
// a consumer puts in an arena, so they are plain numbers with a default that is merely a starting
// point. A build that overruns one gets a static_assert naming it, not a corrupt allocation.
#ifndef MMGR_PLAINTEXT_ARENA_SIZE
#define MMGR_PLAINTEXT_ARENA_SIZE 4096u
#endif
#ifndef MMGR_SECURE_ARENA_SIZE
#define MMGR_SECURE_ARENA_SIZE 4096u
#endif

// ---------------------------------------------------------------------------------------------
// Optional modules
// ---------------------------------------------------------------------------------------------
// DMA and the PSRAM pool need hardware that a host does not have, so unlike every other module they
// are not simply selected by CMake - the code that talks to the peripheral has to be absent, not
// merely unlinked. Off by default, which is what makes the whole library host-buildable.
#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
#ifndef MMGR_ENABLE_PSRAM_POOL
#define MMGR_ENABLE_PSRAM_POOL 0
#endif

#if MMGR_ENABLE_DMA
#ifndef MMGR_DMA_CHANNELS
#define MMGR_DMA_CHANNELS 2
#endif
#ifndef MMGR_DMA_BUF_SIZE
#define MMGR_DMA_BUF_SIZE 256
#endif
#endif

// ---------------------------------------------------------------------------------------------
// Optimisation pragmas
// ---------------------------------------------------------------------------------------------
// _Pragma is the only way to reach a pragma from inside a macro. GCC honours a translation-unit
// optimize pragma; clang parses `GCC optimize` and ignores it, which is worse than not emitting it,
// so it is emitted for GCC only.
#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_TU_PRAGMA(directive) _Pragma(#directive)
#else
#define MMGR_TU_PRAGMA(directive)
#endif
#define MMGR_OPTIMIZE_O2 MMGR_TU_PRAGMA(GCC optimize("O2"))

#endif // MMGR_CONFIG_H
