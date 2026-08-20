// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ANCHOR_COST_H
#define MMGR_ANCHOR_COST_H

#include "mmgr_config.h"

/**
 * @file anchor_cost.h
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
 *     -DMMGR_ANCHOR_PROFILE_INET      IPv6 or dotted quad. The alphabet is 0-9 a-f A-F : . / % [ ],
 *                                     so any needle byte outside it - q, z, x, any letter past f -
 *                                     is a perfect anchor.
 *     -DMMGR_ANCHOR_PROFILE_URI       RFC 3986. '/' is the worst anchor there is and ' ' is the
 *                                     best, which is the reverse of prose.
 *     -DMMGR_ANCHOR_PROFILE_ROUTE     HTTP route patterns, where '/' is over half the structure.
 *     -DMMGR_ANCHOR_PROFILE_ENGLISH   prose. Sharper than generic on text and worse on anything
 *                                     else, which is the trade a profile is.
 *
 * Getting it wrong costs accuracy, never correctness. The anchor only decides which byte the first
 * row filters on. Every candidate it passes is still verified in full, so a profile that mismatches
 * the data lets more candidates through and the search gets slower. That is the whole penalty.
 *
 * One table is compiled in. Selecting a profile does not carry the others.
 */
#if defined(MMGR_ANCHOR_PROFILE_ENGLISH)
#include "anchor_cost/anchor_cost_english.h"
#define MMGR_ANCHOR_COST_INIT MMGR_ANCHOR_COST_ENGLISH
#elif defined(MMGR_ANCHOR_PROFILE_URI)
#include "anchor_cost/anchor_cost_uri.h"
#define MMGR_ANCHOR_COST_INIT MMGR_ANCHOR_COST_URI
#elif defined(MMGR_ANCHOR_PROFILE_INET)
#include "anchor_cost/anchor_cost_inet.h"
#define MMGR_ANCHOR_COST_INIT MMGR_ANCHOR_COST_INET
#elif defined(MMGR_ANCHOR_PROFILE_ROUTE)
#include "anchor_cost/anchor_cost_route.h"
#define MMGR_ANCHOR_COST_INIT MMGR_ANCHOR_COST_ROUTE
#else
#include "anchor_cost/anchor_cost_generic.h"
#define MMGR_ANCHOR_COST_INIT MMGR_ANCHOR_COST_GENERIC
#endif

MMGR_BEGIN_DECLS

/** @brief Cost per byte value. Lower is rarer, so lower is a better anchor. NUL is pinned worst. */
static const uint8_t mmgr_anchor_cost[256] MMGR_UNUSED = {MMGR_ANCHOR_COST_INIT};

MMGR_STATIC_ASSERT(sizeof(mmgr_anchor_cost) == 256u, "the anchor cost table must cover every byte");

MMGR_END_DECLS

#endif
