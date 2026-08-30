/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_config.h
 * @brief Build-time settings: widths, cellblock size bounds, feature switches, the assert hook and
 *        the entry point macros.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note Every tunable here is guarded by #ifndef, so a build sets one on the command line or from a
 *       header included ahead of this. MMGR_SWAR_BITS is the exception and is forced below.
 * @note The one place a build is configured. A module reaches this header rather than the platform,
 *       so a width or a switch is answered in one file instead of at each use.
 */
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @brief Width in bits of mmgr_word, the register every SWAR lane operation runs in.
 *
 * @note Derived from UINTPTR_MAX as 64, 32 or 16 when the build does not set it, because on a host
 *       the register a word lands in is the pointer width.
 * @note A build sets it to model a narrower machine on this one. The word32 and word16 environments
 *       in the root CMakeLists.txt define it, so a change that breaks only a narrow word fails a
 *       build here rather than waiting for someone to own that part.
 * @warning A target whose UINTPTR_MAX matches none of the three raises #error rather than guessing a
 *          width. Pass -DMMGR_WORD_BITS=16|32|64.
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
 * @note Follows MMGR_WORD_BITS below 32, and is capped at 32 above that. An index wider than the
 *       word it indexes into buys nothing, and 32 bits already covers every buffer this library is
 *       meant to bound.
 * @note Separate from the word so a 64-bit host can carry 32-bit offsets, which is what the idx16
 *       environment in the root CMakeLists.txt pairs against a 64-bit word.
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
 * @brief Alignment a caller declares its own storage at before handing those bytes to a module.
 *
 * @note Nothing under src reads it. The tests, the benches and the region-edges example are what
 *       write MMGR_ALIGN(MMGR_ALIGN_BYTES) on their arrays; the default is 16.
 * @note Has to be a power of two, which the build checks below. It reaches an alignment specifier,
 *       and one given anything else is ill-formed, so the check is here to name the knob rather than
 *       leave the diagnostic pointing at whichever array was declared with it.
 * @warning Not the alignment a locus_carcerum cell comes back at. That is MMGR_CARCER_ALIGN, which is
 *          sizeof(mmgr_word), and nothing asserts the two agree.
 */
#ifndef MMGR_ALIGN_BYTES

#define MMGR_ALIGN_BYTES 16u
#endif
#if (MMGR_ALIGN_BYTES < 1) || ((MMGR_ALIGN_BYTES & (MMGR_ALIGN_BYTES - 1)) != 0)
#error "MMGR_ALIGN_BYTES must be a power of two - an alignment specifier takes nothing else"
#endif

#include "config/mmgr_types.h"

/**
 * @brief Width in bits of one SWAR word, always equal to MMGR_WORD_BITS.
 *
 * @note The one tunable here that is not a tunable. A SWAR lane operation runs in a register, so the
 *       lane width and the word width are the same fact under two names, and the second name exists
 *       only so a scan reads as a scan.
 * @warning Any earlier definition is discarded rather than respected, so a build cannot set the two
 *          widths apart. Two widths that disagree would put a mask built for one register into an
 *          operation running in another, and nothing downstream tests for that.
 */
#ifdef MMGR_SWAR_BITS
#undef MMGR_SWAR_BITS
#endif
#define MMGR_SWAR_BITS MMGR_WORD_BITS

/**
 * @brief Set to 1 to enable the library's debug-only checks.
 *
 * @note Declared ahead of MMGR_ASSERT because it selects which of the two the assert becomes.
 * @note Takes 0 or 1 and nothing else. A switch read with #if would treat any non-zero as set, so the
 *       check below is what keeps a mistyped value from quietly meaning on.
 */
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif
#if (MMGR_DEBUG_CHECKS != 0) && (MMGR_DEBUG_CHECKS != 1)
#error "MMGR_DEBUG_CHECKS must be 0 or 1"
#endif

/**
 * @brief Runtime assertion hook: a trap under MMGR_DEBUG_CHECKS, inert otherwise.
 *
 * @param[in] cond_ Condition the caller expects to hold.
 * @param[in] msg_  String literal describing the expectation.
 * @note An expectation the library asserts is one a correct caller cannot break, so the shipping
 *       form pays nothing for it. It expands to a sizeof, which type checks cond_ and never
 *       evaluates it. The checks build evaluates it instead and stops on the spot, so a caller that
 *       broke one fails a test rather than carrying on with the damage done.
 * @note A build may define its own before including this header, and neither form below is then
 *       used. A target with no stderr and no abort wants that.
 * @warning With MMGR_DEBUG_CHECKS at 0, a failed expectation produces no diagnostic and no trap. It
 *          is not a runtime check and cannot be read as one.
 * @warning cond_ must carry no side effect. The shipping form never evaluates it, so an assert
 *          holding the work would do nothing in the build that ships.
 */
#ifndef MMGR_ASSERT
#if MMGR_DEBUG_CHECKS
#include <stdio.h>
#include <stdlib.h>
// fflush(NULL) before the trap, or a harness that buffers its progress on stdout loses every line of
// it to the abort and reports the failure with nothing naming which case reached it
#define MMGR_ASSERT(cond_, msg_)                                                                                       \
    ((cond_) ? (void)0                                                                                                 \
             : (void)(fprintf(stderr, "MMGR_ASSERT failed: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL),  \
                      abort()))
#else
#define MMGR_ASSERT(cond_, msg_) ((void)sizeof((cond_) ? 1 : 0), (void)0)
#endif
#endif

/**
 * @brief Stops the program on an illegal call, in every build.
 *
 * @param[in] msg_ String literal naming what was violated.
 * @note Separate from MMGR_ASSERT because the two answer different questions. MMGR_ASSERT states an
 *       expectation a correct caller cannot break, and it is inert in the build that ships. This one
 *       marks a call that is illegal to make, so it has to hold in that build too.
 * @note Reached where continuing would be worse than stopping. A release handed a prisoner from
 *       another cellblock is the case it exists for: the caller has shown it does not know which
 *       cellblock owns that memory, so its next release and its next allocation are both suspect,
 *       and on a maximum security cellblock a quiet return would leave bytes unzeroed that the
 *       caller believes were wiped.
 * @note Does not return. Every call site is written on that basis and carries no fallback after it.
 * @note What is unconditional is the stop, not the diagnostic. MMGR_DEBUG_CHECKS selects which of the
 *       two forms below is used, and both halt. Only the checks form names what was violated, which
 *       is what keeps this header free of libc in the build that ships.
 * @warning A build may define its own before including this header, and neither form below is then
 *          used. A target that reports a fault through its own handler wants that, and so does a
 *          test harness, which would otherwise hang on the shipping form rather than failing.
 * @warning The shipping form spins. A target without abort has no other way to stop, and a halt is
 *          what a debugger can catch.
 */
#ifndef MMGR_FATAL
#if MMGR_DEBUG_CHECKS
#include <stdio.h>
#include <stdlib.h>
// fflush(NULL) before the trap, for the same reason MMGR_ASSERT flushes: a buffered harness would
// otherwise lose the lines naming what reached this
#define MMGR_FATAL(msg_)                                                                                               \
    ((void)(fprintf(stderr, "MMGR_FATAL: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL), abort()))
#else
// No libc here, so the trap is a halt. The cast to void discards msg_, which nothing reads on this
// arm and would otherwise be an unused argument
#define MMGR_FATAL(msg_)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(msg_);                                                                                                  \
        for (;;)                                                                                                       \
        {                                                                                                              \
        }                                                                                                              \
    } while (0)
#endif
#endif

/**
 * @brief Bytes in the largest plaintext confinium this build will declare.
 *
 * @note Allocates nothing and sizes no cellblock. A cellblock's extent is the size in its
 *       MMGR_MINIMUM_SECURITY or MMGR_MAXIMUM_SECURITY declaration, and nothing in locus_carcerum reads
 *       this. What it does is feed MMGR_CARCER_MAX below, which is a bound other modules size their
 *       worst case against, so it is a statement of intent about the prison sites you are going to
 *       declare.
 * @note A cellblock declared larger than MMGR_CARCER_MAX fails the build at its own declaration, and
 *       the assert in MMGR_CARCER_BODY names this knob and MMGR_SECURE_CONFIN_SIZE as the two that
 *       raise it. Nothing has to be remembered.
 */
#ifndef MMGR_PLAINTEXT_CONFIN_SIZE
#define MMGR_PLAINTEXT_CONFIN_SIZE 4096u
#endif

/**
 * @brief Bytes in the largest secure confinium this build will declare.
 *
 * @note The same kind of number as MMGR_PLAINTEXT_CONFIN_SIZE, and it sizes no cellblock either.
 *       Separate from it because a build may hold far less secure storage than plaintext, and
 *       MMGR_CARCER_MAX takes the larger of the two.
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
 * @note A cellblock declared larger than this fails the build where it is declared. MMGR_CARCER_BODY
 *       asserts against this value, so the bound cannot be under-stated without the build saying so
 *       and naming the two knobs that raise it.
 * @warning The static asserts in verbum_scrutor.h check that the word count covers this value, which
 *          is a different question. They would still hold on a build whose cellblocks overran it,
 *          and the assert at the declaration is what makes that unreachable.
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
 * @note Takes 0 or 1, checked below for the reason MMGR_DEBUG_CHECKS is.
 */
#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
#if (MMGR_ENABLE_DMA != 0) && (MMGR_ENABLE_DMA != 1)
#error "MMGR_ENABLE_DMA must be 0 or 1"
#endif

/**
 * @brief Set to 1 to build the confinium_externum external memory path.
 *
 * @note mmgr.h includes confinium_externum.h only when this is set.
 * @note Takes 0 or 1, checked below for the reason MMGR_DEBUG_CHECKS is.
 */
#ifndef MMGR_ENABLE_EXTRAM

#define MMGR_ENABLE_EXTRAM 0
#endif
#if (MMGR_ENABLE_EXTRAM != 0) && (MMGR_ENABLE_EXTRAM != 1)
#error "MMGR_ENABLE_EXTRAM must be 0 or 1"
#endif

/**
 * @brief Set to 1 to build the time-based ring behavior.
 *
 * @note MMGR_RING_ATTACH_US exists only when this is set.
 * @note Takes 0 or 1, checked below for the reason MMGR_DEBUG_CHECKS is.
 */
#ifndef MMGR_ENABLE_CLOCK
#define MMGR_ENABLE_CLOCK 0
#endif
#if (MMGR_ENABLE_CLOCK != 0) && (MMGR_ENABLE_CLOCK != 1)
#error "MMGR_ENABLE_CLOCK must be 0 or 1"
#endif

/**
 * @brief Microseconds to allow the platform to settle around an attach or a detach.
 *
 * @note The wait is for register propagation. Attaching or detaching DMA against a region writes the
 *       platform's setup registers, and those do not take effect the moment they are written, so
 *       work started immediately would run against a half-configured path.
 * @note Non-blocking. It is a deadline to check against, not a spin, so work that is not ready yet
 *       is deferred rather than waited on.
 * @note A port layer reads this and holds the timer itself. Nothing under src spends it, which is
 *       what makes it a figure the integration is given rather than a wait this library performs.
 * @note What has to settle belongs to the part rather than to this library, so a build sets the
 *       value its part needs.
 * @warning The default of 100 is a placeholder. Microseconds is the expected order, and neither the
 *          unit nor the figure has been measured on hardware. Do not read it as a tested value.
 * @warning Defined only when MMGR_ENABLE_CLOCK is set.
 */
#if MMGR_ENABLE_CLOCK
#ifndef MMGR_RING_ATTACH_US
#define MMGR_RING_ATTACH_US 100u
#endif
#endif

#if MMGR_ENABLE_DMA
/**
 * @brief DMA channels memoriam_praetereo carries.
 *
 * @note Fixed at the build rather than counted at run time, so the channel state is emitted as data
 *       and a channel index is bounded before it is used rather than tested at each use.
 * @note Accepts 1 through 256, and the build stops outside that. At zero every entry point rejects
 *       every index, since each one tests against this count before it does anything, so the DMA path
 *       would link and then refuse every call. Above 256 the extra channels cannot be named: channel
 *       is a uint8_t in PraetCfg, PraetTransferCfg and mmgr_praet_event.
 * @warning Defined only when MMGR_ENABLE_DMA is set. A build reading it with DMA off has no channel
 *          state to go with it.
 */
#ifndef MMGR_PRAET_CHANNELS

#define MMGR_PRAET_CHANNELS 2u
#endif
#if (MMGR_PRAET_CHANNELS < 1) || (MMGR_PRAET_CHANNELS > 256)
#error "MMGR_PRAET_CHANNELS must be 1 to 256 - a channel index is carried in a uint8_t"
#endif

/**
 * @brief Bytes each DMA channel buffers.
 *
 * @note Multiplied by MMGR_PRAET_CHANNELS to size the storage the module emits, so raising either
 *       one raises the static footprint of the DMA path.
 * @note Accepts 1 through 65535, and the build stops outside that. The ceiling is what a transfer
 *       length can hold, bytes being a uint16_t in PraetTransferCfg and mmgr_praet_event, so a size
 *       above it describes room no caller can ask for.
 * @warning Defined only when MMGR_ENABLE_DMA is set.
 */
#ifndef MMGR_PRAET_BUF_SIZE

#define MMGR_PRAET_BUF_SIZE 256u
#endif
#if (MMGR_PRAET_BUF_SIZE < 1) || (MMGR_PRAET_BUF_SIZE > 65535)
#error "MMGR_PRAET_BUF_SIZE must be 1 to 65535 - a transfer length is carried in a uint16_t"
#endif
#endif

/**
 * @brief Defines a value-returning entry point that forwards an argument pack.
 *
 * @param[in] entry_prefix_   Public entry point prefix, such as mmgr_infin_.
 * @param[in] backend_prefix_ Backend function prefix, such as infin_.
 * @param[in] CtxType_        Type of the compound literal the backend receives, such as InfinCtx.
 * @param[in] CfgType_        Type the emitted entry takes a pointer to, such as InfinCfg.
 * @param[in] ReturnType_     Return type of the emitted function.
 * @param[in] name_           Core name, pasted onto both prefixes.
 * @param[in] ...             Initializers for the CtxType_ literal, written in terms of args.
 * @return                    What the backend returns.
 * @note One shape for every entry in the library, so a caller meets the same call at each module.
 *       The entry tests nothing; whatever checking an operation needs belongs in the backend it
 *       names.
 * @warning The initializers dereference args, so it must not be NULL [BORROWS].
 */
#define GENERIC_ENTRY(entry_prefix_, backend_prefix_, CtxType_, CfgType_, ReturnType_, name_, ...)                     \
    ReturnType_ entry_prefix_##name_(const CfgType_ *args)                                                             \
    {                                                                                                                  \
        return MMGR_CALL(backend_prefix_##name_, CtxType_, __VA_ARGS__);                                               \
    }

/**
 * @brief Defines a void entry point that forwards an argument pack.
 *
 * @param[in] entry_prefix_   Public entry point prefix, such as mmgr_infin_.
 * @param[in] backend_prefix_ Backend function prefix, such as infin_.
 * @param[in] CtxType_        Type of the compound literal the backend receives, such as InfinCtx.
 * @param[in] CfgType_        Type the emitted entry takes a pointer to, such as InfinCfg.
 * @param[in] name_           Core name, pasted onto both prefixes.
 * @param[in] ...             Initializers for the CtxType_ literal, written in terms of args.
 * @note The same body as GENERIC_ENTRY, without the return. Two macros rather than one because the
 *       return type is not a parameter that can be void here; writing `void` where ReturnType_ goes
 *       would still emit `return backend(...)` on a void call.
 * @warning The initializers dereference args, so it must not be NULL [BORROWS].
 */
#define GENERIC_ENTRY_V(entry_prefix_, backend_prefix_, CtxType_, CfgType_, name_, ...)                                \
    void entry_prefix_##name_(const CfgType_ *args)                                                                    \
    {                                                                                                                  \
        MMGR_CALL(backend_prefix_##name_, CtxType_, __VA_ARGS__);                                                      \
    }

#endif
