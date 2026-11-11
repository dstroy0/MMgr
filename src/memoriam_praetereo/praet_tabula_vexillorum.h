/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_tabula_vexillorum.h
 * @brief Every bit of the channel flag word, and the assertions that keep the map from drift.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note The map is ours, and for that reason it needs a check. Nothing outside this library assigns
 *       these bits. A status added at the wrong offset or a region moved one byte down is a change
 *       no compiler would question. No test would necessarily reach it.
 * @note A token id names each status and its bit derives from it. A name and its bit cannot
 *       disagree. What the assertions below check is the id set. It is dense, no two ids collide,
 *       and the three fields do not overlap each other.
 * @note The map is the same on every build. A status a build never sets still owns its bit. A flag
 *       word read on one part has to mean what it means on another.
 */
#ifndef MMGR_PRAET_TABULA_VEXILLORUM_H
#define MMGR_PRAET_TABULA_VEXILLORUM_H

/** @addtogroup mod_praet
 *  @{ */

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Detached: the channel holds no engine and no vector.
 */
#define PRAET_SEPARATUS 0u

/**
 * @brief Attached: the channel holds the engine and will take a transfer.
 */
#define PRAET_ADNEXUS 1u

/**
 * @brief Busy: the engine moves bytes on this channel.
 *
 * @note The DMA busy, not the reader/setter's lock. The two are separate concerns and the lock lives
 *       in its own volatile.
 */
#define PRAET_BUSY 2u

/**
 * @brief Ok: the last transfer finished and the channel is still attached.
 *
 * @note The transfer's result, not the channel's health. A channel that failed carries PRAET_ERROR
 *       alongside, and so the two are separate bits.
 */
#define PRAET_OK 3u

/**
 * @brief Bits the quaternary core occupies, at the bottom of the word.
 */
#define PRAET_CORE_BITS 2u

/**
 * @brief The two bits the quaternary core occupies.
 *
 * @note The core sits at the bottom with every status above it. So a read of the state is a mask and
 *       a read of a status is a test. Nothing shifts.
 */
#define PRAET_CORE_MASK ((uint32_t)((1u << PRAET_CORE_BITS) - 1u))

/**
 * @brief Token id of the status set while the channel holds an interrupt vector.
 */
#define PRAET_ID_CLAIMED 0u

/**
 * @brief Token id of the status set while the engine settles.
 */
#define PRAET_ID_SETTLING 1u

/**
 * @brief Token id of the status set while a detach is in progress.
 */
#define PRAET_ID_SEPARANS 2u

/**
 * @brief Token id of the status set where the last transfer ended in an error.
 */
#define PRAET_ID_ERROR 3u

/**
 * @brief Token id of the status set where an active channel went unkicked for a whole window.
 */
#define PRAET_ID_STALLED 4u

/**
 * @brief Token id of the status set where a stalled transfer went back.
 */
#define PRAET_ID_ABANDONED 5u

/**
 * @brief Token id of the status set where a stalled transfer's bytes counted as unsafe.
 */
#define PRAET_ID_SCRUBBED 6u

/**
 * @brief Token id of the status set where a recovery measured the boundary word.
 */
#define PRAET_ID_MEASURED 7u

/**
 * @brief How many status token ids there are.
 *
 * @note Raise this with the addition of a status, and the assertion on PRAET_EVERY_STATUS fails until the
 *       new one appears there too.
 */
#define PRAET_STATUS_COUNT 8u

/**
 * @brief First bit the statuses occupy, the bit above the core.
 */
#define PRAET_STATUS_SHIFT PRAET_CORE_BITS

/**
 * @brief The bit a status token id owns.
 *
 * @param[in] id_ Status token id, below PRAET_STATUS_COUNT.
 * @return        That id's bit, placed above the core.
 * @note Every status below comes through this, and so no status carries a hand-written bit that
 *       could disagree with the id beside it.
 */
#define PRAET_STATUS_BIT(id_) ((uint32_t)1u << (PRAET_STATUS_SHIFT + (id_)))

/**
 * @brief Every bit the statuses occupy, derived from how many there are.
 */
#define PRAET_STATUS_MASK ((uint32_t)(((1u << PRAET_STATUS_COUNT) - 1u) << PRAET_STATUS_SHIFT))

/**
 * @brief Set while the channel holds an interrupt vector.
 *
 * @note Claimed at attach and released at detach. There is no reason to hold a vector for a memory
 *       mover that moves nothing, and the claim gives the context one owner.
 */
#define PRAET_CLAIMED PRAET_STATUS_BIT(PRAET_ID_CLAIMED)

/**
 * @brief Set while the engine settles and will take no transfer.
 */
#define PRAET_SETTLING PRAET_STATUS_BIT(PRAET_ID_SETTLING)

/**
 * @brief Set where a detach is in progress and the channel did not finish it.
 */
#define PRAET_SEPARANS PRAET_STATUS_BIT(PRAET_ID_SEPARANS)

/**
 * @brief Set where the last transfer ended in an error, clear where it completed.
 */
#define PRAET_ERROR PRAET_STATUS_BIT(PRAET_ID_ERROR)

/**
 * @brief Set where an active channel went a whole keepalive window with no kick.
 *
 * @note Says the engine stopped, and says nothing about whether the transfer finished. A
 *       stalled channel is still busy, because nothing told this library otherwise.
 */
#define PRAET_STALLED PRAET_STATUS_BIT(PRAET_ID_STALLED)

/**
 * @brief Set where a stalled transfer went back and its bytes stayed as they were.
 *
 * @note Owns its bit on every build. A build with recovery off never sets it, and the bit stays
 *       reserved so a flag word means one thing everywhere.
 */
#define PRAET_ABANDONED PRAET_STATUS_BIT(PRAET_ID_ABANDONED)

/**
 * @brief Set where a stalled transfer resolved with its bytes counted as unsafe.
 *
 * @note The bytes are the caller's and this context has no pointer to them, and so nothing here scrubs
 *       anything. The flag records the decision; the caller zeroes its own storage.
 */
#define PRAET_SCRUBBED PRAET_STATUS_BIT(PRAET_ID_SCRUBBED)

/**
 * @brief Set where a recovery measured the word the engine was in the middle of.
 *
 * @note What tells a reader that the boundary checksum means anything. A checksum of zero is a legal
 *       one. A reader that took the value alone could not tell a measured word from an unmeasured
 *       context.
 * @note A context whose declaration chose the DISABLE token never sets this. Neither does a
 *       recovery whose sample already landed on a word boundary, since there is no partial word to
 *       measure.
 */
#define PRAET_MEASURED PRAET_STATUS_BIT(PRAET_ID_MEASURED)

/**
 * @brief Every status, listed by name.
 *
 * @note Written out by hand, because this is the half of the check that has to come from
 *       somewhere other than PRAET_STATUS_COUNT. A compare of a list of names against a count neither
 *       one produced is what catches a duplicated id or a status nobody placed.
 */
#define PRAET_EVERY_STATUS                                                                                             \
    (PRAET_CLAIMED | PRAET_SETTLING | PRAET_SEPARANS | PRAET_ERROR | PRAET_STALLED | PRAET_ABANDONED |                 \
     PRAET_SCRUBBED | PRAET_MEASURED)

/**
 * @brief First bit of the region descriptor.
 *
 * @note A byte on a byte boundary. The descriptor is a number derived from the address space behind
 *       the region, and so nothing here stores an address. Its place at a byte edge lets a build
 *       that wants to compress it to eight bits do so and move no status.
 * @note The third byte and not the second. Statuses grow upward from the core and the region has to
 *       start above all of them. At bit 8 it would share a bit with PRAET_SCRUBBED, which would read
 *       as a region whose low bit flips when a recovery marks a transfer scrubbed.
 */
#define PRAET_REGION_SHIFT 16u

/**
 * @brief How wide the region descriptor is.
 */
#define PRAET_REGION_BITS 8u

/**
 * @brief The largest descriptor the region field holds.
 */
#define PRAET_REGION_MAX ((uint32_t)((1u << PRAET_REGION_BITS) - 1u))

/**
 * @brief The bits the region descriptor occupies once shifted into place.
 */
#define PRAET_REGION_MASK (PRAET_REGION_MAX << PRAET_REGION_SHIFT)

/**
 * @brief A region descriptor placed in the field the map gives it.
 *
 * @param[in] region_ Region descriptor, narrowed to what the field holds.
 * @return            region_ in position, and never a bit outside PRAET_REGION_MASK.
 * @note The one way to write the field. A narrow cast at the write site instead would put the field's
 *       width in two places. The copy that drifts is the single one nothing checks.
 */
#define PRAET_REGION_FIELD(region_) (((uint32_t)(region_) & PRAET_REGION_MAX) << PRAET_REGION_SHIFT)

/**
 * @brief The regions a channel can attach over.
 *
 * @note Token ids, the same way the statuses above are token ids. A region arrives at an attach as one
 *       of these names. A mistyped one is an undeclared identifier that carries the name as
 *       written, and a caller cannot pass a byte that means nothing.
 * @note Two of them, because ParsMemoriaeInternae and ParsMemoriaeExternum are the two declarations a
 *       pool can have. A part with more address spaces than that adds ids here and the
 *       assertion below keeps them inside the field.
 * @note The values are ordinals. A build that wants a descriptor it can infer an address space from
 *       replaces the numbers and changes nothing else. The field is a byte and every read of it
 *       goes through PRAET_REGION_FIELD.
 */
typedef enum
{
    PRAET_REGION_INTERNAL = 0,
    PRAET_REGION_EXTERNAL = 1
} PraetRegion;

/**
 * @brief The largest region token id there is.
 *
 * @note Raise this with the addition of a region, and the assertion below fails until the field is wide
 *       enough to hold it.
 */
#define PRAET_REGION_ID_HIGHEST PRAET_REGION_EXTERNAL

/**
 * @brief Every bit the map accounts for.
 *
 * @note What a flag word may carry. A build whose writes stay inside this cannot produce a word with
 *       a bit nobody named. So nothing has to look at a word at run time to find that out.
 */
#define PRAET_MAP_MASK (PRAET_CORE_MASK | PRAET_STATUS_MASK | PRAET_REGION_MASK)

// Every value the core mask can hold has a name, and the four names are distinct. One bit per
// core value turns both halves of that into one comparison
EMBED_STATIC_ASSERT(((1u << PRAET_SEPARATUS) | (1u << PRAET_ADNEXUS) | (1u << PRAET_BUSY) | (1u << PRAET_OK)) ==
                        ((1u << (PRAET_CORE_MASK + 1u)) - 1u),
                    "the four core states are not exactly the values PRAET_CORE_MASK holds");

// The id set is dense, nothing collides, and nothing is absent. A duplicated id drops a bit out of
// the list. An id at or above the count puts one outside the mask, and a status left out of
// PRAET_EVERY_STATUS leaves a hole. All three come out as this one inequality
EMBED_STATIC_ASSERT(PRAET_EVERY_STATUS == PRAET_STATUS_MASK,
                    "the status token ids are not dense and distinct, or PRAET_EVERY_STATUS is missing one");

EMBED_STATIC_ASSERT((PRAET_CORE_MASK & PRAET_STATUS_MASK) == 0u, "a status sits on top of the quaternary core");

EMBED_STATIC_ASSERT((PRAET_STATUS_MASK & PRAET_REGION_MASK) == 0u, "a status sits on top of the region descriptor");

EMBED_STATIC_ASSERT((PRAET_CORE_MASK & PRAET_REGION_MASK) == 0u,
                    "the region descriptor sits on top of the quaternary core");

EMBED_STATIC_ASSERT(PRAET_REGION_SHIFT >= (PRAET_STATUS_SHIFT + PRAET_STATUS_COUNT),
                    "the region descriptor starts below the highest status");

// A descriptor a build wants to read or write as a byte has to be addressable as one
EMBED_STATIC_ASSERT((PRAET_REGION_SHIFT % 8u) == 0u, "the region descriptor is not on a byte boundary");

EMBED_STATIC_ASSERT((PRAET_REGION_SHIFT + PRAET_REGION_BITS) <= 32u,
                    "the map runs off the top of the 32 bit flag word");

// The write reaches the whole field and never a bit past it, whatever the caller hands over. Both
// halves matter: a field that cannot fill loses descriptors, and one that overruns lands in a
// status
EMBED_STATIC_ASSERT(PRAET_REGION_FIELD(PRAET_REGION_MAX) == PRAET_REGION_MASK,
                    "PRAET_REGION_FIELD cannot fill the field the map gives it");

EMBED_STATIC_ASSERT((PRAET_REGION_FIELD(0xFFFFFFFFu) & (uint32_t)~PRAET_REGION_MASK) == 0u,
                    "PRAET_REGION_FIELD writes outside the region descriptor");

// Nothing a flag word carries sits outside the map, and the map has nothing in it that no field
// claims. A word is one of these three things end to end
EMBED_STATIC_ASSERT((PRAET_CORE_MASK | PRAET_EVERY_STATUS | PRAET_REGION_MASK) == PRAET_MAP_MASK,
                    "the map is not exactly the core, the statuses and the region");

// A region token id that does not fit the field would land truncated, and the channel would
// report a region nobody asked for
EMBED_STATIC_ASSERT((uint32_t)PRAET_REGION_ID_HIGHEST <= PRAET_REGION_MAX,
                    "there are more region token ids than the region descriptor field can hold");

EMBED_END_DECLS

/** @} */

#endif
