/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum.c
 * @brief The cellblock's two tiers over the caller's bytes: a take is one swap on the tiers word, and a
 *        give merges the cell with its free neighbors while it holds the cell, and frees it last.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note A cellblock is a run of bytes with a word ahead of each cell and two words behind it: the
 *       cell's owner, then its tail, the cell's own offset. The persistent tier allocates upward from
 *       base and the
 *       temporary tier allocates downward from size, and the gap between them is what either can still
 *       take.
 * @note A take reads the tiers word, finds the room between the two boundaries, and moves one of them
 *       by one swap on the word read; a swap that fails is a word another context moved first, and the
 *       take reads it again and decides over what it says now. No context waits for another.
 * @note A give holds its cell busy and reads the two cells beside it, the one above at the cell's end
 *       and the one below through the tail just ahead of the cell, and merges with each that reads
 *       free. A cell at a tier's boundary then goes back to the gap by one swap on the tiers word,
 *       marked dead, and any other goes free. A tier has one moving bit, and the context whose swap
 *       takes it is the one that merges, returns, grows or fits; a give that finds it held frees its
 *       cell and has the holder read the boundary again, and nothing waits on it. No byte outside the
 *       tiers reads as a free cell. A persistent take the gap cannot meet takes a free
 *       cell that fits, split where the remainder holds another cell.
 * @note A take lays its words after the swap that gave it the bytes, by a swap from the word it reads
 *       there once they are its own, the word ahead of the payload last. A header another context has
 *       laid since is refused and never written over.
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

/** @brief The state of a cell returned to the gap or merged into its neighbor. */
#define CARCER_DEAD 2u

/** @brief The bit a context holds while it moves a tier's memory. */
#define CARCER_MOVING ((embed_atomic_usize_value)1u)

/** @brief The bit a give sets for the holder of the moving bit to read the boundary again. */
#define CARCER_MOVING_AGAIN ((embed_atomic_usize_value)2u)

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
 * @brief Offset of a cell's tail, behind its owner's word: what carcer_tail_word makes of the cell's own
 *        offset, which the cell above reads to reach it.
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
 * @brief The tail of the cell at off.
 *
 * @param[in] off The cell's offset.
 * @return        The offset in a cell's word's shape, in the dead state.
 * @note A later cell can lay its word ahead on bytes that held a tail, and those bytes are read before
 *       the take's words stand. An offset as it is reads as a free word, its low bits being zero, with a
 *       size a cell could have. In this shape it reads dead.
 */
EMBED_INLINE embed_atomic_usize_value carcer_tail_word(size_t off)
{
    return carcer_word(off, CARCER_DEAD);
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
 * @return                  The cell, or NULL when the gap cannot meet it [RETURNS OWNERSHIP].
 * @note Both tiers reach this. The tiers word is read, the room between the two boundaries is tested,
 *       and the one boundary moves by one swap on the tiers word read. A swap that fails is a word
 *       another context moved, and the test runs again over what it holds now.
 * @note The cell's words are laid once the swap has landed, each read first and then swapped from what
 *       the read found: the tail first, the cell's offset, then the owner's word, and the word ahead
 *       last, the size in the
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
EMBED_INLINE void *carcer_grow(CarcerCellBlock *cellblock, size_t want, embed_bool down, CellOwner *owner)
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
    // Explicit cast carries the owner's address as the integer the owner's word holds
    const embed_atomic_usize_value named = (embed_atomic_usize_value)(uintptr_t)owner;
    const embed_atomic_usize_value tail_word = carcer_tail_word(off);

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
 * @brief The half-open offset range one tier occupies.
 *
 * @note lo and hi are offsets from the cellblock's base, and both tiers read upward from lo.
 */
typedef struct
{
    size_t lo; /**< Offset of the first cell in the tier. */
    size_t hi; /**< One past the last byte the tier occupies. */
} CarcerTier;

/**
 * @brief Reads a tier's range off the tiers word.
 *
 * @param[in] cellblock  Cellblock the tier is in [BORROWS].
 * @param[in] persistent EMBED_TRUE for the persistent tier.
 * @return               The tier as the word stands.
 */
EMBED_INLINE CarcerTier carcer_tier_read(const CarcerCellBlock *cellblock, embed_bool persistent)
{
    const embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);
    const CarcerTier tier = {persistent ? 0u : mmgr_tiers_temporary_top(tiers),
                             persistent ? mmgr_tiers_persistent_end(tiers) : cellblock->size};

    return tier;
}

/**
 * @brief Reads the word at off and says whether a free cell stands there.
 *
 * @param[in]  cellblock Cellblock to read [BORROWS].
 * @param[in]  off       The offset, a multiple of MMGR_CARCER_ALIGN.
 * @param[in]  bound     The offset the cell must end at or below.
 * @param[out] word      The word read [BORROWS].
 * @return               EMBED_TRUE where the word reads free, its size is a whole number of words that
 *                       ends at or below bound, and the tail behind that size names off.
 * @warning Bytes no take has laid yet, which read as a free word and whose tail names their own offset,
 *          are taken as a cell.
 */
EMBED_INLINE embed_bool carcer_free_read(const CarcerCellBlock *cellblock, size_t off, size_t bound,
                                         embed_atomic_usize_value *word)
{
    if (off >= bound)
    {
        return EMBED_FALSE;
    }
    *word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
    const size_t size = carcer_word_size(*word);

    return (embed_bool)((carcer_word_state(*word) == CARCER_FREE) && (size >= MMGR_CARCER_ALIGN) && (size <= bound) &&
                        ((size & (MMGR_CARCER_ALIGN - 1u)) == 0u) && (carcer_end_of(off, size) <= bound) &&
                        (embed_atomic_usize_load(carcer_word_at(cellblock, carcer_tail_of(off, size))) == carcer_tail_word(off)));
}

/**
 * @brief Merges a held cell with the free cells beside it, each read once.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     tier      The cell's tier.
 * @param[in,out] at        The cell's offset, moved to the lower cell's where that one merges [BORROWS].
 * @param[in,out] size      The cell's payload size, raised by what merges [BORROWS].
 * @note The give holds the tier's moving bit and the cell busy through this, and no other context
 *       touches its word. The word
 *       ahead of the cell above and the tail ahead of this cell's word are words while it stands: the
 *       boundary at either end of a held cell does not move, and whatever is laid against it lays its
 *       word or its tail there.
 * @note The cell above goes from free to dead and the cell below from free to busy, one swap each, and
 *       the merged cell is laid behind that swap: its tail, then its word. A swap that does not land is
 *       another context's take or merge, and that side stays as it is.
 * @note The owner word behind the merged cell reads none, as every free cell's does.
 * @warning The tail names the cell below, and a take can have reused that cell's bytes in the instant
 *          between the two reads. carcer_free_read carries what that leaves.
 */
EMBED_INLINE void carcer_coalesce(const CarcerCellBlock *cellblock, CarcerTier tier, size_t *at, size_t *size)
{
    const size_t above = carcer_end_of(*at, *size);
    embed_atomic_usize_value word;

    if ((above < tier.hi) && carcer_free_read(cellblock, above, tier.hi, &word))
    {
        const size_t above_size = carcer_word_size(word);

        if (carcer_swapped(cellblock, above, word, carcer_word(above_size, CARCER_DEAD)))
        {
            embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(above, above_size)), carcer_tail_word(*at));
            embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(*at, *size)), 0u);
            *size += CARCER_HDR + above_size;
            embed_atomic_usize_store(carcer_word_at(cellblock, *at), carcer_word(*size, CARCER_BUSY));
        }
    }
    if (*at <= tier.lo)
    {
        return;
    }
    const embed_atomic_usize_value tail = embed_atomic_usize_load(carcer_word_at(cellblock, *at - CARCER_WORD));
    const size_t below = carcer_word_size(tail);

    if ((carcer_word_state(tail) != CARCER_DEAD) || (below < tier.lo) || (below >= *at) ||
        ((below & (MMGR_CARCER_ALIGN - 1u)) != 0u) || !carcer_free_read(cellblock, below, *at, &word) ||
        (carcer_end_of(below, carcer_word_size(word)) != *at) ||
        !carcer_swapped(cellblock, below, word, carcer_word(carcer_word_size(word), CARCER_BUSY)))
    {
        return;
    }
    const size_t merged = carcer_word_size(word) + CARCER_HDR + *size;

    embed_atomic_usize_store(carcer_word_at(cellblock, *at - CARCER_WORD), 0u);
    embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(*at, *size)), carcer_tail_word(below));
    embed_atomic_usize_store(carcer_word_at(cellblock, *at), carcer_word(*size, CARCER_DEAD));
    embed_atomic_usize_store(carcer_word_at(cellblock, below), carcer_word(merged, CARCER_BUSY));
    *at = below;
    *size = merged;
}

/**
 * @brief Says whether a tier's boundary stands at a cell.
 *
 * @param[in] tiers      The tiers word.
 * @param[in] persistent EMBED_TRUE for the persistent tier.
 * @param[in] at         The cell's offset.
 * @param[in] end        The offset just past it.
 * @return               EMBED_TRUE where the persistent end is at the cell's end, or the temporary top at
 *                       the cell.
 */
EMBED_INLINE embed_bool carcer_at_boundary(embed_atomic_usize_value tiers, embed_bool persistent, size_t at, size_t end)
{
    return (embed_bool)(persistent ? (mmgr_tiers_persistent_end(tiers) == end) : (mmgr_tiers_temporary_top(tiers) == at));
}

/**
 * @brief Returns a held cell at its tier's boundary to the gap, and frees it where it does not stand there.
 *
 * @param[in,out] cellblock  Cellblock the cell sits in [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier.
 * @param[in]     off        The cell's offset, its word busy.
 * @param[in]     size       Its payload size.
 * @note The context holds the tier's moving bit, and no other context moves this tier's memory. At the
 *       boundary the word goes dead and the boundary moves past the cell by one swap on the tiers word.
 *       Where a take has moved that boundary, or the cell does not stand there, the word goes free.
 * @note The next cell is read once the boundary stands on it, and returned too where it reads free, as a
 *       cell freed while another context held the bit does. It is read with the bit held, and it stands
 *       where it stood: a take lays its cell on the gap's side of the boundary.
 */
EMBED_INLINE void carcer_returned(CarcerCellBlock *cellblock, embed_bool persistent, size_t off, size_t size)
{
    size_t at = off;
    size_t held = size;

    for (;;)
    {
        const size_t end = carcer_end_of(at, held);
        embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);
        embed_atomic_usize_value state = CARCER_BUSY;
        embed_bool returned = EMBED_FALSE;

        if (carcer_at_boundary(tiers, persistent, at, end))
        {
            state = CARCER_DEAD;
            embed_atomic_usize_store(carcer_word_at(cellblock, at), carcer_word(held, CARCER_DEAD));
            embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(at, held)), 0u);
            while (!returned && carcer_at_boundary(tiers, persistent, at, end))
            {
                returned = embed_atomic_usize_swap_if(&cellblock->tiers, &tiers,
                                                      persistent ? MMGR_CARCER_TIERS(at, mmgr_tiers_temporary_top(tiers))
                                                                 : MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers), end));
            }
        }
        if (!returned)
        {
            embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(at, held)), carcer_tail_word(at));
            (void)carcer_swapped(cellblock, at, carcer_word(held, state), carcer_word(held, CARCER_FREE));
            return;
        }
        size_t next = end;

        if (persistent)
        {
            if (at < (CARCER_HDR + MMGR_CARCER_ALIGN))
            {
                return;
            }
            const embed_atomic_usize_value tail = embed_atomic_usize_load(carcer_word_at(cellblock, at - CARCER_WORD));

            next = carcer_word_size(tail);
            if ((carcer_word_state(tail) != CARCER_DEAD) || (next >= at) || ((next & (MMGR_CARCER_ALIGN - 1u)) != 0u))
            {
                return;
            }
        }
        embed_atomic_usize_value word;

        if (!carcer_free_read(cellblock, next, persistent ? at : cellblock->size, &word) ||
            (persistent && (carcer_end_of(next, carcer_word_size(word)) != at)) ||
            !carcer_swapped(cellblock, next, word, carcer_word(carcer_word_size(word), CARCER_BUSY)))
        {
            return;
        }
        at = next;
        held = carcer_word_size(word);
    }
}

/**
 * @brief Lays the cell a fit has claimed: split where the remainder holds another cell, its owner's word,
 *        its tail and its word, and counted.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     off       Offset of the cell, whose word reads busy with size.
 * @param[in]     size      Payload the cell carries.
 * @param[in]     want      Payload the take asked for, already rounded.
 * @param[in,out] owner     The registered owner the cell is counted to [BORROWS].
 * @return                  The cell [RETURNS OWNERSHIP].
 * @note Only splits when the remainder can carry its own words and a payload. Otherwise the prisoner
 *       keeps the whole cell and its slack. The remainder's word is laid ahead of the tail that names
 *       it, and its owner word reads none as the free cell's did.
 */
EMBED_INLINE void *carcer_split(CarcerCellBlock *cellblock, size_t off, size_t size, size_t want, CellOwner *owner)
{
    // Split only when the tail left over can carry its own words and a whole word behind them
    const embed_bool splits = (embed_bool)(size >= (want + CARCER_HDR + MMGR_CARCER_ALIGN));
    const size_t kept = splits ? want : size;

    if (splits)
    {
        const size_t remainder = carcer_end_of(off, want);
        const size_t remainder_size = size - want - CARCER_HDR;

        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_foot_of(remainder, remainder_size)), 0u);
        embed_atomic_usize_store(carcer_word_at(cellblock, remainder), carcer_word(remainder_size, CARCER_FREE));
        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(remainder, remainder_size)),
                                 carcer_tail_word(remainder));
    }
    // Explicit cast carries the owner's address as the integer the owner's word holds
    embed_atomic_usize_store(carcer_word_at(cellblock, carcer_foot_of(off, kept)),
                             (embed_atomic_usize_value)(uintptr_t)owner);
    embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(off, kept)), carcer_tail_word(off));
    embed_atomic_usize_store(carcer_word_at(cellblock, off), carcer_word(kept, CARCER_BUSY));
    carcer_owner_took(owner, kept);
    return cellblock->base + off + CARCER_WORD;
}

/**
 * @brief A tier's moving bit.
 *
 * @param[in,out] cellblock  Cellblock the tier is in [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier.
 * @return                   The word that holds it [BORROWS].
 */
EMBED_INLINE embed_atomic_usize *carcer_moving(CarcerCellBlock *cellblock, embed_bool persistent)
{
    return persistent ? &cellblock->persistent_moving : &cellblock->temporary_moving;
}

/**
 * @brief Takes a tier's moving bit where no context holds it.
 *
 * @param[in,out] moving The tier's word [BORROWS].
 * @return               EMBED_TRUE where this context holds the bit. Nothing waits here.
 */
EMBED_INLINE embed_bool carcer_moving_taken(embed_atomic_usize *moving)
{
    embed_atomic_usize_value found = 0u;

    return embed_atomic_usize_swap_if(moving, &found, CARCER_MOVING);
}

/**
 * @brief Takes a tier's moving bit, or has the context that holds it read the boundary again.
 *
 * @param[in,out] moving The tier's word [BORROWS].
 * @return               EMBED_TRUE where this context holds the bit. EMBED_FALSE where another holds it,
 *                       with CARCER_MOVING_AGAIN set for that context to read.
 * @note Nothing waits here. Each repeat of the loop is a swap another context's swap beat.
 */
EMBED_INLINE embed_bool carcer_moving_asked(embed_atomic_usize *moving)
{
    embed_atomic_usize_value found = embed_atomic_usize_load(moving);

    for (;;)
    {
        const embed_atomic_usize_value wanted = (found == 0u) ? CARCER_MOVING : (found | CARCER_MOVING_AGAIN);

        if (embed_atomic_usize_swap_if(moving, &found, wanted))
        {
            return (embed_bool)(wanted == CARCER_MOVING);
        }
    }
}

/**
 * @brief Returns to the gap what stands free at a tier's boundary.
 *
 * @param[in,out] cellblock  Cellblock the tier is in [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier.
 * @note The context holds the tier's moving bit.
 */
EMBED_INLINE void carcer_boundary_swept(CarcerCellBlock *cellblock, embed_bool persistent)
{
    const CarcerTier tier = carcer_tier_read(cellblock, persistent);
    size_t at = tier.lo;
    embed_atomic_usize_value word;

    if (persistent)
    {
        if (tier.hi < (CARCER_HDR + MMGR_CARCER_ALIGN))
        {
            return;
        }
        const embed_atomic_usize_value tail = embed_atomic_usize_load(carcer_word_at(cellblock, tier.hi - CARCER_WORD));

        at = carcer_word_size(tail);
        if ((carcer_word_state(tail) != CARCER_DEAD) || (at >= tier.hi) || ((at & (MMGR_CARCER_ALIGN - 1u)) != 0u))
        {
            return;
        }
    }
    if (carcer_free_read(cellblock, at, tier.hi, &word) &&
        (!persistent || (carcer_end_of(at, carcer_word_size(word)) == tier.hi)) &&
        carcer_swapped(cellblock, at, word, carcer_word(carcer_word_size(word), CARCER_BUSY)))
    {
        carcer_returned(cellblock, persistent, at, carcer_word_size(word));
    }
}

/**
 * @brief Lets a tier's moving bit go, reading the boundary again first for each give that found it held.
 *
 * @param[in,out] cellblock  Cellblock the tier is in [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier.
 */
EMBED_INLINE void carcer_moving_let_go(CarcerCellBlock *cellblock, embed_bool persistent)
{
    embed_atomic_usize *const moving = carcer_moving(cellblock, persistent);

    for (;;)
    {
        embed_atomic_usize_value held = CARCER_MOVING;

        if (embed_atomic_usize_swap_if(moving, &held, 0u))
        {
            return;
        }
        embed_atomic_usize_store(moving, CARCER_MOVING);
        carcer_boundary_swept(cellblock, persistent);
    }
}

/**
 * @brief Finds a free cell in the persistent tier large enough for want and takes it, atomically against
 *        everything that moves the tier's memory.
 *
 * @param[in,out] cellblock Cellblock to search [BORROWS].
 * @param[in]     want      Payload wanted, already rounded.
 * @param[in,out] owner     The registered owner the cell is counted to [BORROWS].
 * @return                  The cell, or NULL when no cell fits or another context holds the tier's moving
 *                          bit [RETURNS OWNERSHIP].
 * @note Reached only where the gap cannot meet the take, and the take would otherwise be refused. First
 *       fit: a best fit would read the whole tier to save slack the split already recovers.
 * @note With the bit held no cell in the tier moves: a give frees its cell and merges nothing, and a
 *       growth is refused. A take out of the gap goes on, landing past the tier as it stands, and a word
 *       it has not laid yet, dead or no cell at all, ends the search.
 * @warning A tier holding many cells costs their number, in the case the gap is spent.
 */
EMBED_INLINE void *carcer_fit(CarcerCellBlock *cellblock, size_t want, CellOwner *owner)
{
    if (!carcer_moving_taken(&cellblock->persistent_moving))
    {
        return NULL;
    }
    const CarcerTier tier = carcer_tier_read(cellblock, EMBED_TRUE);
    size_t off = tier.lo;
    void *prisoner = NULL;

    while ((off < tier.hi) && (prisoner == NULL))
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(cellblock, off));
        const size_t size = carcer_word_size(word);
        embed_atomic_usize_value found;

        if ((carcer_word_state(word) == CARCER_DEAD) || (size < MMGR_CARCER_ALIGN) || (size > tier.hi) ||
            ((size & (MMGR_CARCER_ALIGN - 1u)) != 0u) || (carcer_end_of(off, size) > tier.hi))
        {
            break;
        }
        // A cell fits only on both counts: free, and holding at least the payload asked for
        if ((size >= want) && carcer_free_read(cellblock, off, tier.hi, &found) && (found == word) &&
            carcer_swapped(cellblock, off, word, carcer_word(size, CARCER_BUSY)))
        {
            prisoner = carcer_split(cellblock, off, size, want, owner);
        }
        off = carcer_end_of(off, size);
    }
    carcer_moving_let_go(cellblock, EMBED_TRUE);
    return prisoner;
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
 * @note A temporary cell moves down by the growth. Its word is written at the new top first, its tail
 *       then names the new offset, its payload moved down with memor.move_down, and the owner's word behind the payload stands where
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
        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(off, carried)), 0u);
        embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(off, want)), carcer_tail_word(off));
        // Explicit cast carries the owner's address as the integer its word holds
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
    embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(at, want)), carcer_tail_word(at));
    memor.move_down(&(MemoriaCfg){
        .dst = cellblock->base + at + CARCER_WORD, .src = cellblock->base + off + CARCER_WORD, .bytes = carried});
    if (zeroing)
    {
        mmgr_zero_buf(cellblock->base + at + CARCER_WORD + carried, extra);
    }
    const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(cellblock, carcer_foot_of(at, want)));

    // A word read as none is a cell a rewind has dropped meanwhile, and the top read below finds it
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
 * @brief The take both tiers and both security levels share: the owner tested, then the take out of the
 *        gap, and on the persistent tier a free cell looked for where the gap cannot meet it.
 *
 * @param[in,out] cellblock Cellblock to take from [BORROWS].
 * @param[in]     size      Bytes wanted.
 * @param[in,out] owner     The owner named [BORROWS].
 * @param[in]     down      EMBED_TRUE for the tier that grows down.
 * @return                  The cell, or NULL [RETURNS OWNERSHIP].
 */
EMBED_INLINE void *carcer_take(CarcerCellBlock *cellblock, size_t size, CellOwner *owner, embed_bool down)
{
    if (!carcer_owner_registered(owner))
    {
        return NULL;
    }
    const size_t want = carcer_want(size);
    void *const prisoner = carcer_grow(cellblock, want, down, owner);

    return ((prisoner != NULL) || down) ? prisoner : carcer_fit(cellblock, want, owner);
}

void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_take(cellblock, size, owner, EMBED_FALSE);
}

void *mmgr_persistent_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_take(cellblock, size, owner, EMBED_FALSE);
}

void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_take(cellblock, size, owner, EMBED_TRUE);
}

void *mmgr_temporary_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    return carcer_take(cellblock, size, owner, EMBED_TRUE);
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
 * @note A word that names none is a cell given back already or dropped by a rewind, and nothing is
 *       told. A word naming another owner is told to the port through
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
 *        where asked, the owner's word claimed, the cell merged with its free neighbors, then returned
 *        to the gap or freed.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner  First byte of the cell [TAKES OWNERSHIP].
 * @param[in]     owner     The owner named [BORROWS].
 * @param[in]     zeroing   Whether the payload is zeroed before the give.
 * @note The cell's bytes come off the owner its owner word names, claimed by one swap of that word to
 *       none: a rewind dropping a temporary cell in the same instant claims it
 *       the same way, and the bytes come off once between them. A give naming another owner changes
 *       nothing, the payload included.
 * @note The tier is read off the address ahead of the give, while the cell is busy and no give can move
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
    // A word that is not busy is a cell given back already, or dropped, and the give ends here
    if (carcer_word_state(embed_atomic_usize_load(carcer_word_at(cellblock, off))) != CARCER_BUSY)
    {
        return;
    }
    embed_atomic_usize *const moving = carcer_moving(cellblock, persistent);

    if (carcer_moving_taken(moving))
    {
        size_t at = off;
        size_t held = size;

        carcer_coalesce(cellblock, carcer_tier_read(cellblock, persistent), &at, &held);
        carcer_returned(cellblock, persistent, at, held);
        carcer_moving_let_go(cellblock, persistent);
        return;
    }
    // Another context is moving this tier's memory: the cell goes free and nothing moves. The bit is asked for
    // after the cell is free, and whichever context reads the boundary next finds it free
    (void)carcer_swapped(cellblock, off, carcer_word(size, CARCER_BUSY), carcer_word(size, CARCER_FREE));
    if (carcer_moving_asked(moving))
    {
        carcer_boundary_swept(cellblock, persistent);
        carcer_moving_let_go(cellblock, persistent);
    }
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
    // A growth moves its tier's boundary and the cell's words: refused while another context moves this tier's memory
    const embed_bool persistent = (embed_bool)(off < mmgr_cellblock_persistent_end(cellblock));

    if (!carcer_moving_taken(carcer_moving(cellblock, persistent)))
    {
        return NULL;
    }
    void *const grown = carcer_grown(cellblock, off, held, want, owner, zeroing);

    carcer_moving_let_go(cellblock, persistent);
    return grown;
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
