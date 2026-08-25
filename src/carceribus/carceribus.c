/**
 * @brief Double-ended pool: one block allocator, run from both ends of the same free middle.
 *
 * @note The two ends are the same machinery under two names. Each keeps a chain of blocks, each
 *       block behind its own header, and each carves from the free middle between them. All that
 *       differs is which way the boundary moves: the persistent end grows up from base, the interim
 *       end grows down from the top. Everything else - the fit, the split, the merge - is shared.
 * @note What separates them is lifetime, not mechanism. The persistent end holds what outlives a
 *       call - key material, runtime seeds, constants a run works out once - so a tenancy there is
 *       released one at a time and lives as long as it likes. The interim end holds what a call
 *       needs while it runs, and the whole run comes back at once through a mark.
 * @note Bytes are cleared on release rather than on hand-out, so the cost is paid once and by the
 *       tenancy that knew its bytes were worth clearing. What the persistent end is for is the
 *       reason mmgr_carcer_secura_reddo exists, and the reason it is the ordinary release there
 *       rather than the exceptional one.
 * @note Reaches nothing outside config.
 */
#include "carceribus/carceribus.h"

/**
 * @brief What a block carries ahead of its payload.
 *
 * @note used is a size_t rather than a flag so the header keeps the payload aligned without padding
 *       the compiler would have to be asked about.
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
 * @brief The half-open offset range one end's chain occupies.
 *
 * @note Both ends walk their chain the same way, from lo upward. Only which boundary a carve moves
 *       differs, so the walk, the fit and the merge below take one of these and no direction.
 */
typedef struct
{
    size_t lo; /**< Offset of the first block in the chain. */
    size_t hi; /**< One past the last byte the chain occupies. */
} CarcerChain;

/**
 * @brief Rounds n up to a whole machine word.
 *
 * @param[in] n Count to round.
 * @return      n raised to the next multiple of MMGR_CARCER_ALIGN.
 */
MMGR_INLINE size_t carcer_round(size_t n)
{
    return (n + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u);
}

/**
 * @brief Zeroes n bytes at *b, advancing the pointer and taking them off *left.
 *
 * @param[in,out] b    Walking pointer [BORROWS].
 * @param[in,out] left Bytes still to clear [BORROWS].
 * @param[in]     n    Bytes to clear now.
 * @note Both edges of the wipe are this walk, so it is written once. The stores stay volatile: an
 *       edge is as dead to the caller as the middle, and a guarantee with a hole in it is none.
 */
MMGR_INLINE void carcer_zero_bytes(volatile uint8_t **b, size_t *left, size_t n)
{
    size_t i = 0u;

    while (i < n)
    {
        **b = 0u;
        (*b)++;
        i++;
    }
    *left -= n;
}

/**
 * @brief Reads the block header at offset off in the pool.
 *
 * @param[in] pool Pool to look in [BORROWS].
 * @param[in] off  Offset of the header, always a multiple of MMGR_CARCER_ALIGN.
 * @return         The header [BORROWS].
 * @note The cast goes through void *; base is aligned by the region macro and every offset a chain
 *       walks is a whole number of words, so the header always lands where it fits.
 */
MMGR_INLINE CarcerBlk *carcer_blk(const CarcerCtx *pool, size_t off)
{
    return (CarcerBlk *)(void *)(pool->base + off);
}

/**
 * @brief Steps past the block at off to the next one in its chain.
 *
 * @param[in] w   Pool the chain runs in [BORROWS].
 * @param[in] off Offset of the block to step past.
 * @return        Offset of the block after it.
 * @note A block is its header and its payload, and every walk in this file steps by exactly that.
 */
MMGR_INLINE size_t carcer_next(const CarcerCtx *w, size_t off)
{
    return off + CARCER_HDR + carcer_blk(w, off)->size;
}

/**
 * @brief Returns the offset of a tenancy's own header.
 *
 * @param[in] pool Pool the tenancy came from [BORROWS].
 * @param[in] at   First byte of the tenancy [BORROWS].
 * @return         Offset of its header in the pool.
 */
MMGR_INLINE size_t carcer_off_of(const CarcerCtx *pool, const void *at)
{
    return (size_t)((const uint8_t *)at - pool->base) - CARCER_HDR;
}

/**
 * @brief Returns the chain the persistent end keeps.
 *
 * @param[in] w Pool to read [BORROWS].
 * @return      Its offset range, which starts at base and ends where the end has grown to.
 */
MMGR_INLINE CarcerChain carcer_up(const CarcerCtx *w)
{
    CarcerChain ch;

    ch.lo = 0u;
    ch.hi = w->persist_end;
    return ch;
}

/**
 * @brief Returns the chain the interim end keeps.
 *
 * @param[in] w Pool to read [BORROWS].
 * @return      Its offset range, which starts where the end has grown down to and ends at the top.
 */
MMGR_INLINE CarcerChain carcer_down(const CarcerCtx *w)
{
    CarcerChain ch;

    ch.lo = w->interim_top;
    ch.hi = w->size;
    return ch;
}

/**
 * @brief Splits block b when what is left would hold another block.
 *
 * @param[in,out] w   Pool the block sits in [BORROWS].
 * @param[in,out] b   Block to split [BORROWS].
 * @param[in]     off Offset of b in the pool.
 * @param[in]     n   Payload the first half keeps.
 * @note Only splits when the remainder can carry a header and a payload of its own; otherwise the
 *       tenant keeps the whole block and its slack.
 */
MMGR_INLINE void carcer_split(const CarcerCtx *w, CarcerBlk *b, size_t off, size_t n)
{
    if (b->size >= (n + CARCER_HDR + MMGR_CARCER_ALIGN))
    {
        CarcerBlk *const nb = carcer_blk(w, off + CARCER_HDR + n);

        nb->size = b->size - n - CARCER_HDR;
        nb->used = 0u;
        b->size = n;
    }
}

/**
 * @brief Finds a free block in ch large enough for n and takes it.
 *
 * @param[in,out] w  Pool to search [BORROWS].
 * @param[in]     ch Chain to walk.
 * @param[in]     n  Payload wanted, already rounded.
 * @return           The tenancy, or NULL when no block in the chain fits [BORROWS].
 * @note First fit: the first block that can hold the request takes it, rather than the best. A best
 *       fit would walk the whole chain every time to save slack a split already recovers.
 */
MMGR_INLINE void *carcer_fit(const CarcerCtx *w, CarcerChain ch, size_t n)
{
    size_t off = ch.lo;

    while (off < ch.hi)
    {
        CarcerBlk *const b = carcer_blk(w, off);

        if ((b->used == 0u) && (b->size >= n))
        {
            carcer_split(w, b, off, n);
            b->used = 1u;
            return w->base + off + CARCER_HDR;
        }
        off = carcer_next(w, off);
    }
    return NULL;
}

/**
 * @brief Lays a fresh block of n payload bytes at offset off.
 *
 * @param[in,out] w   Pool to carve in [BORROWS].
 * @param[in]     off Offset the header goes at.
 * @param[in]     n   Payload the block carries.
 * @return            The tenancy [BORROWS].
 */
MMGR_INLINE void *carcer_carve(const CarcerCtx *w, size_t off, size_t n)
{
    CarcerBlk *const b = carcer_blk(w, off);

    b->size = n;
    b->used = 1u;
    return w->base + off + CARCER_HDR;
}

/**
 * @brief Returns the bytes lying between the two ends.
 *
 * @param[in] w Pool to read [BORROWS].
 * @return      interim_top minus persist_end, or 0 when they have met.
 */
MMGR_INLINE size_t carcer_middle(const CarcerCtx *w)
{
    return (w->interim_top > w->persist_end) ? (w->interim_top - w->persist_end) : 0u;
}

/**
 * @brief Merges every run of adjacent free blocks in ch, and reports the last block's offset.
 *
 * @param[in,out] w  Pool whose chain to walk [BORROWS].
 * @param[in]     ch Chain to merge.
 * @return           Offset of the last block, or ch.lo when the chain is empty.
 * @note A merged block is looked at again rather than stepped past, so a run of three or more
 *       collapses in one pass.
 * @note The last offset comes back from this walk rather than a second one. Trimming needs it, and
 *       this walk has already been to every block to find it.
 */
MMGR_INLINE size_t carcer_coalesce(const CarcerCtx *w, CarcerChain ch)
{
    size_t off = ch.lo;
    size_t last = ch.lo;

    while (off < ch.hi)
    {
        CarcerBlk *const cur = carcer_blk(w, off);
        const size_t next_off = carcer_next(w, off);

        if ((cur->used == 0u) && (next_off < ch.hi))
        {
            CarcerBlk *const nxt = carcer_blk(w, next_off);

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
 * @brief Arguments for every backend in this file, grouped by the calls that read them.
 *
 * @note Each backend reads one group; MMGR_CALL zeroes the members it is not given.
 */
typedef struct
{
    CarcerCtx *pool; /**< Pool to act on [BORROWS]. */
    size_t size;     /**< Byte count the call takes or clears. */
    const void *at;  /**< Address owns tests, which it reads the value of alone [BORROWS]. */
    void *tenancy;   /**< Bytes a wipe clears or a release gives back [BORROWS]. */
    size_t mark;     /**< Interim top interim_reddo restores. */
} CarcerOp;

/**
 * @brief Records a new high-water figure for one end.
 *
 * @param[in,out] hw   Figure to raise [BORROWS].
 * @param[in]     used Bytes in use by the end that moved.
 * @note Branchless: the comparison becomes a mask that selects the larger of the two.
 */
MMGR_INLINE void carcer_hw(size_t *hw, size_t used)
{
    const size_t hw_mask = 0u - (size_t)(used > *hw);

    *hw = (*hw & ~hw_mask) | (used & hw_mask);
}

/**
 * @brief Raises one end's high-water figure, or expands to nothing where the build tracks none.
 *
 * @param[in] pool_   Pool to record against.
 * @param[in] member_ Figure to raise, which only exists when the build tracks one.
 * @param[in] used_   Bytes in use by the end that moved.
 * @note A macro rather than a call, because the member it names is not declared at all when the
 *       build does not track it, and an argument cannot refer to a member that is not there.
 */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define CARCER_HW(pool_, member_, used_) carcer_hw(&(pool_)->member_, (used_))
#else
#define CARCER_HW(pool_, member_, used_) ((void)0)
#endif

/**
 * @brief Takes c->size bytes from the persistent end.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 */
/**
 * @brief Carves a fresh block for n bytes out of the free middle, at whichever end asked.
 *
 * @param[in,out] w    Pool to carve in [BORROWS].
 * @param[in]     n    Payload wanted, already rounded.
 * @param[in]     down MMGR_TRUE for the end that grows down.
 * @return             The tenancy, or NULL when the middle cannot meet it [BORROWS].
 * @note Both ends reach this. All that differs is which boundary moves and which way, so the size
 *       test, the carve and the high-water are written once rather than at each end.
 * @note Fails closed: a request the middle cannot meet moves no boundary at all.
 */
MMGR_INLINE void *carcer_grow(CarcerCtx *w, size_t n, mmgr_bool down)
{
    const size_t need = CARCER_HDR + n;

    if (need > carcer_middle(w))
    {
        return NULL;
    }
    if (down)
    {
        w->interim_top -= need;
        CARCER_HW(w, interim_hw, w->size - w->interim_top);
        return carcer_carve(w, w->interim_top, n);
    }

    void *const pl = carcer_carve(w, w->persist_end, n);

    w->persist_end += need;
    CARCER_HW(w, persist_hw, w->persist_end);
    return pl;
}

/**
 * @brief Rounds a request to a whole word, with nothing still getting an address of its own.
 *
 * @param[in] want Bytes the caller asked for.
 * @return         The payload a block will carry.
 * @note Both ends ask the same question of a request, so they ask it in the same place.
 */
MMGR_INLINE size_t carcer_want(size_t want)
{
    return carcer_round((want != 0u) ? want : MMGR_CARCER_ALIGN);
}

MMGR_INLINE void *carcer_persist_capio(const CarcerOp *c)
{
    CarcerCtx *const w = c->pool;
    const size_t n = carcer_want(c->size);
    void *const reused = carcer_fit(w, carcer_up(w), n);

    return (reused != NULL) ? reused : carcer_grow(w, n, MMGR_FALSE);
}

/**
 * @brief Takes c->size bytes from the interim end.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note The mirror of the persistent take: the same fit over its own chain, and a carve that moves
 *       the boundary down instead of up.
 */
MMGR_INLINE void *carcer_interim_capio(const CarcerOp *c)
{
    // No fit walk here, unlike the persistent end. A first fit costs a step per block already in the
    // chain, so a run of takes before one release is quadratic in the length of the run - and a run
    // of takes before one release is exactly what this end is for. Nothing is reused because nothing
    // is released one at a time; the mark takes the whole run back. That keeps a take O(1), which is
    // what the interim end buys over the persistent one.
    return carcer_grow(c->pool, carcer_want(c->size), MMGR_TRUE);
}

/**
 * @brief Writes zeros over c->size bytes at c->tenancy.
 *
 * @param[in,out] c Address and extent to clear [BORROWS].
 */
MMGR_INLINE void carcer_wipe(const CarcerOp *c)
{
    // volatile is the guarantee: the bytes are dead to the caller the moment this returns, so a
    // plain store here is a dead store the optimiser is free to drop, and the wipe would be a
    // comment rather than a promise.
    //
    // Machine-width stores, with byte edges only where the address or the length is not a whole
    // word. volatile is per access, so a volatile word store is exactly as un-elidable as a
    // volatile byte store: the guarantee is unchanged and the store count drops by the width.
    // Both edges are normally empty, since every address this module hands out is aligned and every
    // extent it records is rounded, so what runs is the word loop.
    volatile uint8_t *b = (volatile uint8_t *)c->tenancy;
    size_t left = c->size;
    size_t edge = (size_t)(((uintptr_t)b) & (MMGR_CARCER_ALIGN - 1u));

    // The head: bytes up to the first word boundary. edge is the distance to it, so this is the
    // same walk the tail below does and both reach carcer_zero_bytes rather than each spelling it.
    edge = (edge != 0u) ? (MMGR_CARCER_ALIGN - edge) : 0u;
    edge = (edge < left) ? edge : left;
    carcer_zero_bytes(&b, &left, edge);

    volatile mmgr_word *w = (volatile mmgr_word *)(volatile void *)b;

    while (left >= MMGR_CARCER_ALIGN)
    {
        *w = (mmgr_word)0;
        w++;
        left -= MMGR_CARCER_ALIGN;
    }

    b = (volatile uint8_t *)(volatile void *)w;
    carcer_zero_bytes(&b, &left, left);
}

/**
 * @brief Gives the tenancy at c->tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS].
 * @note Which end the tenancy came from is read from its address rather than named by the caller,
 *       so a release cannot be given to the wrong end.
 */
MMGR_INLINE void carcer_persist_reddo(const CarcerOp *c)
{
    if (c->tenancy == NULL)
    {
        return;
    }

    CarcerCtx *const w = c->pool;
    const size_t off = carcer_off_of(w, c->tenancy);

    carcer_blk(w, off)->used = 0u;

    if (off < w->persist_end)
    {
        const CarcerChain ch = carcer_up(w);
        const size_t last = carcer_coalesce(w, ch);

        // A free block at the top of this chain goes back to the middle, so the ends recover
        if ((w->persist_end > 0u) && (carcer_blk(w, last)->used == 0u))
        {
            w->persist_end = last;
        }
    }
    else
    {
        const CarcerChain ch = carcer_down(w);

        (void)carcer_coalesce(w, ch);

        CarcerBlk *const first = carcer_blk(w, w->interim_top);

        // The interim chain's own boundary is its first block, so that is the one to give back
        if ((w->interim_top < w->size) && (first->used == 0u))
        {
            w->interim_top += CARCER_HDR + first->size;
        }
    }
}

/**
 * @brief Zeroes the tenancy at c->tenancy, then gives it back.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS].
 * @note The one step that separates a wiped release from a plain one; the give-back is shared.
 * @note The extent comes from the block's own header, so a caller cannot under-wipe a tenancy.
 */
MMGR_INLINE void carcer_secura_reddo(const CarcerOp *c)
{
    if (c->tenancy == NULL)
    {
        return;
    }

    const CarcerCtx *const w = c->pool;
    const CarcerBlk *const b = carcer_blk(w, carcer_off_of(w, c->tenancy));

    MMGR_CALL(carcer_wipe, CarcerOp, .tenancy = c->tenancy, .size = b->size);
    carcer_persist_reddo(c);
}

/**
 * @brief Returns the pool's current interim top.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The value of interim_top.
 */
MMGR_INLINE size_t carcer_interim_mark(const CarcerOp *c)
{
    return c->pool->interim_top;
}

/**
 * @brief Assigns the interim top the value c->mark carries.
 *
 * @param[in,out] c Pool and the mark to restore [BORROWS].
 * @note Drops every block the end carved since that mark in one step, without walking them: the
 *       chain below the mark is simply no longer part of it.
 */
MMGR_INLINE void carcer_interim_reddo(const CarcerOp *c)
{
    c->pool->interim_top = c->mark;
}

/**
 * @brief Gives the whole interim end back at once.
 *
 * @param[in,out] c Pool to act on [BORROWS].
 */
MMGR_INLINE void carcer_interim_reset(const CarcerOp *c)
{
    MMGR_CALL(carcer_interim_reddo, CarcerOp, .pool = c->pool, .mark = c->pool->size);
}

/**
 * @brief Returns whether c->at lies inside the pool's bytes.
 *
 * @param[in] c Pool and the address to test [BORROWS].
 * @return      MMGR_TRUE when c->at is at or after base and before base plus size.
 */
MMGR_INLINE mmgr_bool carcer_owns(const CarcerOp *c)
{
    // Explicit casts to uintptr_t let one unsigned compare cover both ends: below base wraps high
    return (mmgr_bool)(((uintptr_t)c->at - (uintptr_t)c->pool->base) < c->pool->size);
}

/**
 * @brief Returns the bytes lying between the two ends.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The free middle.
 */
MMGR_INLINE size_t carcer_octas_praesto(const CarcerOp *c)
{
    return carcer_middle(c->pool);
}

/**
 * @brief Rounds c->size up to a whole machine word.
 *
 * @param[in] c The count to round [BORROWS].
 * @return      The rounded count.
 */
MMGR_INLINE size_t carcer_align_up(const CarcerOp *c)
{
    return carcer_round(c->size);
}

/**
 * @brief The pool argument the entries that act on one forward.
 *
 * @note Names c from the entry point's own parameter, so the table below carries fields alone.
 */
#define CARCER_P .pool = c->pool

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_carcer_ and carcer_ prefixes, which the two share.
 */
#define CARCER_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_carcer_, carcer_, CarcerOp, CarcerCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_carcer_ and carcer_ prefixes, which the two share.
 */
#define CARCER_ENTRY_V(name, ...) GENERIC_ENTRY_V(mmgr_carcer_, carcer_, CarcerOp, CarcerCfg, name, __VA_ARGS__)

/**
 * @brief The pool surface, one line per entry point.
 *
 * @note Each is documented at its declaration in carceribus.h.
 */
CARCER_ENTRY(void *, persist_capio, CARCER_P, .size = c->size)
CARCER_ENTRY_V(persist_reddo, CARCER_P, .tenancy = c->tenancy)
CARCER_ENTRY_V(secura_reddo, CARCER_P, .tenancy = c->tenancy)
CARCER_ENTRY(void *, interim_capio, CARCER_P, .size = c->size)
CARCER_ENTRY(size_t, interim_mark, CARCER_P)
CARCER_ENTRY_V(interim_reddo, CARCER_P, .mark = c->mark)
CARCER_ENTRY_V(interim_reset, CARCER_P)
CARCER_ENTRY(mmgr_bool, owns, CARCER_P, .at = c->at)
CARCER_ENTRY(size_t, octas_praesto, CARCER_P)
CARCER_ENTRY_V(wipe, .tenancy = c->tenancy, .size = c->size)
CARCER_ENTRY(size_t, align_up, .size = c->size)
