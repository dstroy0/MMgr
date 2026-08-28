/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Double-ended pool: one block allocator, run from both ends of the same free middle.
 *
 * @note Both ends share the fit, the split and the merge. Only the boundary direction differs: the
 *       persistent end grows up from base, the interim end grows down from size.
 * @note Persistent tenancies are released one at a time, in any order. Interim tenancies are released
 *       together by mark.
 * @note Bytes are cleared on release, not on hand-out. A take returns whatever the last tenant left.
 * @note Reaches nothing outside config.
 */
#include "carceribus/carceribus.h"

/**
 * @brief What a block carries ahead of its payload.
 *
 * @note used is a size_t rather than a flag so the header stays a whole number of words and the
 *       payload behind it needs no padding.
 */
typedef struct
{
    size_t size; /**< Payload bytes behind this header. */
    size_t used; /**< 0 while the block is free, 1 while a tenant holds it. */
} CarcerBlk;

/**
 * @brief Bytes a block header occupies, rounded up so the payload behind it stays aligned.
 */
#define CARCER_HDR ((sizeof(CarcerBlk) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief The half-open offset range one end'seat chain occupies.
 *
 * @note Both ends walk from lo upward, so the walk, the fit and the merge take a chain and no
 *       direction argument.
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
 * @return      want raised to the next multiple of MMGR_CARCER_ALIGN.
 */
MMGR_INLINE size_t carcer_round(size_t want)
{
    return (want + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u);
}

/**
 * @brief Zeroes want bytes at *walk, advancing the pointer and taking them off *left.
 *
 * @param[in,out] walk    Walking pointer [BORROWS].
 * @param[in,out] left Bytes still to clear [BORROWS].
 * @param[in]     want    Bytes to clear now.
 * @note Used for both edges of the wipe. The stores are volatile so the optimizer cannot drop them,
 *       which the word-wide middle relies on too.
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
 */
MMGR_INLINE CarcerBlk *carcer_blk(const CarcerCtx *pool, size_t off)
{
    return (CarcerBlk *)(void *)(pool->base + off);
}

/**
 * @brief Steps past the block at off to the next one in its chain.
 *
 * @param[in] pool   Pool the chain runs in [BORROWS].
 * @param[in] off Offset of the block to step past.
 * @return        Offset of the block after it.
 * @note A block is its header plus its payload; every walk here steps by that.
 */
MMGR_INLINE size_t carcer_next(const CarcerCtx *pool, size_t off)
{
    return off + CARCER_HDR + carcer_blk(pool, off)->size;
}

/**
 * @brief Returns the offset of a tenancy'seat own header.
 *
 * @param[in] pool Pool the tenancy came from [BORROWS].
 * @param[in] at   First byte of the tenancy [BORROWS].
 * @return         Offset of its header in the pool.
 */
MMGR_INLINE size_t carcer_off_of(const CarcerCtx *pool, const void *at)
{
    // Explicit casts take at to a byte pointer so the difference is in bytes, then that ptrdiff_t
    // to the size_t the offset is carried in; at is inside pool, so the difference is never negative
    return (size_t)((const uint8_t *)at - pool->base) - CARCER_HDR;
}

/**
 * @brief Returns the chain the persistent end keeps.
 *
 * @param[in] pool Pool to read [BORROWS].
 * @return      Offsets 0 through persist_end.
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
 * @return      Offsets interim_top through size.
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
 * @param[in,out] pool   Pool the block sits in [BORROWS].
 * @param[in,out] walk   Block to split [BORROWS].
 * @param[in]     off Offset of walk in the pool.
 * @param[in]     want   Payload the first half keeps.
 * @note Only splits when the remainder can carry a header and a payload of its own; otherwise the
 *       tenant keeps the whole block and its slack.
 */
MMGR_INLINE void carcer_split(const CarcerCtx *pool, CarcerBlk *walk, size_t off, size_t want)
{
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
 * @param[in,out] pool  Pool to search [BORROWS].
 * @param[in]     ch Chain to walk.
 * @param[in]     want  Payload wanted, already rounded.
 * @return           The tenancy, or NULL when no block in the chain fits [BORROWS].
 * @note First fit, not best fit. A best fit would walk the whole chain to save slack the split
 *       already recovers.
 */
MMGR_INLINE void *carcer_fit(const CarcerCtx *pool, CarcerChain ch, size_t want)
{
    size_t off = ch.lo;

    while (off < ch.hi)
    {
        CarcerBlk *const walk = carcer_blk(pool, off);

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
 * @param[in,out] pool   Pool to carve in [BORROWS].
 * @param[in]     off Offset the header goes at.
 * @param[in]     want   Payload the block carries.
 * @return            The tenancy [BORROWS].
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
 * @return      interim_top minus persist_end, or 0 when they have met.
 */
MMGR_INLINE size_t carcer_middle(const CarcerCtx *pool)
{
    return (pool->interim_top > pool->persist_end) ? (pool->interim_top - pool->persist_end) : 0u;
}

/**
 * @brief Merges every run of adjacent free blocks in ch, and reports the last block'seat offset.
 *
 * @param[in,out] pool  Pool whose chain to walk [BORROWS].
 * @param[in]     ch Chain to merge.
 * @return           Offset of the last block, or ch.lo when the chain is empty.
 * @note A merged block is revisited rather than stepped past, so a run of three or more collapses in
 *       one pass.
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
 */
MMGR_INLINE void carcer_hw(size_t *hw, size_t used)
{
    // Explicit cast widens the int result of > to size_t, so the negation builds a mask the full
    // width of the members it selects between
    const size_t hw_mask = 0u - (size_t)(used > *hw);

    *hw = (*hw & ~hw_mask) | (used & hw_mask);
}

/**
 * @brief Raises one end'seat high-water figure, or expands to nothing where the build tracks none.
 *
 * @param[in] pool_   Pool to record against.
 * @param[in] member_ Figure to raise, which only exists when the build tracks one.
 * @param[in] used_   Bytes in use by the end that moved.
 * @note A macro, not a call: the member does not exist when the build tracks none, and an argument
 *       cannot name a member that is not declared.
 */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CARCER_HW(pool_, member_, used_) carcer_hw(&(pool_)->member_, (used_))
#else
#define CARCER_HW(pool_, member_, used_) ((void)0)
#endif

/**
 * @brief Carves a fresh block for want bytes out of the free middle, at whichever end asked.
 *
 * @param[in,out] pool    Pool to carve in [BORROWS].
 * @param[in]     want    Payload wanted, already rounded.
 * @param[in]     down MMGR_TRUE for the end that grows down.
 * @return             The tenancy, or NULL when the middle cannot meet it [BORROWS].
 * @note Both ends reach this, so the size test, the carve and the high-water are written once.
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
 * @return         The payload a block will carry; a want of 0 returns MMGR_CARCER_ALIGN.
 * @note Both ends round the same way, so it is done in one place.
 */
MMGR_INLINE size_t carcer_want(size_t want)
{
    return carcer_round((want != 0u) ? want : MMGR_CARCER_ALIGN);
}

/**
 * @brief Takes args->size bytes from the persistent end.
 *
 * @param[in,out] args Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note Reuses a freed block before growing the boundary, which is what makes this end a free list
 *       rather than a cursor.
 * @note The walk is affordable here because releases are interleaved with takes.
 */
void *mmgr_carcer_persist_capio(CarcerCtx *pool, size_t size)
{
    const size_t want = carcer_want(size);
    void *const reused = carcer_fit(pool, carcer_up(pool), want);

    return (reused != NULL) ? reused : carcer_grow(pool, want, MMGR_FALSE);
}

/**
 * @brief Takes args->size bytes from the interim end.
 *
 * @param[in,out] args Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note Carves like the persistent take but moves the boundary down, and does no fit walk.
 * @note The walk is omitted deliberately, not missing. Nothing here is released one at a time, so
 *       there is nothing to reuse, and a first fit would make a run of takes quadratic. A take stays
 *       O(1), which is what this end buys over the persistent one.
 */
void *mmgr_carcer_interim_capio(CarcerCtx *pool, size_t size)
{
    return carcer_grow(pool, carcer_want(size), MMGR_TRUE);
}

/**
 * @brief Writes zeros over args->size bytes at args->tenancy.
 *
 * @param[in,out] args Address and extent to clear [BORROWS].
 * @note Stores are volatile so the optimizer cannot drop them as dead, and machine-width except at
 *       the edges. volatile is per access, so a word store is as un-elidable as a byte store.
 * @warning args->tenancy must be writable for args->size bytes.
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

    // Explicit casts go through volatile void * to reach the word scope the middle stores in; walk is
    // word aligned by the head above, so the word pointer is valid
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
 * @brief Gives the tenancy at args->tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] args Pool and the tenancy to release [BORROWS]; args->tenancy [TAKES OWNERSHIP].
 * @note Which end the tenancy came from is read from its address rather than named by the caller,
 *       so a release cannot be given to the wrong end.
 * @note After coalescing, a free block at the chain'seat own boundary is returned to the middle, so the
 *       ends recover. That boundary is the last block at the persistent end and the first at the
 *       interim end.
 * @warning args->tenancy is dead once this returns; the pool may hand those bytes out again.
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

        if ((pool->interim_top < pool->size) && (first->used == 0u))
        {
            pool->interim_top += CARCER_HDR + first->size;
        }
    }
}

/**
 * @brief Zeroes the tenancy at args->tenancy, then gives it back.
 *
 * @param[in,out] args Pool and the tenancy to release [BORROWS]; args->tenancy [TAKES OWNERSHIP].
 * @note The one step that separates a wiped release from a plain one; the give-back is shared.
 * @note The extent comes from the block'seat own header, so a caller cannot under-wipe a tenancy.
 * @warning args->tenancy is dead once this returns; the pool may hand those bytes out again.
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
 * @brief Returns the pool'seat current interim top.
 *
 * @param[in] args Pool to read [BORROWS].
 * @return      The value of interim_top.
 */
size_t mmgr_carcer_interim_mark(const CarcerCtx *pool)
{
    return pool->interim_top;
}

/**
 * @brief Assigns the interim top the value args->mark carries.
 *
 * @param[in,out] args Pool and the mark to restore [BORROWS].
 * @note Drops every block the end carved since that mark in one step, without walking them.
 * @warning Every interim tenancy taken since args->mark is dead once this returns. Nothing is scrubbed,
 *          so such a pointer still dereferences and returns whatever the next take put there.
 */
void mmgr_carcer_interim_reddo(CarcerCtx *pool, size_t mark)
{
    pool->interim_top = mark;
}

/**
 * @brief Zeroes every interim byte taken since mark, then restores the top.
 *
 * @param[in,out] pool    Pool to rewind [BORROWS].
 * @param[in]     mark Top to restore.
 * @note Wipes before the top moves, so the bytes are already zero at the instant they become
 *       available. Reclaiming first would leave a window in which the very next take sees them.
 * @note The extent comes from the two tops rather than a block header, so a run of takes is cleared
 *       in one pass and a caller cannot under-wipe by naming fewer bytes than it holds.
 */
void mmgr_carcer_interim_secura_reddo(CarcerCtx *pool, size_t mark)
{
    const size_t top = pool->interim_top;

    if ((mark > top) && (mark <= pool->size))
    {
        mmgr_carcer_wipe(pool->base + top, mark - top);
    }
    mmgr_carcer_interim_reddo(pool, mark);
}

/**
 * @brief Gives the whole interim end back at once.
 *
 * @param[in,out] args Pool to act on [BORROWS].
 * @note carcer_interim_reddo against the pool'seat own size, which is where the end starts.
 * @warning Every interim tenancy the pool has handed out is dead once this returns.
 */
void mmgr_carcer_interim_reset(CarcerCtx *pool)
{
    mmgr_carcer_interim_reddo(pool, pool->size);
}

/**
 * @brief Returns whether at lies inside the pool'seat bytes.
 *
 * @param[in] region    Region the pool belongs to [BORROWS].
 * @param[in] pool Which pool of it.
 * @param[in] at   Address to test [BORROWS].
 * @return         MMGR_TRUE when at is at or after the pool'seat first byte and before its last.
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
 * @param[in] args Pool to read [BORROWS].
 * @return      The free middle.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCtx *pool)
{
    return carcer_middle(pool);
}

/**
 * @brief Rounds args->size up to a whole machine word.
 *
 * @param[in] args The count to round [BORROWS].
 * @return      The rounded count.
 */
size_t mmgr_carcer_align_up(size_t size)
{
    return carcer_round(size);
}
