/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_praefinitum.h
 * @brief Every knob this module reads, what it takes when the caller did not set it, and the warning
 *        that says so.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note A knob that quietly takes a default is how a build breaks in a way nobody traces back. Three
 *       knobs get set, two do not, the part misbehaves, and the compiler said nothing.
 * @note Nothing here stops the build. Every unset knob takes its default, raises a warning naming
 *       itself and the value it took, and leaves a flag behind. praet_iudex.h reads those
 *       flags at the end and stops once, listing what to go fix.
 * @note That order is the whole point. An #error halts its translation unit where it stands, so a
 *       build missing four knobs reports one, gets fixed, and reports the next - four rounds to learn
 *       four facts that were all known on the first. Warning here and stopping at the end hands the
 *       whole basket over at once.
 * @note Nothing about it needs a build system. It is preprocessor directives, so a bare gcc or clang
 *       on one file behaves the same way as any tree that drives them. A keep-going build reaches
 *       across translation units on top of this, which is a separate half of the same idea and not
 *       something this depends on.
 * @note #warning is a GNU and clang directive and is standard from C23. This library targets those
 *       two compilers and no others, so it is available everywhere this builds.
 * @note Every message is a #warning directive rather than a macro over _Pragma. GCC's warning pragma
 *       keeps only the first string literal after it, so a message written as adjacent literals
 *       arrives cut off at the first one - which still reads as a finished sentence and still passes a
 *       check written against its opening words. That got past a green sweep column here once. A
 *       #warning takes the rest of the line and arrives whole.
 * @warning A directive cannot come out of a macro expansion, so this only works where the report sits
 *          at file scope. Every knob here does. The one report that fires from inside a macro is the
 *          boundary word answer at a declaration, and praet_ordo.h reaches that a different way.
 * @warning No message interpolates a macro. Neither #warning nor the pragma expands one, so a name in
 *          the middle of a message arrives as the name of the macro rather than its value.
 */
#ifndef MMGR_TEST_PRAET_PRAEFINITUM_H
#define MMGR_TEST_PRAET_PRAEFINITUM_H

/**
 * @brief Logical channels one context carries.
 *
 * @note Per part, and the caller is the one who knows which part. There is no number here that is
 *       right for everyone.
 */
#ifndef PRAET_CHANNELS
#define PRAET_CHANNELS 8u
#define PRAET_UNSET_CHANNELS 1
#warning "PRAET_CHANNELS was not set and took the library default of 8. Set it to the channels your part gives one engine."
#endif

/**
 * @brief Microseconds the engine spends settling before it will take a transfer.
 *
 * @note Microseconds, not milliseconds. DMA timing is tight enough that a millisecond is not a unit
 *       anything here can be expressed in.
 * @warning At EMBED_WORD_BITS=16 a microsecond count wraps at 65535, about 65 milliseconds. No
 *          engine takes that long to come up, so this knob is safe at every width.
 */
#ifndef PRAET_SETTLE_MICROS
#define PRAET_SETTLE_MICROS 0u
#define PRAET_UNSET_SETTLE_MICROS 1
#warning "PRAET_SETTLE_MICROS was not set and took the library default of 0, so this build waits for nothing after an attach. Set it to what your engine takes to come up."
#endif

/**
 * @brief Microseconds a running channel may go without the port kicking it before it reads stalled.
 *
 * @note A watchdog window, not a transfer length. The port kicks this whenever the engine shows a
 *       sign of life, and a window that goes unkicked says the channel stopped moving - it never
 *       says the transfer finished. Completion arrives from the port, because how long a transfer
 *       takes is not something this library can know.
 * @note That is also why the width is not a problem. A watchdog window is short on purpose, so the
 *       65 millisecond ceiling a 16-bit word puts on a microsecond count is far above any value
 *       worth setting here, while a transfer's runtime has no bound at all.
 */
#ifndef PRAET_KEEPALIVE_MICROS
#define PRAET_KEEPALIVE_MICROS 1000u
#define PRAET_UNSET_KEEPALIVE_MICROS 1
#warning "PRAET_KEEPALIVE_MICROS was not set and took the library default of 1000. Set it to how long a moving channel may go unkicked on your part before it has stopped."
#endif

/**
 * @brief Whether a stalled transfer can be backed out or scrubbed.
 *
 * @note The capability switch. Off is what a caller gets by not answering, because recovery costs
 *       three arrays per channel and a caller who never said where the bytes were cannot back
 *       anything out of them.
 * @note With it on, submit takes the start and the length, kick reports how far the engine got, and
 *       praet_ordo_resolve exists. With it off none of those members are in the context and none
 *       of those entries are declared, so a call site that wanted them fails to compile instead of
 *       linking against a version that records nothing.
 */
#ifndef PRAET_RECOVERY
#define PRAET_RECOVERY 0
#define PRAET_UNSET_RECOVERY 1
#warning "PRAET_RECOVERY was not set and took the library default of 0, so a stalled transfer cannot be backed out or scrubbed in this build. Set it to 1 to have that machinery, 0 to say you meant to leave it out."
#endif

// A capability switch is on or off, and a third value is somebody reading it as a count or a channel
// number. Every test on it below would take the on arm silently. This one stops where it stands
// rather than joining the basket, because every gate after it would be answering a question about a
// value nobody can read
#if (PRAET_RECOVERY != 0) && (PRAET_RECOVERY != 1)
#error "PRAET_RECOVERY must be 0 or 1. It is a capability switch, not a count."
#endif

// The boundary word check is not a build knob and has no default. It is answered once per context, in
// the declaration, by a token nobody writes by accident. See PraetOrdoContext in
// praet_ordo.h. A -D that sets it here reads as a build-wide switch, which it is not
#ifdef PRAET_RECOVERY_CRC
#error "PRAET_RECOVERY_CRC is not a build knob. The boundary word check is answered per context, in the declaration: PraetOrdoContext(name, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE) or the DISABLE token."
#endif

#endif
