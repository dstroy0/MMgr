/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum.c
 * @brief LocusCarcerum's two tiers over the caller's bytes: a take is one pass through the cellblock's turnstiles
 *        and one fetch-add on its tier's boundary, a give is one fetch-or on the cell's owner word and one store on
 *        its header, and everything that moves a boundary any other way closes the cellblock, drains the turnstiles
 *        and works alone.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 *
 * @note A cellblock is a run of bytes with a word ahead of each cell and one behind it: the header, the cell's
 *       payload size in the busy or the free state, and the owner word, the owner's place with the nanosecond of
 *       the take. The persistent tier allocates upward from base and the temporary tier downward from size, and
 *       the gap between the two boundaries is what either can still take.
 * @note Every choice on a hot path is a mask, a select, or a row of a two-row table of entries: a take that the
 *       gap cannot meet lays its words on the cellblock's sink word, adds its bump to the tier's overrun, and answers
 *       NULL through the same stores a take that lands makes. No path branches on what another context did.
 * @note A reader's pass is a badge-in at the cellblock's turnstiles: the lane claimed with one fetch-or, the badge
 *       written with the owner's place and the clock's reading, the work, and the lane let go. The turnstiles' host
 *       hooks, embed_word_wait, embed_word_wake and embed_clock_ns, are the program's to define, as embed_turnstile.h
 *       says: a build with none fails at the link, naming them. A writer sets the
 *       cellblock's closed bit with one fetch-or, drains the lanes, and works with no pass under way: the rewind,
 *       the compaction that merges free runs and trims the end, the growth and the move. A pass that finds the
 *       closed bit backs off one step on it and tries once more; a pass that finds no lane does the same.
 * @note Every take, give and growth names an owner of the table MMGR_OWNERS lays. A cell counts to the owner its
 *       owner word names, and a give or a growth naming another owner is refused and told to the port. The
 *       completion bit of the owner word is set once, by one fetch-or, and the cell's bytes come off its owner
 *       in the context whose fetch-or found it clear.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "memoria_operor/memoria_operor.h"

/** @brief The state of a cell no context holds. */
#define CARCER_FREE ((embed_atomic_usize_value)0u)

/** @brief The state of a cell a context holds. */
#define CARCER_BUSY ((embed_atomic_usize_value)1u)

/** @brief The bits of a cell's header that carry its state. */
#define CARCER_STATE ((embed_atomic_usize_value)3u)

/** @brief The bit of the closed word a writer holds while it works alone. */
#define CARCER_CLOSED ((embed_atomic_word_value)1u)

/** @brief The bit of the closed word a context sets while it sleeps on the word. */
#define CARCER_CLOSED_SLEEPERS ((embed_atomic_word_value)2u)

/** @brief The first backoff step: a pause with no sleep, the sleeps reached as the steps double. */
#define CARCER_STEP_FIRST 1u

/**
 * @brief Bytes one of a cell's two words occupies, rounded up so the payload between them stays aligned.
 *
 * @note The rounding is a mask, and a mask rounds only because MMGR_CARCER_ALIGN is a power of two, which
 *       locus_carcerum.h asserts.
 */
#define CARCER_WORD ((sizeof(embed_atomic_usize) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief Bytes a cell costs beyond its payload: the header ahead of it and the owner word behind it.
 *
 * @note Charged on top of the payload every time a cell is allocated.
 */
#define CARCER_HDR (2u * CARCER_WORD)

/**
 * @brief The two boundaries as the one word carries them: the persistent end in the low half, the temporary top in
 *        the high.
 *
 * @param[in] end The persistent end.
 * @param[in] top The temporary top.
 * @return        The word.
 * @note Explicit casts carry each offset into its half; the bits of a top past its half leave the word, which is the
 *       wrap of a bump down the bumps' tests refuse.
 */
EMBED_INLINE embed_atomic_usize_value carcer_bounds(size_t end, size_t top)
{
    return ((embed_atomic_usize_value)top << MMGR_CARCER_HALF_BITS) |
           ((embed_atomic_usize_value)end & MMGR_CARCER_HALF_MASK);
}

/**
 * @brief The persistent end a bounds word carries.
 *
 * @param[in] word The word.
 * @return         Its low half.
 */
EMBED_INLINE size_t carcer_bounds_end(embed_atomic_usize_value word)
{
    // Explicit cast reads the half as the offset it holds
    return (size_t)(word & MMGR_CARCER_HALF_MASK);
}

/**
 * @brief The temporary top a bounds word carries.
 *
 * @param[in] word The word.
 * @return         Its high half.
 */
EMBED_INLINE size_t carcer_bounds_top(embed_atomic_usize_value word)
{
    // Explicit cast reads the half as the offset it holds
    return (size_t)(word >> MMGR_CARCER_HALF_BITS);
}

/**
 * @brief Raises one tier's high-water mark, or reads its arguments and does nothing where the build tracks none.
 *
 * @param[in] series_cellularum_ Cellblock to record against.
 * @param[in] member_            Mark to raise, which only exists when the build tracks one.
 * @param[in] used_              Bytes in use by the tier that moved.
 * @warning The two arms do not evaluate the same things. Where the build tracks none, the cellblock and the bytes
 *          are evaluated and discarded and the member is not named, and nothing passed here may carry a side
 *          effect.
 */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CARCER_HW(series_cellularum_, member_, used_)                                                                  \
    ((void)embed_atomic_usize_raise(&(series_cellularum_)->member_, (used_)))
#else
#define CARCER_HW(series_cellularum_, member_, used_) ((void)(series_cellularum_), (void)(used_))
#endif

/**
 * @brief Every bit set where a test holds, none where it does not.
 *
 * @param[in] set The test's result.
 * @return        The mask.
 */
EMBED_INLINE size_t carcer_all(int set)
{
    // Explicit cast carries the test's 0 or 1 into the width the mask is taken in
    return 0u - (size_t)(set != 0);
}

/**
 * @brief One of two counts, by a mask.
 *
 * @param[in] mask       Every bit set for the first, none for the second.
 * @param[in] when_set   The count answered where the mask is set.
 * @param[in] when_clear The count answered where it is clear.
 * @return               The count picked.
 */
EMBED_INLINE size_t carcer_pick(size_t mask, size_t when_set, size_t when_clear)
{
    return (when_set & mask) | (when_clear & ~mask);
}

/**
 * @brief One of two addresses, by a mask.
 *
 * @param[in] mask       Every bit set for the first, none for the second.
 * @param[in] when_set   The address answered where the mask is set [BORROWS].
 * @param[in] when_clear The address answered where it is clear [BORROWS].
 * @return               The address picked [BORROWS].
 * @note Explicit casts take both addresses to the integer the mask is applied to, and the pick back.
 */
EMBED_INLINE void *carcer_pick_at(size_t mask, void *when_set, void *when_clear)
{
    return (void *)(((uintptr_t)when_set & (uintptr_t)mask) | ((uintptr_t)when_clear & ~(uintptr_t)mask));
}

/**
 * @brief Rounds want up to a whole machine word.
 *
 * @param[in] want Count to round.
 * @return         want rounded up to a multiple of MMGR_CARCER_ALIGN, and want itself when it already is one.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask. A want within a word of SIZE_MAX wraps to 0, and a
 *          request that wraps is met with a small cell in place of a refusal.
 */
EMBED_INLINE size_t carcer_round(size_t want)
{
    return (want + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u);
}

/**
 * @brief Rounds a request up to a whole word, one word for a request of nothing.
 *
 * @param[in] want Bytes the caller asked for.
 * @return         The payload a cell carries for this request.
 */
EMBED_INLINE size_t carcer_want(size_t want)
{
    return carcer_round(carcer_pick(carcer_all(want != 0u), want, MMGR_CARCER_ALIGN));
}

/**
 * @brief Zeroes want bytes at *walk, advancing the pointer and taking them off *left.
 *
 * @param[in,out] walk Walking pointer, left one past the last byte cleared [BORROWS].
 * @param[in,out] left Bytes still to clear, reduced by want [BORROWS].
 * @param[in]     want Bytes to clear now.
 * @note The stores are volatile. Clearing bytes nothing reads afterwards is not dropped as dead work.
 * @warning want comes off *left with no test. Both call sites hold want at or below what is left.
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
 * @param[in] series Cellblock to look in [BORROWS].
 * @param[in] off    Offset of the word, a multiple of MMGR_CARCER_ALIGN.
 * @return           The word [BORROWS].
 * @note The cast goes through void *. base is aligned by the site macro and every offset here is a whole number
 *       of words, and the word is always correctly aligned.
 */
EMBED_INLINE embed_atomic_usize *carcer_word_at(const SeriesCellularum *series, size_t off)
{
    return (embed_atomic_usize *)(void *)(series->base + off);
}

/**
 * @brief The word at off where a mask is set, and the cellblock's sink word where it is clear.
 *
 * @param[in] series Cellblock to look in [BORROWS].
 * @param[in] mask   Every bit set for the word at off.
 * @param[in] off    Offset of the word.
 * @return           The word a store lands on [BORROWS].
 * @note A store that must not land writes the sink: the store stands on every path and no path branches on it.
 * @note The offset is masked first: where the mask is clear, the word read is the cellblock's first, never one past
 *       its bytes, which a refused bump's offset is.
 */
EMBED_INLINE embed_atomic_usize *carcer_word_picked(SeriesCellularum *series, size_t mask, size_t off)
{
    return carcer_pick_at(mask, carcer_word_at(series, off & mask), &series->sink);
}

/**
 * @brief A cell's header built from a size and a state.
 *
 * @param[in] size  The payload size, a multiple of MMGR_CARCER_ALIGN.
 * @param[in] state CARCER_FREE or CARCER_BUSY.
 * @return          The header.
 */
EMBED_INLINE embed_atomic_usize_value carcer_word(size_t size, embed_atomic_usize_value state)
{
    // Explicit cast carries the size into the word, which is size_t's width, ahead of the shift
    return ((embed_atomic_usize_value)size << 2u) | state;
}

/**
 * @brief The size a cell's header carries.
 *
 * @param[in] word The header.
 * @return         Its size.
 */
EMBED_INLINE size_t carcer_word_size(embed_atomic_usize_value word)
{
    // Explicit cast reads the word's upper bits, size_t's width less two, as the size they hold
    return (size_t)(word >> 2u);
}

/**
 * @brief The state a cell's header carries.
 *
 * @param[in] word The header.
 * @return         CARCER_FREE or CARCER_BUSY.
 */
EMBED_INLINE embed_atomic_usize_value carcer_word_state(embed_atomic_usize_value word)
{
    return word & CARCER_STATE;
}

/**
 * @brief Offset of the owner word behind a cell's payload.
 *
 * @param[in] off  The cell's offset.
 * @param[in] size Its payload size.
 * @return         The offset of the owner word.
 */
EMBED_INLINE size_t carcer_foot_of(size_t off, size_t size)
{
    return off + CARCER_WORD + size;
}

/**
 * @brief Offset just past a cell: past its header, its payload and its owner word.
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
 * @brief The offset of a cell's header from its payload's address.
 *
 * @param[in] series Cellblock the cell came from [BORROWS].
 * @param[in] at     First byte of the payload [BORROWS].
 * @return           Offset of the header in the cellblock.
 * @warning at is taken to be a cell of this cellblock. Every caller bounds it with mmgr_who_owns_buf first, and
 *          an address inside the cellblock but off a cell boundary still yields an offset reading bytes which
 *          are not a cell's header.
 */
EMBED_INLINE size_t carcer_off_of(const SeriesCellularum *series, const void *at)
{
    // Explicit casts take at to a byte pointer so the difference is in bytes, then that ptrdiff_t to the size_t
    // the offset is carried in
    return (size_t)((const uint8_t *)at - series->base) - CARCER_WORD;
}

/**
 * @brief The payload size of the cell at off, read off its header.
 *
 * @param[in] series Cellblock the cell sits in [BORROWS].
 * @param[in] off    The cell's offset.
 * @return           The size.
 */
EMBED_INLINE size_t carcer_size_at(const SeriesCellularum *series, size_t off)
{
    return carcer_word_size(embed_atomic_usize_load(carcer_word_at(series, off)));
}

/** @brief The owner a count lands on where no owner is named: a record outside the table, counted and read by none. */
static CellOwner carcer_owner_none = {"", 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};

/** @brief Bytes of the cellblock a call handed none reads: one cell's words and a word of payload, zero. */
static EMBED_ALIGN(64) uint8_t carcer_bytes_none[4u * sizeof(embed_atomic_usize)];

/** @brief The cellblock a give or a growth handed none reads in its place: over carcer_bytes_none, taking nothing. */
static SeriesCellularum carcer_block_none = MMGR_CARCER_STATE(carcer_bytes_none, sizeof(carcer_bytes_none));

/**
 * @brief The owner's record, written through its identity.
 *
 * @param[in] owner The owner [BORROWS].
 * @return          The record [BORROWS].
 * @note Explicit cast takes the const off the identity: the record is MMgr's to write, and the caller's const
 *       says only that the caller does not.
 */
EMBED_INLINE CellOwner *carcer_owner_record(const CellOwner *owner)
{
    return (CellOwner *)(uintptr_t)owner;
}

/**
 * @brief The owner at a place of the table, and the record counted by none where the place names no owner.
 *
 * @param[in] place A place field's value, counted from 1.
 * @return          The record [BORROWS].
 */
EMBED_INLINE CellOwner *carcer_owner_at(size_t place)
{
    const size_t listed = carcer_all(place != 0u) & carcer_all(place <= mmgr_owner_table_count);

    return carcer_pick_at(listed, &mmgr_owner_table[carcer_pick(listed, place - 1u, 0u)], &carcer_owner_none);
}

/**
 * @brief The place a cell's owner word names.
 *
 * @param[in] named The owner word.
 * @return          Its place field, 0 where it names no owner.
 */
EMBED_INLINE size_t carcer_owner_place(embed_atomic_usize_value named)
{
    // Explicit cast reads the place field, which the word's width bounds, as the count it is
    return (size_t)((named & MMGR_OWNER_PLACE_MASK) >> MMGR_OWNER_PLACE_SHIFT);
}

/**
 * @brief The owner a cell's owner word names.
 *
 * @param[in] named The owner word.
 * @return          The owner whose place the word's place field holds, NULL where the field is 0 or past the
 *                  table [BORROWS].
 */
EMBED_INLINE CellOwner *carcer_owner_found(embed_atomic_usize_value named)
{
    CellOwner *const at = carcer_owner_at(carcer_owner_place(named));

    return carcer_pick_at(carcer_all(at != &carcer_owner_none), at, NULL);
}

/**
 * @brief The owner word a take writes behind the cell for an owner: its place, no completion, no signal, and
 *        the nanosecond read now in the timestamp's bits.
 *
 * @param[in] owner The owner [BORROWS].
 * @return          The word.
 */
EMBED_INLINE embed_atomic_usize_value carcer_owner_word(const CellOwner *owner)
{
    // Explicit casts carry the place, which the field's width bounds, and the clock's low bits into the word
    return ((embed_atomic_usize_value)owner->place << MMGR_OWNER_PLACE_SHIFT) |
           ((embed_atomic_usize_value)mmgr_carcer_hw_clock_ns() & MMGR_OWNER_TIME_MASK);
}

/**
 * @brief Whether an owner word names an owner: one AND on the place field.
 *
 * @param[in] named The owner word.
 * @param[in] owner The owner [BORROWS].
 * @return          Every bit set where the word's place is the owner's.
 */
EMBED_INLINE size_t carcer_owner_is(embed_atomic_usize_value named, const CellOwner *owner)
{
    // Explicit cast carries the place into the field the word holds it in
    return carcer_all((named & MMGR_OWNER_PLACE_MASK) ==
                      ((embed_atomic_usize_value)owner->place << MMGR_OWNER_PLACE_SHIFT));
}

embed_bool mmgr_owner_listed(const CellOwner *owner)
{
    // Explicit casts read the two addresses as the integers the table's span is measured in: the answer is read
    // off the address alone, and no record is read, so an address past the table or between two records is safe
    const uintptr_t off = (uintptr_t)owner - (uintptr_t)&mmgr_owner_table[0];
    const uintptr_t span = (uintptr_t)mmgr_owner_table_count * (uintptr_t)sizeof(CellOwner);
    // an address below the table wraps off past the span and is refused with the rest
    const size_t within = carcer_all(off < span) & carcer_all((off % (uintptr_t)sizeof(CellOwner)) == 0u);

    // Explicit cast narrows the mask's low bit to the embed_bool container
    return (embed_bool)(within & 1u);
}

/**
 * @brief Weak default for the library's clock, which answers 0 every time.
 *
 * @return 0.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port defines the name to
 *       replace it.
 */
EMBED_WEAK uint64_t mmgr_carcer_hw_clock_ns(void)
{
    return 0u;
}

/**
 * @brief Weak default for a program that declares no owners: a table of none, so that every take is refused.
 *
 * @note EMBED_WEAK marks these weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. MMGR_OWNERS defines both
 *       names in the program's one translation unit for its owners, and these stand only where it is not
 *       expanded. A translation unit that expands MMGR_OWNERS and includes this source defines
 *       MMGR_OWNERS_IN_THIS_UNIT ahead of the include and these are left out.
 */
#if !defined(MMGR_OWNERS_IN_THIS_UNIT)
EMBED_WEAK CellOwner mmgr_owner_table[1] = {{"", 1u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)}};
EMBED_WEAK const size_t mmgr_owner_table_count = 0u;
#endif

/**
 * @brief Weak default for the report of a give or a growth naming an owner other than the cell's, which does
 *        nothing.
 *
 * @param[in] named      The owner the give or the growth named [BORROWS].
 * @param[in] cell_owner The owner the cell counts to [BORROWS].
 * @param[in] bytes      The cell's payload bytes.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port defines the name to
 *       replace it. A translation unit that defines the name and includes this source defines
 *       MMGR_MISNAMED_IN_THIS_UNIT ahead of the include and this is left out.
 */
#if !defined(MMGR_MISNAMED_IN_THIS_UNIT)
EMBED_WEAK void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
}
#endif

/** @brief The row of the report table a give naming the cell's own owner takes: nothing told. */
static void carcer_told_none(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
}

/**
 * @brief Counts a cell's payload to the persistent tier's running total.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     bytes  The payload bytes.
 */
static void carcer_tier_counted(SeriesCellularum *series, size_t bytes)
{
    (void)embed_atomic_usize_fetch_add(&series->persistent_in_use, bytes);
}

/**
 * @brief Takes a cell's payload off the persistent tier's running total.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     bytes  The payload bytes.
 */
static void carcer_tier_uncounted(SeriesCellularum *series, size_t bytes)
{
    (void)embed_atomic_usize_fetch_sub(&series->persistent_in_use, bytes);
}

/** @brief The row of a tier table a temporary cell takes: the persistent total untouched. */
static void carcer_tier_none(SeriesCellularum *series, size_t bytes)
{
    (void)series;
    (void)bytes;
}

/**
 * @brief Counts a cell's payload to its owner and to the cellblock's running totals.
 *
 * @param[in,out] series  Cellblock the cell is in [BORROWS].
 * @param[in]     owner      The owner [BORROWS].
 * @param[in]     bytes      The payload bytes.
 * @param[in]     persistent Every bit set where the cell lies in the persistent tier.
 * @note The owner's peak is raised to its count after the add. The persistent total is a row of a two-row
 *       table, the row the tier.
 */
static void carcer_counted(SeriesCellularum *series, const CellOwner *owner, size_t bytes, size_t persistent)
{
    static void (*const tiers[2])(SeriesCellularum *, size_t) = {carcer_tier_none, carcer_tier_counted};
    CellOwner *const record = carcer_owner_record(owner);
    // Explicit cast reads the count before the add, size_t's width, as the bytes it holds
    const size_t in_use = (size_t)embed_atomic_usize_fetch_add(&record->in_use, bytes) + bytes;

    (void)embed_atomic_usize_raise(&record->hw, in_use);
    (void)embed_atomic_usize_fetch_add(&series->in_use, bytes);
    tiers[persistent & 1u](series, bytes);
}

/**
 * @brief Takes a cell's payload off its owner and the cellblock's running totals.
 *
 * @param[in,out] series  Cellblock the cell is in [BORROWS].
 * @param[in]     owner      The owner [BORROWS].
 * @param[in]     bytes      The payload bytes.
 * @param[in]     persistent Every bit set where the cell lies in the persistent tier.
 */
static void carcer_uncounted(SeriesCellularum *series, const CellOwner *owner, size_t bytes, size_t persistent)
{
    static void (*const tiers[2])(SeriesCellularum *, size_t) = {carcer_tier_none, carcer_tier_uncounted};

    (void)embed_atomic_usize_fetch_sub(&carcer_owner_record(owner)->in_use, bytes);
    (void)embed_atomic_usize_fetch_sub(&series->in_use, bytes);
    tiers[persistent & 1u](series, bytes);
}

/** @brief The row of a count table a refused take or a cell counted off already takes: nothing counted. */
static void carcer_count_none(SeriesCellularum *series, const CellOwner *owner, size_t bytes, size_t persistent)
{
    (void)series;
    (void)owner;
    (void)bytes;
    (void)persistent;
}

/**
 * @brief Takes the cell at off off the owner its owner word names, once.
 *
 * @param[in,out] series  Cellblock the cell sits in [BORROWS].
 * @param[in]     off        The cell's offset.
 * @param[in]     size       Its payload size.
 * @param[in]     persistent Every bit set where the cell lies in the persistent tier.
 * @param[in]     permit     Every bit set for a claim, none for a claim that touches nothing.
 * @note One fetch-or sets the completion bit on the owner word, on the sink where the claim is not permitted,
 *       and the context whose fetch-or found the bit clear counts the cell off: a give and a rewind reaching the
 *       same cell count it off once between them. A word naming no owner counts off the record counted by none.
 */
EMBED_INLINE void carcer_foot_claimed(SeriesCellularum *series, size_t off, size_t size, size_t persistent,
                                      size_t permit)
{
    static void (*const counts[2])(SeriesCellularum *, const CellOwner *, size_t, size_t) = {carcer_count_none,
                                                                                             carcer_uncounted};
    const embed_atomic_usize_value was = embed_atomic_usize_fetch_or(
        carcer_word_picked(series, permit, carcer_foot_of(off, size)), MMGR_OWNER_COMPLETE & permit);
    const size_t first =
        permit & carcer_all((was & MMGR_OWNER_COMPLETE) == 0u) & carcer_all(carcer_owner_place(was) != 0u);

    counts[first & 1u](series, carcer_owner_at(carcer_owner_place(was)), size, persistent);
}

/**
 * @brief A wake of nothing, the row of the wake table a clear sleeper flag takes.
 *
 * @param[in] word The word [BORROWS].
 */
static void carcer_wake_none(embed_atomic_word *word)
{
    (void)word;
}

/**
 * @brief The boundaries settled with the cellblock closed: each tier's overrun taken back off its boundary and
 *        zeroed, by plain stores.
 *
 * @param[in,out] series Cellblock to settle [BORROWS].
 * @note The writer's path, the first step of every exclusive section. With every pass drained, the persistent end
 *       is the chain's end plus the bytes of every bump the gap refused since the last settle, which is the
 *       persistent overrun; the temporary top is the chain's start less the temporary overrun. The stores put each
 *       boundary at its chain. docs/concepts/locus-carcerum-algorithm.md states the invariant and its proof.
 */
EMBED_INLINE void carcer_settled(SeriesCellularum *series)
{
    const embed_atomic_usize_value word = embed_atomic_usize_load(&series->bounds);
    const size_t end = carcer_bounds_end(word);
    const size_t top = carcer_bounds_top(word);
    // Explicit casts read the words as the byte counts they hold
    const size_t over_end = (size_t)embed_atomic_usize_load(&series->persistent_overrun);
    const size_t over_top = (size_t)embed_atomic_usize_load(&series->temporary_overrun);

    embed_atomic_usize_store(&series->bounds, carcer_bounds(end - over_end, top + over_top));
    embed_atomic_usize_store(&series->persistent_overrun, 0u);
    embed_atomic_usize_store(&series->temporary_overrun, 0u);
}

/**
 * @brief The cellblock closed to passes: the closed bit taken with one fetch-or, a backoff step on the word for
 *        each fetch-or that found it taken, and every pass begun before the drain waited out.
 *
 * @param[in,out] series Cellblock to close [BORROWS].
 * @note The writer's path. A pass that badges in after this reads the closed bit and touches nothing; a pass that
 *       badged in before is waited on by the drain. Two writers take turns on the bit. The boundaries are settled
 *       once the drain is over, so every exclusive section reads each at its chain.
 */
EMBED_INLINE void carcer_shut(SeriesCellularum *series)
{
    unsigned int step = CARCER_STEP_FIRST;
    embed_atomic_word_value was = embed_atomic_word_fetch_or(&series->closed, CARCER_CLOSED);

    while ((was & CARCER_CLOSED) != 0u)
    {
        step = embed_turnstile_backed_off(&series->closed, was, CARCER_CLOSED_SLEEPERS, step, 1u);
        was = embed_atomic_word_fetch_or(&series->closed, CARCER_CLOSED);
    }
    embed_turnstiles_drain(&series->turnstiles);
    carcer_settled(series);
}

/**
 * @brief The cellblock opened to passes again: the closed bit and the sleeper flag cleared with one fetch-and,
 *        and the sleepers on the word woken where the flag was set.
 *
 * @param[in,out] series Cellblock to open [BORROWS].
 */
EMBED_INLINE void carcer_opened(SeriesCellularum *series)
{
    static void (*const wakes[2])(embed_atomic_word *) = {carcer_wake_none, embed_word_wake};
    const embed_atomic_word_value was =
        embed_atomic_word_fetch_and(&series->closed, ~(CARCER_CLOSED | CARCER_CLOSED_SLEEPERS));

    wakes[(was >> 1u) & 1u](&series->closed);
}

/**
 * @brief The work a pass does, once its lane is claimed: its arguments, and a permit mask that is every bit set
 *        where the work may touch the cellblock and none where it lays its words on the sink.
 *
 * @param[in,out] series  Cellblock the pass is through [BORROWS].
 * @param[in]     context The work's arguments [BORROWS].
 * @param[in]     permit  Every bit set for the work, none for a pass that found the cellblock closed or no lane.
 * @return                The work's answer as an integer, 0 where the permit was none.
 */
typedef uintptr_t (*CarcerWork)(SeriesCellularum *series, const void *context, size_t permit);

/** @brief A leave of nothing, the row of the leave table a pass with no lane takes. */
static void carcer_leave_none(EmbedTurnstiles *set, unsigned int at)
{
    (void)set;
    (void)at;
}

/** @brief One row of touches, counted to a lane: its key and its touches. */
typedef struct
{
    embed_atomic_usize key;     /**< The badge id plus one, CARCER_ROW_OTHERS for the last row, zero where unused. */
    embed_atomic_usize touches; /**< The passes through the lane. */
} CarcerTouchRow;

/** @brief The key of the row that counts every lane past the others. */
#define CARCER_ROW_OTHERS (~(embed_atomic_usize_value)0u)

/**
 * @brief One movable cell's place in the translation table: the mutable word its const identity points at.
 *
 * @note word is the offset of the cell's payload in the cellblock, zero where the slot is free. No payload lies
 *       at offset zero, since a cell's header stands ahead of it.
 */
struct CarcerMovableSlot
{
    embed_atomic_usize word; /**< Where the cell stands. */
};

EMBED_STATIC_ASSERT(sizeof(CarcerMovableSlot) == sizeof(embed_atomic_usize),
                    "CarcerMovableSlot is not the one word MMGR_MOVABLE_STORAGE_BYTES counts");
EMBED_STATIC_ASSERT(sizeof(CarcerTouchRow) == (2u * sizeof(embed_atomic_usize)),
                    "CarcerTouchRow is not the two words MMGR_MOVABLE_STORAGE_BYTES counts");

/**
 * @brief The movable option's state, at the start of the storage the program gave.
 *
 * @note claimed is one bit a slot, set while the slot names a cell; the bits past cells in its last word are set
 *       for good. lowest is the word of claimed a claim's search starts at, lowered by each release. named is one
 *       past the highest slot named, which bounds the mover's reads.
 */
typedef struct
{
    CarcerMovableSlot *slot;             /**< The table. */
    size_t cells;                        /**< Slots at slot. */
    embed_atomic_usize *claimed;         /**< One bit a slot, set while it names a cell. */
    size_t claimed_words;                /**< Words at claimed. */
    embed_atomic_usize lowest;           /**< The word of claimed a search starts at. */
    embed_atomic_usize named;            /**< One past the highest slot named. */
    CarcerTouchRow *row;                 /**< The rows of touches, one a lane, the last for the lanes past them. */
    size_t rows;                         /**< Rows at row. */
    size_t width;                        /**< The lanes open bound the mover reads the cellblock quiet under. */
    embed_atomic_usize passes;           /**< Passes through the turnstiles, which the mover reads as pressure. */
    embed_bool wipes;                    /**< Whether a cell left behind is zeroed first. */
    size_t (*thread_id)(void);           /**< The calling thread's id, for the badge of a pass with no owner. */
    embed_bool (*thread_wait)(uint32_t); /**< The mover's sleep. */
    void (*thread_yield)(void);          /**< What the mover calls between moves. */
    void (*thread_moving)(embed_bool);   /**< Told as a sort's moves start and end. */
} CarcerMovable;

EMBED_STATIC_ASSERT(sizeof(CarcerMovable) <= MMGR_MOVABLE_HEAD_BYTES,
                    "CarcerMovable has outgrown MMGR_MOVABLE_HEAD_BYTES");

static size_t carcer_thread_none(void);
static void carcer_yield_none(void);
static void carcer_moving_none(embed_bool moving);

/** @brief A slot that names no cell, where a call with no cell reads. */
static CarcerMovableSlot carcer_slot_none = {EMBED_ATOMIC_INIT(0u)};

/** @brief The claim word of the option with none, where a release with no cell clears nothing. */
static embed_atomic_usize carcer_claimed_none = EMBED_ATOMIC_INIT(0u);

/** @brief A row of touches no lane counts to, where a reader past the rows reads. */
static CarcerTouchRow carcer_row_none = {EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};

/**
 * @brief The option a cellblock with none reads: the slot of none and the claim word of none as its table, and the
 *        row of none as its rows, so that a release with no cell and a reader of the rows index an object, a width of
 *        none, and calls of nothing.
 */
static CarcerMovable carcer_movable_none = {&carcer_slot_none,
                                            0u,
                                            &carcer_claimed_none,
                                            0u,
                                            EMBED_ATOMIC_INIT(0u),
                                            EMBED_ATOMIC_INIT(0u),
                                            &carcer_row_none,
                                            0u,
                                            0u,
                                            EMBED_ATOMIC_INIT(0u),
                                            EMBED_FALSE,
                                            carcer_thread_none,
                                            NULL,
                                            carcer_yield_none,
                                            carcer_moving_none};

/**
 * @brief A cellblock's movable option.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @return           The option's state, or NULL without it [BORROWS].
 */
EMBED_INLINE CarcerMovable *carcer_movable_of(const SeriesCellularum *series)
{
    // Explicit casts read the word's integer back as the address it holds
    return (CarcerMovable *)(uintptr_t)embed_atomic_uptr_load(&series->movable);
}

/** @brief The option's row a lane touches: the lane's own, or the last for a lane past the rows. */
static void carcer_touched(CarcerMovable *movable, unsigned int at, embed_atomic_word_value id)
{
    const size_t last = movable->rows - 1u;
    const size_t past = carcer_all((size_t)at >= last);
    CarcerTouchRow *const row = &movable->row[carcer_pick(past, last, (size_t)at)];

    // Explicit cast carries the id into the word the key holds it in
    embed_atomic_usize_store(&row->key, carcer_pick(past, (size_t)CARCER_ROW_OTHERS, (size_t)id + 1u));
    (void)embed_atomic_usize_fetch_add(&row->touches, 1u);
}

/** @brief The row of the touch table an option with no rows takes. */
static void carcer_touched_none(CarcerMovable *movable, unsigned int at, embed_atomic_word_value id)
{
    (void)movable;
    (void)at;
    (void)id;
}

/** @brief One pass counted to the option, for the mover's reading of pressure. */
static void carcer_pass_counted(CarcerMovable *movable)
{
    (void)embed_atomic_usize_fetch_add(&movable->passes, 1u);
}

/** @brief The row of the count table a cellblock with no option takes. */
static void carcer_pass_counted_none(CarcerMovable *movable)
{
    (void)movable;
}

/**
 * @brief One pass: a badge-in by id, the closed bit read, the work done with the permit the lane and the bit
 *        allow, and the lane let go.
 *
 * @param[in,out] series  Cellblock the pass is through [BORROWS].
 * @param[in]     id      The badge's id: an owner's place, or a thread's id.
 * @param[in]     work    The work [BORROWS].
 * @param[in]     context Its arguments [BORROWS].
 * @param[out]    missed  Every bit set where the pass found the cellblock closed or no lane [BORROWS].
 * @return                The work's answer, 0 where the pass missed.
 * @note The permit is one mask: the lane's claim and the closed bit as read after the badge-in. A pass that badged
 *       in before a writer's drain read the lanes is waited on by that drain; one that badged in after reads the
 *       closed bit. The leave is a row of a two-row table, the row whether a lane was claimed.
 */
static uintptr_t carcer_attempted(SeriesCellularum *series, embed_atomic_word_value id, CarcerWork work,
                                  const void *context, size_t *missed)
{
    static void (*const leaves[2])(EmbedTurnstiles *, unsigned int) = {embed_turnstile_leave, carcer_leave_none};
    static void (*const touches[2])(CarcerMovable *, unsigned int, embed_atomic_word_value) = {carcer_touched_none,
                                                                                               carcer_touched};
    static void (*const counts[2])(CarcerMovable *) = {carcer_pass_counted_none, carcer_pass_counted};
    const unsigned int at = embed_turnstile_enter(&series->turnstiles, id);
    const size_t none = carcer_all(at == EMBED_TURNSTILE_NONE);
    const size_t closed = carcer_all((embed_atomic_word_load(&series->closed) & CARCER_CLOSED) != 0u);
    const size_t permit = ~(none | closed);
    CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = permit & carcer_all(movable != NULL);
    CarcerMovable *const option = carcer_pick_at(has, movable, &carcer_movable_none);
    const size_t has_rows = has & carcer_all(option->rows != 0u);

    counts[has & 1u](option);
    touches[has_rows & 1u](option, at, id);
    const uintptr_t answer = work(series, context, permit);

    leaves[none & 1u](&series->turnstiles, at);
    *missed = ~permit;
    return answer;
}

/**
 * @brief Work carried through the turnstiles: one pass, and for a pass that missed a backoff step on the closed
 *        word and the badge-in again, until a pass lands.
 *
 * @param[in,out] series  Cellblock the work is on [BORROWS].
 * @param[in]     id      The badge's id.
 * @param[in]     work    The work [BORROWS].
 * @param[in]     context Its arguments [BORROWS].
 * @return                The work's answer.
 * @note The hot path is the one pass. A miss is the cellblock closed to a writer or no lane free, and the reader
 *       badges in again after a backoff step: a pause for the first steps, then a sleep on the closed word that
 *       doubles to EMBED_TURNSTILE_BACKOFF_MOST_MS, which the writer's open wakes. Every repeat is a pass that
 *       found the cellblock closed.
 */
EMBED_INLINE uintptr_t carcer_passed(SeriesCellularum *series, embed_atomic_word_value id, CarcerWork work,
                                     const void *context)
{
    unsigned int step = CARCER_STEP_FIRST;
    size_t missed = 0u;
    uintptr_t answer = carcer_attempted(series, id, work, context, &missed);

    while (missed != 0u)
    {
        step = embed_turnstile_backed_off(&series->closed, embed_atomic_word_load(&series->closed),
                                          CARCER_CLOSED_SLEEPERS, step, 1u);
        answer = carcer_attempted(series, id, work, context, &missed);
    }
    return answer;
}

/** @brief The arguments of a take. */
typedef struct
{
    size_t want;            /**< Payload wanted, rounded. */
    const CellOwner *owner; /**< The owner the cell counts to, listed [BORROWS]. */
    size_t down;            /**< Every bit set for the tier that grows down. */
    size_t permit;          /**< Every bit set for a take, none for one refused ahead of the pass. */
} CarcerTakeCtx;

/**
 * @brief Lays a cell's two words and counts it, where a mask is set; on the sink and to nothing where it is clear.
 *
 * @param[in,out] series  Cellblock the cell lies in [BORROWS].
 * @param[in]     taken      Every bit set for a cell.
 * @param[in]     off        The cell's offset.
 * @param[in]     size       Its payload size.
 * @param[in]     owner      The owner it counts to [BORROWS].
 * @param[in]     persistent Every bit set where the cell lies in the persistent tier.
 * @return                   The payload's address as an integer, 0 where the mask is clear.
 */
EMBED_INLINE uintptr_t carcer_laid(SeriesCellularum *series, size_t taken, size_t off, size_t size,
                                   const CellOwner *owner, size_t persistent)
{
    static void (*const counts[2])(SeriesCellularum *, const CellOwner *, size_t, size_t) = {carcer_count_none,
                                                                                             carcer_counted};

    embed_atomic_usize_store(carcer_word_picked(series, taken, carcer_foot_of(off, size)), carcer_owner_word(owner));
    embed_atomic_usize_store(carcer_word_picked(series, taken, off), carcer_word(size, CARCER_BUSY));
    counts[taken & 1u](series, owner, size, persistent);
    // Explicit cast carries the base as the integer the offset is added to and the mask is applied to: a refused bump's
    // offset lies past the cellblock, and the sum is an integer's, never a pointer's
    return ((uintptr_t)series->base + off + CARCER_WORD) & (uintptr_t)taken;
}

/**
 * @brief Raises the high-water mark of the tier a take moved.
 *
 * @param[in,out] series Cellblock to record against [BORROWS].
 * @param[in]     end    The persistent end after the take.
 * @param[in]     top    The temporary top after the take.
 */
EMBED_INLINE void carcer_marked(SeriesCellularum *series, size_t end, size_t top)
{
    CARCER_HW(series, persistent_hw, end);
    CARCER_HW(series, temporary_hw, series->size - top);
}

/**
 * @brief A take out of the gap, the work of a pass: the tier's boundary bumped by one fetch-add, the room read off
 *        the other boundary, the bump's bytes added to the tier's overrun where the gap could not meet it, and
 *        the cell's words laid.
 *
 * @param[in,out] series  Cellblock to take from [BORROWS].
 * @param[in]     context The take's arguments [BORROWS].
 * @param[in]     permit  Every bit set for the take, none for a pass that lays its words on the sink.
 * @return                The payload's address as an integer, 0 where the gap could not meet the take.
 * @note The upward tier's end moves by the cell's bytes and the downward tier's top by their negation, one
 *       fetch-add either way, and the cell is the bytes the bump covered: from the old end upward, or from the
 *       new top upward. The room is the other boundary as read after the bump: a cell that crosses it, a bump
 *       that wrapped below the cellblock, or a boundary read past the cellblock's size, which is another tier's
 *       bump wrapped, answers 0. A refused bump moves nothing back: the boundary stays where the bump moved it and
 *       the bytes go on the tier's overrun by one more fetch-add, 0 for a bump that landed. Between writers a
 *       boundary only moves toward the other, so every bump of the tier after a refused one is refused too, and
 *       the writer the refused take goes on to settles the boundary first. docs/concepts/locus-carcerum-algorithm.md
 *       states the invariant and its proof.
 */
static uintptr_t carcer_bumped(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerTakeCtx *const take = context;
    const size_t down = take->down;
    const size_t up = ~down;
    // the word read ahead: a bump up whose half already stands past the cellblock's most bytes moves nothing, so the
    // bumps a round refuses move the end into the half's headroom and never carry into the top's half; a want past
    // MMGR_CARCER_WANT_MOST goes to the writer's path unmoved, which bounds what the bumps in flight add
    const embed_atomic_usize_value ahead = embed_atomic_usize_load(&series->bounds);
    const size_t within = carcer_all(take->want <= MMGR_CARCER_WANT_MOST) &
                          (down | carcer_all(carcer_bounds_end(ahead) <= MMGR_CARCER_BYTES_MOST));
    const size_t allowed = permit & take->permit & within;
    const size_t need = (CARCER_HDR + take->want) & allowed;
    // Explicit casts carry the need into the word: added to the low half for a bump up, taken off the high half for
    // a bump down, where a borrow leaves the word and the low half stands
    const embed_atomic_usize_value moved =
        (embed_atomic_usize_value)carcer_pick(down, (0u - need) << MMGR_CARCER_HALF_BITS, need);
    embed_atomic_usize *const overrun = carcer_pick_at(down, &series->temporary_overrun, &series->persistent_overrun);
    // the one fetch-add: the bump's own boundary moved, and the other read as it stood at that same instant
    const embed_atomic_usize_value word = embed_atomic_usize_fetch_add(&series->bounds, moved);
    const size_t end_was = carcer_bounds_end(word);
    const size_t top_was = carcer_bounds_top(word);
    const size_t was = carcer_pick(down, top_was, end_was);
    const size_t across = carcer_pick(down, end_was, top_was);
    const size_t now = carcer_pick(down, top_was - need, end_was + need);
    // a boundary past the cellblock's size is the other tier's bump wrapped: no room either way
    const size_t fits_up = carcer_all(now <= across) & carcer_all(now >= was) & carcer_all(across <= series->size);
    const size_t fits_down = carcer_all(now >= across) & carcer_all(now <= was) & carcer_all(now <= series->size);
    const size_t taken = allowed & ((fits_up & up) | (fits_down & down));
    const size_t off = carcer_pick(down, now, was);

    (void)embed_atomic_usize_fetch_add(overrun, need & ~taken);
    carcer_marked(series, carcer_pick(taken & up, now, 0u), carcer_pick(taken & down, now, series->size));
    return carcer_laid(series, taken, off, take->want, take->owner, up);
}

/** @brief What one walk of the persistent tier found: a free run that holds the take, and the end it left. */
typedef struct
{
    size_t found; /**< Every bit set where a run was taken. */
    size_t at;    /**< The run's offset. */
    size_t size;  /**< The run's payload size, its cells merged. */
    size_t end;   /**< The persistent end after the walk: the trailing free run returned to the gap. */
} CarcerSweep;

/**
 * @brief One walk of the persistent tier with the cellblock closed: every run of free cells merged into one free
 *        cell, the first run that holds want and lies below an offset taken, and the free run reaching the end
 *        returned to the gap.
 *
 * @param[in,out] series Cellblock to walk [BORROWS].
 * @param[in]     want   Payload wanted, rounded.
 * @param[in]     below  The offset the run taken must end at or below.
 * @param[in]     permit Every bit set for the walk, none for a walk that reads nothing and takes nothing.
 * @return               What the walk found.
 * @note The writer's path, with no pass under way: every header read is a laid cell's, and every store is plain.
 *       The loop steps a cell at a time and its body holds no branch: a busy cell closes the run before it, the
 *       run is laid as one free cell on its own offset or on the sink, and the take is the first closed run that
 *       fits. A run that reaches the end is not laid: the end moves down to its offset.
 * @warning Costs the tier's cells. Reached only where the gap could not meet a take, and by the mover.
 */
EMBED_INLINE CarcerSweep carcer_swept(SeriesCellularum *series, size_t want, size_t below, size_t permit)
{
    const size_t end = carcer_bounds_end(embed_atomic_usize_load(&series->bounds));
    const size_t walked = end & permit;
    size_t off = 0u;
    size_t run_at = 0u;
    size_t run_size = 0u;
    CarcerSweep sweep = {0u, 0u, 0u, end};

    while (off < walked)
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(series, off));
        const size_t size = carcer_word_size(word);
        const size_t free = carcer_all(carcer_word_state(word) == CARCER_FREE);
        const size_t open = carcer_all(run_size != 0u);
        const size_t closes = ~free & open;
        const size_t takes = closes & ~sweep.found & carcer_all(run_size >= want) & carcer_all(off <= below);

        embed_atomic_usize_store(carcer_word_picked(series, closes, carcer_foot_of(run_at, run_size)), 0u);
        embed_atomic_usize_store(carcer_word_picked(series, closes, run_at), carcer_word(run_size, CARCER_FREE));
        sweep.at = carcer_pick(takes, run_at, sweep.at);
        sweep.size = carcer_pick(takes, run_size, sweep.size);
        sweep.found |= takes;
        run_at = carcer_pick(free & ~open, off, run_at);
        run_size = (run_size + size + (CARCER_HDR & open)) & free;
        off = carcer_end_of(off, size);
    }
    sweep.end = carcer_pick(carcer_all(run_size != 0u), run_at, end);
    return sweep;
}

/**
 * @brief The temporary top walked up over the free cells at it, with the cellblock closed.
 *
 * @param[in] series Cellblock to read [BORROWS].
 * @param[in] permit Every bit set for the walk, none for a walk that answers the top as it stands.
 * @return           The offset of the first busy cell above the top, or the cellblock's size where none stands.
 * @note The writer's path: every header above the top is a laid cell's. The loop steps a free cell at a time and
 *       its body holds no branch.
 */
EMBED_INLINE size_t carcer_top_walked(const SeriesCellularum *series, size_t permit)
{
    size_t at = carcer_bounds_top(embed_atomic_usize_load(&series->bounds));
    size_t more = permit & carcer_all(at < series->size);

    while (more != 0u)
    {
        const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(series, at));
        const size_t free = carcer_all(carcer_word_state(word) == CARCER_FREE);

        at += carcer_end_of(0u, carcer_word_size(word)) & free;
        more &= free & carcer_all(at < series->size);
    }
    return at;
}

/**
 * @brief Lays a take over a free run it was found in: the run split where its rest holds a cell of its own.
 *
 * @param[in,out] series Cellblock the run lies in [BORROWS].
 * @param[in]     sweep  The run, found or not.
 * @param[in]     want   Payload wanted, rounded.
 * @return               The payload the take carries: want where the run splits, the whole run where it does not,
 *                       and want where no run was found.
 * @note The rest is laid as a free cell behind the take, its header and its owner word of none, on the sink where
 *       the run does not split.
 */
EMBED_INLINE size_t carcer_split(SeriesCellularum *series, CarcerSweep sweep, size_t want)
{
    const size_t splits = sweep.found & carcer_all(sweep.size >= (want + CARCER_HDR + MMGR_CARCER_ALIGN));
    const size_t rest_at = carcer_end_of(sweep.at, want);
    const size_t rest_size = (sweep.size - want - CARCER_HDR) & splits;

    embed_atomic_usize_store(carcer_word_picked(series, splits, carcer_foot_of(rest_at, rest_size)), 0u);
    embed_atomic_usize_store(carcer_word_picked(series, splits, rest_at), carcer_word(rest_size, CARCER_FREE));
    return carcer_pick(sweep.found & ~splits, sweep.size, want);
}

/**
 * @brief A take the gap could not meet, with the cellblock closed: for a persistent take the tier walked for a free
 *        run and the end trimmed of its trailing free run, for a temporary take the top trimmed up over the free
 *        cells at it, and the gap read exactly and bumped where it holds the take.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     want   Payload wanted, rounded.
 * @param[in]     owner  The owner the cell counts to [BORROWS].
 * @param[in]     down   Every bit set for the tier that grows down.
 * @return               The payload's address as an integer, 0 where no free run and no gap holds the take.
 * @note The writer's path. Both boundaries are stored plain: no pass is under way. The free cells a give left at
 *       the top while another writer held the closed bit are returned to the gap here.
 */
static uintptr_t carcer_spilled(SeriesCellularum *series, size_t want, const CellOwner *owner, size_t down)
{
    const size_t up = ~down;
    const size_t need = CARCER_HDR + want;

    carcer_shut(series);
    const CarcerSweep sweep = carcer_swept(series, want, series->size, up);
    const size_t top = carcer_top_walked(series, down);
    const size_t room = carcer_all(top >= sweep.end) & carcer_all((top - sweep.end) >= need) & ~sweep.found;
    const size_t end = sweep.end + (need & room & up);
    const size_t top_after = top - (need & room & down);
    const size_t off = carcer_pick(sweep.found, sweep.at, carcer_pick(down, top_after, sweep.end));
    const size_t kept = carcer_split(series, sweep, want);

    embed_atomic_usize_store(&series->bounds, carcer_bounds(end, top_after));
    carcer_marked(series, end, top_after);
    const uintptr_t prisoner = carcer_laid(series, sweep.found | room, off, kept, owner, up);

    carcer_opened(series);
    return prisoner;
}

/** @brief The row of the spill table a take that landed in the gap takes. */
static uintptr_t carcer_spilled_none(SeriesCellularum *series, size_t want, const CellOwner *owner, size_t down)
{
    (void)series;
    (void)want;
    (void)owner;
    (void)down;
    return 0u;
}

/**
 * @brief The take both tiers and both security levels share: the owner tested, the bump through the turnstiles,
 *        and the exclusive path where the gap could not meet it.
 *
 * @param[in,out] series Cellblock to take from [BORROWS].
 * @param[in]     size   Bytes wanted.
 * @param[in]     owner  The owner named, or NULL [BORROWS].
 * @param[in]     down   Every bit set for the tier that grows down.
 * @param[in]     permit Every bit set for a take, none for one refused ahead of this.
 * @return               The cell, or NULL [RETURNS OWNERSHIP].
 * @note An owner not of the table permits nothing, and the pass runs on the record counted by none. The exclusive
 *       path is a row of a two-row table, the row whether the bump answered a cell.
 */
EMBED_INLINE void *carcer_taken(SeriesCellularum *series, size_t size, const CellOwner *owner, size_t down,
                                size_t permit)
{
    static uintptr_t (*const spills[2])(SeriesCellularum *, size_t, const CellOwner *, size_t) = {carcer_spilled_none,
                                                                                                  carcer_spilled};
    const size_t listed = permit & carcer_all(mmgr_owner_listed(owner));
    const CellOwner *const safe = carcer_pick_at(listed, carcer_owner_record(owner), &carcer_owner_none);
    const CarcerTakeCtx take = {carcer_want(size), safe, down, listed};
    // Explicit cast carries the owner's place as the badge's id
    const uintptr_t bumped = carcer_passed(series, (embed_atomic_word_value)safe->place, carcer_bumped, &take);
    const size_t spilled = listed & carcer_all(bumped == 0u);

    // Explicit cast reads the integer back as the address it holds
    return (void *)(bumped | spills[spilled & 1u](series, take.want, safe, down));
}

void *mmgr_persistent_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner)
{
    return carcer_taken(series, size, owner, 0u, carcer_all(1));
}

void *mmgr_persistent_max_security_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner)
{
    return carcer_taken(series, size, owner, 0u, carcer_all(1));
}

void *mmgr_temporary_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner)
{
    return carcer_taken(series, size, owner, carcer_all(1), carcer_all(1));
}

void *mmgr_temporary_max_security_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner)
{
    return carcer_taken(series, size, owner, carcer_all(1), carcer_all(1));
}

void mmgr_zero_buf(void *prisoner, size_t size)
{
    // the run stores one machine word a step, so its boundary and its step are the word's width, which the cell
    // grain may exceed
    const size_t width = sizeof(embed_word);
    // Explicit cast takes the prisoner to a volatile byte pointer, the scope the edge walks use
    volatile uint8_t *walk = (volatile uint8_t *)prisoner;
    size_t left = size;
    // Explicit casts take walk to uintptr_t for the alignment test, then that result to the size_t edge is
    // carried in
    size_t edge = (size_t)(((uintptr_t)walk) & (width - 1u));

    // Head: bytes up to the first word boundary, where the run below starts aligned
    edge = carcer_pick(carcer_all(edge != 0u), width - edge, 0u);
    edge = carcer_pick(carcer_all(edge < left), edge, left);
    carcer_zero_bytes(&walk, &left, edge);

    // Explicit casts go through volatile void * to reach the word scope the run stores in. The head above
    // leaves walk on a word boundary whenever it had the bytes to reach one
    volatile embed_word *word_walk = (volatile embed_word *)(volatile void *)walk;

    while (left >= width)
    {
        // Store, pointer advance and count advance are separate statements; *word_walk++ = 0 would put an
        // increment inside the volatile store. Explicit cast gives the zero the word scope
        *word_walk = (embed_word)0;
        word_walk++;
        left -= width;
    }

    // Explicit casts return to the byte scope for the tail, which is under one word
    walk = (volatile uint8_t *)(volatile void *)word_walk;
    carcer_zero_bytes(&walk, &left, left);
}

/** @brief The stop a prisoner from another cellblock meets: there is no state to carry on from. */
static void carcer_stopped(void)
{
    MMGR_FATAL("a prisoner was handed to a cellblock that does not hold it");
}

/** @brief The row of the stop table a prisoner of this cellblock takes. */
static void carcer_stopped_none(void)
{
}

/**
 * @brief Whether a prisoner is this cellblock's, with the program stopped where it is another's.
 *
 * @param[in] series   Cellblock to read [BORROWS].
 * @param[in] prisoner The prisoner, or NULL [BORROWS].
 * @param[in] given    Every bit set where a prisoner was given at all.
 * @note A prisoner of another cellblock has no state to carry on from, and the stop is a row of a two-row table.
 */
EMBED_INLINE void carcer_owned(const SeriesCellularum *series, const void *prisoner, size_t given)
{
    static void (*const stops[2])(void) = {carcer_stopped_none, carcer_stopped};
    const size_t foreign = given & ~carcer_all(mmgr_who_owns_buf(series, prisoner));

    stops[foreign & 1u]();
}

/**
 * @brief A cell as a give or a growth reads it: its offset, its size, and whether the owner named is its own.
 */
typedef struct
{
    size_t off;      /**< The cell's offset. */
    size_t size;     /**< Its payload size. */
    size_t proceeds; /**< Every bit set where the cell is the owner's and still counted. */
} CarcerCell;

/**
 * @brief Reads a prisoner's cell for a give or a growth naming an owner.
 *
 * @param[in] series   Cellblock the prisoner came from [BORROWS].
 * @param[in] prisoner First byte of the payload, a cell of this cellblock [BORROWS].
 * @param[in] owner    The owner named [BORROWS].
 * @param[in] permit   Every bit set for the call.
 * @return             The cell, proceeding where its owner word names owner and is not complete.
 * @note A word counted off already names no owner any more and nothing is told. A word naming another listed owner
 *       is told to the port through mmgr_carcer_hw_misnamed, a row of a two-row table.
 */
EMBED_INLINE CarcerCell carcer_cell_read(const SeriesCellularum *series, const void *prisoner, const CellOwner *owner,
                                         size_t permit)
{
    static void (*const tells[2])(const CellOwner *, const CellOwner *, size_t) = {carcer_told_none,
                                                                                   mmgr_carcer_hw_misnamed};
    const size_t off = carcer_off_of(series, prisoner);
    const size_t size = carcer_size_at(series, off);
    const embed_atomic_usize_value named = embed_atomic_usize_load(carcer_word_at(series, carcer_foot_of(off, size)));
    const size_t counted = permit & carcer_all((named & MMGR_OWNER_COMPLETE) == 0u);
    const size_t mine = counted & carcer_owner_is(named, owner);
    const CellOwner *const found = carcer_owner_found(named);
    const size_t another = counted & ~mine & carcer_all(found != NULL);
    const CarcerCell cell = {off, size, mine};

    tells[another & 1u](owner, found, size);
    return cell;
}

/**
 * @brief A tier's boundary trimmed with the cellblock closed: the persistent end down to the first free run
 *        reaching it, the temporary top up over the free cells at it.
 *
 * @param[in,out] series  Cellblock to trim, its closed bit this context's [BORROWS].
 * @param[in]     persistent Every bit set for the persistent tier, none for the temporary one.
 * @note The writer's path, reached by a give of the cell at its tier's boundary, once the give has set the
 *       header free: the tier winds back as its newest cells go, and a cellblock whose every cell went back
 *       reads idle. The persistent tier's walk merges every free run on the way.
 */
static void carcer_trimmed_held(SeriesCellularum *series, size_t persistent)
{
    embed_turnstiles_drain(&series->turnstiles);
    carcer_settled(series);
    const CarcerSweep sweep = carcer_swept(series, SIZE_MAX, series->size, persistent);
    const size_t top = carcer_top_walked(series, ~persistent);

    embed_atomic_usize_store(&series->bounds, carcer_bounds(sweep.end, top));
    carcer_opened(series);
}

/** @brief The row of the trim table a give below its tier's boundary, or one that found the closed bit held, takes. */
static void carcer_trimmed_none(SeriesCellularum *series, size_t persistent)
{
    (void)series;
    (void)persistent;
}

/**
 * @brief A tier's boundary trimmed where the closed bit is free to take: one fetch-or, and the trim where the
 *        bit was clear.
 *
 * @param[in,out] series  Cellblock to trim [BORROWS].
 * @param[in]     persistent Every bit set for the persistent tier, none for the temporary one.
 * @note A give waits for no writer: where another holds the bit, the cell stays free in its tier for that
 *       writer's walk, a later trim, a take the gap cannot meet or a rewind, and the fetch-or changed nothing.
 */
static void carcer_trimmed(SeriesCellularum *series, size_t persistent)
{
    static void (*const trims[2])(SeriesCellularum *, size_t) = {carcer_trimmed_none, carcer_trimmed_held};
    const embed_atomic_word_value was = embed_atomic_word_fetch_or(&series->closed, CARCER_CLOSED);

    trims[(unsigned int)((was & CARCER_CLOSED) == 0u)](series, persistent);
}

/** @brief The bit of a give's answer that is set where the cell went back. */
#define CARCER_GIVEN_BACK ((size_t)1u)

/** @brief The bit of a give's answer that is set where the cell stood at its tier's boundary. */
#define CARCER_GIVEN_AT_BOUNDARY ((size_t)2u)

/** @brief The bit of a give's answer that is set where the cell lay in the persistent tier. */
#define CARCER_GIVEN_PERSISTENT ((size_t)4u)

/**
 * @brief The give both security levels share: the cell read, the payload zeroed where asked, the owner word
 *        claimed and the bytes counted off, and the header set free.
 *
 * @param[in,out] site      Cellblock the prisoner came from, NULL where there is none [BORROWS].
 * @param[in]     prisoner  First byte of the payload, or NULL [TAKES OWNERSHIP].
 * @param[in]     owner     The owner named, or NULL [BORROWS].
 * @param[in]     zeroing   Every bit set where the payload is zeroed before the give.
 * @param[in]     permit    Every bit set for the give.
 * @return                  CARCER_GIVEN_BACK where the cell went back, with CARCER_GIVEN_AT_BOUNDARY where it stood
 *                          at its tier's boundary and CARCER_GIVEN_PERSISTENT where it lay in the persistent tier.
 * @note No pass: a busy cell's words are its holder's alone, and the compaction reads a header free only after
 *       this store, which comes last. The tier is read off the end: a persistent cell lies below it. The caller
 *       trims the boundary a cell stood at, outside any pass of its own.
 * @note A give naming another owner, or a cell given back already, changes nothing, the payload included: every
 *       store lands on the sink and every count is a row of none. A give with no cellblock, no prisoner or no owner
 *       reads the record over bytes of none and changes nothing.
 */
EMBED_INLINE size_t carcer_given(SeriesCellularum *site, void *prisoner, const CellOwner *owner, size_t zeroing,
                                 size_t permit)
{
    const size_t given = permit & carcer_all(site != NULL) & carcer_all(prisoner != NULL) & carcer_all(owner != NULL);
    SeriesCellularum *const series = carcer_pick_at(given, site, &carcer_block_none);
    void *const safe = carcer_pick_at(given, prisoner, series->base + CARCER_WORD);
    const CellOwner *const named = carcer_pick_at(given, carcer_owner_record(owner), &carcer_owner_none);

    carcer_owned(series, prisoner, given);
    const CarcerCell cell = carcer_cell_read(series, safe, named, given);
    const embed_atomic_usize_value word = embed_atomic_usize_load(&series->bounds);
    const size_t end = carcer_bounds_end(word);
    const size_t top = carcer_bounds_top(word);
    const size_t persistent = carcer_all(cell.off < end);
    const size_t at_boundary =
        carcer_pick(persistent, carcer_all(carcer_end_of(cell.off, cell.size) == end), carcer_all(cell.off == top));

    mmgr_zero_buf(safe, cell.size & zeroing & cell.proceeds);
    carcer_foot_claimed(series, cell.off, cell.size, persistent, cell.proceeds);
    embed_atomic_usize_store(carcer_word_picked(series, cell.proceeds, cell.off), carcer_word(cell.size, CARCER_FREE));
    return cell.proceeds &
           (CARCER_GIVEN_BACK | (CARCER_GIVEN_AT_BOUNDARY & at_boundary) | (CARCER_GIVEN_PERSISTENT & persistent));
}

/**
 * @brief A give as the release entries make it: the give, and the boundary trimmed where the cell stood at it.
 *
 * @param[in,out] series   Cellblock the prisoner came from [BORROWS].
 * @param[in]     prisoner First byte of the payload, or NULL [TAKES OWNERSHIP].
 * @param[in]     owner    The owner named, or NULL [BORROWS].
 * @param[in]     zeroing  Every bit set where the payload is zeroed before the give.
 * @note The trim is a row of a two-row table, the row the give's boundary bit.
 */
EMBED_INLINE void carcer_released(SeriesCellularum *series, void *prisoner, const CellOwner *owner, size_t zeroing)
{
    static void (*const trims[2])(SeriesCellularum *, size_t) = {carcer_trimmed_none, carcer_trimmed};
    const size_t given = carcer_given(series, prisoner, owner, zeroing, carcer_all(1));

    trims[(given & CARCER_GIVEN_AT_BOUNDARY) >> 1u](series, carcer_all((given & CARCER_GIVEN_PERSISTENT) != 0u));
}

void mmgr_persistent_buf_release(SeriesCellularum *series, void *prisoner, const CellOwner *owner)
{
    carcer_released(series, prisoner, owner, 0u);
}

void mmgr_persistent_max_security_buf_release(SeriesCellularum *series, void *prisoner, const CellOwner *owner)
{
    carcer_released(series, prisoner, owner, carcer_all(1));
}

/**
 * @brief The growth both grow entries share, with the cellblock closed: the newest cell of its tier grown in
 *        place, the persistent end up or the temporary top down, by plain stores.
 *
 * @param[in,out] site      Cellblock the prisoner came from, NULL where there is none [BORROWS].
 * @param[in,out] prisoner  First byte of the payload, or NULL [BORROWS].
 * @param[in]     size      Payload bytes the cell is to carry.
 * @param[in]     owner     The owner the cell's owner word names, or NULL [BORROWS].
 * @param[in]     zeroing   Every bit set where the bytes the growth adds are zeroed.
 * @return                  The cell after the growth, moved for a temporary cell; the prisoner itself where it
 *                          carries size already; or NULL with nothing changed.
 * @note The writer's path. A persistent cell is the newest where the end stands at its end, a temporary cell where
 *       the top stands at its header; the gap is read exactly with no pass under way. A persistent cell keeps its
 *       address, its owner word written past the growth and its header with the new size. A temporary cell moves
 *       down by the growth: its header written at the new top, its payload moved down, its owner word standing
 *       where it stood. The bytes the growth adds are zeroed where asked.
 */
static void *carcer_grown(SeriesCellularum *site, void *prisoner, size_t size, const CellOwner *owner, size_t zeroing)
{
    static void (*const counts[2])(SeriesCellularum *, const CellOwner *, size_t, size_t) = {carcer_count_none,
                                                                                             carcer_counted};
    const size_t given = carcer_all(site != NULL) & carcer_all(prisoner != NULL) & carcer_all(owner != NULL);
    SeriesCellularum *const series = carcer_pick_at(given, site, &carcer_block_none);
    void *const safe = carcer_pick_at(given, prisoner, series->base + CARCER_WORD);
    const CellOwner *const named = carcer_pick_at(given, carcer_owner_record(owner), &carcer_owner_none);

    carcer_owned(series, prisoner, given);
    carcer_shut(series);
    const CarcerCell cell = carcer_cell_read(series, safe, named, given);
    const size_t want = carcer_want(size);
    const size_t grows = cell.proceeds & carcer_all(want > cell.size);
    const size_t extra = (want - cell.size) & grows;
    const embed_atomic_usize_value word = embed_atomic_usize_load(&series->bounds);
    const size_t end = carcer_bounds_end(word);
    const size_t top = carcer_bounds_top(word);
    const size_t up = carcer_all(cell.off < end);
    const size_t newest =
        carcer_pick(up, carcer_all(end == carcer_end_of(cell.off, cell.size)), carcer_all(top == cell.off));
    const size_t room = carcer_all(top >= end) & carcer_all((top - end) >= extra);
    const size_t moving = grows & newest & room;
    const size_t at = cell.off - (extra & moving & ~up);
    const size_t grown = carcer_pick(moving, want, cell.size);

    embed_atomic_usize_store(&series->bounds, carcer_bounds(end + (extra & moving & up), top - (extra & moving & ~up)));
    // the words: a persistent cell's owner word past the growth, a temporary cell's header at the new top
    embed_atomic_usize_store(carcer_word_picked(series, moving & up, carcer_foot_of(cell.off, want)),
                             carcer_owner_word(named));
    embed_atomic_usize_store(carcer_word_picked(series, moving, at), carcer_word(grown, CARCER_BUSY));
    memor.move_down(&(MemoriaCfg){.dst = series->base + at + CARCER_WORD,
                                  .src = series->base + cell.off + CARCER_WORD,
                                  .bytes = cell.size & moving & ~up});
    // the bytes added: past the old owner word for a persistent cell, past the moved payload for a temporary one
    mmgr_zero_buf(series->base + at + CARCER_WORD + cell.size, extra & zeroing & moving);
    counts[(moving & 1u)](series, named, extra, up);
    carcer_marked(series, end + (extra & moving & up), top - (extra & moving & ~up));
    carcer_opened(series);
    // Explicit cast carries the payload's address as the integer the mask is applied to, and back
    return (void *)((uintptr_t)(series->base + at + CARCER_WORD) & (uintptr_t)(moving | (cell.proceeds & ~grows)));
}

void *mmgr_buf_grow(SeriesCellularum *series, void *prisoner, size_t size, const CellOwner *owner)
{
    return carcer_grown(series, prisoner, size, owner, 0u);
}

void *mmgr_max_security_buf_grow(SeriesCellularum *series, void *prisoner, size_t size, const CellOwner *owner)
{
    return carcer_grown(series, prisoner, size, owner, carcer_all(1));
}

size_t mmgr_temporary_buf_mark(const SeriesCellularum *series)
{
    return mmgr_series_cellularum_temporary_top(series);
}

/**
 * @brief Rewinds the temporary tier to mark with the cellblock closed: every cell from the top to mark counted off
 *        its owner, the dropped bytes zeroed where asked, and the top stored at mark.
 *
 * @param[in,out] series  Cellblock to rewind [BORROWS].
 * @param[in]     mark    Top to restore, as mmgr_temporary_buf_mark reported it.
 * @param[in]     zeroing Every bit set where the dropped bytes are zeroed.
 * @note The writer's path. A mark at or below the top as it stands, or past the cellblock's size, is one this
 *       cellblock never handed out, and the rewind stops at the top: the walk is empty and the store puts the top
 *       where it was. The walk steps a cell at a time, every cell laid, and counts each off through its owner
 *       word's completion bit, once.
 */
static void carcer_rewound(SeriesCellularum *series, size_t mark, size_t zeroing)
{
    carcer_shut(series);
    const embed_atomic_usize_value word = embed_atomic_usize_load(&series->bounds);
    const size_t top = carcer_bounds_top(word);
    const size_t valid = carcer_all(mark <= series->size) & carcer_all(mark > top);
    const size_t to = carcer_pick(valid, mark, top);
    size_t off = top;

    while (off < to)
    {
        const size_t size = carcer_size_at(series, off);

        carcer_foot_claimed(series, off, size, 0u, carcer_all(1));
        off = carcer_end_of(off, size);
    }
    mmgr_zero_buf(series->base + top, (to - top) & zeroing);
    embed_atomic_usize_store(&series->bounds, carcer_bounds(carcer_bounds_end(word), to));
    carcer_opened(series);
}

void mmgr_temporary_buf_release(SeriesCellularum *series, size_t mark)
{
    carcer_rewound(series, mark, 0u);
}

void mmgr_max_security_buf_return(SeriesCellularum *series, size_t mark)
{
    carcer_rewound(series, mark, carcer_all(1));
}

void mmgr_temporary_buf_reset(SeriesCellularum *series)
{
    mmgr_temporary_buf_release(series, series->size);
}

const CellOwner *mmgr_owner_of_buf(const SeriesCellularum *series, const void *prisoner)
{
    const size_t owns = carcer_all(prisoner != NULL) & carcer_all(mmgr_who_owns_buf(series, prisoner));
    const void *const safe = carcer_pick_at(owns, (void *)(uintptr_t)prisoner, series->base + CARCER_WORD);
    const size_t off = carcer_off_of(series, safe);
    const embed_atomic_usize_value word = embed_atomic_usize_load(carcer_word_at(series, off));
    const size_t size = carcer_word_size(word);
    const size_t busy = owns & carcer_all(carcer_word_state(word) == CARCER_BUSY) &
                        carcer_all(carcer_end_of(off, size) <= series->size);
    const size_t foot = carcer_pick(busy, carcer_foot_of(off, size), off);

    return carcer_pick_at(busy, carcer_owner_found(embed_atomic_usize_load(carcer_word_at(series, foot))), NULL);
}

void mmgr_owner_bytes_taken(const CellOwner *owner, size_t bytes)
{
    CellOwner *const record = carcer_owner_record(owner);
    // Explicit cast reads the count before the add, size_t's width, as the bytes it holds
    const size_t in_use = (size_t)embed_atomic_usize_fetch_add(&record->in_use, bytes) + bytes;

    (void)embed_atomic_usize_raise(&record->hw, in_use);
}

void mmgr_owner_bytes_given(const CellOwner *owner, size_t bytes)
{
    (void)embed_atomic_usize_fetch_sub(&carcer_owner_record(owner)->in_use, bytes);
}

size_t mmgr_series_cellularum_fragmentation(const SeriesCellularum *series)
{
    const size_t occupied = mmgr_series_cellularum_persistent_end(series);
    // Explicit cast reads the word as the bytes it counts
    const size_t in_use = (size_t)embed_atomic_usize_load(&series->persistent_in_use);
    const size_t holes = (occupied - in_use) & carcer_all(occupied > in_use);
    const size_t measured = carcer_all(occupied >= 100u);

    // The hundredth of the bytes occupied divides, since a product of the holes and a hundred can pass what a
    // size_t counts on a narrow word; a tier under a hundred bytes divides by one and answers 0
    return (holes / carcer_pick(measured, occupied / 100u, 1u)) & measured;
}

/**
 * @brief The place of the lowest bit set in a word, 0 for a word of none.
 *
 * @param[in] bits The word.
 * @return         The place from 0.
 */
EMBED_INLINE size_t carcer_lowest(embed_atomic_usize_value bits)
{
    // Explicit casts widen the word to the count's operand, which is as wide as any claim word, every bit set
    // standing in for a word of none; the lowest bit isolated and placed by six masked steps
    const unsigned long long whole = (unsigned long long)bits | (0ull - (unsigned long long)(bits == 0u));
    const unsigned long long low = whole & (0ull - whole);
    size_t place = 0u;

    place += ((low & 0xFFFFFFFF00000000ull) != 0u) ? 32u : 0u;
    place += ((low & 0xFFFF0000FFFF0000ull) != 0u) ? 16u : 0u;
    place += ((low & 0xFF00FF00FF00FF00ull) != 0u) ? 8u : 0u;
    place += ((low & 0xF0F0F0F0F0F0F0F0ull) != 0u) ? 4u : 0u;
    place += ((low & 0xCCCCCCCCCCCCCCCCull) != 0u) ? 2u : 0u;
    place += ((low & 0xAAAAAAAAAAAAAAAAull) != 0u) ? 1u : 0u;
    return place;
}

/**
 * @brief One claim on the table: the word of claimed with a free bit nearest the search's start found, its lowest
 *        free bit set with one fetch-or, and the slot named where the fetch-or found the bit clear.
 *
 * @param[in,out] movable The option [BORROWS].
 * @param[in]     payload The payload offset the slot is to hold.
 * @param[in]     permit  Every bit set for a claim, none for one that takes nothing.
 * @return                The slot, or NULL where every slot is named or another claim set the same bit first.
 * @note The loop steps every word of claimed from the start, wrapping, and its body holds no branch: the first
 *       word with a free bit is picked by mask. The start moves to the word claimed.
 */
static CarcerMovableSlot *carcer_slot_claimed(CarcerMovable *movable, size_t payload, size_t permit)
{
    const size_t words = movable->claimed_words;
    // Explicit cast reads the word as the index it holds
    const size_t from = (size_t)embed_atomic_usize_load(&movable->lowest);
    size_t at_word = 0u;
    size_t got = 0u;

    for (size_t tried = 0u; tried < words; tried += 1u)
    {
        const size_t reached = from + tried;
        const size_t place = reached - (words & carcer_all(reached >= words));
        const embed_atomic_usize_value free = ~embed_atomic_usize_load(&movable->claimed[place]);
        const size_t has = carcer_all(free != 0u);

        at_word = carcer_pick(has & ~got, place, at_word);
        got |= has;
    }
    const embed_atomic_usize_value free = ~embed_atomic_usize_load(&movable->claimed[at_word]);
    const embed_atomic_usize_value bit = free & (0u - free) & permit;
    const embed_atomic_usize_value was = embed_atomic_usize_fetch_or(&movable->claimed[at_word], bit);
    const size_t won = carcer_all((bit & ~was) != 0u);
    const size_t index = (at_word * MMGR_MOVABLE_CLAIM_BITS) + carcer_lowest(bit);
    CarcerMovableSlot *const slot =
        carcer_pick_at(won, &movable->slot[index & carcer_all(index < movable->cells)], &carcer_slot_none);

    embed_atomic_usize_store(&slot->word, payload & won);
    embed_atomic_usize_store(&movable->lowest, carcer_pick(won, at_word, from));
    (void)embed_atomic_usize_raise(&movable->named, (index + 1u) & won);
    return carcer_pick_at(won, slot, NULL);
}

/** @brief The row of the claim table a take that answered no cell takes. */
static CarcerMovableSlot *carcer_slot_claimed_none(CarcerMovable *movable, size_t payload, size_t permit)
{
    (void)movable;
    (void)payload;
    (void)permit;
    return NULL;
}

/** @brief A give of a cell no slot could name, the row of the give table that take takes. */
static void carcer_given_back(SeriesCellularum *series, void *prisoner, const CellOwner *owner, size_t zeroing)
{
    carcer_released(series, prisoner, owner, zeroing);
}

/** @brief The row of the give table a named cell takes. */
static void carcer_given_none(SeriesCellularum *series, void *prisoner, const CellOwner *owner, size_t zeroing)
{
    (void)series;
    (void)prisoner;
    (void)owner;
    (void)zeroing;
}

const CarcerMovableSlot *mmgr_movable_buf_alloc(SeriesCellularum *series, size_t size, const CellOwner *owner)
{
    static CarcerMovableSlot *(*const claims[2])(CarcerMovable *, size_t, size_t) = {carcer_slot_claimed_none,
                                                                                     carcer_slot_claimed};
    static void (*const gives[2])(SeriesCellularum *, void *, const CellOwner *, size_t) = {carcer_given_none,
                                                                                            carcer_given_back};
    CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = carcer_all(movable != NULL);
    void *const prisoner = carcer_taken(series, size, owner, 0u, has);
    const size_t taken = carcer_all(prisoner != NULL);
    // Explicit casts take the payload's address to the offset the slot holds
    const size_t payload = (size_t)((uint8_t *)prisoner - series->base) & taken;
    CarcerMovableSlot *const first = claims[taken & 1u](movable, payload, carcer_all(1));
    const size_t missed = taken & carcer_all(first == NULL);
    CarcerMovableSlot *const slot = carcer_pick_at(missed, claims[missed & 1u](movable, payload, carcer_all(1)), first);
    const CarcerMovable *const option = carcer_pick_at(has, movable, &carcer_movable_none);
    const size_t wipes = carcer_all(option->wipes != 0u);

    gives[(taken & carcer_all(slot == NULL)) & 1u](series, prisoner, owner, wipes);
    return slot;
}

/** @brief The arguments of a movable release. */
typedef struct
{
    CarcerMovable *movable;  /**< The option, the none option for a cellblock without one [BORROWS]. */
    CarcerMovableSlot *slot; /**< The slot, the one naming no cell for a NULL identity [BORROWS]. */
    const CellOwner *owner;  /**< The owner named [BORROWS]. */
} CarcerSlotGiveCtx;

/**
 * @brief A movable release, the work of a pass: the slot read, the cell given back where the owner is its own, the
 *        slot's word and its bit of claimed cleared, and the search's start lowered to its word.
 *
 * @param[in,out] series  Cellblock the cell is in [BORROWS].
 * @param[in]     context The release's arguments [BORROWS].
 * @param[in]     permit  Every bit set for the release.
 * @return                The give's bits where the cell went back, 0 where the slot named none, the owner was
 *                        another's, or the pass missed.
 */
static uintptr_t carcer_slot_given(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerSlotGiveCtx *const give = context;
    CarcerMovable *const movable = give->movable;
    // Explicit cast reads the word as the offset it holds
    const size_t payload = (size_t)embed_atomic_usize_load(&give->slot->word);
    const size_t named = permit & carcer_all(payload != 0u);
    void *const prisoner = series->base + (payload & named) + (CARCER_WORD & ~named);
    const size_t wipes = carcer_all(movable->wipes != 0u);
    const size_t given = carcer_given(series, prisoner, give->owner, wipes, named);
    const size_t went = carcer_all((given & CARCER_GIVEN_BACK) != 0u);
    // Explicit cast reads the slot's distance from the table's start as the place it is
    const size_t place = (size_t)(give->slot - movable->slot) & went;
    const size_t word = place / MMGR_MOVABLE_CLAIM_BITS;
    // Explicit cast carries the bit's place into the word it is a bit of
    const embed_atomic_usize_value bit = ((embed_atomic_usize_value)1u << (place % MMGR_MOVABLE_CLAIM_BITS)) & went;
    embed_atomic_usize *const claimed = carcer_pick_at(went, &movable->claimed[word], &series->sink);
    // Explicit cast reads the word as the index it holds
    const size_t lowest = (size_t)embed_atomic_usize_load(&movable->lowest);

    embed_atomic_usize_store(carcer_pick_at(went, &give->slot->word, &series->sink), 0u);
    (void)embed_atomic_usize_fetch_and(claimed, ~bit);
    embed_atomic_usize_store(&movable->lowest, carcer_pick(went & carcer_all(word < lowest), word, lowest));
    return given;
}

embed_bool mmgr_movable_buf_release(SeriesCellularum *series, const CarcerMovableSlot *cell, const CellOwner *owner)
{
    CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = carcer_all(movable != NULL) & carcer_all(cell != NULL) & carcer_all(owner != NULL);
    // Explicit cast takes the const off the identity: the slot is MMgr's to write, and the caller's const says
    // only that the caller does not
    const CarcerSlotGiveCtx give = {carcer_pick_at(has, movable, &carcer_movable_none),
                                    carcer_pick_at(has, (CarcerMovableSlot *)(uintptr_t)cell, &carcer_slot_none),
                                    carcer_pick_at(has, carcer_owner_record(owner), &carcer_owner_none)};
    static void (*const trims[2])(SeriesCellularum *, size_t) = {carcer_trimmed_none, carcer_trimmed};
    const CellOwner *const named = give.owner;
    // Explicit casts carry the owner's place as the badge's id, and the pass's integer back as the give's bits
    const size_t given =
        (size_t)carcer_passed(series, (embed_atomic_word_value)named->place, carcer_slot_given, &give) & has;

    // the boundary trimmed after the pass: a writer waits out every pass, this one's among them
    trims[(given & CARCER_GIVEN_AT_BOUNDARY) >> 1u](series, carcer_all((given & CARCER_GIVEN_PERSISTENT) != 0u));
    // Explicit cast narrows the answer's low bit to the embed_bool container
    return (embed_bool)(given & CARCER_GIVEN_BACK);
}

/**
 * @brief The badge id of a pass with no owner: the thread's id plus one where the option has thread_id, 1 without.
 *
 * @param[in] movable The option [BORROWS].
 * @return            The id, nonzero.
 */
EMBED_INLINE embed_atomic_word_value carcer_thread_badge(const CarcerMovable *movable)
{
    // Explicit cast carries the thread's id into the badge's word
    return (embed_atomic_word_value)movable->thread_id() + 1u;
}

/** @brief The thread id a program that gave none reads: 0 for every thread. */
static size_t carcer_thread_none(void)
{
    return 0u;
}

/**
 * @brief Where a span of a movable cell starts, and whether it fits the cell.
 */
typedef struct
{
    uint8_t *at; /**< The span's first byte, the cellblock's first byte where it does not fit [BORROWS]. */
    size_t fits; /**< Every bit set where the slot names a cell and the span lies in it. */
    size_t room; /**< The bytes from the span's start to the cell's end. */
} CarcerSpan;

/**
 * @brief Reads a slot and places a span in its cell.
 *
 * @param[in] series Cellblock the cell is in [BORROWS].
 * @param[in] slot   The slot, the one naming no cell for a NULL identity [BORROWS].
 * @param[in] offset Where in the cell the span starts.
 * @param[in] bytes  The span's bytes.
 * @param[in] permit Every bit set for the call.
 * @return           The span placed.
 */
EMBED_INLINE CarcerSpan carcer_span_read(const SeriesCellularum *series, const CarcerMovableSlot *slot, size_t offset,
                                         size_t bytes, size_t permit)
{
    // Explicit cast reads the word as the offset it holds
    const size_t payload = (size_t)embed_atomic_usize_load(&slot->word);
    const size_t named = permit & carcer_all(payload != 0u);
    const size_t size = carcer_size_at(series, (payload - CARCER_WORD) & named);
    const size_t fits = named & carcer_all(offset <= size) & carcer_all(bytes <= (size - offset));
    const CarcerSpan span = {series->base + ((payload + offset) & fits), fits, (size - offset) & fits};

    return span;
}

/** @brief The arguments of a read, a write, a compare or a measure of a movable cell. */
typedef struct
{
    const CarcerMovableCfg *args;   /**< The caller's arguments [BORROWS]. */
    const CarcerMovableSlot *cell;  /**< The cell's slot, the one naming no cell for NULL [BORROWS]. */
    const CarcerMovableSlot *other; /**< The other cell's slot, the one naming no cell for NULL [BORROWS]. */
    uint8_t *sink;                  /**< Where a copy of nothing lands [BORROWS]. */
} CarcerSpanCtx;

/** @brief A read, the work of a pass: args->bytes copied out of the cell where the span fits. */
static uintptr_t carcer_span_read_out(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerSpanCtx *const call = context;
    const CarcerSpan span = carcer_span_read(series, call->cell, call->args->offset, call->args->bytes, permit);

    memor.cpy(&(MemoriaCfg){.dst = carcer_pick_at(span.fits, call->args->dst, call->sink),
                            .src = span.at,
                            .bytes = call->args->bytes & span.fits});
    return span.fits & 1u;
}

/** @brief A write, the work of a pass: args->bytes copied into the cell where the span fits. */
static uintptr_t carcer_span_written(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerSpanCtx *const call = context;
    const CarcerSpan span = carcer_span_read(series, call->cell, call->args->offset, call->args->bytes, permit);

    memor.cpy(&(MemoriaCfg){.dst = carcer_pick_at(span.fits, span.at, call->sink),
                            .src = carcer_pick_at(span.fits, (void *)(uintptr_t)call->args->src, call->sink),
                            .bytes = call->args->bytes & span.fits});
    return span.fits & 1u;
}

/**
 * @brief A compare, the work of a pass: the cell's span ordered against args->src, or against the other cell's
 *        span where one is named; 1 where a span does not fit.
 *
 * @note The answer is an embed_iword carried in the pass's integer: the caller reads it back as such.
 */
static uintptr_t carcer_span_compared(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerSpanCtx *const call = context;
    const size_t against_cell = carcer_all(call->args->other_cell != NULL);
    const CarcerSpan span = carcer_span_read(series, call->cell, call->args->offset, call->args->bytes, permit);
    const CarcerSpan other =
        carcer_span_read(series, call->other, call->args->other_offset, call->args->bytes, permit & against_cell);
    const size_t fits = span.fits & (other.fits | ~against_cell);
    const void *const other_at = carcer_pick_at(against_cell, other.at, (void *)(uintptr_t)call->args->src);
    const embed_iword order =
        memor.cmp(&(MemoriaCfg){.src = carcer_pick_at(fits, span.at, call->sink),
                                .other = carcer_pick_at(fits, (void *)(uintptr_t)other_at, call->sink),
                                .bytes = call->args->bytes & fits});

    // Explicit casts carry the order, an embed_iword, as the pass's integer, 1 where a span did not fit
    return ((uintptr_t)order & (uintptr_t)fits) | ((uintptr_t)1u & ~(uintptr_t)fits);
}

/** @brief A measure, the work of a pass: the bytes from args->offset to the cell's next zero, or to its end. */
static uintptr_t carcer_span_measured(SeriesCellularum *series, const void *context, size_t permit)
{
    const CarcerSpanCtx *const call = context;
    const CarcerSpan span = carcer_span_read(series, call->cell, call->args->offset, 0u, permit);
    const uint8_t *const zero = memor.chr(&(MemoriaCfg){.src = span.at, .val = 0u, .bytes = span.room});
    const size_t found = carcer_all(zero != NULL);
    // Explicit cast reads the distance, which the cell's size bounds, as the bytes it is
    const size_t measured = carcer_pick(found, (size_t)(zero - span.at) & found, span.room);

    return (uintptr_t)(measured & span.fits);
}

/**
 * @brief A call on a movable cell carried through the turnstiles, badged with the calling thread's id.
 *
 * @param[in,out] series Cellblock the cell is in [BORROWS].
 * @param[in]     args   The caller's arguments [BORROWS].
 * @param[in]     work   The work [BORROWS].
 * @return               The work's answer, 0 without the option.
 */
EMBED_INLINE uintptr_t carcer_span_passed(const SeriesCellularum *series, const CarcerMovableCfg *args, CarcerWork work)
{
    // Explicit cast takes the const off the cellblock: a pass writes its turnstiles, and the caller's const says
    // only that the caller does not write the cells
    SeriesCellularum *const passed = (SeriesCellularum *)(uintptr_t)series;
    CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = carcer_all(movable != NULL);
    const CarcerSpanCtx call = {
        args, carcer_pick_at(carcer_all(args->cell != NULL), (void *)(uintptr_t)args->cell, &carcer_slot_none),
        carcer_pick_at(carcer_all(args->other_cell != NULL), (void *)(uintptr_t)args->other_cell, &carcer_slot_none),
        (uint8_t *)(void *)&passed->sink};
    // Explicit cast narrows the badge to the turnstile word's width, which bounds every badge
    const embed_atomic_word_value id = (embed_atomic_word_value)carcer_pick(
        has, (size_t)carcer_thread_badge(carcer_pick_at(has, movable, &carcer_movable_none)), 1u);

    return carcer_passed(passed, id, work, &call) & (uintptr_t)has;
}

embed_bool mmgr_movable_buf_read(const SeriesCellularum *series, const CarcerMovableCfg *args)
{
    // Explicit cast narrows the pass's integer to the embed_bool container
    return (embed_bool)carcer_span_passed(series, args, carcer_span_read_out);
}

embed_bool mmgr_movable_buf_write(SeriesCellularum *series, const CarcerMovableCfg *args)
{
    // Explicit cast narrows the pass's integer to the embed_bool container
    return (embed_bool)carcer_span_passed(series, args, carcer_span_written);
}

embed_iword mmgr_movable_buf_compare(const SeriesCellularum *series, const CarcerMovableCfg *args)
{
    const CarcerMovable *const movable = carcer_movable_of(series);
    const uintptr_t ordered = carcer_span_passed(series, args, carcer_span_compared);

    // Explicit cast reads the pass's integer back as the order it carries; a cellblock with no option orders 1
    return (embed_iword)carcer_pick(carcer_all(movable != NULL), (size_t)ordered, 1u);
}

size_t mmgr_movable_buf_measure(const SeriesCellularum *series, const CarcerMovableCfg *args)
{
    // Explicit cast reads the pass's integer back as the bytes it carries
    return (size_t)carcer_span_passed(series, args, carcer_span_measured);
}

/**
 * @brief The highest named cell below an offset that is smaller than a size.
 *
 * @param[in]  movable The option [BORROWS].
 * @param[in]  series  Cellblock the cells are in [BORROWS].
 * @param[in]  below   The offset the cell's payload lies below.
 * @param[in]  refused The size the cell is smaller than.
 * @param[out] payload The cell's payload offset, 0 where none is [BORROWS].
 * @return             Its slot, the one naming no cell where none is [BORROWS].
 * @note The loop steps every named slot and its body holds no branch: the highest that qualifies is picked by mask.
 */
EMBED_INLINE CarcerMovableSlot *carcer_slot_highest(const CarcerMovable *movable, const SeriesCellularum *series,
                                                    size_t below, size_t refused, size_t *payload)
{
    // Explicit cast reads the word as the slots it counts, which the table's size bounds
    const size_t named = (size_t)embed_atomic_usize_load(&movable->named);
    CarcerMovableSlot *highest = &carcer_slot_none;
    size_t highest_at = 0u;

    for (size_t place = 0u; place < named; place += 1u)
    {
        // Explicit cast reads the word as the offset it holds
        const size_t at = (size_t)embed_atomic_usize_load(&movable->slot[place].word);
        const size_t stands = carcer_all(at != 0u);
        const size_t size = carcer_size_at(series, (at - CARCER_WORD) & stands);
        const size_t better =
            stands & carcer_all(at > highest_at) & carcer_all(at < below) & carcer_all(size < refused);

        highest = carcer_pick_at(better, &movable->slot[place], highest);
        highest_at = carcer_pick(better, at, highest_at);
    }
    *payload = highest_at;
    return highest;
}

embed_bool mmgr_movable_buf_move(SeriesCellularum *series)
{
    const CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = carcer_all(movable != NULL);
    const CarcerMovable *const option = carcer_pick_at(has, (void *)(uintptr_t)movable, &carcer_movable_none);
    size_t below = series->size & has;
    size_t refused = series->size;
    size_t moved = 0u;
    size_t stands = has;

    carcer_shut(series);
    while ((stands & ~moved) != 0u)
    {
        size_t payload = 0u;
        CarcerMovableSlot *const slot = carcer_slot_highest(option, series, below, refused, &payload);

        stands = carcer_all(payload != 0u);
        const size_t off = (payload - CARCER_WORD) & stands;
        const size_t size = carcer_size_at(series, off);
        // the first free run below the cell that holds it, the tier merged and trimmed on the way
        const CarcerSweep sweep = carcer_swept(series, size, off, stands);
        const size_t kept = carcer_split(series, sweep, size);
        const size_t wipes = carcer_all(option->wipes != 0u);

        // the end after the move: the cell's offset where the cell stood at the end, the walk's end elsewhere
        const size_t at_end = sweep.found & carcer_all(carcer_end_of(off, size) == sweep.end);

        embed_atomic_usize_store(&series->bounds,
                                 carcer_bounds(carcer_pick(at_end, off, sweep.end),
                                               carcer_bounds_top(embed_atomic_usize_load(&series->bounds))));
        memor.cpy(
            &(MemoriaCfg){.dst = carcer_pick_at(sweep.found, series->base + sweep.at + CARCER_WORD, &series->sink),
                          .src = series->base + payload,
                          .bytes = size & sweep.found});
        // the copy's words: its owner word the cell's own, carried over, and its header; then the slot names it
        embed_atomic_usize_store(carcer_word_picked(series, sweep.found, carcer_foot_of(sweep.at, kept)),
                                 embed_atomic_usize_load(carcer_word_at(series, carcer_foot_of(off, size))));
        embed_atomic_usize_store(carcer_word_picked(series, sweep.found, sweep.at), carcer_word(kept, CARCER_BUSY));
        embed_atomic_usize_store(carcer_pick_at(sweep.found, &slot->word, &series->sink), sweep.at + CARCER_WORD);
        // the cell left behind goes free, its owner word none and its payload zeroed where the option wipes
        mmgr_zero_buf(series->base + payload, size & sweep.found & wipes);
        embed_atomic_usize_store(carcer_word_picked(series, sweep.found, carcer_foot_of(off, size)), 0u);
        embed_atomic_usize_store(carcer_word_picked(series, sweep.found, off), carcer_word(size, CARCER_FREE));
        moved |= sweep.found;
        refused = carcer_pick(stands & ~sweep.found, size, refused);
        below = carcer_pick(stands, payload, below);
    }
    carcer_opened(series);
    // Explicit cast narrows the mask's low bit to the embed_bool container
    return (embed_bool)(moved & 1u);
}

/**
 * @brief Whether the mover reads the cellblock quiet: twice the lanes open at or under the option's width plus
 *        one, and no pass since the last reading.
 *
 * @param[in]     series  Cellblock to read [BORROWS].
 * @param[in]     movable The option [BORROWS].
 * @param[in,out] passes  The passes at the last reading, moved to the passes now [BORROWS].
 * @return                Every bit set where it is quiet.
 */
EMBED_INLINE size_t carcer_quiet(const SeriesCellularum *series, const CarcerMovable *movable, size_t *passes)
{
    // Explicit cast reads the word as the count it holds
    const size_t now = (size_t)embed_atomic_usize_load(&movable->passes);
    const size_t steady = carcer_all(now == *passes);

    *passes = now;
    return steady & carcer_all(((size_t)embed_turnstile_lanes(&series->turnstiles) * 2u) <= (movable->width + 1u));
}

/**
 * @brief One sort: cells moved one at a time, the cellblock opened between moves and the processor yielded, until
 *        no cell moves or the lanes open rise past quiet.
 *
 * @param[in,out] series  Cellblock whose cells to move [BORROWS].
 * @param[in,out] movable The option [BORROWS].
 * @return                EMBED_TRUE where the sort ran until nothing more would move, EMBED_FALSE where the
 *                        pressure rose and it backed off.
 */
EMBED_INLINE embed_bool carcer_mover_sorted(SeriesCellularum *series, CarcerMovable *movable, size_t *passes)
{
    size_t steady = carcer_all(1);
    size_t moving = carcer_all(1);

    movable->thread_moving(EMBED_TRUE);
    while ((steady & moving) != 0u)
    {
        moving = carcer_all(mmgr_movable_buf_move(series));
        movable->thread_yield();
        steady = carcer_quiet(series, movable, passes);
    }
    movable->thread_moving(EMBED_FALSE);
    // Explicit cast narrows the mask's low bit to the embed_bool container
    return (embed_bool)(~moving & 1u);
}

/**
 * @brief The sort a reading calls for: run, and the wait and what it left set by whether it finished.
 *
 * @param[in,out] series  Cellblock whose cells to move [BORROWS].
 * @param[in,out] movable The option [BORROWS].
 * @param[in,out] wait    The milliseconds until the next reading, MMGR_MOVABLE_READ_MS after a sort that finished
 *                        and doubled up to MMGR_MOVABLE_BACKOFF_MOST_MS after one that backed off [BORROWS].
 * @param[in,out] left    The fragmentation a finished sort left [BORROWS].
 * @param[in,out] passes  The passes at the last reading [BORROWS].
 */
static void carcer_sort_run(SeriesCellularum *series, CarcerMovable *movable, uint32_t *wait, size_t *left,
                            size_t *passes)
{
    const size_t finished = carcer_all(carcer_mover_sorted(series, movable, passes));
    const size_t doubled = carcer_pick(carcer_all(((size_t)*wait * 2u) < MMGR_MOVABLE_BACKOFF_MOST_MS),
                                       (size_t)*wait * 2u, MMGR_MOVABLE_BACKOFF_MOST_MS);

    *left = carcer_pick(finished, mmgr_series_cellularum_fragmentation(series), *left);
    // Explicit cast narrows the pick to the wait's width, which both arms fit
    *wait = (uint32_t)carcer_pick(finished, MMGR_MOVABLE_READ_MS, doubled);
}

/** @brief The row of the sort table a reading that calls for no sort takes. */
static void carcer_sort_none(SeriesCellularum *series, CarcerMovable *movable, uint32_t *wait, size_t *left,
                             size_t *passes)
{
    (void)series;
    (void)movable;
    (void)wait;
    (void)left;
    (void)passes;
}

/**
 * @brief One of the mover's readings of its cellblock, and the sort it starts where the reading calls for one.
 *
 * @param[in,out] series  Cellblock whose cells to move [BORROWS].
 * @param[in,out] movable The option [BORROWS].
 * @param[in,out] wait    The milliseconds until the next reading [BORROWS].
 * @param[in,out] left    The fragmentation the last finished sort left, zero under the threshold [BORROWS].
 * @param[in,out] quiet   The readings in a row that found the cellblock quiet [BORROWS].
 * @param[in,out] passes  The passes at the last reading [BORROWS].
 * @note A reading under the threshold settles everything. One over it waits for MMGR_MOVABLE_QUIET_READINGS quiet
 *       readings in a row, then sorts; a sort that backed off doubles the wait up to MMGR_MOVABLE_BACKOFF_MOST_MS,
 *       and one that finished records what it left, which a reading no higher than it waits on.
 */
EMBED_INLINE void carcer_mover_read(SeriesCellularum *series, CarcerMovable *movable, uint32_t *wait, size_t *left,
                                    size_t *quiet, size_t *passes)
{
    static void (*const sorts_run[2])(SeriesCellularum *, CarcerMovable *, uint32_t *, size_t *,
                                      size_t *) = {carcer_sort_none, carcer_sort_run};
    const size_t percent = mmgr_series_cellularum_fragmentation(series);
    const size_t settled = carcer_all(percent < MMGR_MOVABLE_SETTLE_PERCENT);
    const size_t risen = ~settled & carcer_all(percent > *left);
    const size_t counted = risen & carcer_quiet(series, movable, passes);
    const size_t readings = (*quiet + 1u) & counted;
    const size_t sorts = risen & carcer_all(readings >= MMGR_MOVABLE_QUIET_READINGS);

    *left &= ~settled;
    *quiet = readings & ~settled & ~sorts;
    *wait = (uint32_t)carcer_pick(settled, MMGR_MOVABLE_READ_MS, *wait);
    sorts_run[sorts & 1u](series, movable, wait, left, passes);
}

void mmgr_movable_run(SeriesCellularum *series)
{
    CarcerMovable *const movable = carcer_movable_of(series);
    uint32_t wait = MMGR_MOVABLE_READ_MS;
    size_t left = 0u;
    size_t quiet = 0u;
    // Explicit cast reads the word as the count it holds
    size_t passes = (size_t)embed_atomic_usize_load(&movable->passes);

    while (movable->thread_wait(wait))
    {
        carcer_mover_read(series, movable, &wait, &left, &quiet, &passes);
    }
}

/**
 * @brief What the thread the program gave runs.
 *
 * @param[in,out] context The cellblock [BORROWS].
 */
static void carcer_mover_entered(void *context)
{
    mmgr_movable_run((SeriesCellularum *)context);
}

/** @brief A yield of nothing, for a program that gave no thread_yield. */
static void carcer_yield_none(void)
{
}

/** @brief A moving report of nothing, for a program that gave no thread_moving. */
static void carcer_moving_none(embed_bool moving)
{
    (void)moving;
}

/** @brief A thread start of nothing, for a program that gave no thread or no wait: the option assigned, nothing moving.
 */
static embed_bool carcer_started_none(void (*run)(void *context), void *context)
{
    (void)run;
    (void)context;
    return EMBED_TRUE;
}

embed_bool mmgr_movable_init(SeriesCellularum *series, const CarcerMovableInitCfg *args, embed_bool wipes)
{
    const size_t words = (args->cells + MMGR_MOVABLE_CLAIM_BITS - 1u) / MMGR_MOVABLE_CLAIM_BITS;
    const size_t fits =
        carcer_all(carcer_movable_of(series) == NULL) & carcer_all(args->storage != NULL) &
        carcer_all(args->cells != 0u) & carcer_all(args->width != 0u) &
        carcer_all(args->storage_bytes >= MMGR_MOVABLE_STORAGE_BYTES(args->cells, args->width, args->rows));

    if (fits == 0u)
    {
        return EMBED_FALSE;
    }
    uint8_t *const storage = (uint8_t *)args->storage;
    CarcerMovable *const movable = (CarcerMovable *)args->storage;
    uint8_t *const slots = storage + MMGR_MOVABLE_HEAD_BYTES;
    uint8_t *const claimed = slots + (args->cells * sizeof(embed_atomic_usize));
    uint8_t *const rows = claimed + (words * sizeof(embed_atomic_usize));

    movable->slot = (CarcerMovableSlot *)(void *)slots;
    movable->cells = args->cells;
    movable->claimed = (embed_atomic_usize *)(void *)claimed;
    movable->claimed_words = words;
    movable->row = (CarcerTouchRow *)(void *)rows;
    movable->rows = args->rows;
    movable->width = args->width;
    embed_atomic_usize_store(&movable->passes, 0u);
    movable->wipes = wipes;
    // a call the program gave none of is the call of nothing
    movable->thread_id = (args->thread_id != NULL) ? args->thread_id : carcer_thread_none;
    movable->thread_wait = args->thread_wait;
    movable->thread_yield = (args->thread_yield != NULL) ? args->thread_yield : carcer_yield_none;
    movable->thread_moving = (args->thread_moving != NULL) ? args->thread_moving : carcer_moving_none;
    // the bits past the cells in the last word of claimed are set for good: no claim lands on them
    const size_t spare = (words * MMGR_MOVABLE_CLAIM_BITS) - args->cells;
    const embed_atomic_usize_value past = ~(((embed_atomic_usize_value)1u << (MMGR_MOVABLE_CLAIM_BITS - spare)) - 1u) &
                                          (0u - (embed_atomic_usize_value)(spare != 0u));

    embed_atomic_usize_store(&movable->claimed[words - 1u], past);
    // Explicit cast carries the state's address as the integer the word holds
    embed_atomic_uptr_store(&series->movable, (embed_atomic_uptr_value)(uintptr_t)movable);
    const int threaded = (args->thread_started != NULL) && (args->thread_wait != NULL);
    embed_bool (*const started)(void (*)(void *), void *) = threaded ? args->thread_started : carcer_started_none;

    return started(carcer_mover_entered, series);
}

size_t mmgr_movable_queue_depth(const SeriesCellularum *series)
{
    const CarcerMovable *const movable = carcer_movable_of(series);

    // Explicit cast reads the readers queued as the count it is
    return (size_t)embed_atomic_u32_load(&series->turnstiles.waiting) & carcer_all(movable != NULL);
}

embed_bool mmgr_movable_touches(const SeriesCellularum *series, size_t at, size_t *id, size_t *touches)
{
    const CarcerMovable *const movable = carcer_movable_of(series);
    const size_t has = carcer_all(movable != NULL);
    const CarcerMovable *const option = carcer_pick_at(has, (void *)(uintptr_t)movable, &carcer_movable_none);
    const size_t rows = has & carcer_all(at < option->rows);
    const CarcerTouchRow *const row = carcer_pick_at(rows, &option->row[at & rows], &carcer_row_none);
    const embed_atomic_usize_value key = embed_atomic_usize_load(&row->key);
    const size_t used = rows & carcer_all(key != 0u);
    const size_t others = used & carcer_all(key == CARCER_ROW_OTHERS);

    // Explicit casts read the words as the id and the count they hold
    *id = carcer_pick(others, SIZE_MAX, ((size_t)key - 1u) & used);
    *touches = (size_t)embed_atomic_usize_load(&row->touches) & used;
    // Explicit cast narrows the mask's low bit to the embed_bool container
    return (embed_bool)(used & 1u);
}

embed_bool mmgr_who_owns_buf(const SeriesCellularum *series, const void *at)
{
    // Explicit casts to uintptr_t let one unsigned compare cover both ends: below base wraps high. Explicit cast
    // narrows the int result of < to the embed_bool container
    return (embed_bool)(((uintptr_t)at - (uintptr_t)series->base) < series->size);
}

size_t mmgr_buf_available(const SeriesCellularum *series)
{
    const embed_atomic_usize_value word = embed_atomic_usize_load(&series->bounds);
    const size_t end = carcer_bounds_end(word);
    const size_t top = carcer_bounds_top(word);

    // a top below the end is a refused bump's overrun not yet settled: the gap reads 0
    return (top - end) & carcer_all(top >= end);
}

size_t mmgr_align_up_buf(size_t size)
{
    return carcer_round(size);
}

void mmgr_series_cellularum_emptied(SeriesCellularum *series)
{
    embed_atomic_usize_store(&series->bounds, carcer_bounds(0u, series->size));
    embed_atomic_usize_store(&series->persistent_overrun, 0u);
    embed_atomic_usize_store(&series->temporary_overrun, 0u);
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    embed_atomic_usize_store(&series->persistent_hw, 0u);
    embed_atomic_usize_store(&series->temporary_hw, 0u);
#endif
    embed_atomic_usize_store(&series->in_use, 0u);
    embed_atomic_usize_store(&series->persistent_in_use, 0u);
    embed_atomic_uptr_store(&series->movable, 0u);
    embed_atomic_word_store(&series->closed, 0u);
    embed_atomic_usize_store(&series->sink, 0u);
    memor.set(&(MemoriaCfg){.dst = &series->turnstiles, .val = 0u, .bytes = sizeof(series->turnstiles)});
}
