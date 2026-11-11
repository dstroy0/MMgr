/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr.h
 * @brief The embedded_types headers every module builds on.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Every module header includes this one. The widths, the word, the static assertion, the
 *       attribute wrappers, the dispatch layout assertions and the entry point macros all come from
 *       embedded_types. Nothing here declares an MMgr copy of any of them.
 * @note Only the two gated module headers below come in from here. A module includes what it
 *       needs itself, and an umbrella could not also be the header each module includes.
 * @note mmgr_string_shim.h is separate. Its include changes what the <string.h> names mean, which
 *       is a decision a consumer makes for itself.
 */
#ifndef MMGR_H
#define MMGR_H

#include "embed_compiler_directives.h"
#include "embed_dispatch_layout.h"
#include "embed_types.h"

/**
 * @brief The DMA module, reached only where MMGR_ENABLE_DMA holds.
 *
 * @note memoriam_praetereo.h guards its own contents on MMGR_ENABLE_DMA as well.
 */
#if MMGR_ENABLE_DMA
#include "memoriam_praetereo/memoriam_praetereo.h"
#endif

/**
 * @brief The external memory module, reached only where MMGR_ENABLE_EXTRAM holds.
 *
 * @note memoria_externa.h guards its own contents on MMGR_ENABLE_EXTRAM as well.
 */
#if MMGR_ENABLE_EXTRAM
#include "memoria_externa/memoria_externa.h"
#endif

/**
 * @brief Alignment a caller declares its own storage at before it hands those bytes to a module.
 *
 * @note Nothing under src reads it. The tests, the benches and the region-edges example are what
 *       write EMBED_ALIGN(MMGR_ALIGN_BYTES) on their arrays. The default is 16.
 * @note Has to be a power of two, which the build checks below. It reaches an alignment specifier,
 *       and one given anything else is not legal C. The check is here to name the knob,
 *       where the diagnostic would otherwise point at whichever array carries it.
 * @warning Not the alignment a locus_carcerum cell comes back at. That is MMGR_CARCER_ALIGN, which is
 *          sizeof(embed_word), and nothing asserts the two agree.
 */
#ifndef MMGR_ALIGN_BYTES

#define MMGR_ALIGN_BYTES 16u
#endif
#if (MMGR_ALIGN_BYTES < 1) || ((MMGR_ALIGN_BYTES & (MMGR_ALIGN_BYTES - 1)) != 0)
#error "MMGR_ALIGN_BYTES must be a power of two - an alignment specifier takes nothing else"
#endif

/**
 * @brief Attribute that places a pool in external memory.
 *
 * @note Named here and filled by the port. Which section a part puts external memory in is the
 *       part's business, and so this library names the knob and supplies no value for it. A build
 *       defines it ahead of this header, or on the command line.
 * @note Read only by ParsMemoriaeExternum, which exists only where MMGR_ENABLE_EXTRAM holds.
 * @warning Empty by default, and an external pool then lands wherever the linker puts an ordinary
 *          one. Nothing diagnoses that. A build with external memory on has to supply this.
 */
#ifndef MMGR_EXTRAM_ATTR
#define MMGR_EXTRAM_ATTR
#endif

/**
 * @brief Marks a declaration so that a reference to it fails the build, with a message.
 *
 * @param[in] msg_ Text the diagnostic reports, as a string literal.
 * @note Reported at the site that referenced the declaration, and so the message names what a caller did.
 *       The linker can only say what it found. This is the reason to reach for it over a token: the
 *       linker names a symbol, this names the mistake.
 * @warning The diagnostic arrives only where a reference survives to the end of compilation, and so what
 *          this marks has to be something nothing legitimately calls.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(error) is 0. A reference then compiles and
 *          the misuse goes unreported.
 */
#if EMBED_HAS_ATTRIBUTE(error)
#define MMGR_ERROR_ATTR(msg_) __attribute__((error(msg_)))
#else
#define MMGR_ERROR_ATTR(msg_)
#endif

/**
 * @brief States which argument of an allocation entry gives the extent of what it returns.
 *
 * @param[in] arg_ One-based position of the byte count in the entry's parameter list.
 * @note Feeds the compiler's object-size analysis, which can then report a write past the cell a
 *       caller received at the line that wrote it. Without it the returned pointer carries the
 *       extent of the pool it came out of, and a cell overrun reads as a legal access.
 * @note Costs nothing. Measured byte-identical with and without, both for a direct call and for one
 *       through a dispatch table.
 * @warning The bound survives only where the compiler does not fold the call away. An entry reached
 *          through its dispatch table keeps it. The same entry inlined at a direct call collapses to an
 *          offset into the pool, and the cell-level extent goes.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(alloc_size) is 0, and arg_ plays no part. The
 *          diagnostics it would raise do not appear, and nothing reports that.
 */
#if EMBED_HAS_ATTRIBUTE(alloc_size)
#define MMGR_ALLOC_SIZE(arg_) __attribute__((alloc_size(arg_)))
#else
#define MMGR_ALLOC_SIZE(arg_)
#endif

/**
 * @brief Fails the link when a second translation unit declares a pool name.
 *
 * @param[in] name_ Pool under declaration.
 * @note An initialized object with external linkage. So two units with one pool name give the
 *       linker two strong definitions of this symbol, and it refuses them by name. The enumerator
 *       guard cannot see across a unit boundary; this is what does.
 * @note The pool's own array stays static, and so it is this token and not the bytes that collide. That
 *       keeps the storage private to its unit while the name stays answerable across all of them.
 * @warning Costs one object per pool, in .bss or .data as the toolchain decides. Fixed and small,
 *          but not nothing on a part where pools are the point.
 */
#define MMGR_PARS_TOKEN(name_) int mmgr_pars_token_##name_ = 0

/**
 * @brief Fails the build when a pool name appears in a second declaration.
 *
 * @param[in] name_ Pool under declaration.
 * @note The enumerator is the guard. A second declaration of one name collides on it, and the
 *       compiler prints the identifier. So the identifier reads as the reason and names no
 *       mechanism.
 * @note Two pools of one name would sit at unique addresses and the language would take them. The
 *       name is all a consumer gets. A name that means internal memory in one place and external in
 *       another leaves the placement out of reach of the line that uses it. Those two differ in what
 *       a DMA engine can address and in settle time.
 * @warning One translation unit is as far as this guard reaches. The enumerator settles while a
 *          unit compiles. A pool of the same name declared in a separate unit does not collide
 *          on it. To catch that one is the linker's to do, over a symbol it can see.
 */
#define MMGR_PARS_DECLARED_ONCE(name_)                                                                                 \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_declared_twice_but_a_pool_symbol_names_one_region_only = 0                                             \
    }

/**
 * @brief Fails the build when a second dresser claims a pool.
 *
 * @param[in] name_ Pool under claim.
 * @note Emitted by whatever lays state over a pool: a cellblock, a ring, anything that writes its
 *       own records into those bytes. It sits here and in none of them, because the rule is the
 *       pool's and not a dresser's. A copy in each would let the next dresser come with none.
 * @note Keyed on the pool alone, with no dresser's name in it. Every other symbol a site emits
 *       carries the site's name, and so two sites over one pool collide only there. Each would
 *       lay its own records over the same bytes in the belief that it owned them.
 * @note Costs nothing. An enumerator emits no storage and settles while the unit compiles.
 * @warning Says one dresser claims a pool. It says nothing about a direct reach for the bytes,
 *          which is a separate question with separate guards.
 */
#define MMGR_PARS_CLAIMED_ONCE(name_)                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_claimed_twice_but_a_pool_is_dressed_once = 0                                                           \
    }

/**
 * @brief Declares a pool of bytes in internal memory.
 *
 * @param[in] name_  Name of the pool, which is also the array.
 * @param[in] bytes_ Bytes in it.
 * @note A pool is a block of bytes, alone. Hand it to LocusCarcerum for a cellblock, to
 *       mmgr_anular_init for a ring, or to a memor entry to work on it as the bytes it is. A pool
 *       nobody hands anywhere is a pool that exists with no claim on it.
 * @note The count comes as name_##_bytes as well as the array. A consumer has the extent with no
 *       derivation. The assertion is what compares the two, and it has content only because the two
 *       come from separate places.
 * @note Aligned to MMGR_ALIGN_BYTES, and that leaves that knob the contract the header states.
 *       No caller writes it out.
 */
#define ParsMemoriaeInternae(name_, bytes_)                                                                            \
    MMGR_PARS_DECLARED_ONCE(name_);                                                                                    \
    MMGR_PARS_TOKEN(name_);                                                                                            \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_bytes = (bytes_)                                                                                       \
    };                                                                                                                 \
    EMBED_ALIGN(MMGR_ALIGN_BYTES) static uint8_t mmgr_pars_storage_##name_[bytes_];                                    \
    static const uint8_t *const name_ EMBED_UNUSED = mmgr_pars_storage_##name_;                                        \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) == name_##_bytes, #name_ " is not the size it was declared")

/**
 * @brief Declares a pool whose bytes the system gives at run time: its name, with no storage and no size.
 *
 * @param[in] name_ Name of the pool.
 * @note The program asks the system for the bytes once, at the size it finds it needs, and hands them to the
 *       dresser. MemoriaAnularis over this pool gives a ring whose address the linker settles, whose state
 *       names the bytes wherever the system put them.
 * @note For bytes that must come from the system. A graphics device that places a buffer over them
 *       opens the allocation they are, which an array in the program's image is not.
 */
#define ParsMemoriaeHospes(name_)                                                                                      \
    MMGR_PARS_DECLARED_ONCE(name_);                                                                                    \
    MMGR_PARS_TOKEN(name_)

/**
 * @brief Declares a pool of bytes in external memory.
 *
 * @param[in] name_  Name of the pool, which is also the array.
 * @param[in] bytes_ Bytes in it.
 * @note The same declaration as ParsMemoriaeInternae, with MMGR_EXTRAM_ATTR. Which of the two a caller
 *       writes settles which memory a pool sits in, and nothing afterwards inspects an address to
 *       find out.
 * @warning Exists only where MMGR_ENABLE_EXTRAM holds. A build without external memory fails
 *          on the name, where it would otherwise quietly place the bytes internally.
 */
#if MMGR_ENABLE_EXTRAM
#define ParsMemoriaeExternum(name_, bytes_)                                                                            \
    MMGR_PARS_DECLARED_ONCE(name_);                                                                                    \
    MMGR_PARS_TOKEN(name_);                                                                                            \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_bytes = (bytes_)                                                                                       \
    };                                                                                                                 \
    EMBED_ALIGN(MMGR_ALIGN_BYTES) static uint8_t mmgr_pars_storage_##name_[bytes_] MMGR_EXTRAM_ATTR;                   \
    static const uint8_t *const name_ EMBED_UNUSED = mmgr_pars_storage_##name_;                                        \
    EMBED_STATIC_ASSERT(sizeof(mmgr_pars_storage_##name_) == name_##_bytes, #name_ " is not the size it was declared")
#endif

/**
 * @brief Runtime assertion hook, inert here.
 *
 * @param[in] cond_ Condition the caller expects to hold.
 * @param[in] msg_  String literal that describes the expectation.
 * @note An expectation the library asserts is one a correct caller cannot break, and so the form here
 *       pays nothing for it. It expands to a sizeof, which type checks cond_ and never evaluates it.
 * @note A build that wants a failed expectation to report and stop defines its own ahead of this
 *       header, and the guard below stands down. The file test/support/mmgr_host_traps.h does this
 *       for the checks environment. It is where the stdio and stdlib that a report needs live.
 * @warning Not a runtime check, and nobody can read it as one. A failed expectation produces no
 *          diagnostic and no trap in this form.
 * @warning cond_ must carry no side effect, since this form never evaluates it.
 */
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond_, msg_) ((void)sizeof((cond_) ? 1 : 0), (void)0)
#endif

/**
 * @brief Stops the program on an illegal call, in every build.
 *
 * @param[in] msg_ String literal that names the broken rule.
 * @note Separate from MMGR_ASSERT because the two answer different questions. MMGR_ASSERT states an
 *       expectation a correct caller cannot break, and it is inert in the build that ships. This one
 *       marks a call that is illegal to make, and so it has to hold in that build too.
 * @note Reached where to go on would be worse than to stop. A release handed a prisoner from
 *       another cellblock is the case it exists for. The caller showed it does not know which
 *       cellblock owns that memory, and so its next release and its next allocation are both suspect.
 *       On a maximum security cellblock a quiet return would leave bytes unzeroed that the caller
 *       believes are clear.
 * @note Does not return. Every call site rests on that basis and carries no fallback after it.
 * @note The stop is unconditional and the diagnostic is not. The form here halts and names nothing,
 *       as keeps this header free of libc.
 * @warning A build may define its own before it includes this header, and the form below then stands
 *          down. A target that reports a fault through its own handler wants that. So does a test
 *          harness, which would otherwise hang on this form and never report a failure.
 * @warning This form spins. A target without abort has no other way to stop, and a halt is what a
 *          debugger can catch.
 */
#ifndef MMGR_FATAL
// No libc here, and so the trap is a halt. The cast to void discards msg_, which nothing reads on this
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
