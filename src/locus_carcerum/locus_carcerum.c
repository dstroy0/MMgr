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
 * @note A cellblock is a run of bytes with a word ahead of each cell and two words behind it: the
 *       cell's owner, then its tail, the cell's own offset on the persistent tier and the end of its
 *       lifetime on the temporary one. The persistent tier allocates upward from base and the
 *       temporary tier allocates downward from size, and the gap between them is what either can still
 *       take.
 * @note A take reads the tiers word, finds the room between the two boundaries, and moves one of them
 *       by one swap on the word read; a swap that fails is a word another context moved first, and the
 *       take reads it again and decides over what it says now. A give clears the busy bit of the
 *       cell's word by one and, and the word the and hands back is the cell's size. No context waits
 *       for another, and no context steps through another context's cells.
 * @note A cell given back below a tier's boundary stays in the tier, free, until the cells between it
 *       and the boundary are given back too. The give that frees the cell at the boundary trims it
 *       off by one swap on the tiers word, then reads the cell below through the tail behind it, and
 *       trims that one the same way while it reads free. Every cell trimmed off is marked dead. No
 *       byte outside the tiers reads as a free cell.
 * @note A take lays its words after the swap that gave it the bytes, by a swap from the word it reads
 *       there once they are its own, the word ahead of the payload last. A header another context has
 *       laid since is refused and never written over.
 * @note A temporary cell's tail holds the end of the lifetime its take gave it, on the library's
 *       clock. A take gives back the cells at the top whose lifetimes have ended before it takes its
 *       own, and tells the port of each.
 * @note Every take, give and growth names a registered owner. The owners are kept in one queue, in
 *       the order they registered, and an owner's place in it with the time it registered is its key.
 *       A cell counts to the owner its owner word names, and a give or a growth naming another owner
 *       is refused and told to the port.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "memoria_operor/memoria_operor.h"

/** @brief The state of a cell no context holds. */
#define CARCER_FREE 0u

/** @brief The state of a cell a context holds. */
#define CARCER_BUSY 1u

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
 * @brief Bytes a cell costs beyond its payload: the word ahead of it, the owner's word behind it and
 *        the tail behind that.
 *
 * @note Charged on top of the payload every time a cell is allocated. An allocation of size bytes
 *       costs the gap this much more than size.
 */
#define CARCER_HDR (3u * CARCER_WORD)

/** @brief A difference of two clock readings at or above this is a time not yet reached. */
#define CARCER_CLOCK_AHEAD 0x80000000u

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
 * @param[in] state CARCER_FREE, CARCER_BUSY or CARCER_DEAD.
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
 * @return         CARCER_FREE, CARCER_BUSY or CARCER_DEAD.
 */
EMBED_INLINE embed_atomic_usize_value carcer_word_state(embed_atomic_usize_value word)
{
    return word & CARCER_STATE;
}

/**
 * @brief Offset of the word behind a cell's payload, which names the cell's owner.
 *
 * @param[in] off  The cell's offset.
 * @param[in] size Its payload size.
 * @return         The offset of the owner's word.
 */
EMBED_INLINE size_t carcer_foot_of(size_t off, size_t size)
{
    return off + CARCER_WORD + size;
}

/**
 * @brief Offset of a cell's tail, behind its owner's word: the cell's own offset on the persistent
 *        tier, the end of its lifetime on the temporary one.
 *
 * @param[in] off  The cell's offset.
 * @param[in] size Its payload size.
 * @return         The offset of the tail.
 */
EMBED_INLINE size_t carcer_tail_of(size_t off, size_t size)
{
    return off + (2u * CARCER_WORD) + size;
}

/**
 * @brief Offset just past a cell: past its word, its payload, its owner's word and its tail.
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
 * @brief Counts a cell's payload to its owner and raises the owner's peak.
 *
 * @param[in,out] owner The owner [BORROWS].
 * @param[in]     bytes The cell's payload bytes.
 */
EMBED_INLINE void carcer_owner_took(CellOwner *owner, size_t bytes)
{
    // Explicit cast reads the count before the add, size_t's width, as the bytes it holds
    const size_t in_use = (size_t)embed_atomic_usize_fetch_add(&owner->in_use, bytes) + bytes;

    (void)embed_atomic_usize_raise(&owner->hw, in_use);
}

/**
 * @brief Takes a cell's payload bytes off its owner.
 *
 * @param[in,out] owner The owner [BORROWS].
 * @param[in]     bytes The cell's payload bytes.
 */
EMBED_INLINE void carcer_owner_gave(CellOwner *owner, size_t bytes)
{
    (void)embed_atomic_usize_fetch_sub(&owner->in_use, bytes);
}

/** @brief The owner queue: every registered owner, as the integer of its address, in the order it registered. */
static embed_atomic_uptr s_owner_queue[MMGR_CARCER_OWNERS] = {EMBED_ATOMIC_INIT(0u)};

/** @brief The owners in the queue, at most MMGR_CARCER_OWNERS. */
static embed_atomic_usize s_owner_count = EMBED_ATOMIC_INIT(0u);

/** @brief 1 while one context registers an owner, which the others wait out; 0 otherwise. */
static embed_atomic_usize s_owner_registering = EMBED_ATOMIC_INIT(0u);

/**
 * @brief Whether an owner may take: it is not NULL and its registration has landed.
 *
 * @param[in] owner The owner named [BORROWS].
 * @return          EMBED_TRUE where it may.
 */
EMBED_INLINE embed_bool carcer_owner_registered(const CellOwner *owner)
{
    // Explicit cast narrows the int result of the test to the embed_bool container
    return (embed_bool)((owner != NULL) && (embed_atomic_usize_load(&owner->place) != 0u));
}

/**
 * @brief Whether two owners' names are the same bytes.
 *
 * @param[in] left  One name, ended by a zero byte [BORROWS].
 * @param[in] right The other, ended by a zero byte [BORROWS].
 * @return          EMBED_TRUE where every byte up to and including the zero matches.
 */
EMBED_INLINE embed_bool carcer_names_same(const char *left, const char *right)
{
    size_t at = 0u;

    while ((left[at] == right[at]) && (left[at] != '\0'))
    {
        at += 1u;
    }
    // Explicit cast narrows the int result of == to the embed_bool container
    return (embed_bool)(left[at] == right[at]);
}

/**
 * @brief Whether an address is one of the registered owners'.
 *
 * @param[in] named The address, as the integer a cell's owner word holds.
 * @return          The owner where one is registered at that address, NULL where none is [BORROWS].
 * @note A cell's owner word is read before any context has said the bytes are a cell's. The
 *       address is matched against the queue before it is written through.
 */
EMBED_INLINE CellOwner *carcer_owner_found(embed_atomic_usize_value named)
{
    // Explicit cast reads the count, size_t's width, as the entries it counts
    const size_t count = (size_t)embed_atomic_usize_load(&s_owner_count);

    for (size_t at = 0u; (at < count) && (at < MMGR_CARCER_OWNERS); at += 1u)
    {
        // Explicit cast widens the owner word's integer to the queue entry's to compare the two
        if (embed_atomic_uptr_load(&s_owner_queue[at]) == (embed_atomic_uptr_value)named)
        {
            // Explicit casts read the owner word's integer back as the address it holds
            return (CellOwner *)(uintptr_t)named;
        }
    }
    return NULL;
}

embed_bool mmgr_owner_register(CellOwner *owner)
{
    if ((owner == NULL) || (owner->name == NULL))
    {
        return EMBED_FALSE;
    }
    embed_atomic_usize_value idle = 0u;

    while (!embed_atomic_usize_swap_if(&s_owner_registering, &idle, 1u))
    {
        // another context registers now; idle holds what it read, and is put back to 0 to wait again
        idle = 0u;
    }
    // Explicit cast reads the count, size_t's width, as the entries it counts
    const size_t count = (size_t)embed_atomic_usize_load(&s_owner_count);
    embed_bool refused = (embed_bool)((count >= MMGR_CARCER_OWNERS) || (embed_atomic_usize_load(&owner->place) != 0u));

    for (size_t at = 0u; !refused && (at < count); at += 1u)
    {
        // Explicit casts read the entry's integer back as the address it holds
        const CellOwner *const listed = (const CellOwner *)(uintptr_t)embed_atomic_uptr_load(&s_owner_queue[at]);

        refused = carcer_names_same(listed->name, owner->name);
    }
    if (!refused)
    {
        owner->registered_ms = mmgr_carcer_hw_clock_ms();
        // Explicit cast carries the owner's address as the integer the queue holds
        embed_atomic_uptr_store(&s_owner_queue[count], (embed_atomic_uptr_value)(uintptr_t)owner);
        embed_atomic_usize_store(&s_owner_count, count + 1u);
        embed_atomic_usize_store(&owner->place, count + 1u);
    }
    embed_atomic_usize_store(&s_owner_registering, 0u);
    // Explicit cast narrows the int result of ! to the embed_bool container
    return (embed_bool)!refused;
}

size_t mmgr_owner_count(void)
{
    // Explicit cast reads the count, size_t's width, as the entries it counts
    const size_t count = (size_t)embed_atomic_usize_load(&s_owner_count);

    return (count < MMGR_CARCER_OWNERS) ? count : MMGR_CARCER_OWNERS;
}

CellOwner *mmgr_owner_at(size_t at)
{
    if (at >= mmgr_owner_count())
    {
        return NULL;
    }
    // Explicit casts read the entry's integer back as the address it holds
    return (CellOwner *)(uintptr_t)embed_atomic_uptr_load(&s_owner_queue[at]);
}

/**
 * @brief Weak default for the library's clock, which answers 0 every time.
 *
 * @return 0.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port defines the
 *       name to replace it.
 */
EMBED_WEAK uint32_t mmgr_carcer_hw_clock_ms(void)
{
    return 0u;
}

/**
 * @brief Weak default for the report of a temporary cell given back at the end of its lifetime, which
 *        does nothing.
 *
 * @param[in] owner      The owner the cell was counted to [BORROWS].
 * @param[in] bytes      The cell's payload bytes.
 * @param[in] overdue_ms Milliseconds the cell stood past its lifetime.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port defines the
 *       name to replace it.
 */
EMBED_WEAK void mmgr_carcer_hw_expired(const CellOwner *owner, size_t bytes, uint32_t overdue_ms)
{
    (void)owner;
    (void)bytes;
    (void)overdue_ms;
}

/**
 * @brief Weak default for the report of a give or a growth naming an owner other than the cell's, which
 *        does nothing.
 *
 * @param[in] named      The owner the give or the growth named [BORROWS].
 * @param[in] cell_owner The owner the cell counts to, NULL where its word names no registered owner
 *                       [BORROWS].
 * @param[in] bytes      The cell's payload bytes.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port defines the
 *       name to replace it.
 */
EMBED_WEAK void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
}

/**
 * @brief Clears the busy bit of a cell's word by one and, and reads the word as it was.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     off       The cell's offset.
 * @return                  The word before the and: its size, and the state the cell was in.
 * @note The one and is the whole give. The word it hands back is what this context needs to know,
 *       and the cell is free from this instant for whichever trim reaches it.
 * @note The and clears the busy bit alone. A dead word carries the dead bit and no busy bit, and the
 *       and leaves it dead: a give of a cell a rewind has dropped touches nothing.
 */
EMBED_INLINE embed_atomic_usize_value carcer_freed(const CarcerCellBlock *cellblock, size_t off)
{
    return embed_atomic_usize_fetch_and(carcer_word_at(cellblock, off), ~(embed_atomic_usize_value)CARCER_BUSY);
}

/**
 * @brief Takes a cell's bytes off the owner its owner word names, once.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     off       The cell's offset.
 * @param[in]     size      Its payload size.
 * @note The owner word is swapped from the owner read to none, and the context whose swap lands
 *       takes the bytes off that owner. A give and a rewind reaching the same cell at once both come
 *       here, and the bytes come off once between them. A word that reads none names no owner and is
 *       left as it is.
 */
EMBED_INLINE void carcer_foot_claimed(const CarcerCellBlock *cellblock, size_t off, size_t size)
{
    const size_t foot = carcer_foot_of(off, size);
    const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(cellblock, foot));

    if ((named != 0u) && carcer_swapped(cellblock, foot, named, 0u))
    {
        // Explicit casts read the owner word's integer back as the address it holds
        carcer_owner_gave((CellOwner *)(uintptr_t)named, size);
    }
}

/**
 * @brief Marks every temporary cell from off up to bound dead and takes its bytes off its owner.
 *
 * @param[in,out] cellblock Cellblock the cells sit in [BORROWS].
 * @param[in]     off       Offset of the lowest cell dropped.
 * @param[in]     bound     Offset the dropped cells end at: the mark of a rewind, or the top as it stands.
 * @note Each cell is claimed by one swap of its word, busy or free, for the dead word of the same size,
 *       and the context whose swap lands takes the bytes off the owner the owner word names. Two
 *       contexts dropping the same cells count each once between them.
 * @note The walk steps by each cell's size and ends at bound, at a word that reads dead already, or at
 *       a word that is no cell: a size under one word or one that reaches past bound. A take whose
 *       words are not yet laid stands as such a word, and the cells above it are dropped by that take
 *       once it finds the top above its cell.
 */
EMBED_INLINE void carcer_dropped(CarcerCellBlock *cellblock, size_t off, size_t bound)
{
    while (off < bound)
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
        const size_t size = carcer_word_size(word);

        if ((carcer_word_state(word) == CARCER_DEAD) || (size < MMGR_CARCER_ALIGN) ||
            (carcer_end_of(off, size) > bound) ||
            !carcer_swapped(cellblock, off, word, carcer_word(size, CARCER_DEAD)))
        {
            return;
        }
        carcer_foot_claimed(cellblock, off, size);
        off = carcer_end_of(off, size);
    }
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
 * @param[in,out] owner     The registered owner the cell is counted to [BORROWS].
 * @param[in]     ends_ms   The clock reading a temporary cell's lifetime ends at; not read for a
 *                          persistent cell.
 * @return                  The cell, or NULL when the gap cannot meet it [RETURNS OWNERSHIP].
 * @note Both tiers reach this. The tiers word is read, the room between the two boundaries is tested,
 *       and the one boundary moves by one swap on the tiers word read. A swap that fails is a word
 *       another context moved, and the test runs again over what it holds now.
 * @note The cell's words are laid once the swap has landed, each read first and then swapped from what
 *       the read found: the tail first, the cell's offset on the persistent tier and the end of its
 *       lifetime on the temporary one, then the owner's word, and the word ahead last, the size in the
 *       busy state. A context that reads the word ahead as busy reads the words behind as this take laid
 *       them. The reads come after the swap, since the bytes are
 *       this context's from that instant and from no earlier one: a read taken before it can be of a
 *       cell long gone, with the tiers word back at the value read by the time the swap lands. Outside
 *       the tiers every word is dead, and nothing writes a dead word but the take that owns its bytes.
 *       The swaps land; where one does not, the take answers NULL with the boundary where it stands.
 * @note A temporary cell is counted to its owner once its words stand, and the tiers word is read once
 *       more. A top above the cell is a rewind that dropped it between the swap and the count: the take
 *       drops its own cell and every dropped cell above it the rewind could not step over, and answers
 *       NULL.
 * @note Fails closed. A request the gap cannot meet moves no boundary at all.
 */
EMBED_INLINE void *carcer_grow(CarcerCellBlock *cellblock, size_t want, embed_bool down, CellOwner *owner,
                               uint32_t ends_ms)
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
    const size_t tail = carcer_tail_of(off, want);
    const embed_atomic_usize_value found_word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
    const embed_atomic_usize_value found_foot = embed_atomic_usize_load(carcer_word_at(cellblock, foot));
    const embed_atomic_usize_value found_tail = embed_atomic_usize_load(carcer_word_at(cellblock, tail));
    // Explicit casts carry the owner's address as the integer the owner's word holds, and the offset as
    // the integer a persistent tail holds
    const embed_atomic_usize_value named = (embed_atomic_usize_value)(uintptr_t)owner;
    const embed_atomic_usize_value tail_word = down ? (embed_atomic_usize_value)ends_ms : (embed_atomic_usize_value)off;

    if (!carcer_swapped(cellblock, tail, found_tail, tail_word) || !carcer_swapped(cellblock, foot, found_foot, named) ||
        !carcer_swapped(cellblock, off, found_word, carcer_word(want, CARCER_BUSY)))
    {
        return NULL;
    }
    carcer_owner_took(owner, want);
    if (down)
    {
        CARCER_HW(cellblock, temporary_hw, cellblock->size - off);
        const size_t top = mmgr_cellblock_temporary_top(cellblock);

        if (top > off)
        {
            carcer_dropped(cellblock, off, top);
            return NULL;
        }
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
 * @note The cell below is reached through the tail behind it, which holds its offset. It is trimmed
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
        // Explicit cast reads the tail behind the cell below, size_t's width, as the offset it holds
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
                                       MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers),
                                                         carcer_end_of(off, size))))
    {
        if (mmgr_tiers_temporary_top(tiers) != off)
        {
            (void)carcer_swapped(cellblock, off, carcer_word(size, CARCER_DEAD), carcer_word(size, CARCER_FREE));
            return;
        }
    }
}

/**
 * @brief Gives back the cells at the temporary top whose lifetimes have ended, one after another while
 *        the top reads so, and tells the port of each.
 *
 * @param[in,out] cellblock Cellblock whose temporary top to read [BORROWS].
 * @param[in]     zeroing   Whether a cell given back is zeroed first, as a maximum security give zeroes.
 * @note A cell at the top is given back where its word reads busy with the clock past the end of its
 *       lifetime, as its holder's give would give it: the owner's word is claimed by one swap to none
 *       and its bytes come off that owner, the payload is zeroed where zeroing is set while the word
 *       ahead still reads busy, the word ahead goes from busy to free by one swap, and the top trim takes
 *       it off. A give or a rewind reaching the cell in the same instant claims the owner's word the same
 *       way, and the bytes come off once between them.
 * @note A word ahead that reads free is left as it is. Between another take's swap of the tiers word
 *       and its word ahead being laid, the top stands on bytes no take has laid, and those can read as
 *       a free word; a free cell is trimmed by the give that freed it, or dropped by a rewind.
 * @note The owner's word is read before any context has said the bytes are a cell's: the address it
 *       holds is matched against the owner queue before it is written through, and a word naming no
 *       registered owner ends the walk.
 * @note The walk ends at the first cell still in its lifetime, at a word that is no cell, or where a
 *       trim does not land.
 * @warning A word ahead read as busy and swapped from that reading is taken as the cell read. A holder
 *          that gave the cell, a trim, and a take laying a cell of the same size and owner at the same
 *          offset, all between the read and the swap, have their new cell given back in its place.
 * @warning Bytes no take has laid yet that read as a busy word, a lifetime word past its end and, behind
 *          the payload that word names, the address of a registered owner, are taken as a cell.
 * @warning A rewind by the cell's holder in the same instant moves the top over the cell without
 *          waiting for the zeroing: a take landing on those bytes before the zeroing ends has its bytes
 *          zeroed under it.
 */
EMBED_INLINE void carcer_expired_given(CarcerCellBlock *cellblock, embed_bool zeroing)
{
    embed_bool clocked = EMBED_FALSE;
    uint32_t now = 0u;

    for (;;)
    {
        const size_t top = mmgr_cellblock_temporary_top(cellblock);

        if ((cellblock->size - top) < (CARCER_HDR + MMGR_CARCER_ALIGN))
        {
            return;
        }
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, top));
        const size_t size = carcer_word_size(word);
        const embed_atomic_usize_value state = carcer_word_state(word);

        if ((state != CARCER_BUSY) || (size < MMGR_CARCER_ALIGN) ||
            (carcer_end_of(top, size) > cellblock->size))
        {
            return;
        }
        if (!clocked)
        {
            now = mmgr_carcer_hw_clock_ms();
            clocked = EMBED_TRUE;
        }
        // Explicit cast narrows the lifetime's word to the 32-bit clock reading its take wrote into it
        const uint32_t ends_ms = (uint32_t)embed_atomic_usize_load(carcer_word_at(cellblock, carcer_tail_of(top, size)));
        const uint32_t overdue_ms = now - ends_ms;

        if (overdue_ms >= CARCER_CLOCK_AHEAD)
        {
            return;
        }
        const size_t foot = carcer_foot_of(top, size);
        const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(cellblock, foot));
        CellOwner *const owner = carcer_owner_found(named);

        if ((owner == NULL) || !carcer_swapped(cellblock, foot, named, 0u))
        {
            return;
        }
        carcer_owner_gave(owner, size);
        mmgr_carcer_hw_expired(owner, size, overdue_ms);
        if (zeroing)
        {
            mmgr_zero_buf(cellblock->base + top + CARCER_WORD, size);
        }
        if (!carcer_swapped(cellblock, top, word, carcer_word(size, CARCER_FREE)))
        {
            return;
        }
        carcer_top_trimmed(cellblock, top, size);
        if (mmgr_cellblock_temporary_top(cellblock) == top)
        {
            return;
        }
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

/**
 * @brief Grows the newest cell of its tier in place, to a larger payload.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     off       The cell's offset.
 * @param[in]     carried   Its payload size as its word carries it.
 * @param[in]     want      The payload it grows to, rounded, above carried.
 * @param[in,out] owner     The owner the cell's owner word names, which the growth counts to [BORROWS].
 * @param[in]     zeroing   Whether the bytes the growth adds are zeroed.
 * @return                  The cell's first payload byte after the growth, moved for a temporary cell,
 *                          or NULL with nothing changed.
 * @note The cell is the newest of its tier where that tier's boundary stands at its edge: the
 *       persistent end at the cell's end, the temporary top at the cell's word. The boundary moves by
 *       one swap on the tiers word, by the growth alone. A swap that finds neither boundary at the cell
 *       is a cell taken since, and the growth is refused; one that finds the other boundary moved is
 *       read again.
 * @note A persistent cell keeps its address. Its tail and its owner's word are written past the growth
 *       first, then its own word with the new size. The bytes the growth adds, the two words the payload
 *       stood ahead of among them, are zeroed where zeroing is set.
 * @note A temporary cell moves down by the growth. Its word is written at the new top first, its
 *       payload moved down with memor.move_down, and the owner's word behind the payload stands where
 *       it stood. The bytes between the moved payload and that word are the old word and payload,
 *       zeroed where zeroing is set.
 * @note A temporary growth reads the top once more, as a take does: a top above the grown cell is a
 *       rewind that dropped it meanwhile, and the growth drops what the rewind left and answers NULL.
 */
EMBED_INLINE void *carcer_grown(CarcerCellBlock *cellblock, size_t off, size_t carried, size_t want,
                                CellOwner *owner, embed_bool zeroing)
{
    const size_t extra = want - carried;
    embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);
    embed_bool down;

    for (;;)
    {
        const size_t end = mmgr_tiers_persistent_end(tiers);
        const size_t top = mmgr_tiers_temporary_top(tiers);

        down = (top == off);
        if ((!down && (end != carcer_end_of(off, carried))) || ((top - end) < extra))
        {
            return NULL;
        }
        const embed_atomic_usize_value moved =
            down ? MMGR_CARCER_TIERS(end, off - extra) : MMGR_CARCER_TIERS(end + extra, top);

        if (embed_atomic_usize_swap_if(&cellblock->tiers, &tiers, moved))
        {
            break;
        }
    }
    if (!down)
    {
        // Explicit casts carry the cell's offset as the integer the tail holds, and the owner's address as
        // the integer its word holds
        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(off, want)), (embed_atomic_usize_value)off);
        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_foot_of(off, want)),
                                 (embed_atomic_usize_value)(uintptr_t)owner);
        embed_atomic_usize_store(carcer_word_at(cellblock, off), carcer_word(want, CARCER_BUSY));
        if (zeroing)
        {
            mmgr_zero_buf(cellblock->base + carcer_foot_of(off, carried), extra);
        }
        carcer_owner_took(owner, extra);
        CARCER_HW(cellblock, persistent_hw, carcer_end_of(off, want));
        return cellblock->base + off + CARCER_WORD;
    }
    const size_t at = off - extra;

    embed_atomic_usize_store(carcer_word_at(cellblock, at), carcer_word(want, CARCER_BUSY));
    memor.move_down(&(MemoriaCfg){
        .dst = cellblock->base + at + CARCER_WORD, .src = cellblock->base + off + CARCER_WORD, .bytes = carried});
    if (zeroing)
    {
        mmgr_zero_buf(cellblock->base + at + CARCER_WORD + carried, extra);
    }
    const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(cellblock, carcer_foot_of(at, want)));

    // A word read as none is a cell a rewind or the end of its lifetime has dropped meanwhile, and the
    // top read below finds it
    if (named != 0u)
    {
        // Explicit casts read the owner word's integer back as the owner's address
        carcer_owner_took((CellOwner *)(uintptr_t)named, extra);
    }
    CARCER_HW(cellblock, temporary_hw, cellblock->size - at);
    const size_t top = mmgr_cellblock_temporary_top(cellblock);

    if (top > at)
    {
        carcer_dropped(cellblock, at, top);
        return NULL;
    }
    return cellblock->base + at + CARCER_WORD;
}

/**
 * @brief The persistent take both security levels share: the owner tested, the take, and where the gap
 *        cannot meet it, the backstop run once and the take tried again.
 *
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size      Bytes wanted.
 * @param[in,out] owner     The owner named [BORROWS].
 * @param[in]     zeroing   Whether the backstop zeroes a cell it gives back.
 * @return                  The cell, or NULL [RETURNS OWNERSHIP].
 */
EMBED_INLINE void *carcer_persistent_take(CarcerCellBlock *cellblock, size_t size, CellOwner *owner, embed_bool zeroing)
{
    if (!carcer_owner_registered(owner))
    {
        return NULL;
    }
    const size_t want = carcer_want(size);
    void *const prisoner = carcer_grow(cellblock, want, EMBED_FALSE, owner, 0u);

    if (prisoner != NULL)
    {
        return prisoner;
    }
    const size_t top = mmgr_cellblock_temporary_top(cellblock);

    carcer_expired_given(cellblock, zeroing);
    return (mmgr_cellblock_temporary_top(cellblock) != top) ? carcer_grow(cellblock, want, EMBED_FALSE, owner, 0u)
                                                            : NULL;
}

/**
 * @brief The temporary take both security levels share: the owner and the lifetime tested, the backstop
 *        run, and the take.
 *
 * @param[in,out] cellblock   Cellblock to take from [BORROWS].
 * @param[in]     size        Bytes wanted.
 * @param[in,out] owner       The owner named [BORROWS].
 * @param[in]     lifetime_ms The cell's lifetime, in milliseconds.
 * @param[in]     zeroing     Whether the backstop zeroes a cell it gives back.
 * @return                    The cell, or NULL [RETURNS OWNERSHIP].
 */
EMBED_INLINE void *carcer_temporary_take(CarcerCellBlock *cellblock, size_t size, CellOwner *owner, uint32_t lifetime_ms,
                                         embed_bool zeroing)
{
    if (!carcer_owner_registered(owner) || (lifetime_ms == 0u) || (lifetime_ms >= CARCER_CLOCK_AHEAD))
    {
        return NULL;
    }
    carcer_expired_given(cellblock, zeroing);
    return carcer_grow(cellblock, carcer_want(size), EMBED_TRUE, owner, mmgr_carcer_hw_clock_ms() + lifetime_ms);
}

void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_persistent_take(cellblock, size, owner, EMBED_FALSE);
}

void *mmgr_persistent_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_persistent_take(cellblock, size, owner, EMBED_TRUE);
}

void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner, uint32_t lifetime_ms)
{
    return carcer_temporary_take(cellblock, size, owner, lifetime_ms, EMBED_FALSE);
}

void *mmgr_temporary_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner,
                                            uint32_t lifetime_ms)
{
    return carcer_temporary_take(cellblock, size, owner, lifetime_ms, EMBED_TRUE);
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

/**
 * @brief Whether the owner a give or a growth names is the owner the cell's owner word names.
 *
 * @param[in] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in] off       The cell's offset.
 * @param[in] size      Its payload size.
 * @param[in] owner     The owner named [BORROWS].
 * @return              EMBED_TRUE where the two are the same owner.
 * @note A word that names none is a cell given back already or dropped by a rewind or by the end of
 *       its lifetime, and nothing is told. A word naming another owner is told to the port through
 *       mmgr_carcer_hw_misnamed, with the owner the word names where it is a registered one.
 */
EMBED_INLINE embed_bool carcer_owner_named(const CarcerCellBlock *cellblock, size_t off, size_t size, const CellOwner *owner)
{
    const embed_atomic_usize_value held = embed_atomic_usize_load(carcer_word_at(cellblock, carcer_foot_of(off, size)));

    // Explicit cast carries the owner's address as the integer the owner's word holds
    if (held == (embed_atomic_usize_value)(uintptr_t)owner)
    {
        return EMBED_TRUE;
    }
    if (held != 0u)
    {
        mmgr_carcer_hw_misnamed(owner, carcer_owner_found(held), size);
    }
    return EMBED_FALSE;
}

/**
 * @brief The give both security levels share: the owner tested against the cell's, the payload zeroed
 *        where asked, the owner's word claimed and the cell freed and trimmed.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner  First byte of the cell [TAKES OWNERSHIP].
 * @param[in]     owner     The owner named [BORROWS].
 * @param[in]     zeroing   Whether the payload is zeroed before the give.
 * @note The cell's bytes come off the owner its owner word names, claimed by one swap of that word to
 *       none: a rewind or the end of a lifetime dropping a temporary cell in the same instant claims it
 *       the same way, and the bytes come off once between them. A give naming another owner changes
 *       nothing, the payload included.
 * @note The tier is read off the address ahead of the give, while the cell is busy and no trim can move
 *       the end past it: a persistent cell lies below the end, a temporary one at or above the top, and
 *       the end never passes the top.
 */
EMBED_INLINE void carcer_given(CarcerCellBlock *cellblock, void *prisoner, const CellOwner *owner, embed_bool zeroing)
{
    if ((prisoner == NULL) || (owner == NULL))
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
    const size_t size = carcer_held_size(cellblock, off);

    if (!carcer_owner_named(cellblock, off, size, owner))
    {
        return;
    }
    const embed_bool persistent = (embed_bool)(off < mmgr_cellblock_persistent_end(cellblock));

    if (zeroing)
    {
        mmgr_zero_buf(prisoner, size);
    }
    carcer_foot_claimed(cellblock, off, size);
    // A word that was not busy is a cell given back already, or dropped: the and has touched nothing a
    // holder relies on, and the give ends here
    if (carcer_word_state(carcer_freed(cellblock, off)) != CARCER_BUSY)
    {
        return;
    }
    if (persistent)
    {
        carcer_trimmed(cellblock, off, size);
        return;
    }
    carcer_top_trimmed(cellblock, off, size);
}

void mmgr_persistent_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    carcer_given(cellblock, prisoner, owner, EMBED_FALSE);
}

void mmgr_persistent_max_security_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    carcer_given(cellblock, prisoner, owner, EMBED_TRUE);
}

/**
 * @brief The growth both grow entries share: the cell found, its size read, and the request rounded.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in,out] prisoner  First byte of the cell [BORROWS].
 * @param[in]     size      Payload bytes the cell is to carry.
 * @param[in,out] owner     The owner the cell's owner word names [BORROWS].
 * @param[in]     zeroing   Whether the bytes the growth adds are zeroed.
 * @return                  The cell after the growth, the prisoner itself where it carries size already,
 *                          or NULL with nothing changed, as for a NULL prisoner, a NULL owner, or an
 *                          owner other than the cell's.
 */
EMBED_INLINE void *carcer_grow_asked(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner,
                                     embed_bool zeroing)
{
    if ((prisoner == NULL) || (owner == NULL))
    {
        return NULL;
    }
    if (!mmgr_who_owns_buf(cellblock, prisoner))
    {
        MMGR_FATAL("a prisoner was grown in a cellblock that does not hold it");
    }
    const size_t off = carcer_off_of(cellblock, prisoner);
    const size_t held = carcer_held_size(cellblock, off);
    const size_t want = carcer_want(size);

    if (!carcer_owner_named(cellblock, off, held, owner))
    {
        return NULL;
    }
    if (want <= held)
    {
        return prisoner;
    }
    return carcer_grown(cellblock, off, held, want, owner, zeroing);
}

void *mmgr_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner)
{
    return carcer_grow_asked(cellblock, prisoner, size, owner, EMBED_FALSE);
}

void *mmgr_max_security_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner)
{
    return carcer_grow_asked(cellblock, prisoner, size, owner, EMBED_TRUE);
}

size_t mmgr_temporary_buf_mark(const CarcerCellBlock *cellblock)
{
    return mmgr_cellblock_temporary_top(cellblock);
}

/**
 * @brief Rewinds the temporary tier to mark: every cell from the top to mark is dropped off its owner
 *        and marked dead, zeroed where asked, and the top moves to mark by one swap on the tiers word.
 *
 * @param[in,out] cellblock Cellblock to rewind [BORROWS].
 * @param[in]     mark      Top to restore, as mmgr_temporary_buf_mark reported it.
 * @param[in]     zeroing   EMBED_TRUE where the dropped bytes are zeroed before the top moves.
 * @note A mark below the top as it stands is one this cellblock never handed out, and the rewind
 *       refuses it, as it refuses one past the cellblock's size. The cells are dropped and zeroed
 *       while they still lie in the tier, where no take lands on them; the swap then moves the top
 *       over them in one step. Where the swap fails because a take lowered the top meanwhile, that
 *       take's cells are dropped and zeroed too and the swap runs again; where another rewind raised
 *       it past mark, there is nothing left to do.
 */
EMBED_INLINE void carcer_rewound(CarcerCellBlock *cellblock, size_t mark, embed_bool zeroing)
{
    if (mark > cellblock->size)
    {
        return;
    }
    embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);

    while (mark > mmgr_tiers_temporary_top(tiers))
    {
        const size_t top = mmgr_tiers_temporary_top(tiers);

        carcer_dropped(cellblock, top, mark);
        if (zeroing)
        {
            mmgr_zero_buf(cellblock->base + top, mark - top);
        }
        if (embed_atomic_usize_swap_if(&cellblock->tiers, &tiers, MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers), mark)))
        {
            return;
        }
    }
}

void mmgr_temporary_buf_release(CarcerCellBlock *cellblock, size_t mark)
{
    carcer_rewound(cellblock, mark, EMBED_FALSE);
}

void mmgr_max_security_buf_return(CarcerCellBlock *cellblock, size_t mark)
{
    carcer_rewound(cellblock, mark, EMBED_TRUE);
}

void mmgr_temporary_buf_reset(CarcerCellBlock *cellblock)
{
    mmgr_temporary_buf_release(cellblock, cellblock->size);
}

CellOwner *mmgr_owner_of_buf(const CarcerCellBlock *cellblock, const void *prisoner)
{
    if ((prisoner == NULL) || !mmgr_who_owns_buf(cellblock, prisoner))
    {
        return NULL;
    }
    const size_t off = carcer_off_of(cellblock, prisoner);
    const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
    const size_t size = carcer_word_size(word);

    if ((carcer_word_state(word) != CARCER_BUSY) || (carcer_end_of(off, size) > cellblock->size))
    {
        return NULL;
    }
    return carcer_owner_found(embed_atomic_usize_load(carcer_word_at(cellblock, carcer_foot_of(off, size))));
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

    // Every free cell the persistent tier holds, and every cell the temporary tier holds, is marked dead
    // before the tiers are dropped. No byte the tiers give back then reads as a cell. The cellblock is
    // quiet: every word read here is written and every step lands on a cell. The temporary cells are
    // marked by the drop without their owners counted off: a suite that empties a cellblock resets its
    // owners itself
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
    off = mmgr_cellblock_temporary_top(cellblock);
    while (off < cellblock->size)
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
        const size_t size = carcer_word_size(word);

        if (carcer_word_state(word) != CARCER_DEAD)
        {
            (void)carcer_swapped(cellblock, off, word, carcer_word(size, CARCER_DEAD));
        }
        off = carcer_end_of(off, size);
    }
    embed_atomic_usize_store(&cellblock->tiers, MMGR_CARCER_TIERS(0u, cellblock->size));
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
