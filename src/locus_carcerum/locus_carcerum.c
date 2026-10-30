/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum.c
 * @brief The cellblock's two tiers over the caller's bytes: a take is one swap on the tiers word, a
 *        give is one and on the cell's word, and nothing walks.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note A cellblock is a run of bytes with a word ahead of each cell and a word behind it. The
 *       persistent tier allocates upward from base and the temporary tier allocates downward from
 *       size, and the gap between them is what either can still take.
 * @note A take reads the tiers word, finds the room between the two boundaries, and moves one of them
 *       by one swap on the word read; a swap that fails is a word another context moved first, and the
 *       take reads it again and decides over what it says now. A give clears the busy bit of the
 *       cell's word by one and, and the word the and hands back is the cell's size. No context waits
 *       for another, and no context steps through another context's cells.
 * @note A cell given back below a tier's boundary stays in the tier, free, until the cells between it
 *       and the boundary are given back too. The give that frees the cell at the boundary trims it
 *       off by one swap on the tiers word, then reads the cell below through the word behind it, and
 *       trims that one the same way while it reads free. Every cell trimmed off is marked dead. No
 *       byte outside the tiers reads as a free cell.
 * @note A take lays its words after the swap that gave it the bytes, by a swap from the word it reads
 *       there once they are its own. A header another context has laid since is refused and never
 *       written over.
 */
#include "locus_carcerum/locus_carcerum.h"

/** @brief The state of a cell no context holds. */
#define CARCER_FREE 0u

/** @brief The state of a cell a context holds. */
#define CARCER_HELD 1u

/** @brief The state of a cell trimmed off its tier, whose bytes are the gap's. */
#define CARCER_DEAD 2u

/** @brief The bits of a cell's word that carry its state. */
#define CARCER_STATE ((embed_atomic_usize_value)3u)

/**
 * @brief Bytes one of a cell's two words occupies, rounded up so the payload between them stays
 *        aligned.
 *
 * @note The rounding is a mask, and a mask rounds only because MMGR_CARCER_ALIGN is a power of two,
 *       which locus_carcerum.h asserts.
 */
#define CARCER_WORD ((sizeof(embed_atomic_usize) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief Bytes a cell costs beyond its payload: the word ahead of it and the word behind it.
 *
 * @note Charged on top of the payload every time a cell is allocated. An allocation of size bytes
 *       costs the gap this much more than size.
 */
#define CARCER_HDR (2u * CARCER_WORD)

/**
 * @brief Rounds want up to a whole machine word.
 *
 * @param[in] want Count to round.
 * @return         want rounded up to a multiple of MMGR_CARCER_ALIGN, and want itself when it
 *                 already is one.
 * @note The mask is what rounds, which holds only because MMGR_CARCER_ALIGN is a power of two.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask. A want within a word of SIZE_MAX wraps
 *          to 0. Every allocation rounds its request through here, and a request that wraps is met
 *          with a small cell in place of a refusal.
 */
EMBED_INLINE size_t carcer_round(size_t want)
{
    return (want + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u);
}

/**
 * @brief Zeroes want bytes at *walk, advancing the pointer and taking them off *left.
 *
 * @param[in,out] walk Walking pointer, left one past the last byte cleared [BORROWS].
 * @param[in,out] left Bytes still to clear, reduced by want [BORROWS].
 * @param[in]     want Bytes to clear now.
 * @note Used for the two edges of the zeroing. The word-wide run between them does its own stores,
 *       through its own volatile pointer, and does not come through here.
 * @note The stores are volatile. Clearing bytes nothing reads afterwards is not dropped as dead
 *       work.
 * @warning want comes off *left with no test. A want above *left wraps it. Both call sites hold
 *          want at or below what is left.
 * @warning *walk must be writable for want bytes.
 */
EMBED_INLINE void carcer_zero_bytes(volatile uint8_t **walk, size_t *left, size_t want)
{
    size_t index = 0u;

    while (index < want)
    {
        // Store, pointer advance and count advance are separate statements. Folding them into
        // *(*walk)++ = 0u would put an increment inside the volatile store
        **walk = 0u;
        (*walk)++;
        index++;
    }
    *left -= want;
}

/**
 * @brief The atomic word at offset off in the cellblock.
 *
 * @param[in] cellblock Cellblock to look in [BORROWS].
 * @param[in] off       Offset of the word, always a multiple of MMGR_CARCER_ALIGN.
 * @return              The word [BORROWS].
 * @note The cast goes through void *. base is aligned by the site macro and every offset here is a
 *       whole number of words, and the word is always correctly aligned.
 * @warning off is added to base with nothing holding it against the cellblock's size. Every caller
 *          here derives off from the tiers word or from a cell's own words, except the releases, which
 *          take an offset from the address the caller handed them.
 */
EMBED_INLINE embed_atomic_usize *carcer_word_at(const CarcerCellBlock *cellblock, size_t off)
{
    return (embed_atomic_usize *)(void *)(cellblock->base + off);
}

/**
 * @brief A cell's word built from a size and a state.
 *
 * @param[in] size  The payload size, a multiple of MMGR_CARCER_ALIGN.
 * @param[in] state CARCER_FREE, CARCER_HELD or CARCER_DEAD.
 * @return          The word.
 */
EMBED_INLINE embed_atomic_usize_value carcer_word(size_t size, embed_atomic_usize_value state)
{
    // Explicit cast carries the size into the word, which is size_t's width, ahead of the shift
    return ((embed_atomic_usize_value)size << 2u) | state;
}

/**
 * @brief The size a cell's word carries.
 *
 * @param[in] word The word.
 * @return         Its size.
 */
EMBED_INLINE size_t carcer_word_size(embed_atomic_usize_value word)
{
    // Explicit cast reads the word's upper bits, size_t's width less two, as the size they hold
    return (size_t)(word >> 2u);
}

/**
 * @brief The state a cell's word carries.
 *
 * @param[in] word The word.
 * @return         CARCER_FREE, CARCER_HELD or CARCER_DEAD.
 */
EMBED_INLINE embed_atomic_usize_value carcer_word_state(embed_atomic_usize_value word)
{
    return word & CARCER_STATE;
}

/**
 * @brief Offset of the word behind a cell, which holds the cell's own offset.
 *
 * @param[in] off  The cell's offset.
 * @param[in] size Its payload size.
 * @return         The offset of the word behind the payload.
 */
EMBED_INLINE size_t carcer_foot_of(size_t off, size_t size)
{
    return off + CARCER_WORD + size;
}

/**
 * @brief Offset just past a cell: past its word, its payload and the word behind it.
 *
 * @param[in] off  The cell's offset.
 * @param[in] size Its payload size.
 * @return         The offset the next cell up begins at.
 */
EMBED_INLINE size_t carcer_end_of(size_t off, size_t size)
{
    return off + CARCER_HDR + size;
}

/**
 * @brief Moves a word from the value read to a new one, by one swap.
 *
 * @param[in,out] cellblock Cellblock the word sits in [BORROWS].
 * @param[in]     off       The word's offset.
 * @param[in]     from      The word as this context read it, which the swap expects.
 * @param[in]     to        The word to put.
 * @return                  EMBED_TRUE where the swap landed, EMBED_FALSE where the word is not from:
 *                          another context touched it since, and this context touches nothing.
 */
EMBED_INLINE embed_bool carcer_swapped(const CarcerCellBlock *cellblock, size_t off, embed_atomic_usize_value from,
                                       embed_atomic_usize_value to)
{
    embed_atomic_usize_value expected = from;

    return embed_atomic_usize_swap_if(carcer_word_at(cellblock, off), &expected, to);
}

/**
 * @brief Returns the offset of a cell's own word.
 *
 * @param[in] cellblock Cellblock the cell came from [BORROWS].
 * @param[in] at        First byte of the cell [BORROWS].
 * @return              Offset of its word in the cellblock.
 * @warning at is taken to be a cell of this cellblock and nothing here tests that it is. Both
 *          releases bound their address with mmgr_who_owns_buf before reaching this, and what arrives
 *          lies inside the cellblock. That bound does not say a cell begins there. An address
 *          inside these bytes but off a cell boundary still yields an offset reading bytes which are
 *          not a cell's word.
 */
EMBED_INLINE size_t carcer_off_of(const CarcerCellBlock *cellblock, const void *at)
{
    // Explicit casts take at to a byte pointer so the difference is in bytes, then that ptrdiff_t to
    // the size_t the offset is carried in. An at below base makes the difference negative and the
    // cast wraps it high: that is the unchecked case the warning above describes
    return (size_t)((const uint8_t *)at - cellblock->base) - CARCER_WORD;
}

/**
 * @brief Raises one tier's high-water mark, or expands to nothing where the build tracks none.
 *
 * @param[in] cellblock_ Cellblock to record against.
 * @param[in] member_    Mark to raise, which only exists when the build tracks one.
 * @param[in] used_      Bytes in use by the tier that moved.
 * @note A macro in place of a call. The member does not exist when the build tracks none, and an
 *       argument cannot name a member that is not declared.
 * @warning The two arms do not evaluate the same things. Where the build tracks none, not one of the
 *          three arguments is evaluated, and nothing passed here may carry a side effect.
 */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CARCER_HW(cellblock_, member_, used_) ((void)embed_atomic_usize_raise(&(cellblock_)->member_, (used_)))
#else
#define CARCER_HW(cellblock_, member_, used_) ((void)0)
#endif

/**
 * @brief Takes a cell for want bytes out of the gap, on whichever tier asked.
 *
 * @param[in,out] cellblock Cellblock to allocate in [BORROWS].
 * @param[in]     want      Payload wanted, already rounded.
 * @param[in]     down      EMBED_TRUE for the tier that grows down.
 * @return                  The cell, or NULL when the gap cannot meet it [RETURNS OWNERSHIP].
 * @note Both tiers reach this. The tiers word is read, the room between the two boundaries is tested,
 *       and the one boundary moves by one swap on the tiers word read. A swap that fails is a word
 *       another context moved, and the test runs again over what it holds now.
 * @note The cell's two words are laid once the swap has landed, each read first and then swapped from
 *       what the read found: the word ahead becomes the size in the busy state, the word behind becomes
 *       the cell's offset. The reads come after the swap, since the bytes are this context's from that
 *       instant and from no earlier one: a read taken before it can be of a cell long gone, with the
 *       tiers word back at the value read by the time the swap lands. Outside the tiers every word is
 *       dead, and nothing writes a dead word but the take that owns its bytes. The swaps land; where
 *       one does not, the take answers NULL with the boundary where it stands.
 * @note Fails closed. A request the gap cannot meet moves no boundary at all.
 */
EMBED_INLINE void *carcer_grow(CarcerCellBlock *cellblock, size_t want, embed_bool down)
{
    const size_t need = CARCER_HDR + want;
    embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);
    size_t off;

    for (;;)
    {
        const size_t end = mmgr_tiers_persistent_end(tiers);
        const size_t top = mmgr_tiers_temporary_top(tiers);

        if ((top - end) < need)
        {
            return NULL;
        }
        off = down ? (top - need) : end;
        const embed_atomic_usize_value moved = down ? MMGR_CARCER_TIERS(end, off) : MMGR_CARCER_TIERS(off + need, top);

        if (embed_atomic_usize_swap_if(&cellblock->tiers, &tiers, moved))
        {
            break;
        }
    }
    const size_t foot = carcer_foot_of(off, want);
    const embed_atomic_usize_value found_word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
    const embed_atomic_usize_value found_foot = embed_atomic_usize_load(carcer_word_at(cellblock, foot));

    if (!carcer_swapped(cellblock, off, found_word, carcer_word(want, CARCER_HELD)) ||
        !carcer_swapped(cellblock, foot, found_foot, off))
    {
        return NULL;
    }
    if (down)
    {
        CARCER_HW(cellblock, temporary_hw, cellblock->size - off);
    }
    else
    {
        CARCER_HW(cellblock, persistent_hw, off + need);
    }
    return cellblock->base + off + CARCER_WORD;
}

/**
 * @brief Rounds a request up to a whole word.
 *
 * @param[in] want Bytes the caller asked for.
 * @return         The payload a cell carries for this request. A want of 0 returns MMGR_CARCER_ALIGN.
 * @note Both tiers round the same way, and it is done in one place.
 * @warning A want within a word of SIZE_MAX wraps in the rounding and comes back small, and the cell
 *          carries far less than was asked for. Neither allocator tests the request before this.
 */
EMBED_INLINE size_t carcer_want(size_t want)
{
    return carcer_round((want != 0u) ? want : MMGR_CARCER_ALIGN);
}

/**
 * @brief Clears the busy bit of a cell's word by one and, and reads the word as it was.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     off       The cell's offset.
 * @return                  The word before the and: its size, and the state the cell was in.
 * @note The one and is the whole give. The word it hands back is what this context needs to know,
 *       and the cell is free from this instant for whichever trim reaches it.
 */
EMBED_INLINE embed_atomic_usize_value carcer_freed(const CarcerCellBlock *cellblock, size_t off)
{
    return embed_atomic_usize_fetch_and(carcer_word_at(cellblock, off), ~CARCER_STATE);
}

/**
 * @brief Trims a free cell off the persistent end, and every free cell below it in turn.
 *
 * @param[in,out] cellblock Cellblock whose tier ends at the cell [BORROWS].
 * @param[in]     off       Offset of the free cell.
 * @param[in]     size      Its payload size.
 * @note Each cell comes off in two swaps. Its word goes from free to dead first, and that swap is the
 *       claim on the trim: two gives trimming the same cell at once swap from the same free word, and
 *       the give whose swap lands trims it while the other stops. Then the tiers word goes from the end
 *       at the cell's end to the end at its start. Where the top moved meanwhile, that swap is read
 *       again; where the end moved, a take grew a cell above, and the word goes back from dead to
 *       free: the cell stays in the tier, free, for the give of the cell above to trim.
 * @note A dead word stands on every byte outside the tiers. No such byte reads as a free cell, and a
 *       take that lands on dead bytes swaps the dead word for its own.
 * @note The cell below is reached through the word behind it, which holds its offset. It is trimmed
 *       only where its word reads free with a size that ends exactly at this cell. A word not yet
 *       laid by a take, or bytes that are no cell, end the trim.
 */
EMBED_INLINE void carcer_trimmed(CarcerCellBlock *cellblock, size_t off, size_t size)
{
    for (;;)
    {
        const size_t end = carcer_end_of(off, size);
        embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);

        if ((mmgr_tiers_persistent_end(tiers) != end) ||
            !carcer_swapped(cellblock, off, carcer_word(size, CARCER_FREE), carcer_word(size, CARCER_DEAD)))
        {
            return;
        }
        while (!embed_atomic_usize_swap_if(&cellblock->tiers, &tiers, MMGR_CARCER_TIERS(off, mmgr_tiers_temporary_top(tiers))))
        {
            if (mmgr_tiers_persistent_end(tiers) != end)
            {
                (void)carcer_swapped(cellblock, off, carcer_word(size, CARCER_DEAD), carcer_word(size, CARCER_FREE));
                return;
            }
        }
        if (off < (CARCER_HDR + MMGR_CARCER_ALIGN))
        {
            return;
        }
        // Explicit cast reads the word behind the cell below, size_t's width, as the offset it holds
        const size_t below = (size_t)embed_atomic_usize_load(carcer_word_at(cellblock, off - CARCER_WORD));

        if ((below >= off) || ((below & (MMGR_CARCER_ALIGN - 1u)) != 0u))
        {
            return;
        }
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, below));
        const size_t below_size = carcer_word_size(word);

        if ((carcer_word_state(word) != CARCER_FREE) || (below_size < MMGR_CARCER_ALIGN) ||
            (carcer_end_of(below, below_size) != off))
        {
            return;
        }
        off = below;
        size = below_size;
    }
}

/**
 * @brief Trims a free cell off the temporary top, where it stands there.
 *
 * @param[in,out] cellblock Cellblock whose tier begins at the cell [BORROWS].
 * @param[in]     off       Offset of the free cell.
 * @param[in]     size      Its payload size.
 * @note The same two swaps as the persistent trim: the word from free to dead, then the tiers word from
 *       the top at the cell to the top past it, read again where the end moved, and the word back to
 *       free where the top moved, since a take grew a cell below. A cell below the top stays in the
 *       tier, free, until a rewind drops it: this tier gives its cells back by mark, and a single give
 *       trims only the cell at the top.
 */
EMBED_INLINE void carcer_top_trimmed(CarcerCellBlock *cellblock, size_t off, size_t size)
{
    embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);

    if ((mmgr_tiers_temporary_top(tiers) != off) ||
        !carcer_swapped(cellblock, off, carcer_word(size, CARCER_FREE), carcer_word(size, CARCER_DEAD)))
    {
        return;
    }
    while (!embed_atomic_usize_swap_if(&cellblock->tiers, &tiers,
                                       MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers), carcer_end_of(off, size))))
    {
        if (mmgr_tiers_temporary_top(tiers) != off)
        {
            (void)carcer_swapped(cellblock, off, carcer_word(size, CARCER_DEAD), carcer_word(size, CARCER_FREE));
            return;
        }
    }
}

/**
 * @brief Adds an owner to the cellblock's list where it is not in it and the list has room.
 *
 * @param[in,out] cellblock Cellblock whose list to add to [BORROWS].
 * @param[in,out] owner     The owner that took a cell [BORROWS].
 * @note An owner listed by this cellblock already is found by listed_in and costs no walk. One last
 *       listed elsewhere is looked for in this list before it is added, and an owner taking from two
 *       cellblocks in turn appears once in each.
 * @note The first listing of an owner anywhere is claimed by one swap of listed_in from none to this
 *       cellblock, and the winner alone appends. An owner already listed elsewhere is appended by
 *       the context that finds it absent, and two contexts finding it absent in the same instant
 *       append it twice; the list is a reader's view, and a reader takes an owner once however often
 *       it appears.
 * @note A place in the list is taken by a swap on the count, which never passes the room, and the
 *       entry is written after. A reader that sees the count ahead of the entry reads an empty entry
 *       and steps past it.
 * @note Expands to nothing where MMGR_ENABLE_OWNERS is 0, and the list does not exist.
 */
EMBED_INLINE void carcer_owner_listed(CarcerCellBlock *cellblock, CellOwner *owner)
{
#if MMGR_ENABLE_OWNERS
    // Explicit casts carry the two addresses as the integers the atomic words hold
    const embed_atomic_uptr_value here = (embed_atomic_uptr_value)(uintptr_t)cellblock;
    const embed_atomic_uptr_value who = (embed_atomic_uptr_value)(uintptr_t)owner;
    embed_atomic_uptr_value listed = embed_atomic_uptr_load(&owner->listed_in);

    if (listed == here)
    {
        return;
    }
    if (listed != 0u)
    {
        // Explicit cast reads the count, size_t's width, as the entries it counts
        const size_t count = (size_t)embed_atomic_usize_load(&cellblock->owner_count);

        for (size_t at = 0u; (at < count) && (at < MMGR_CARCER_OWNERS); at += 1u)
        {
            if (embed_atomic_uptr_load(&cellblock->owners[at]) == who)
            {
                embed_atomic_uptr_store(&owner->listed_in, here);
                return;
            }
        }
    }
    else if (!embed_atomic_uptr_swap_if(&owner->listed_in, &listed, here))
    {
        // another context listed the owner first; where it listed it here there is nothing to add,
        // and where it listed it elsewhere the next take of this cellblock looks for it in this list
        return;
    }
    embed_atomic_usize_value place = embed_atomic_usize_load(&cellblock->owner_count);

    while ((place < MMGR_CARCER_OWNERS) && !embed_atomic_usize_swap_if(&cellblock->owner_count, &place, place + 1u))
    {
        // place now holds the count another context left, and the test runs again over it
    }
    // An owner past the list's room is in no list: it names none and is looked for on its next take
    if (place < MMGR_CARCER_OWNERS)
    {
        // Explicit cast narrows the place, below MMGR_CARCER_OWNERS, to the index it is
        embed_atomic_uptr_store(&cellblock->owners[(size_t)place], who);
        embed_atomic_uptr_store(&owner->listed_in, here);
    }
    else
    {
        embed_atomic_uptr_store(&owner->listed_in, listed);
    }
#else
    (void)cellblock;
    (void)owner;
#endif
}

/**
 * @brief Counts a cell's payload to its owner and raises the owner's peak.
 *
 * @param[in,out] owner The owner, or NULL to count to none [BORROWS].
 * @param[in]     bytes The cell's payload bytes.
 */
EMBED_INLINE void carcer_owner_took(CellOwner *owner, size_t bytes)
{
    if (owner != NULL)
    {
        // Explicit cast reads the count before the add, size_t's width, as the bytes it holds
        const size_t in_use = (size_t)embed_atomic_usize_fetch_add(&owner->in_use, bytes) + bytes;

        (void)embed_atomic_usize_raise(&owner->hw, in_use);
    }
}

/**
 * @brief The payload size of a cell this context holds.
 *
 * @param[in] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in] off       The cell's offset.
 * @return              The size its word carries.
 * @note A busy cell's word is written by its holder alone, and the holder reads back what it wrote.
 */
EMBED_INLINE size_t carcer_held_size(const CarcerCellBlock *cellblock, size_t off)
{
    return carcer_word_size(embed_atomic_usize_load(carcer_word_at(cellblock, off)));
}

void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    const size_t want = carcer_want(size);
    void *const prisoner = carcer_grow(cellblock, want, EMBED_FALSE);

    if ((prisoner != NULL) && (owner != NULL))
    {
        carcer_owner_took(owner, want);
        carcer_owner_listed(cellblock, owner);
    }
    return prisoner;
}

void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size)
{
    return carcer_grow(cellblock, carcer_want(size), EMBED_TRUE);
}

void mmgr_zero_buf(void *prisoner, size_t size)
{
    // Explicit cast takes the prisoner to a volatile byte pointer, the scope the edge walks use
    volatile uint8_t *walk = (volatile uint8_t *)prisoner;
    size_t left = size;
    // Explicit casts take walk to uintptr_t for the alignment test, then that result to the size_t
    // edge is carried in
    size_t edge = (size_t)(((uintptr_t)walk) & (MMGR_CARCER_ALIGN - 1u));

    // Head: bytes up to the first word boundary, where the loop below starts aligned
    edge = (edge != 0u) ? (MMGR_CARCER_ALIGN - edge) : 0u;
    edge = (edge < left) ? edge : left;
    carcer_zero_bytes(&walk, &left, edge);

    // Explicit casts go through volatile void * to reach the word scope the run stores in. The head
    // above leaves walk on a word boundary whenever it had the bytes to reach one. Where it ran out
    // first, left is 0 and neither the loop nor the tail reads through this pointer
    volatile embed_word *word_walk = (volatile embed_word *)(volatile void *)walk;

    while (left >= MMGR_CARCER_ALIGN)
    {
        // Store, pointer advance and count advance are separate statements; *word_walk++ = 0 would
        // put an increment inside the volatile store. Explicit cast gives the zero the word scope
        *word_walk = (embed_word)0;
        word_walk++;
        left -= MMGR_CARCER_ALIGN;
    }

    // Explicit casts return to the byte scope for the tail, which is under one word
    walk = (volatile uint8_t *)(volatile void *)word_walk;
    carcer_zero_bytes(&walk, &left, left);
}

void mmgr_persistent_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    if (prisoner == NULL)
    {
        return;
    }
    // One unsigned compare catches a prisoner from another cellblock. Releasing it is illegal, not
    // unlucky, and this stops in place of returning: there is no state to carry on from
    if (!mmgr_who_owns_buf(cellblock, prisoner))
    {
        MMGR_FATAL("a prisoner was released to a cellblock that does not hold it");
    }

    const size_t off = carcer_off_of(cellblock, prisoner);
    const embed_atomic_usize_value was = carcer_freed(cellblock, off);
    const size_t size = carcer_word_size(was);

    // A word that was not busy is a cell given back already, or bytes that are no cell: the and has
    // touched nothing a holder relies on, and the give ends here
    if (carcer_word_state(was) != CARCER_HELD)
    {
        return;
    }
    if (owner != NULL)
    {
        (void)embed_atomic_usize_fetch_sub(&owner->in_use, size);
    }
    // The tier is read off the address: a persistent cell lies below the end, a temporary one at or
    // above the top, and the end never passes the top
    if (off < mmgr_cellblock_persistent_end(cellblock))
    {
        carcer_trimmed(cellblock, off, size);
    }
    else
    {
        carcer_top_trimmed(cellblock, off, size);
    }
}

void mmgr_persistent_max_security_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    if (prisoner == NULL)
    {
        return;
    }
    // The same test as the plain release, taken ahead of the word read below. A quiet return here
    // would report a wipe that never happened, and this stops instead
    if (!mmgr_who_owns_buf(cellblock, prisoner))
    {
        MMGR_FATAL("a prisoner was released to a maximum security cellblock that does not hold it");
    }
    mmgr_zero_buf(prisoner, carcer_held_size(cellblock, carcer_off_of(cellblock, prisoner)));
    mmgr_persistent_buf_release(cellblock, prisoner, owner);
}

size_t mmgr_temporary_buf_mark(const CarcerCellBlock *cellblock)
{
    return mmgr_cellblock_temporary_top(cellblock);
}

void mmgr_temporary_buf_release(CarcerCellBlock *cellblock, size_t mark)
{
    if (mark > cellblock->size)
    {
        return;
    }
    embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);

    // A mark below the top as it stands is one this cellblock never handed out, and the rewind refuses
    // it. The swap moves the top in one step over whatever cells were taken since the read, and the
    // test runs again over the top they left
    while (mark > mmgr_tiers_temporary_top(tiers))
    {
        if (embed_atomic_usize_swap_if(&cellblock->tiers, &tiers, MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers), mark)))
        {
            return;
        }
    }
}

void mmgr_max_security_buf_return(CarcerCellBlock *cellblock, size_t mark)
{
    const size_t top = mmgr_cellblock_temporary_top(cellblock);

    // Zero only for a mark above the top and inside the cellblock; [top, mark) is what is live
    if ((mark > top) && (mark <= cellblock->size))
    {
        mmgr_zero_buf(cellblock->base + top, mark - top);
    }
    mmgr_temporary_buf_release(cellblock, mark);
}

void mmgr_temporary_buf_reset(CarcerCellBlock *cellblock)
{
    mmgr_temporary_buf_release(cellblock, cellblock->size);
}

embed_bool mmgr_who_owns_buf(const CarcerCellBlock *cellblock, const void *at)
{
    // Explicit casts to uintptr_t let one unsigned compare cover both ends: below base wraps high
    // Explicit cast narrows the int result of < to the embed_bool container
    return (embed_bool)(((uintptr_t)at - (uintptr_t)cellblock->base) < cellblock->size);
}

size_t mmgr_buf_available(const CarcerCellBlock *cellblock)
{
    const embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);

    return mmgr_tiers_temporary_top(tiers) - mmgr_tiers_persistent_end(tiers);
}

size_t mmgr_align_up_buf(size_t size)
{
    return carcer_round(size);
}

void mmgr_cellblock_emptied(CarcerCellBlock *cellblock)
{
    const size_t end = mmgr_cellblock_persistent_end(cellblock);
    size_t off = 0u;

    // Every free cell the persistent tier holds is marked dead before the tier is dropped. No byte the
    // tiers give back then reads as a free cell. The cellblock is quiet: every word read here is
    // written and every step lands on a cell
    while (off < end)
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
        const size_t size = carcer_word_size(word);

        if (carcer_word_state(word) == CARCER_FREE)
        {
            (void)carcer_swapped(cellblock, off, word, carcer_word(size, CARCER_DEAD));
        }
        off = carcer_end_of(off, size);
    }
    embed_atomic_usize_store(&cellblock->tiers, MMGR_CARCER_TIERS(0u, cellblock->size));
#if MMGR_ENABLE_OWNERS
    embed_atomic_usize_store(&cellblock->owner_count, 0u);
#endif
}

#if MMGR_ENABLE_HOST
#include <stdint.h>
#include <stdlib.h>

embed_bool mmgr_host_cellblock_made(CarcerCellBlock **kept, size_t bytes)
{
    const size_t head = (sizeof(CarcerCellBlock) + MMGR_ALIGN_BYTES - 1u) & ~(size_t)(MMGR_ALIGN_BYTES - 1u);

    if ((bytes < (2u * MMGR_CARCER_ALIGN)) || (bytes > MMGR_CARCER_BYTES_MOST))
    {
        return 0u;
    }
    uint8_t *const block = calloc(1u, head + bytes + MMGR_ALIGN_BYTES);

    if (block == NULL)
    {
        return 0u;
    }
    // Explicit casts read the block's address as the integer the alignment is taken on
    const uintptr_t at = (uintptr_t)block;
    const uintptr_t base = (at + head + MMGR_ALIGN_BYTES - 1u) & ~(uintptr_t)(MMGR_ALIGN_BYTES - 1u);
    const CarcerCellBlock made = MMGR_CARCER_STATE((uint8_t *)base, bytes);
    // The record is laid byte by byte into the block's first bytes, which are its first use
    const uint8_t *const from = (const uint8_t *)&made;

    for (size_t byte = 0u; byte < sizeof(made); byte += 1u)
    {
        block[byte] = from[byte];
    }
    mmgr_host_cellblock_freed(kept);
    *kept = (CarcerCellBlock *)(void *)block;
    return 1u;
}

void mmgr_host_cellblock_freed(CarcerCellBlock **kept)
{
    if (*kept != NULL)
    {
        // The record is the block's first bytes, and freeing it frees the block whole
        free(*kept);
        *kept = NULL;
    }
}

embed_bool mmgr_host_cellblock_zero(const CarcerCellBlock *cellblock)
{
    if ((cellblock == NULL) || (mmgr_cellblock_persistent_end(cellblock) != 0u) ||
        (mmgr_cellblock_temporary_top(cellblock) != cellblock->size))
    {
        return 0u;
    }
    for (size_t byte = 0u; byte < cellblock->size; byte += 1u)
    {
        if (cellblock->base[byte] != 0u)
        {
            return 0u;
        }
    }
    return 1u;
}
#endif
