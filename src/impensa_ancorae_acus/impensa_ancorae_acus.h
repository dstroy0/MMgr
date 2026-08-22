// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_IMPENSA_ANCORAE_ACUS_H
#define MMGR_IMPENSA_ANCORAE_ACUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file impensa_ancorae_acus.h
 * @brief Which byte a search should anchor on, as a cost.
 *
 * Cost is how common a byte is, so the picker takes the minimum and anchors on the rarest byte the
 * needle has. It is a probability scan. It does not have to be right, only right more often than
 * not.
 *
 * The table itself does not cross this header. One profile's .c holds it, file local, and what
 * comes out is the cost of a byte. A table in a header is a copy of itself in every translation
 * unit that reads it and a name every one of them has to not collide with; a byte in and a byte
 * out is neither.
 *
 * The default is generic and assumes nothing beyond "this is byte data and printable ASCII is what
 * strings are made of". A build that knows what it is searching picks a profile and gets a much
 * sharper first row, because a byte that cannot occur under that grammar scores best:
 *
 *     -DMMGR_ANCORAE_FORMA_INET      IPv6 or dotted quad. The alphabet is 0-9 a-f A-F : . / % [ ],
 *                                     so any needle byte outside it - q, z, x, any letter past f -
 *                                     is a perfect anchor.
 *     -DMMGR_ANCORAE_FORMA_URI       RFC 3986. '/' is the worst anchor there is and ' ' is the
 *                                     best, which is the reverse of prose.
 *     -DMMGR_ANCORAE_FORMA_ROUTE     HTTP route patterns, where '/' is over half the structure.
 *     -DMMGR_ANCORAE_FORMA_ENGLISH   prose. Sharper than generic on text and worse on anything
 *                                     else, which is the trade a profile is.
 *
 * Getting it wrong costs accuracy, never correctness. The anchor only decides which byte the first
 * row filters on. Every candidate it passes is still verified in full, so a profile that mismatches
 * the data lets more candidates through and the search gets slower. That is the whole penalty.
 *
 * One profile is compiled in. Selecting one does not carry the others - the build compiles that
 * .c and no other, so the four it did not pick are not in the image at all.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
/**
 * @brief What a cost lookup is given.
 *
 * Public, and in the header, because the caller is what builds it. The member is const:
 * nothing writes to a config once the caller has built it.
 *
 * Not the module's context. AncoraeCtx is what the body works with and it is file local.
 */
typedef struct
{
    const uint8_t b; /**< The byte being costed. */
} AncoraeCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    uint8_t (*impensa)(const AncoraeCfg *c);
} ImpensaAncoraeAcusNs;
MMGR_NS_LAYOUT(ImpensaAncoraeAcusNs, impensa);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
uint8_t mmgr_ancorae_impensa(const AncoraeCfg *c);
/** @} */

/**
 * @brief The byte a cost is asked for, and nothing that merely converts to one.
 *
 * uint8_t, not char. Whether char is signed is the implementation's to decide, so a byte at or
 * above 0x80 arrives negative on one target and positive on the next.
 */
#define MMGR_ANCORAE_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

/**
 * @brief What @p b_ costs as an anchor.
 *
 * Positional in, so the struct and the designator never reach a call site.
 */
#define mmgr_ancorae_impensa(b_) (MMGR_ANCORAE_IS_BYTE(b_), ancorae.impensa(&(AncoraeCfg){.b = (b_)}))

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS ImpensaAncoraeAcusNs ancorae MMGR_UNUSED = {
    .impensa = mmgr_ancorae_impensa,
};

MMGR_FINIS_DECLS

#endif
