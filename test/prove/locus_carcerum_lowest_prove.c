/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_lowest_prove.c
 * @brief Lowest.lean proved on the C. carcer_lowest gives the place of the lowest bit of every word. A claim on a
 *        word with its lowest free bit at any place names the entry at that place and sets that bit.
 *
 * @note The translation unit comes in whole, as the thread clients take it, and so the harness reaches the two
 *       static functions. The option has one word of claimed and one entry for each bit of the word.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 */
#include "locus_carcerum/locus_carcerum.h"

#include "prove.h"

#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define PROVE_OWNERS(owner_) owner_(s_claimer, "claimer")
MMGR_OWNERS(PROVE_OWNERS)
PROVE_NO_MISNAMED()

/** @brief The entries of the option's table, one for each bit of the word of claimed. */
#define PROVE_CELLS MMGR_MOVABLE_CLAIM_BITS

/** @brief The payload offset a claim stores in the entry it names. */
#define PROVE_PAYLOAD ((size_t)64u)

/** @brief The option's table. */
static CarcerMovableEntry s_entries[PROVE_CELLS];

/** @brief The option's one word of claimed. */
static embed_atomic_usize s_claimed;

/** @brief The option: the table and the word, and every other member zero. */
static CarcerMovable s_option = {.entry = s_entries, .cells = PROVE_CELLS, .claimed = &s_claimed, .claimed_words = 1u};

/**
 * @brief Lowest.lean's lowest_single and isolated_place on carcer_entry_claimed. The word has each bit below a place
 *        on, the bit at the place off, and any bits above. A claim on that word names the entry at the place.
 */
static void prove_claimed(void)
{
    const size_t place = nondet_size();
    // Explicit cast takes the input to the width of the word of claimed
    const embed_atomic_usize_value above = (embed_atomic_usize_value)nondet_size();

    PROVE_ASSUME(place < PROVE_CELLS);
    // Explicit cast widens the one bit to the word before the shift
    const embed_atomic_usize_value bit = (embed_atomic_usize_value)1u << place;
    const embed_atomic_usize_value below = bit - 1u;
    const embed_atomic_usize_value held = below | (above & ~(below | bit));

    embed_atomic_usize_store(&s_claimed, held);
    const CarcerMovableEntry *const entry = carcer_entry_claimed(&s_option, PROVE_PAYLOAD, carcer_all(1));

    PROVE_ASSERT(entry == &s_entries[place], "a claim names the entry at the place of the lowest free bit");
    PROVE_ASSERT(embed_atomic_usize_load(&s_claimed) == (held | bit), "a claim sets the bit of the entry it names");
    PROVE_ASSERT(embed_atomic_usize_load(&s_entries[place].word) == PROVE_PAYLOAD, "the entry named holds the payload");
}

/** @brief Lowest.lean's lowest_lt, lowest_zero, lowest_on and lowest_least on carcer_lowest, for every word. */
static void prove_lowest_placed(void)
{
    // Explicit cast takes the input to the width of a claim word
    const embed_atomic_usize_value bits = (embed_atomic_usize_value)nondet_size();
    const size_t place = carcer_lowest(bits);
    const size_t none = carcer_all(bits == 0u);

    PROVE_ASSERT(place < MMGR_MOVABLE_CLAIM_BITS, "the place lies in the word");
    PROVE_ASSUME(place < MMGR_MOVABLE_CLAIM_BITS);
    PROVE_ASSERT((none == 0u) || (place == 0u), "a word of none reads 0");
    PROVE_ASSERT((none != 0u) || (((bits >> place) & 1u) == 1u), "the bit at the place is on");
    // Explicit cast widens the one bit to the word before the shift
    PROVE_ASSERT((bits & (((embed_atomic_usize_value)1u << place) - 1u)) == 0u, "no bit below the place is on");
}

int main(void)
{
    prove_claimed();
    prove_lowest_placed();
    PROVE_DONE();
    return 0;
}
