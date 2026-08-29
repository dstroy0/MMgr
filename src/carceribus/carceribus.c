/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file carceribus.c
 * @brief Double-ended pool: one block allocator, run from both ends of the same free middle.
 *
 * @note Both ends carve through the same grow and merge through the same coalesce. Only the boundary
 *       direction differs: the persistent end grows up from base, the interim end grows down from
 *       size. The fit walk and the split are the persistent end's alone; the interim take carves and
 *       nothing more.
 * @note A tenancy from either end can be released one at a time, since the release reads which end it
 *       came from off its address. The interim end can also be given back by mark, or all at once.
 * @note Nothing is cleared on hand-out, so a take returns whatever the last tenant left. Of the five
 *       releases here two clear first; a pool's declaration decides which of a pair its entries reach.
 * @note Reaches nothing outside config.
 */
#include "carceribus/carceribus.h"

/**
 * @brief What a block carries ahead of its payload.
 *
 * @note size counts the payload alone. Every walk here adds CARCER_HDR itself to step to the next
 *       block, and every fit test compares against the payload.
 * @note The header lies immediately ahead of the bytes handed out, so a tenancy's header is reached
 *       by subtracting CARCER_HDR from its address and no chain is walked to find it.
 */
typedef struct
{
    size_t size; /**< Payload bytes behind this header. */
    size_t used; /**< 0 while the block is free, 1 while a tenant holds it. */
} CarcerBlk;

/**
 * @brief Bytes a block header occupies, rounded up so the payload behind it stays aligned.
 *
 * @note The rounding is a mask, and a mask rounds only because MMGR_CARCER_ALIGN is a power of two,
 *       which carceribus.h asserts.
 * @note Charged on top of the payload every time a block is carved, so a take of size bytes costs
 *       the middle this much more than size.
 */
#define CARCER_HDR ((sizeof(CarcerBlk) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief The half-open offset range one end's chain occupies.
 *
 * @note lo and hi are offsets from the pool's base, not addresses; carcer_blk is what turns one into
 *       a header pointer.
 * @note Both chains are walked from lo upward, so the fit and the merge take a chain and no
 *       direction argument. Only the merge is handed both of them; the fit belongs to the persistent
 *       end.
 */
typedef struct
{
    size_t lo; /**< Offset of the first block in the chain. */
    size_t hi; /**< One past the last byte the chain occupies. */
} CarcerChain;

/**
 * @brief Rounds want up to a whole machine word.
 *
 * @param[in] want Count to round.
 * @return         want rounded up to a multiple of MMGR_CARCER_ALIGN, and want itself when it
 *                 already is one.
 * @note The mask is what rounds, which holds only because MMGR_CARCER_ALIGN is a power of two.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask, so a want within a word of SIZE_MAX wraps
 *          to 0. Every take rounds its request through here, and a request that wraps is met with a
 *          small block rather than refused.
 */
MMGR_INLINE size_t carcer_round(size_t want)
{
    return (want + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u);
}

/**
 * @brief Zeroes want bytes at *walk, advancing the pointer and taking them off *left.
 *
 * @param[in,out] walk Walking pointer, left one past the last byte cleared [BORROWS].
 * @param[in,out] left Bytes still to clear, reduced by want [BORROWS].
 * @param[in]     want Bytes to clear now.
 * @note Used for the two edges of the wipe. The word-wide middle between them does its own stores,
 *       through its own volatile pointer, and does not come through here.
 * @note The stores are volatile, so clearing bytes nothing reads afterwards is not dropped as dead
 *       work.
 * @warning want comes off *left with no test, so a want above *left wraps it. Both call sites hold
 *          want at or below what is left.
 * @warning *walk must be writable for want bytes.
 */
MMGR_INLINE void carcer_zero_bytes(volatile uint8_t **walk, size_t *left, size_t want)
{
    size_t index = 0u;

    while (index < want)
    {
        // Store, pointer advance and count advance are separate statements; folding them into
        // *(*walk)++ = 0u would put an increment inside the volatile store
        **walk = 0u;
        (*walk)++;
        index++;
    }
    *left -= want;
}

/**
 * @brief Reads the block header at offset off in the pool.
 *
 * @param[in] pool Pool to look in [BORROWS].
 * @param[in] off  Offset of the header, always a multiple of MMGR_CARCER_ALIGN.
 * @return         The header [BORROWS].
 * @note The cast goes through void *. base is aligned by the region macro and every offset a chain
 *       walks is a whole number of words, so the header is always correctly aligned.
 * @warning The pool is const here and the header is not. The fit, the split and the merge all hold a
 *          const pool and write through what this hands back.
 * @warning off is added to base with nothing holding it against the pool's size. Every caller here
 *          walks a chain whose bounds came from the pool, except the two releases, which pass an
 *          offset taken from the address the caller handed them.
 */
MMGR_INLINE CarcerBlk *carcer_blk(const CarcerCtx *pool, size_t off)
{
    return (CarcerBlk *)(void *)(pool->base + off);
}

/**
 * @brief Steps past the block at off to the next one in its chain.
 *
 * @param[in] pool Pool the chain runs in [BORROWS].
 * @param[in] off  Offset of the block to step past.
 * @return         Offset of the block after it.
 * @note A block is its header plus its payload; every walk here steps by that.
 * @warning The step is the block's own recorded size, and the result is not held against the pool.
 *          It reaches or passes the chain's hi at the end of a walk, which is what each loop test is
 *          there for.
 */
MMGR_INLINE size_t carcer_next(const CarcerCtx *pool, size_t off)
{
    return off + CARCER_HDR + carcer_blk(pool, off)->size;
}

/**
 * @brief Returns the offset of a tenancy's own header.
 *
 * @param[in] pool Pool the tenancy came from [BORROWS].
 * @param[in] at   First byte of the tenancy [BORROWS].
 * @return         Offset of its header in the pool.
 * @warning at is taken to be a tenancy of this pool and nothing tests that it is. The releases hand
 *          on whatever they were given, so an address from elsewhere yields an offset that reads
 *          bytes which are not a header. mmgr_carcer_owns bounds an address to the pool but does not
 *          say a tenancy begins there, so it narrows this and does not close it.
 */
MMGR_INLINE size_t carcer_off_of(const CarcerCtx *pool, const void *at)
{
    // Explicit casts take at to a byte pointer so the difference is in bytes, then that ptrdiff_t to
    // the size_t the offset is carried in. An at below base makes the difference negative and the
    // cast wraps it high, which is the unchecked case the warning above describes
    return (size_t)((const uint8_t *)at - pool->base) - CARCER_HDR;
}

/**
 * @brief Returns the chain the persistent end keeps.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         lo of 0, hi of persist_end.
 * @note A copy of the bounds as they stand at the call, not a view of them. A release that trims
 *       persist_end leaves a chain taken before it naming a hi the end no longer reaches.
 */
MMGR_INLINE CarcerChain carcer_up(const CarcerCtx *pool)
{
    CarcerChain ch;

    ch.lo = 0u;
    ch.hi = pool->persist_end;
    return ch;
}

/**
 * @brief Returns the chain the interim end keeps.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         lo of interim_top, hi of the pool's size.
 * @note A copy of the bounds as they stand at the call, not a view of them. Here it is lo that moves,
 *       since every take at this end lowers the top; the persistent end's lo is always 0.
 */
MMGR_INLINE CarcerChain carcer_down(const CarcerCtx *pool)
{
    CarcerChain ch;

    ch.lo = pool->interim_top;
    ch.hi = pool->size;
    return ch;
}

/**
 * @brief Splits block walk when what is left would hold another block.
 *
 * @param[in]     pool Pool the block sits in [BORROWS].
 * @param[in,out] walk Block to split, left carrying want [BORROWS].
 * @param[in]     off  Offset of walk in the pool.
 * @param[in]     want Payload the first half keeps.
 * @note Only splits when the remainder can carry a header and a payload of its own; otherwise the
 *       tenant keeps the whole block and its slack.
 * @note The remainder is left free where it lies. Nothing links it to its neighbors and nothing
 *       merges it here; a later walk steps onto it and the merge finds it then.
 * @warning off must be walk's own offset. The two are not checked against each other, and a
 *          mismatched pair writes the second header where no block begins.
 */
MMGR_INLINE void carcer_split(const CarcerCtx *pool, CarcerBlk *walk, size_t off, size_t want)
{
    // Split only when the tail left over can carry its own header and a whole word behind it
    if (walk->size >= (want + CARCER_HDR + MMGR_CARCER_ALIGN))
    {
        CarcerBlk *const nb = carcer_blk(pool, off + CARCER_HDR + want);

        nb->size = walk->size - want - CARCER_HDR;
        nb->used = 0u;
        walk->size = want;
    }
}

/**
 * @brief Finds a free block in ch large enough for want and takes it.
 *
 * @param[in] pool Pool to search [BORROWS].
 * @param[in] ch   Chain to walk.
 * @param[in] want Payload wanted, already rounded.
 * @return         The tenancy, or NULL when no block in the chain fits [RETURNS OWNERSHIP].
 * @note First fit, not best fit. A best fit would walk the whole chain to save slack the split
 *       already recovers.
 * @note The block is marked used inside the walk, so a chain that yields a tenancy has already
 *       given it away; there is no found-but-not-taken result.
 * @warning The whole chain is walked in the failing case, so a take at an end holding many blocks
 *          costs their number.
 */
MMGR_INLINE void *carcer_fit(const CarcerCtx *pool, CarcerChain ch, size_t want)
{
    size_t off = ch.lo;

    while (off < ch.hi)
    {
        CarcerBlk *const walk = carcer_blk(pool, off);

        // A block fits only on both counts: free, and holding at least the payload asked for
        if ((walk->used == 0u) && (walk->size >= want))
        {
            carcer_split(pool, walk, off, want);
            walk->used = 1u;
            return pool->base + off + CARCER_HDR;
        }
        off = carcer_next(pool, off);
    }
    return NULL;
}

/**
 * @brief Lays a fresh block of want payload bytes at offset off.
 *
 * @param[in] pool Pool to carve in [BORROWS].
 * @param[in] off  Offset the header goes at.
 * @param[in] want Payload the block carries.
 * @return         The tenancy [RETURNS OWNERSHIP].
 * @note Only the header is written. The payload behind it is left holding whatever the last tenant
 *       of those bytes put there.
 * @warning off must have CARCER_HDR + want bytes behind it, and nothing here tests that it does.
 *          carcer_grow measures the middle first and is the only caller.
 */
MMGR_INLINE void *carcer_carve(const CarcerCtx *pool, size_t off, size_t want)
{
    CarcerBlk *const walk = carcer_blk(pool, off);

    walk->size = want;
    walk->used = 1u;
    return pool->base + off + CARCER_HDR;
}

/**
 * @brief Returns the bytes lying between the two ends.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         interim_top minus persist_end, or 0 once the ends have met.
 * @note Both offsets are unsigned, so the test is what stops a crossing from reading as a middle
 *       larger than the pool. carcer_grow's size test is what keeps them from crossing at all.
 * @warning The two ends are read one after the other, so the answer is a snapshot. carcer_grow tests
 *          a request against it and then moves a boundary, and a take from a preempting handler
 *          landing between the two carves from the same figure this one did.
 */
MMGR_INLINE size_t carcer_middle(const CarcerCtx *pool)
{
    return (pool->interim_top > pool->persist_end) ? (pool->interim_top - pool->persist_end) : 0u;
}

/**
 * @brief Merges every run of adjacent free blocks in ch, and reports the last block's offset.
 *
 * @param[in] pool Pool whose chain to walk [BORROWS].
 * @param[in] ch   Chain to merge.
 * @return         Offset of the last block, or ch.lo when the chain is empty.
 * @note A merged block is revisited rather than stepped past, so a run of three or more collapses in
 *       one pass. The walk still ends: the revisited block is larger by what it swallowed, so the
 *       step recomputed from it reaches further than the one before.
 * @note The last offset is returned from this walk so trimming needs no second one.
 */
MMGR_INLINE size_t carcer_coalesce(const CarcerCtx *pool, CarcerChain ch)
{
    size_t off = ch.lo;
    size_t last = ch.lo;

    while (off < ch.hi)
    {
        CarcerBlk *const cur = carcer_blk(pool, off);
        const size_t next_off = carcer_next(pool, off);

        // A merge needs both: this block free, and a next block still inside the chain to merge in
        if ((cur->used == 0u) && (next_off < ch.hi))
        {
            CarcerBlk *const nxt = carcer_blk(pool, next_off);

            if (nxt->used == 0u)
            {
                cur->size += CARCER_HDR + nxt->size;
                continue;
            }
        }
        last = off;
        off = next_off;
    }
    return last;
}

/**
 * @brief Records a new high-water figure for one end.
 *
 * @param[in,out] hw   Figure to raise [BORROWS].
 * @param[in]     used Bytes in use by the end that moved.
 * @note Branchless: the comparison becomes a mask that selects the larger of the two.
 * @warning The figure is read and then written, as two steps. A take from a preempting handler
 *          landing between them leaves that take's reach unrecorded.
 */
MMGR_INLINE void carcer_hw(size_t *hw, size_t used)
{
    // Explicit cast widens the int result of > to size_t, so the negation builds a mask the full
    // width of the members it selects between
    const size_t hw_mask = 0u - (size_t)(used > *hw);

    *hw = (*hw & ~hw_mask) | (used & hw_mask);
}

/**
 * @brief Raises one end's high-water figure, or expands to nothing where the build tracks none.
 *
 * @param[in] pool_   Pool to record against.
 * @param[in] member_ Figure to raise, which only exists when the build tracks one.
 * @param[in] used_   Bytes in use by the end that moved.
 * @note A macro, not a call: the member does not exist when the build tracks none, and an argument
 *       cannot name a member that is not declared.
 * @warning The two arms do not evaluate the same things. Where the build tracks none, not one of the
 *          three arguments is evaluated, so nothing passed here may carry a side effect.
 */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CARCER_HW(pool_, member_, used_) carcer_hw(&(pool_)->member_, (used_))
#else
#define CARCER_HW(pool_, member_, used_) ((void)0)
#endif

/**
 * @brief Carves a fresh block for want bytes out of the free middle, at whichever end asked.
 *
 * @param[in,out] pool Pool to carve in [BORROWS].
 * @param[in]     want Payload wanted, already rounded.
 * @param[in]     down MMGR_TRUE for the end that grows down.
 * @return             The tenancy, or NULL when the middle cannot meet it [RETURNS OWNERSHIP].
 * @note Both ends reach this, so the size test, the carve and the high-water are written once.
 * @note The test is the header plus the payload against the middle, so a want the size of the whole
 *       middle is refused.
 * @note Fails closed: a request the middle cannot meet moves no boundary at all.
 */
MMGR_INLINE void *carcer_grow(CarcerCtx *pool, size_t want, mmgr_bool down)
{
    const size_t need = CARCER_HDR + want;

    if (need > carcer_middle(pool))
    {
        return NULL;
    }
    if (down)
    {
        pool->interim_top -= need;
        CARCER_HW(pool, interim_hw, pool->size - pool->interim_top);
        return carcer_carve(pool, pool->interim_top, want);
    }

    void *const pl = carcer_carve(pool, pool->persist_end, want);

    pool->persist_end += need;
    CARCER_HW(pool, persist_hw, pool->persist_end);
    return pl;
}

/**
 * @brief Rounds a request up to a whole word.
 *
 * @param[in] want Bytes the caller asked for.
 * @return         The least payload a block may carry for this request, since a reused block keeps
 *                 its slack; a want of 0 returns MMGR_CARCER_ALIGN.
 * @note Both ends round the same way, so it is done in one place.
 * @warning A want within a word of SIZE_MAX wraps in the rounding and comes back small, so the block
 *          carries far less than was asked for. Neither take tests the request before this.
 */
MMGR_INLINE size_t carcer_want(size_t want)
{
    return carcer_round((want != 0u) ? want : MMGR_CARCER_ALIGN);
}

/**
 * @brief Takes size bytes from the persistent end.
 *
 * @param[in,out] pool Pool to take from [BORROWS].
 * @param[in]     size Bytes wanted.
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [RETURNS OWNERSHIP].
 * @note Reuses a freed block before growing the boundary, which is what makes this end a free list
 *       rather than a cursor.
 * @note The reuse walk runs first and costs the chain its full length whenever nothing in it fits;
 *       only then does the boundary move.
 */
void *mmgr_carcer_persist_capio(CarcerCtx *pool, size_t size)
{
    const size_t want = carcer_want(size);
    void *const reused = carcer_fit(pool, carcer_up(pool), want);

    return (reused != NULL) ? reused : carcer_grow(pool, want, MMGR_FALSE);
}

/**
 * @brief Takes size bytes from the interim end.
 *
 * @param[in,out] pool Pool to take from [BORROWS].
 * @param[in]     size Bytes wanted.
 * @return             Start of the tenancy, or NULL when the pool cannot meet it [RETURNS OWNERSHIP].
 * @note Carves like the persistent take but moves the boundary down, and does no fit walk.
 * @note The walk is omitted deliberately, not missing. A first fit would make a run of takes
 *       quadratic, and this end is meant to be given back wholesale rather than picked over. A take
 *       stays O(1), which is what this end buys over the persistent one.
 * @note A single release at this end trims only when the freed block sits at the top. Anything freed
 *       below it merges with its free neighbors and stays in the chain, and since no take here walks
 *       that chain, nothing reuses it before the next rewind.
 */
void *mmgr_carcer_interim_capio(CarcerCtx *pool, size_t size)
{
    return carcer_grow(pool, carcer_want(size), MMGR_TRUE);
}

/**
 * @brief Writes zeros over size bytes at tenancy.
 *
 * @param[in,out] tenancy First byte to clear [BORROWS].
 * @param[in]     size    Bytes to clear.
 * @note The stores are volatile, so clearing bytes nothing reads afterwards is not dropped as dead
 *       work. Whole words go down between the two edges, and volatile counts per access, so a word
 *       store is kept for the same reason a byte store is.
 * @warning tenancy must be writable for size bytes.
 */
void mmgr_carcer_wipe(void *tenancy, size_t size)
{
    // Explicit cast takes the tenancy to a volatile byte pointer, the scope the edge walks use
    volatile uint8_t *walk = (volatile uint8_t *)tenancy;
    size_t left = size;
    // Explicit casts take walk to uintptr_t for the alignment test, then that result to the size_t
    // edge is carried in
    size_t edge = (size_t)(((uintptr_t)walk) & (MMGR_CARCER_ALIGN - 1u));

    // Head: bytes up to the first word boundary, so the loop below starts aligned
    edge = (edge != 0u) ? (MMGR_CARCER_ALIGN - edge) : 0u;
    edge = (edge < left) ? edge : left;
    carcer_zero_bytes(&walk, &left, edge);

    // Explicit casts go through volatile void * to reach the word scope the middle stores in. The
    // head above leaves walk on a word boundary whenever it had the bytes to reach one; where it ran
    // out first, left is 0 and neither the loop nor the tail reads through this pointer
    volatile mmgr_word *pool = (volatile mmgr_word *)(volatile void *)walk;

    while (left >= MMGR_CARCER_ALIGN)
    {
        // Store, pointer advance and count advance are separate statements; *pool++ = 0 would put an
        // increment inside the volatile store. Explicit cast gives the zero the word scope
        *pool = (mmgr_word)0;
        pool++;
        left -= MMGR_CARCER_ALIGN;
    }

    // Explicit casts return to the byte scope for the tail, which is under one word
    walk = (volatile uint8_t *)(volatile void *)pool;
    carcer_zero_bytes(&walk, &left, left);
}

/**
 * @brief Gives a tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] pool    Pool the tenancy came from [BORROWS].
 * @param[in]     tenancy First byte of the tenancy [TAKES OWNERSHIP].
 * @note Which end the tenancy came from is read from its address rather than named by the caller,
 *       so a release cannot be given to the wrong end.
 * @note After coalescing, a free block at the chain's own boundary is returned to the middle, so the
 *       ends recover. That boundary is the last block at the persistent end and the first at the
 *       interim end.
 * @note A NULL tenancy returns without touching the pool.
 * @warning tenancy is dead once this returns; the pool may hand those bytes out again.
 * @warning Nothing tests that tenancy came from this pool. An address from elsewhere is read as a
 *          header and freed into a chain it never belonged to.
 */
void mmgr_carcer_persist_reddo(CarcerCtx *pool, void *tenancy)
{
    if (tenancy == NULL)
    {
        return;
    }

    const size_t off = carcer_off_of(pool, tenancy);

    carcer_blk(pool, off)->used = 0u;

    if (off < pool->persist_end)
    {
        const CarcerChain ch = carcer_up(pool);
        const size_t last = carcer_coalesce(pool, ch);

        // Give bytes back to the middle only when the end holds blocks and its last one is free
        if ((pool->persist_end > 0u) && (carcer_blk(pool, last)->used == 0u))
        {
            pool->persist_end = last;
        }
    }
    else
    {
        const CarcerChain ch = carcer_down(pool);

        (void)carcer_coalesce(pool, ch);

        CarcerBlk *const first = carcer_blk(pool, pool->interim_top);

        // Same trim at the other end: the end must hold blocks, and the one at the top must be free
        if ((pool->interim_top < pool->size) && (first->used == 0u))
        {
            pool->interim_top += CARCER_HDR + first->size;
        }
    }
}

/**
 * @brief Zeroes a tenancy, then gives it back.
 *
 * @param[in,out] pool    Pool the tenancy came from [BORROWS].
 * @param[in,out] tenancy First byte of the tenancy, zeroed before it is released [TAKES OWNERSHIP].
 * @note The one step that separates a wiped release from a plain one; the give-back is shared.
 * @note The extent comes from the block's own header, so a caller cannot under-wipe a tenancy.
 * @note A NULL tenancy returns without touching the pool.
 * @warning tenancy is dead once this returns; the pool may hand those bytes out again.
 * @warning Nothing tests that tenancy came from this pool, and the extent is read from the bytes
 *          lying ahead of it. An address from elsewhere is wiped for whatever length those bytes
 *          happen to hold.
 */
void mmgr_carcer_secura_reddo(CarcerCtx *pool, void *tenancy)
{
    if (tenancy == NULL)
    {
        return;
    }

    const CarcerBlk *const walk = carcer_blk(pool, carcer_off_of(pool, tenancy));

    mmgr_carcer_wipe(tenancy, walk->size);
    mmgr_carcer_persist_reddo(pool, tenancy);
}

/**
 * @brief Returns the pool's current interim top.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         The value of interim_top.
 * @note Good against this pool alone, and only until a restore to an older mark. The restore assigns
 *       the value it is handed without testing it.
 * @warning The figure is a snapshot. A take from a preempting handler lowers the top after this has
 *          read it, and restoring the mark then gives that take's bytes back as well.
 */
size_t mmgr_carcer_interim_mark(const CarcerCtx *pool)
{
    return pool->interim_top;
}

/**
 * @brief Assigns the interim top the value mark carries.
 *
 * @param[in,out] pool Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore, as mmgr_carcer_interim_mark reported it.
 * @note Drops every block the end carved since that mark in one step, without walking them.
 * @warning The top is assigned, not tested. A mark this pool never reported, or one past its size,
 *          is taken as given and moves the end there.
 * @warning Every interim tenancy taken since mark is dead once this returns. Nothing is scrubbed, so
 *          such a pointer still dereferences and reads whatever the next take puts there.
 */
void mmgr_carcer_interim_reddo(CarcerCtx *pool, size_t mark)
{
    pool->interim_top = mark;
}

/**
 * @brief Zeroes every interim byte taken since mark, then restores the top.
 *
 * @param[in,out] pool Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore.
 * @note Wipes before the top moves, so the bytes are already zero at the instant they become
 *       available. Reclaiming first would leave a window in which the very next take sees them.
 * @note The extent comes from the two tops rather than a block header, so a run of takes is cleared
 *       in one pass and a caller cannot under-wipe by naming fewer bytes than it holds.
 * @warning The wipe is guarded and the restore is not. A mark that is not above the current top, or
 *          that lies past the pool's size, rewinds the end with nothing scrubbed.
 * @warning The top is read once, ahead of the wipe. A take from a preempting handler landing between
 *          that read and the restore is dropped unscrubbed, since the extent was settled from the
 *          older top.
 */
void mmgr_carcer_interim_secura_reddo(CarcerCtx *pool, size_t mark)
{
    const size_t top = pool->interim_top;

    // Wipe only for a mark above the top and inside the pool; [top, mark) is what is live to clear
    if ((mark > top) && (mark <= pool->size))
    {
        mmgr_carcer_wipe(pool->base + top, mark - top);
    }
    mmgr_carcer_interim_reddo(pool, mark);
}

/**
 * @brief Gives the whole interim end back at once, scrubbing nothing.
 *
 * @param[in,out] pool Pool to act on [BORROWS].
 * @note mmgr_carcer_interim_reddo against the pool's own size, which is where the end starts.
 * @note No pool entry reaches this one. A region's reset is its own generated wrapper, and that goes
 *       to the wiping rewind wherever the pool was declared MMGR_SECURA.
 * @warning Every interim tenancy the pool has handed out is dead once this returns, and none of
 *          those bytes are scrubbed.
 */
void mmgr_carcer_interim_reset(CarcerCtx *pool)
{
    mmgr_carcer_interim_reddo(pool, pool->size);
}

/**
 * @brief Returns whether at lies inside the pool's bytes.
 *
 * @param[in] pool Pool to test against [BORROWS].
 * @param[in] at   Address to test [BORROWS].
 * @return         MMGR_TRUE when at lies in [base, base + size), the pool's last byte included.
 * @note Any address in the pool answers true, not only the first byte of a tenancy. This tells a
 *       caller where an address is, not what is there.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCtx *pool, const void *at)
{

    // Explicit casts to uintptr_t let one unsigned compare cover both ends: below base wraps high
    // Explicit cast narrows the int result of < to the mmgr_bool container
    return (mmgr_bool)(((uintptr_t)at - (uintptr_t)pool->base) < pool->size);
}

/**
 * @brief Returns the bytes lying between the two ends.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return         The free middle, as carcer_middle reports it.
 * @note A take carved from the middle needs a block header out of the same bytes, so a take of
 *       exactly this many cannot be met.
 * @warning The two ends are read one after the other, so the answer is a snapshot; carcer_middle
 *          carries what that costs a caller.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCtx *pool)
{
    return carcer_middle(pool);
}

/**
 * @brief Rounds size up to a whole machine word.
 *
 * @param[in] size Count to round.
 * @return         The rounded count.
 * @note A size of 0 rounds to 0. The takes carry a request of 0 up to one word before rounding it.
 * @warning MMGR_CARCER_ALIGN - 1 is added before the mask, so a size within a word of SIZE_MAX wraps
 *          to 0.
 */
size_t mmgr_carcer_align_up(size_t size)
{
    return carcer_round(size);
}
