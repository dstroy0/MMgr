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
 *       takes it merges, returns, grows or fits; a give that finds it taken frees its
 *       cell and has the holder read the boundary again, and nothing waits on it. No byte outside the
 *       tiers reads as a free cell. A take the gap cannot meet takes a free cell of its own tier
 *       that fits, split where the remainder holds another cell.
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
 * @note A later cell can lay its word ahead on bytes that carried a tail, and those bytes are read before
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

/**
 * @brief Counts a cell's payload to its owner and to the cellblock's running total.
 *
 * @param[in,out] cellblock Cellblock the cell is in [BORROWS].
 * @param[in,out] owner     The owner [BORROWS].
 * @param[in]     bytes     The cell's payload bytes.
 */
EMBED_INLINE void carcer_cell_counted(CarcerCellBlock *cellblock, CellOwner *owner, size_t bytes)
{
    carcer_owner_took(owner, bytes);
    (void)embed_atomic_usize_fetch_add(&cellblock->in_use, bytes);
}

/** @brief The owner queue: every registered owner, as the integer of its address, in the order it registered. */
static embed_atomic_uptr s_owner_queue[MMGR_CARCER_OWNERS] = {EMBED_ATOMIC_INIT(0u)};

/** @brief The owners in the queue, at most MMGR_CARCER_OWNERS. */
static embed_atomic_usize s_owner_count = EMBED_ATOMIC_INIT(0u);

/** @brief 1 while one context registers an owner, which the others wait out; 0 otherwise. */
static embed_atomic_usize s_owner_registering = EMBED_ATOMIC_INIT(0u);

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
EMBED_INLINE void carcer_foot_claimed(CarcerCellBlock *cellblock, size_t off, size_t size)
{
    const size_t foot = carcer_foot_of(off, size);
    const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(cellblock, foot));

    if ((named != 0u) && carcer_swapped(cellblock, foot, named, 0u))
    {
        // Explicit casts read the owner word's integer back as the address it holds
        carcer_owner_gave((CellOwner *)(uintptr_t)named, size);
        (void)embed_atomic_usize_fetch_sub(&cellblock->in_use, size);
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
            (carcer_end_of(off, size) > bound) || !carcer_swapped(cellblock, off, word, carcer_word(size, CARCER_DEAD)))
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

    if (!carcer_swapped(cellblock, tail, found_tail, tail_word) ||
        !carcer_swapped(cellblock, foot, found_foot, named) ||
        !carcer_swapped(cellblock, off, found_word, carcer_word(want, CARCER_BUSY)))
    {
        return NULL;
    }
    carcer_cell_counted(cellblock, owner, want);
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
                        (embed_atomic_usize_load(carcer_word_at(cellblock, carcer_tail_of(off, size))) ==
                         carcer_tail_word(off)));
}

/**
 * @brief Merges a busy cell with the free cells beside it, each read once.
 *
 * @param[in,out] cellblock Cellblock the cell sits in [BORROWS].
 * @param[in]     tier      The cell's tier.
 * @param[in,out] at        The cell's offset, moved to the lower cell's where that one merges [BORROWS].
 * @param[in,out] size      The cell's payload size, raised by what merges [BORROWS].
 * @note The give holds the tier's moving bit and the cell busy through this, and no other context
 *       touches its word. The word
 *       ahead of the cell above and the tail ahead of this cell's word are words while it stands: the
 *       boundary at either end of a busy cell does not move, and whatever is laid against it lays its
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
            embed_atomic_usize_store(carcer_word_at(cellblock, carcer_tail_of(above, above_size)),
                                     carcer_tail_word(*at));
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
    return (embed_bool)(persistent ? (mmgr_tiers_persistent_end(tiers) == end)
                                   : (mmgr_tiers_temporary_top(tiers) == at));
}

/**
 * @brief Returns a busy cell at its tier's boundary to the gap, and frees it where it does not stand there.
 *
 * @param[in,out] cellblock  Cellblock the cell sits in [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier.
 * @param[in]     off        The cell's offset, its word busy.
 * @param[in]     size       Its payload size.
 * @note The context holds the tier's moving bit, and no other context moves this tier's memory. At the
 *       boundary the word goes dead and the boundary moves past the cell by one swap on the tiers word.
 *       Where a take has moved that boundary, or the cell does not stand there, the word goes free.
 * @note The next cell is read once the boundary stands on it, and returned too where it reads free, as a
 *       cell freed while another context had the bit does. It is read with the bit taken, and it stands
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
                returned =
                    embed_atomic_usize_swap_if(&cellblock->tiers, &tiers,
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
    carcer_cell_counted(cellblock, owner, kept);
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
 * @brief Lets a tier's moving bit go, reading the boundary again first for each give that found it taken.
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
 * @brief Finds a free cell in one tier large enough for want and takes it, atomically against
 *        everything that moves the tier's memory.
 *
 * @param[in,out] cellblock  Cellblock to search [BORROWS].
 * @param[in]     want       Payload wanted, already rounded.
 * @param[in,out] owner      The registered owner the cell is counted to [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier, EMBED_FALSE for the temporary one.
 * @param[in]     below      The offset a cell found lies below: the cellblock's size where any will do.
 * @return                  The cell, or NULL when no cell fits [RETURNS OWNERSHIP].
 * @pre The context holds the tier's moving bit, which this lets go.
 * @note Reached only where the gap cannot meet the take, and the take would otherwise be refused. First
 *       fit: a best fit would read the whole tier to save slack the split already recovers.
 * @note With the bit taken no cell in the tier moves: a give frees its cell and merges nothing, and a
 *       growth is refused. A take out of the gap goes on, landing past the tier as it stands, and a word
 *       it has not laid yet, dead or no cell at all, ends the search.
 * @note A temporary cell is read against the top once it is counted, as a take out of the gap is: a top
 *       above the cell is a rewind that dropped it, and the take answers NULL.
 * @warning A tier holding many cells costs their number, in the case the gap is spent.
 */
EMBED_INLINE void *carcer_fit_held(CarcerCellBlock *cellblock, size_t want, CellOwner *owner, embed_bool persistent,
                                   size_t below)
{
    const CarcerTier tier = carcer_tier_read(cellblock, persistent);
    size_t off = tier.lo;
    void *prisoner = NULL;

    while ((off < tier.hi) && (off < below) && (prisoner == NULL))
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
    carcer_moving_let_go(cellblock, persistent);
    if (!persistent && (prisoner != NULL))
    {
        const size_t taken = carcer_off_of(cellblock, prisoner);
        const size_t top = mmgr_cellblock_temporary_top(cellblock);

        if (top > taken)
        {
            carcer_dropped(cellblock, taken, top);
            return NULL;
        }
    }
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
 *       then names the new offset, its payload moved down with memor.move_down, and the owner's word behind the payload
 * stands where it stood. The bytes between the moved payload and that word are the old word and payload, zeroed where
 * zeroing is set.
 * @note A temporary growth reads the top once more, as a take does: a top above the grown cell is a
 *       rewind that dropped it meanwhile, and the growth drops what the rewind left and answers NULL.
 */
EMBED_INLINE void *carcer_grown(CarcerCellBlock *cellblock, size_t off, size_t carried, size_t want, CellOwner *owner,
                                embed_bool zeroing)
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
        carcer_cell_counted(cellblock, owner, extra);
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
        carcer_cell_counted(cellblock, (CellOwner *)(uintptr_t)named, extra);
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

/** @brief The bit of a slot's word that stands while the cell moves or is given back. */
#define CARCER_SLOT_MOVING ((embed_atomic_usize_value)1u)

/** @brief What a turnstile's first word reads while a take or a give passes through it. */
#define CARCER_TURNSTILE_PASSING ((embed_atomic_uptr_value)1u)

/** @brief What a turnstile's first word reads while the mover or a reader of the rows has it locked. */
#define CARCER_TURNSTILE_LOCKED ((embed_atomic_uptr_value)2u)

/** @brief The key of the row that sums every thread past the others. */
#define CARCER_ROW_OTHERS (~(embed_atomic_usize_value)0u)

/**
 * @brief One movable cell's place in the translation table: the mutable word its const identity points at.
 *
 * @note word is the offset of the cell's payload in the cellblock with the moving bit in its low bit, zero
 *       where the slot is free. A payload is aligned past that bit, and none lies at offset zero, since a
 *       cell's word stands ahead of it.
 */
struct CarcerMovableSlot
{
    embed_atomic_usize word; /**< Where the cell stands, and the moving bit. */
};

/**
 * @brief One way into the cellblock, on a cache line of its own.
 *
 * @note inside is the first thing a worker's call writes: the slot it is reaching,
 *       CARCER_TURNSTILE_PASSING for a take or a give, or CARCER_TURNSTILE_LOCKED, and zero where free. Each
 *       is unmistakable, since a slot's address is aligned past both values. other is the second cell of a
 *       call that reaches two.
 * @note A worker writes its turnstile, orders that write ahead of its reads and then reads the slot; the
 *       mover writes the slot's moving bit, orders that ahead of its reads and then reads the turnstiles.
 *       Whichever runs second sees the other's write, and so a cell never moves under a worker.
 * @note id and touches are plain: the occupant is their only writer, and the swap and store on inside order
 *       them from one occupant to the next.
 */
typedef struct
{
    EMBED_ALIGN(MMGR_MOVABLE_TURNSTILE_BYTES) embed_atomic_uptr inside; /**< What the turnstile holds now. */
    embed_atomic_uptr other;                                            /**< A second cell's slot, or zero. */
    size_t id;                                                          /**< The thread touches counts for. */
    size_t touches;                                                     /**< Its touches not yet summed. */
} CarcerTurnstile;

EMBED_STATIC_ASSERT(sizeof(CarcerTurnstile) == MMGR_MOVABLE_TURNSTILE_BYTES,
                    "CarcerTurnstile is not the MMGR_MOVABLE_TURNSTILE_BYTES it is declared to take");

/** @brief One thread's touches, summed after the fact. */
typedef struct
{
    embed_atomic_usize key;     /**< The thread's id plus one, CARCER_ROW_OTHERS for the last, zero where free. */
    embed_atomic_usize touches; /**< The touches summed to it. */
} CarcerTouchRow;

EMBED_STATIC_ASSERT(sizeof(CarcerTouchRow) == (2u * sizeof(embed_atomic_usize)),
                    "CarcerTouchRow is not the two words MMGR_MOVABLE_STORAGE_BYTES counts");
EMBED_STATIC_ASSERT(sizeof(CarcerMovableSlot) == sizeof(embed_atomic_usize),
                    "CarcerMovableSlot is not the one word MMGR_MOVABLE_STORAGE_BYTES counts");

/**
 * @brief The movable option's state, at the start of the storage the program gave.
 *
 * @note queue_next and queue_serving are the line behind the turnstiles: a context takes the next ticket and
 *       waits until it is served, and their difference is the depth the mover reads as it locks turnstiles.
 * @note passed_by counts the takes and gives that found every turnstile busy and went on. A take never waits
 *       on a turnstile, since a hook a take calls can take again from inside one.
 */
typedef struct
{
    CarcerMovableSlot *slot;             /**< The table. */
    size_t cells;                        /**< Slots at slot. */
    embed_atomic_usize named;            /**< One past the highest slot named: the mover reads no further. */
    embed_atomic_usize lowest;           /**< The slot a take's search starts at. */
    CarcerTurnstile *turnstile;          /**< The turnstiles. */
    size_t width;                        /**< Turnstiles at turnstile. */
    CarcerTouchRow *row;                 /**< The rows of touches. */
    size_t rows;                         /**< Rows at row. */
    embed_atomic_usize queue_next;       /**< The next ticket. */
    embed_atomic_usize queue_serving;    /**< The ticket being served. */
    embed_atomic_usize passed_by;        /**< Takes and gives that found no free turnstile. */
    embed_bool wipes;                    /**< Whether a cell left behind is zeroed first. */
    size_t (*thread_id)(void);           /**< The calling thread's id, where given. */
    embed_bool (*thread_wait)(uint32_t); /**< The mover's sleep, where given. */
    void (*thread_yield)(void);          /**< What a waiting context calls, where given. */
    void (*thread_moving)(embed_bool);   /**< Told as a move starts and ends, where given. */
} CarcerMovable;

EMBED_STATIC_ASSERT(sizeof(CarcerMovable) <= MMGR_MOVABLE_HEAD_BYTES,
                    "CarcerMovable has outgrown MMGR_MOVABLE_HEAD_BYTES");

/**
 * @brief A cellblock's movable option.
 *
 * @param[in] cellblock Cellblock to read [BORROWS].
 * @return              The option's state, or NULL without it.
 */
EMBED_INLINE CarcerMovable *carcer_movable_of(const CarcerCellBlock *cellblock)
{
    // Explicit casts read the word's integer back as the address it holds
    return (CarcerMovable *)(uintptr_t)embed_atomic_uptr_load(&cellblock->movable);
}

/**
 * @brief Gives up the processor while a context waits on another.
 *
 * @param[in] movable The option [BORROWS].
 */
EMBED_INLINE void carcer_yielded(const CarcerMovable *movable)
{
    if (movable->thread_yield != NULL)
    {
        movable->thread_yield();
    }
}

/**
 * @brief Moves a turnstile's unsummed touches to the thread's row.
 *
 * @param[in,out] movable   The option [BORROWS].
 * @param[in,out] turnstile A turnstile this context holds [BORROWS].
 * @note A row is a free one or the thread's own, searched from the first; the last sums every thread past
 *       the others. Reached when a turnstile changes threads and when the rows are read, and not per call.
 */
EMBED_INLINE void carcer_touches_summed(const CarcerMovable *movable, CarcerTurnstile *turnstile)
{
    if ((turnstile->touches == 0u) || (movable->rows == 0u))
    {
        turnstile->touches = 0u;
        return;
    }
    // Explicit cast keeps what of the id a word holds: a row is telemetry, and a narrow word folds ids together
    const embed_atomic_usize_value key = (embed_atomic_usize_value)(turnstile->id + 1u);
    size_t place = 0u;

    while ((place + 1u) < movable->rows)
    {
        embed_atomic_usize_value found = 0u;

        if (embed_atomic_usize_swap_if(&movable->row[place].key, &found, key) || (found == key))
        {
            break;
        }
        place += 1u;
    }
    if ((place + 1u) == movable->rows)
    {
        embed_atomic_usize_store(&movable->row[place].key, CARCER_ROW_OTHERS);
    }
    (void)embed_atomic_usize_fetch_add(&movable->row[place].touches, (embed_atomic_usize_value)turnstile->touches);
    turnstile->touches = 0u;
}

/**
 * @brief Counts one touch on a turnstile this context holds.
 *
 * @param[in]     movable   The option [BORROWS].
 * @param[in,out] turnstile The turnstile [BORROWS].
 * @param[in]     id        The calling thread's id.
 */
EMBED_INLINE void carcer_touch_counted(const CarcerMovable *movable, CarcerTurnstile *turnstile, size_t id)
{
    if (turnstile->id != id)
    {
        carcer_touches_summed(movable, turnstile);
        turnstile->id = id;
    }
    turnstile->touches += 1u;
}

/**
 * @brief The calling thread's id, and the turnstile its search starts at.
 *
 * @param[in]  movable The option [BORROWS].
 * @param[out] start   The turnstile to try first [BORROWS].
 * @return             The id, 0 where the program gave no thread_id.
 * @note An id starts a thread at a turnstile of its own wherever the ids spread, and so a thread mostly
 *       comes back to one cache line. System ids run in steps of a few, which the shift folds in.
 */
EMBED_INLINE size_t carcer_thread_seated(const CarcerMovable *movable, size_t *start)
{
    const size_t id = (movable->thread_id != NULL) ? movable->thread_id() : 0u;

    *start = (id + (id >> 2u)) % movable->width;
    return id;
}

/**
 * @brief Tries every turnstile once, from a start.
 *
 * @param[in,out] movable The option [BORROWS].
 * @param[in]     inside  What the turnstile holds once taken.
 * @param[in]     start   The turnstile to try first.
 * @return                The turnstile taken, or NULL where all are busy.
 */
EMBED_INLINE CarcerTurnstile *carcer_turnstile_tried(const CarcerMovable *movable, embed_atomic_uptr_value inside,
                                                     size_t start)
{
    for (size_t tried = 0u; tried < movable->width; tried += 1u)
    {
        const size_t reached = start + tried;
        CarcerTurnstile *const turnstile =
            &movable->turnstile[(reached < movable->width) ? reached : (reached - movable->width)];
        embed_atomic_uptr_value found = 0u;

        if (embed_atomic_uptr_swap_if(&turnstile->inside, &found, inside))
        {
            return turnstile;
        }
    }
    return NULL;
}

/**
 * @brief Takes a place in the line behind the turnstiles, and waits until it is served.
 *
 * @param[in,out] movable The option [BORROWS].
 */
EMBED_INLINE void carcer_queue_joined(CarcerMovable *movable)
{
    const embed_atomic_usize_value ticket = embed_atomic_usize_fetch_add(&movable->queue_next, 1u);

    while (embed_atomic_usize_load(&movable->queue_serving) != ticket)
    {
        carcer_yielded(movable);
    }
}

/**
 * @brief Leaves the head of the line, for the next ticket to be served.
 *
 * @param[in,out] movable The option [BORROWS].
 */
EMBED_INLINE void carcer_queue_left(CarcerMovable *movable)
{
    (void)embed_atomic_usize_fetch_add(&movable->queue_serving, 1u);
}

/**
 * @brief The contexts in the line now.
 *
 * @param[in] movable The option [BORROWS].
 * @return            The tickets taken and not yet finished.
 */
EMBED_INLINE size_t carcer_queue_depth(const CarcerMovable *movable)
{
    const embed_atomic_usize_value serving = embed_atomic_usize_load(&movable->queue_serving);

    // Explicit cast reads the difference, which the contexts waiting bound, as the count it is
    return (size_t)(embed_atomic_usize_load(&movable->queue_next) - serving);
}

/**
 * @brief Passes a take or a give through a turnstile, where the option is assigned and one is free.
 *
 * @param[in,out] movable The option, or NULL without it [BORROWS].
 * @return                The turnstile taken, or NULL where there is no option or none is free.
 * @note The turnstile is only what the mover reads as occupancy: no cell's safety rests on it, and a take
 *       never waits for one. One that finds none free counts in passed_by, which the mover reads as pressure.
 */
EMBED_INLINE CarcerTurnstile *carcer_passing(CarcerMovable *movable)
{
    if (movable == NULL)
    {
        return NULL;
    }
    size_t start = 0u;
    const size_t id = carcer_thread_seated(movable, &start);
    CarcerTurnstile *const turnstile = carcer_turnstile_tried(movable, CARCER_TURNSTILE_PASSING, start);

    if (turnstile != NULL)
    {
        carcer_touch_counted(movable, turnstile, id);
    }
    else
    {
        (void)embed_atomic_usize_fetch_add(&movable->passed_by, 1u);
    }
    return turnstile;
}

/**
 * @brief Lets go a turnstile carcer_passing took.
 *
 * @param[in,out] turnstile The turnstile, or NULL where none was taken [BORROWS].
 */
EMBED_INLINE void carcer_passed(CarcerTurnstile *turnstile)
{
    if (turnstile != NULL)
    {
        embed_atomic_uptr_store(&turnstile->inside, 0u);
    }
}

/**
 * @brief The first free cell that fits in one tier, the tier's moving bit taken for the read.
 *
 * @param[in,out] cellblock  Cellblock to search [BORROWS].
 * @param[in]     want       Payload wanted, already rounded.
 * @param[in,out] owner      The registered owner the cell is counted to [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier, EMBED_FALSE for the temporary one.
 * @param[in]     below      The offset a cell found lies below: the cellblock's size where any will do.
 * @return                   The cell, or NULL when no cell fits or another context holds the bit
 *                           [RETURNS OWNERSHIP].
 */
EMBED_INLINE void *carcer_fit(CarcerCellBlock *cellblock, size_t want, CellOwner *owner, embed_bool persistent,
                              size_t below)
{
    return carcer_moving_taken(carcer_moving(cellblock, persistent))
               ? carcer_fit_held(cellblock, want, owner, persistent, below)
               : NULL;
}

/**
 * @brief The first free cell that fits in one tier, for a take.
 *
 * @param[in,out] cellblock  Cellblock to search [BORROWS].
 * @param[in]     movable    The cellblock's option, or NULL where none is assigned [BORROWS].
 * @param[in]     want       Payload wanted, already rounded.
 * @param[in,out] owner      The registered owner the cell is counted to [BORROWS].
 * @param[in]     persistent EMBED_TRUE for the persistent tier, EMBED_FALSE for the temporary one.
 * @return                   The cell, or NULL when no cell fits [RETURNS OWNERSHIP].
 * @note The option says the cellblock serves threads, which can wait: the take yields until the bit is its own.
 *       With no option the context may be an interrupt over the bit's holder, and the take is refused at once.
 */
EMBED_INLINE void *carcer_fit_turned(CarcerCellBlock *cellblock, const CarcerMovable *movable, size_t want,
                                     CellOwner *owner, embed_bool persistent)
{
    while (!carcer_moving_taken(carcer_moving(cellblock, persistent)))
    {
        if (movable == NULL)
        {
            return NULL;
        }
        carcer_yielded(movable);
    }
    return carcer_fit_held(cellblock, want, owner, persistent, cellblock->size);
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
    if (!mmgr_owner_registered(owner))
    {
        return NULL;
    }
    const size_t want = carcer_want(size);
    void *const prisoner = carcer_grow(cellblock, want, down, owner);

    if (prisoner != NULL)
    {
        return prisoner;
    }
    const CarcerMovable *const movable = carcer_movable_of(cellblock);
    void *const fitted = carcer_fit_turned(cellblock, movable, want, owner, (embed_bool)!down);

    return ((fitted != NULL) || (movable == NULL)) ? fitted : carcer_fit_turned(cellblock, movable, want, owner, down);
}

void *mmgr_persistent_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));
    void *const prisoner = carcer_take(cellblock, size, owner, EMBED_FALSE);

    carcer_passed(turnstile);
    return prisoner;
}

void *mmgr_persistent_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));
    void *const prisoner = carcer_take(cellblock, size, owner, EMBED_FALSE);

    carcer_passed(turnstile);
    return prisoner;
}

void *mmgr_temporary_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));
    void *const prisoner = carcer_take(cellblock, size, owner, EMBED_TRUE);

    carcer_passed(turnstile);
    return prisoner;
}

void *mmgr_temporary_max_security_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));
    void *const prisoner = carcer_take(cellblock, size, owner, EMBED_TRUE);

    carcer_passed(turnstile);
    return prisoner;
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
EMBED_INLINE embed_bool carcer_owner_named(const CarcerCellBlock *cellblock, size_t off, size_t size,
                                           const CellOwner *owner)
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

/**
 * @brief A release as the guards make it: nothing to give reads nothing, and a cell goes past a turnstile.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from, unread for a NULL prisoner or owner [BORROWS].
 * @param[in]     prisoner  First byte of the cell, or NULL [TAKES OWNERSHIP].
 * @param[in]     owner     The owner named, or NULL [BORROWS].
 * @param[in]     zeroing   Whether the payload is zeroed before the give.
 * @note A host site with no block has a NULL cellblock, and a program gives back a cell it never took
 *       before it makes one.
 */
EMBED_INLINE void carcer_release_passed(CarcerCellBlock *cellblock, void *prisoner, const CellOwner *owner,
                                        embed_bool zeroing)
{
    if ((prisoner == NULL) || (owner == NULL))
    {
        return;
    }
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));

    carcer_given(cellblock, prisoner, owner, zeroing);
    carcer_passed(turnstile);
}

void mmgr_persistent_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    carcer_release_passed(cellblock, prisoner, owner, EMBED_FALSE);
}

void mmgr_persistent_max_security_buf_release(CarcerCellBlock *cellblock, void *prisoner, CellOwner *owner)
{
    carcer_release_passed(cellblock, prisoner, owner, EMBED_TRUE);
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
 *                          or NULL with nothing changed, as for an owner other than the cell's.
 */
EMBED_INLINE void *carcer_grow_asked(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner,
                                     embed_bool zeroing)
{
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

/**
 * @brief A growth as the guards make it: nothing to grow reads nothing, and a cell goes past a turnstile.
 *
 * @param[in,out] cellblock Cellblock the prisoner came from, unread for a NULL prisoner or owner [BORROWS].
 * @param[in,out] prisoner  First byte of the cell, or NULL [BORROWS].
 * @param[in]     size      Payload bytes the cell is to carry.
 * @param[in,out] owner     The owner the cell's owner word names, or NULL [BORROWS].
 * @param[in]     zeroing   Whether the bytes the growth adds are zeroed.
 * @return                  What carcer_grow_asked answers, and NULL for a NULL prisoner or owner.
 */
EMBED_INLINE void *carcer_grow_passed(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner,
                                      embed_bool zeroing)
{
    if ((prisoner == NULL) || (owner == NULL))
    {
        return NULL;
    }
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));
    void *const grown = carcer_grow_asked(cellblock, prisoner, size, owner, zeroing);

    carcer_passed(turnstile);
    return grown;
}

void *mmgr_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner)
{
    return carcer_grow_passed(cellblock, prisoner, size, owner, EMBED_FALSE);
}

void *mmgr_max_security_buf_grow(CarcerCellBlock *cellblock, void *prisoner, size_t size, CellOwner *owner)
{
    return carcer_grow_passed(cellblock, prisoner, size, owner, EMBED_TRUE);
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
        if (embed_atomic_usize_swap_if(&cellblock->tiers, &tiers,
                                       MMGR_CARCER_TIERS(mmgr_tiers_persistent_end(tiers), mark)))
        {
            return;
        }
    }
}

void mmgr_temporary_buf_release(CarcerCellBlock *cellblock, size_t mark)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));

    carcer_rewound(cellblock, mark, EMBED_FALSE);
    carcer_passed(turnstile);
}

void mmgr_max_security_buf_return(CarcerCellBlock *cellblock, size_t mark)
{
    CarcerTurnstile *const turnstile = carcer_passing(carcer_movable_of(cellblock));

    carcer_rewound(cellblock, mark, EMBED_TRUE);
    carcer_passed(turnstile);
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

/**
 * @brief Reads the slots a call reaches, once its turnstile names them, and lets the turnstile go if one moves.
 *
 * @param[in,out] turnstile  The turnstile, holding cell [BORROWS].
 * @param[in]     cell       The cell [BORROWS].
 * @param[in]     other_cell A second cell, or NULL [BORROWS].
 * @param[out]    word       The cell's slot word [BORROWS].
 * @param[out]    other_word The second cell's, where there is one [BORROWS].
 * @return                   EMBED_TRUE where neither is moving, the turnstile kept.
 */
EMBED_INLINE embed_bool carcer_cell_standing(CarcerTurnstile *turnstile, const CarcerMovableSlot *cell,
                                             const CarcerMovableSlot *other_cell, embed_atomic_usize_value *word,
                                             embed_atomic_usize_value *other_word)
{
    if (other_cell != NULL)
    {
        // Explicit casts carry the slot's address as the integer the turnstile's word holds
        embed_atomic_uptr_store(&turnstile->other, (embed_atomic_uptr_value)(uintptr_t)other_cell);
    }
    embed_atomic_ordered();
    const embed_atomic_usize_value read = embed_atomic_usize_load(&cell->word);
    const embed_atomic_usize_value other_read = (other_cell != NULL) ? embed_atomic_usize_load(&other_cell->word) : 0u;

    if (((read | other_read) & CARCER_SLOT_MOVING) != 0u)
    {
        embed_atomic_uptr_store(&turnstile->other, 0u);
        embed_atomic_uptr_store(&turnstile->inside, 0u);
        return EMBED_FALSE;
    }
    *word = read;
    if (other_cell != NULL)
    {
        *other_word = other_read;
    }
    return EMBED_TRUE;
}

/**
 * @brief Takes a turnstile for a call reaching one cell or two, and reads the slots once it is written.
 *
 * @param[in,out] movable    The option [BORROWS].
 * @param[in]     cell       The cell [BORROWS].
 * @param[in]     other_cell A second cell, or NULL [BORROWS].
 * @param[out]    word       The cell's slot word, its moving bit clear [BORROWS].
 * @param[out]    other_word The second cell's, where there is one [BORROWS].
 * @return                   The turnstile, written with both slots, neither of them moving.
 * @note With every turnstile busy, or a cell moving, the context waits in the line, and a move is one copy
 *       long.
 */
EMBED_INLINE CarcerTurnstile *carcer_cell_entered(CarcerMovable *movable, const CarcerMovableSlot *cell,
                                                  const CarcerMovableSlot *other_cell, embed_atomic_usize_value *word,
                                                  embed_atomic_usize_value *other_word)
{
    size_t start = 0u;
    const size_t id = carcer_thread_seated(movable, &start);
    // Explicit cast carries the slot's address as the integer the turnstile's word holds
    const embed_atomic_uptr_value named = (embed_atomic_uptr_value)(uintptr_t)cell;
    embed_bool queued = EMBED_FALSE;

    for (;;)
    {
        CarcerTurnstile *const turnstile = carcer_turnstile_tried(movable, named, start);

        if ((turnstile != NULL) && carcer_cell_standing(turnstile, cell, other_cell, word, other_word))
        {
            if (queued)
            {
                carcer_queue_left(movable);
            }
            carcer_touch_counted(movable, turnstile, id);
            return turnstile;
        }
        if (!queued)
        {
            carcer_queue_joined(movable);
            queued = EMBED_TRUE;
        }
        else
        {
            carcer_yielded(movable);
        }
    }
}

/**
 * @brief Lets go a turnstile carcer_cell_entered took.
 *
 * @param[in,out] turnstile The turnstile [BORROWS].
 */
EMBED_INLINE void carcer_cell_left(CarcerTurnstile *turnstile)
{
    embed_atomic_uptr_store(&turnstile->other, 0u);
    embed_atomic_uptr_store(&turnstile->inside, 0u);
}

/**
 * @brief Whether any turnstile names a slot.
 *
 * @param[in] movable The option [BORROWS].
 * @param[in] slot    The slot [BORROWS].
 * @return            EMBED_TRUE where a context is in the cell.
 */
EMBED_INLINE embed_bool carcer_slot_named(const CarcerMovable *movable, const CarcerMovableSlot *slot)
{
    // Explicit cast carries the slot's address as the integer the turnstile's words hold
    const embed_atomic_uptr_value named = (embed_atomic_uptr_value)(uintptr_t)slot;

    for (size_t place = 0u; place < movable->width; place += 1u)
    {
        if ((embed_atomic_uptr_load(&movable->turnstile[place].inside) == named) ||
            (embed_atomic_uptr_load(&movable->turnstile[place].other) == named))
        {
            return EMBED_TRUE;
        }
    }
    return EMBED_FALSE;
}

/**
 * @brief Takes a slot's moving bit, where no context is in the cell and no move runs.
 *
 * @param[in]     movable The option [BORROWS].
 * @param[in,out] slot    The slot [BORROWS].
 * @param[in,out] word    The word as last read, a free slot or a moving one failing here [BORROWS].
 * @return                EMBED_TRUE where this context holds the bit.
 */
EMBED_INLINE embed_bool carcer_slot_shut(const CarcerMovable *movable, CarcerMovableSlot *slot,
                                         embed_atomic_usize_value *word)
{
    const embed_atomic_usize_value read = *word;

    if ((read & CARCER_SLOT_MOVING) != 0u)
    {
        return EMBED_FALSE;
    }
    if (!embed_atomic_usize_swap_if(&slot->word, word, read | CARCER_SLOT_MOVING))
    {
        return EMBED_FALSE;
    }
    embed_atomic_ordered();
    if (carcer_slot_named(movable, slot))
    {
        (void)embed_atomic_usize_fetch_and(&slot->word, ~CARCER_SLOT_MOVING);
        return EMBED_FALSE;
    }
    return EMBED_TRUE;
}

/**
 * @brief Flips a slot's moving bit off.
 *
 * @param[in,out] slot The slot [BORROWS].
 */
EMBED_INLINE void carcer_slot_opened(CarcerMovableSlot *slot)
{
    (void)embed_atomic_usize_fetch_and(&slot->word, ~CARCER_SLOT_MOVING);
}

/**
 * @brief Whether a span lies inside the cell a slot's word names.
 *
 * @param[in] cellblock Cellblock the cell is in [BORROWS].
 * @param[in] word      The slot's word, its moving bit clear, zero where free.
 * @param[in] offset    Where the span starts.
 * @param[in] bytes     The span's bytes.
 * @return              EMBED_TRUE where the slot names a cell and the span fits it.
 */
EMBED_INLINE embed_bool carcer_span_fits(const CarcerCellBlock *cellblock, embed_atomic_usize_value word, size_t offset,
                                         size_t bytes)
{
    if (word == 0u)
    {
        return EMBED_FALSE;
    }
    // Explicit cast reads the word as the offset it holds
    const size_t size = carcer_held_size(cellblock, (size_t)word - CARCER_WORD);

    return (embed_bool)((offset <= size) && (bytes <= (size - offset)));
}

/**
 * @brief Moves one slot's cell into the first free cell below it.
 *
 * @param[in,out] cellblock Cellblock the cell is in [BORROWS].
 * @param[in]     movable   The option [BORROWS].
 * @param[in,out] slot      The slot [BORROWS].
 * @param[in,out] refused   The least size no free cell has taken, lowered to this cell's where none takes it.
 * @return                  EMBED_TRUE where the cell moved.
 * @note The store that names the copy clears the moving bit with it, and the cell left behind is given back
 *       after: nothing reaches it once the slot reads the copy. A give takes its owner's word off the cell,
 *       and so the owner is read before anything else moves.
 * @note A cell a context is in says nothing about the free cells, and refused stays as it was.
 */
EMBED_INLINE embed_bool carcer_slot_moved(CarcerCellBlock *cellblock, const CarcerMovable *movable,
                                          CarcerMovableSlot *slot, size_t *refused)
{
    embed_atomic_usize_value word = embed_atomic_usize_load(&slot->word);

    if ((word == 0u) || !carcer_slot_shut(movable, slot, &word))
    {
        return EMBED_FALSE;
    }
    // Explicit cast reads the word as the offset it holds
    uint8_t *const prisoner = cellblock->base + (size_t)word;
    const size_t off = carcer_off_of(cellblock, prisoner);
    const size_t size = carcer_held_size(cellblock, off);
    CellOwner *const owner = mmgr_owner_of_buf(cellblock, prisoner);
    void *const copy = (owner != NULL) ? carcer_fit(cellblock, size, owner, EMBED_TRUE, off) : NULL;

    if (copy == NULL)
    {
        *refused = size;
        carcer_slot_opened(slot);
        return EMBED_FALSE;
    }
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = prisoner, .bytes = size});
    // Explicit casts take the copy's address to the offset the slot's word holds
    embed_atomic_usize_store(&slot->word, (embed_atomic_usize_value)((uint8_t *)copy - cellblock->base));
    carcer_given(cellblock, prisoner, owner, movable->wipes);
    return EMBED_TRUE;
}

/**
 * @brief The highest named cell below an offset that is smaller than the size no free cell has taken.
 *
 * @param[in]  cellblock Cellblock the cells are in [BORROWS].
 * @param[in]  movable   The option [BORROWS].
 * @param[in]  below     The offset the cell lies below.
 * @param[in]  refused   The size the cell is smaller than.
 * @param[out] at        Where it stands [BORROWS].
 * @return               Its slot, or NULL where none is.
 * @note A cell given back since the slot was read leaves a word that reads as some size, and the move reads
 *       the slot again before it does anything.
 */
EMBED_INLINE CarcerMovableSlot *carcer_slot_highest(const CarcerCellBlock *cellblock, const CarcerMovable *movable,
                                                    size_t below, size_t refused, size_t *at)
{
    // Explicit cast reads the word as the slots it counts, which the table's size bounds
    const size_t named = (size_t)embed_atomic_usize_load(&movable->named);
    CarcerMovableSlot *highest = NULL;
    size_t highest_at = 0u;

    for (size_t place = 0u; place < named; place += 1u)
    {
        // Explicit cast takes the moving bit off the word, and reads it as the offset it holds
        const size_t payload = (size_t)(embed_atomic_usize_load(&movable->slot[place].word) & ~CARCER_SLOT_MOVING);

        if ((payload > highest_at) && (payload < below) &&
            (carcer_held_size(cellblock, payload - CARCER_WORD) < refused))
        {
            highest = &movable->slot[place];
            highest_at = payload;
        }
    }
    *at = highest_at;
    return highest;
}

/**
 * @brief The turnstiles a worker occupies now, the locked ones not among them.
 *
 * @param[in] movable The option [BORROWS].
 * @return            The count.
 */
EMBED_INLINE size_t carcer_turnstiles_occupied(const CarcerMovable *movable)
{
    size_t occupied = 0u;

    for (size_t place = 0u; place < movable->width; place += 1u)
    {
        const embed_atomic_uptr_value inside = embed_atomic_uptr_load(&movable->turnstile[place].inside);

        occupied += ((inside != 0u) && (inside != CARCER_TURNSTILE_LOCKED)) ? 1u : 0u;
    }
    return occupied;
}

/**
 * @brief What the mover reads as pressure at the turnstiles left open.
 *
 * @param[in] movable The option [BORROWS].
 * @param[out] passed The takes and gives so far that found none free [BORROWS].
 * @return            The contexts in the line now.
 */
EMBED_INLINE size_t carcer_pressure_read(const CarcerMovable *movable, embed_atomic_usize_value *passed)
{
    *passed = embed_atomic_usize_load(&movable->passed_by);
    return carcer_queue_depth(movable);
}

/**
 * @brief Whether the line and the takes turned away have stayed steady since a reading.
 *
 * @param[in] movable The option [BORROWS].
 * @param[in] depth   The line at the reading.
 * @param[in] passed  The takes and gives at the reading.
 * @return            EMBED_TRUE where neither rose.
 */
EMBED_INLINE embed_bool carcer_pressure_steady(const CarcerMovable *movable, size_t depth,
                                               embed_atomic_usize_value passed)
{
    return (embed_bool)((carcer_queue_depth(movable) <= depth) &&
                        (embed_atomic_usize_load(&movable->passed_by) == passed));
}

/**
 * @brief Lets go every turnstile the mover locked.
 *
 * @param[in,out] movable The option [BORROWS].
 * @param[in]     locked  How many it locked, the highest first.
 */
EMBED_INLINE void carcer_turnstiles_opened(const CarcerMovable *movable, size_t locked)
{
    size_t left = locked;

    for (size_t place = movable->width; (place > 0u) && (left > 0u); place -= 1u)
    {
        CarcerTurnstile *const turnstile = &movable->turnstile[place - 1u];

        if (embed_atomic_uptr_load(&turnstile->inside) == CARCER_TURNSTILE_LOCKED)
        {
            embed_atomic_uptr_store(&turnstile->inside, 0u);
            left -= 1u;
        }
    }
}

/**
 * @brief One sort: turnstiles locked while the pressure at the others stays steady, then cells moved.
 *
 * @param[in,out] cellblock Cellblock whose cells to move [BORROWS].
 * @param[in,out] movable   The option [BORROWS].
 * @return                  EMBED_TRUE where the sort ran until nothing more would move. EMBED_FALSE where the
 *                          pressure rose and it backed off.
 * @note A reader of the rows locks a turnstile for the length of its sum, which this can read as locked and
 *       let go early. That reader's store of zero finds it free already, and nothing depends on it.
 */
EMBED_INLINE embed_bool carcer_mover_sorted(CarcerCellBlock *cellblock, CarcerMovable *movable)
{
    const size_t most = movable->width / 2u;
    size_t locked = 0u;
    embed_bool steady = EMBED_TRUE;
    embed_bool finished = EMBED_FALSE;

    for (size_t place = movable->width; steady && (place > 0u) && (locked < most); place -= 1u)
    {
        embed_atomic_uptr_value found = 0u;

        if (embed_atomic_uptr_swap_if(&movable->turnstile[place - 1u].inside, &found, CARCER_TURNSTILE_LOCKED))
        {
            embed_atomic_usize_value passed = 0u;
            const size_t depth = carcer_pressure_read(movable, &passed);

            locked += 1u;
            steady = (embed_bool)(movable->thread_wait(MMGR_MOVABLE_READ_MS) &&
                                  carcer_pressure_steady(movable, depth, passed));
        }
    }
    while (steady && !finished)
    {
        embed_atomic_usize_value passed = 0u;
        const size_t depth = carcer_pressure_read(movable, &passed);

        if (movable->thread_moving != NULL)
        {
            movable->thread_moving(EMBED_TRUE);
        }
        finished = (embed_bool)!mmgr_movable_buf_move(cellblock);
        if (movable->thread_moving != NULL)
        {
            movable->thread_moving(EMBED_FALSE);
        }
        steady = carcer_pressure_steady(movable, depth, passed);
    }
    carcer_turnstiles_opened(movable, locked);
    return finished;
}

/**
 * @brief What the thread the program gave runs.
 *
 * @param[in,out] context The cellblock [BORROWS].
 */
static void carcer_mover_entered(void *context)
{
    mmgr_movable_run((CarcerCellBlock *)context);
}

void mmgr_owner_bytes_taken(CellOwner *owner, size_t bytes)
{
    carcer_owner_took(owner, bytes);
}

void mmgr_owner_bytes_given(CellOwner *owner, size_t bytes)
{
    carcer_owner_gave(owner, bytes);
}

size_t mmgr_cellblock_fragmentation(const CarcerCellBlock *cellblock)
{
    const embed_atomic_usize_value tiers = embed_atomic_usize_load(&cellblock->tiers);
    const size_t occupied = mmgr_tiers_persistent_end(tiers) + (cellblock->size - mmgr_tiers_temporary_top(tiers));
    // Explicit cast reads the word as the bytes it counts
    const size_t in_use = (size_t)embed_atomic_usize_load(&cellblock->in_use);
    const size_t holes = (occupied > in_use) ? (occupied - in_use) : 0u;

    // The hundredth of the bytes occupied divides, since a product of the holes and a hundred can pass what a
    // size_t counts on a narrow word
    return (occupied >= 100u) ? (holes / (occupied / 100u)) : 0u;
}

embed_bool mmgr_movable_init(CarcerCellBlock *cellblock, const CarcerMovableInitCfg *args, embed_bool wipes)
{
    if ((carcer_movable_of(cellblock) != NULL) || (args->storage == NULL) || (args->cells == 0u) ||
        (args->width == 0u) || (args->storage_bytes < MMGR_MOVABLE_STORAGE_BYTES(args->cells, args->width, args->rows)))
    {
        return EMBED_FALSE;
    }
    uint8_t *const storage = (uint8_t *)args->storage;
    CarcerMovable *const movable = (CarcerMovable *)args->storage;
    // Explicit casts read the address as the integer the cache line boundary is taken on, and the offset back
    const uintptr_t past_head = (uintptr_t)storage + MMGR_MOVABLE_HEAD_BYTES;
    const uintptr_t boundary = (uintptr_t)(MMGR_MOVABLE_TURNSTILE_BYTES - 1u);
    uint8_t *const turnstiles = storage + (((past_head + boundary) & ~boundary) - (uintptr_t)storage);
    uint8_t *const rows = turnstiles + (args->width * MMGR_MOVABLE_TURNSTILE_BYTES);
    uint8_t *const slots = rows + (args->rows * sizeof(CarcerTouchRow));

    movable->slot = (CarcerMovableSlot *)(void *)slots;
    movable->cells = args->cells;
    movable->turnstile = (CarcerTurnstile *)(void *)turnstiles;
    movable->width = args->width;
    movable->row = (CarcerTouchRow *)(void *)rows;
    movable->rows = args->rows;
    movable->wipes = wipes;
    movable->thread_id = args->thread_id;
    movable->thread_wait = args->thread_wait;
    movable->thread_yield = args->thread_yield;
    movable->thread_moving = args->thread_moving;
    // Explicit cast carries the state's address as the integer the word holds
    embed_atomic_uptr_store(&cellblock->movable, (embed_atomic_uptr_value)(uintptr_t)movable);
    if ((args->thread_started != NULL) && (args->thread_wait != NULL))
    {
        return args->thread_started(carcer_mover_entered, cellblock);
    }
    return EMBED_TRUE;
}

const CarcerMovableSlot *mmgr_movable_buf_alloc(CarcerCellBlock *cellblock, size_t size, CellOwner *owner)
{
    CarcerMovable *const movable = carcer_movable_of(cellblock);

    if (movable == NULL)
    {
        return NULL;
    }
    CarcerTurnstile *const turnstile = carcer_passing(movable);
    void *const prisoner = carcer_take(cellblock, size, owner, EMBED_FALSE);
    const CarcerMovableSlot *named = NULL;

    if (prisoner != NULL)
    {
        // Explicit casts take the payload's address to the offset the slot's word holds: it lies in the
        // cellblock, which a word counts
        const embed_atomic_usize_value payload = (embed_atomic_usize_value)((uint8_t *)prisoner - cellblock->base);
        // Explicit cast reads the word as the slot it counts: lowest never passes the table's size
        const size_t from = (size_t)embed_atomic_usize_load(&movable->lowest);

        for (size_t tried = 0u; (tried < movable->cells) && (named == NULL); tried += 1u)
        {
            const size_t reached = from + tried;
            const size_t place = (reached < movable->cells) ? reached : (reached - movable->cells);
            embed_atomic_usize_value found = 0u;

            if (embed_atomic_usize_swap_if(&movable->slot[place].word, &found, payload))
            {
                embed_atomic_usize_value start = from;

                // A release since this search began has lowered the start, and the swap leaves that standing
                (void)embed_atomic_usize_swap_if(&movable->lowest, &start, place + 1u);
                (void)embed_atomic_usize_raise(&movable->named, place + 1u);
                named = &movable->slot[place];
            }
        }
        if (named == NULL)
        {
            carcer_given(cellblock, prisoner, owner, movable->wipes);
        }
    }
    carcer_passed(turnstile);
    return named;
}

embed_bool mmgr_movable_buf_release(CarcerCellBlock *cellblock, const CarcerMovableSlot *cell, CellOwner *owner)
{
    if (cell == NULL)
    {
        return EMBED_FALSE;
    }
    CarcerMovable *const movable = carcer_movable_of(cellblock);
    // Explicit cast takes the const off the identity: the slot is MMgr's to write, and the caller's const says
    // only that the caller does not
    CarcerMovableSlot *const slot = (CarcerMovableSlot *)(uintptr_t)cell;
    embed_atomic_usize_value word = embed_atomic_usize_load(&slot->word);

    while ((word == 0u) || !carcer_slot_shut(movable, slot, &word))
    {
        if (word == 0u)
        {
            return EMBED_FALSE;
        }
        carcer_yielded(movable);
        word = embed_atomic_usize_load(&slot->word);
    }
    // Explicit cast reads the word as the offset it holds
    const size_t off = carcer_off_of(cellblock, cellblock->base + (size_t)word);
    const size_t size = carcer_held_size(cellblock, off);

    if (!carcer_owner_named(cellblock, off, size, owner))
    {
        carcer_slot_opened(slot);
        return EMBED_FALSE;
    }
    uint8_t *const prisoner = cellblock->base + (size_t)word;
    CarcerTurnstile *const turnstile = carcer_passing(movable);

    // The slot goes free behind its moving bit: a move finding the offset still there would take whatever
    // cell has since been laid at it for this one
    embed_atomic_usize_store(&slot->word, 0u);
    carcer_given(cellblock, prisoner, owner, movable->wipes);
    carcer_passed(turnstile);
    // Explicit cast reads the slot's distance from the table's start as the place it is, which a word counts
    const embed_atomic_usize_value place = (embed_atomic_usize_value)(slot - movable->slot);
    embed_atomic_usize_value lowest = embed_atomic_usize_load(&movable->lowest);

    // A swap that fails reads lowest again, and a start at or below this slot stands
    while ((place < lowest) && !embed_atomic_usize_swap_if(&movable->lowest, &lowest, place))
    {
    }
    return EMBED_TRUE;
}

embed_bool mmgr_movable_buf_read(CarcerCellBlock *cellblock, const CarcerMovableCfg *args)
{
    embed_atomic_usize_value word = 0u;
    CarcerTurnstile *const turnstile = carcer_cell_entered(carcer_movable_of(cellblock), args->cell, NULL, &word, NULL);
    const embed_bool fits = carcer_span_fits(cellblock, word, args->offset, args->bytes);

    if (fits)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->dst, .src = cellblock->base + (size_t)word + args->offset, .bytes = args->bytes});
    }
    carcer_cell_left(turnstile);
    return fits;
}

embed_bool mmgr_movable_buf_write(CarcerCellBlock *cellblock, const CarcerMovableCfg *args)
{
    embed_atomic_usize_value word = 0u;
    CarcerTurnstile *const turnstile = carcer_cell_entered(carcer_movable_of(cellblock), args->cell, NULL, &word, NULL);
    const embed_bool fits = carcer_span_fits(cellblock, word, args->offset, args->bytes);

    if (fits)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = cellblock->base + (size_t)word + args->offset, .src = args->src, .bytes = args->bytes});
    }
    carcer_cell_left(turnstile);
    return fits;
}

embed_iword mmgr_movable_buf_compare(CarcerCellBlock *cellblock, const CarcerMovableCfg *args)
{
    embed_atomic_usize_value word = 0u;
    embed_atomic_usize_value other_word = 0u;
    CarcerTurnstile *const turnstile =
        carcer_cell_entered(carcer_movable_of(cellblock), args->cell, args->other_cell, &word, &other_word);
    embed_iword order = 1;

    if (carcer_span_fits(cellblock, word, args->offset, args->bytes))
    {
        if (args->other_cell == NULL)
        {
            order = memor.cmp(&(MemoriaCfg){
                .src = cellblock->base + (size_t)word + args->offset, .other = args->src, .bytes = args->bytes});
        }
        else if (carcer_span_fits(cellblock, other_word, args->other_offset, args->bytes))
        {
            order = memor.cmp(&(MemoriaCfg){.src = cellblock->base + (size_t)word + args->offset,
                                            .other = cellblock->base + (size_t)other_word + args->other_offset,
                                            .bytes = args->bytes});
        }
    }
    carcer_cell_left(turnstile);
    return order;
}

size_t mmgr_movable_buf_measure(CarcerCellBlock *cellblock, const CarcerMovableCfg *args)
{
    embed_atomic_usize_value word = 0u;
    CarcerTurnstile *const turnstile = carcer_cell_entered(carcer_movable_of(cellblock), args->cell, NULL, &word, NULL);
    size_t measured = 0u;

    if (carcer_span_fits(cellblock, word, args->offset, 0u))
    {
        const uint8_t *const start = cellblock->base + (size_t)word + args->offset;
        const size_t room = carcer_held_size(cellblock, (size_t)word - CARCER_WORD) - args->offset;
        const uint8_t *const zero = (const uint8_t *)memor.chr(&(MemoriaCfg){.src = start, .val = 0u, .bytes = room});

        // Explicit cast reads the distance, which the cell's size bounds, as the bytes it is
        measured = (zero != NULL) ? (size_t)(zero - start) : room;
    }
    carcer_cell_left(turnstile);
    return measured;
}

embed_bool mmgr_movable_buf_move(CarcerCellBlock *cellblock)
{
    CarcerMovable *const movable = carcer_movable_of(cellblock);

    if (movable == NULL)
    {
        return EMBED_FALSE;
    }
    size_t below = cellblock->size;
    size_t refused = cellblock->size;

    for (;;)
    {
        size_t highest_at = 0u;
        CarcerMovableSlot *const highest = carcer_slot_highest(cellblock, movable, below, refused, &highest_at);

        if (highest == NULL)
        {
            return EMBED_FALSE;
        }
        if (carcer_slot_moved(cellblock, movable, highest, &refused))
        {
            return EMBED_TRUE;
        }
        below = highest_at;
    }
}

/**
 * @brief One of the mover's readings of its cellblock, and the sort it starts where the reading calls for one.
 *
 * @param[in,out] cellblock Cellblock whose cells to move [BORROWS].
 * @param[in,out] movable   The option [BORROWS].
 * @param[in,out] wait      The milliseconds until the next reading [BORROWS].
 * @param[in,out] left      The fragmentation the last finished sort left, zero under the threshold [BORROWS].
 * @param[in,out] quiet     The readings in a row that found low occupancy [BORROWS].
 */
EMBED_INLINE void carcer_mover_read(CarcerCellBlock *cellblock, CarcerMovable *movable, uint32_t *wait, size_t *left,
                                    size_t *quiet)
{
    const size_t percent = mmgr_cellblock_fragmentation(cellblock);

    if (percent < MMGR_MOVABLE_SETTLE_PERCENT)
    {
        *left = 0u;
        *quiet = 0u;
        *wait = MMGR_MOVABLE_READ_MS;
        return;
    }
    if (percent <= *left)
    {
        return;
    }
    *quiet = ((carcer_turnstiles_occupied(movable) * 2u) <= movable->width) ? (*quiet + 1u) : 0u;
    if (*quiet < MMGR_MOVABLE_QUIET_READINGS)
    {
        return;
    }
    *quiet = 0u;
    if (carcer_mover_sorted(cellblock, movable))
    {
        *left = mmgr_cellblock_fragmentation(cellblock);
        *wait = MMGR_MOVABLE_READ_MS;
    }
    else
    {
        *wait = ((*wait * 2u) < MMGR_MOVABLE_BACKOFF_MOST_MS) ? (*wait * 2u) : MMGR_MOVABLE_BACKOFF_MOST_MS;
    }
}

void mmgr_movable_run(CarcerCellBlock *cellblock)
{
    CarcerMovable *const movable = carcer_movable_of(cellblock);
    uint32_t wait = MMGR_MOVABLE_READ_MS;
    size_t left = 0u;
    size_t quiet = 0u;

    while (movable->thread_wait(wait))
    {
        carcer_mover_read(cellblock, movable, &wait, &left, &quiet);
    }
}

size_t mmgr_movable_queue_depth(const CarcerCellBlock *cellblock)
{
    const CarcerMovable *const movable = carcer_movable_of(cellblock);

    return (movable != NULL) ? carcer_queue_depth(movable) : 0u;
}

embed_bool mmgr_movable_touches(CarcerCellBlock *cellblock, size_t at, size_t *id, size_t *touches)
{
    CarcerMovable *const movable = carcer_movable_of(cellblock);

    if ((movable == NULL) || (at >= movable->rows))
    {
        return EMBED_FALSE;
    }
    for (size_t place = 0u; place < movable->width; place += 1u)
    {
        CarcerTurnstile *const turnstile = &movable->turnstile[place];
        embed_atomic_uptr_value found = 0u;

        // Claimed as a worker claims it, and so nothing races the sum. One occupied now comes here later
        if (embed_atomic_uptr_swap_if(&turnstile->inside, &found, CARCER_TURNSTILE_LOCKED))
        {
            carcer_touches_summed(movable, turnstile);
            embed_atomic_uptr_store(&turnstile->inside, 0u);
        }
    }
    const embed_atomic_usize_value key = embed_atomic_usize_load(&movable->row[at].key);

    if (key == 0u)
    {
        return EMBED_FALSE;
    }
    // Explicit casts read the words as the id and the count they hold
    *id = (key == CARCER_ROW_OTHERS) ? SIZE_MAX : ((size_t)key - 1u);
    *touches = (size_t)embed_atomic_usize_load(&movable->row[at].touches);
    return EMBED_TRUE;
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
    embed_atomic_usize_store(&cellblock->in_use, 0u);
    embed_atomic_uptr_store(&cellblock->movable, 0u);
}

#if MMGR_ENABLE_HOST
#include <stdint.h>
#include <stdlib.h>

embed_bool mmgr_host_cellblock_made(CarcerCellBlock **kept, size_t bytes)
{
    const size_t head = (sizeof(CarcerCellBlock) + MMGR_ALIGN_BYTES - 1u) & ~(size_t)(MMGR_ALIGN_BYTES - 1u);

    if ((bytes < (2u * MMGR_CARCER_ALIGN)) || (bytes > MMGR_CARCER_BYTES_MOST) ||
        ((*kept != NULL) && (carcer_movable_of(*kept) != NULL)))
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
