// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_IMPENSA_ANCORAE_ACUS_H
#define MMGR_IMPENSA_ANCORAE_ACUS_H

#include "config/mmgr_config.h"

/**
 * @file impensa_ancorae_acus.h
 * @brief Which byte a search should anchor on, as a cost table.
 *
 * Cost is how common a byte is, so the picker takes the minimum and anchors on the rarest byte the
 * needle has. It is a probability scan. It does not have to be right, only right more often than
 * not.
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
 * One table is compiled in. Selecting a profile does not carry the others.
 */
#if defined(MMGR_ANCORAE_FORMA_ENGLISH)
#include "impensa_ancorae_acus/impensa_ancorae_acus_english.h"
#define MMGR_IMPENSA_ANCORAE_ACUS_INIT MMGR_IMPENSA_ANCORAE_ACUS_ENGLISH
#elif defined(MMGR_ANCORAE_FORMA_URI)
#include "impensa_ancorae_acus/impensa_ancorae_acus_uri.h"
#define MMGR_IMPENSA_ANCORAE_ACUS_INIT MMGR_IMPENSA_ANCORAE_ACUS_URI
#elif defined(MMGR_ANCORAE_FORMA_INET)
#include "impensa_ancorae_acus/impensa_ancorae_acus_inet.h"
#define MMGR_IMPENSA_ANCORAE_ACUS_INIT MMGR_IMPENSA_ANCORAE_ACUS_INET
#elif defined(MMGR_ANCORAE_FORMA_ROUTE)
#include "impensa_ancorae_acus/impensa_ancorae_acus_route.h"
#define MMGR_IMPENSA_ANCORAE_ACUS_INIT MMGR_IMPENSA_ANCORAE_ACUS_ROUTE
#else
#include "impensa_ancorae_acus/impensa_ancorae_acus_generic.h"
#define MMGR_IMPENSA_ANCORAE_ACUS_INIT MMGR_IMPENSA_ANCORAE_ACUS_GENERIC
#endif

MMGR_INCIPE_DECLS

/** @brief Cost per byte value. Lower is rarer, so lower is a better anchor. NUL is pinned worst. */
static const uint8_t mmgr_impensa_ancorae_acus[256] MMGR_UNUSED = {MMGR_IMPENSA_ANCORAE_ACUS_INIT};

MMGR_STATIC_ASSERT(sizeof(mmgr_impensa_ancorae_acus) == 256u, "the anchor cost table must cover every byte");

MMGR_FINIS_DECLS

#endif
