// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "confinium/confinium.h"
#include "memoria_operor/memoria_operor.h"

/**
 * @file confinium.c
 * @brief Tenant bookkeeping. Persist grows up, interim grows down, and they meet in the middle.
 *
 * An entry that takes a byte count and an alignment beside its region takes one parameter, a
 * pointer to ConfinCtx. An entry that takes only the region takes the region: there is no
 * argument list to group, and a struct to carry one pointer is a store and a load to reach what
 * was already in a register.
 */

typedef struct
{
    size_t size;
    size_t used;
} ABlk;

static const size_t AHDR = (sizeof(ABlk) + (MMGR_CONFIN_ALIGN - 1)) & ~(size_t)(MMGR_CONFIN_ALIGN - 1);

/** @brief One take, return or question, of a region or a set of them. */
typedef struct
{
    mmgr_confin *const a;         /**< The region. This address and no other. */
    mmgr_confin_set *const s;     /**< The set. This address and no other. */
    const mmgr_confin_mark *mark; /**< The mark being rewound to. */
    void *base;                   /**< The buffer being bound. */
    void *p;                      /**< The pointer being returned. */
    size_t n;                     /**< Bytes wanted, or the buffer's size. */
    size_t align;                 /**< Alignment wanted. */
} ConfinCtx;

/**
 * @brief Round @c n up to MMGR_CONFIN_ALIGN.
 * @param c The take.
 * @return Rounded count.
 */
MMGR_INLINE size_t confin_align_up(const ConfinCtx *c)
{
    return (c->n + (MMGR_CONFIN_ALIGN - 1)) & ~(size_t)(MMGR_CONFIN_ALIGN - 1);
}

/**
 * @brief Bind a region to its buffer.
 * @param c In/out. The take.
 */
MMGR_INLINE void confin_init(ConfinCtx *c)
{
    const uintptr_t b = (uintptr_t)c->base;
    const uintptr_t ab = (b + (MMGR_CONFIN_MAX_ALIGN - 1)) & ~(uintptr_t)(MMGR_CONFIN_MAX_ALIGN - 1);
    const size_t adj = (size_t)(ab - b);

    c->a->base = (uint8_t *)ab;
    c->a->size = (c->n > adj) ? ((c->n - adj) & ~(size_t)(MMGR_CONFIN_ALIGN - 1)) : 0;
    c->a->persist_end = 0;
    c->a->scratch_top = c->a->size;
    c->a->persist_used = 0;
    c->a->persist_hw = 0;
    c->a->scratch_hw = 0;
}

/**
 * @brief Take @c n bytes from the up-growing end, reusing a free block if one fits.
 * @param c In/out. The take.
 * @return The bytes, or NULL if the two ends would meet.
 */
MMGR_INLINE void *confin_persist_capio(ConfinCtx *c)
{
    mmgr_confin *const a = c->a;

    c->n = c->n ? c->n : MMGR_CONFIN_ALIGN;
    const size_t n = confin_align_up(c);

    size_t off = 0;
    while (off < a->persist_end)
    {
        ABlk *b = (ABlk *)(a->base + off);
        if (!b->used && b->size >= n)
        {
            if (b->size >= n + AHDR + MMGR_CONFIN_ALIGN)
            {
                ABlk *nb = (ABlk *)(a->base + off + AHDR + n);
                nb->size = b->size - n - AHDR;
                nb->used = 0;
                b->size = n;
            }
            b->used = 1;
            a->persist_used += b->size;
            void *pl = a->base + off + AHDR;
            memor.set(pl, 0, b->size);
            return pl;
        }
        off += AHDR + b->size;
    }

    const size_t need = AHDR + n;
    /* The second half is the wrap check. persist_end and need are both bounded by the region size,
       so their sum cannot come back below need without a region larger than the address space.
       Kept because it is what stops a bad size from being read as a small one. */
    if (((a->persist_end + need) <= a->scratch_top) && ((a->persist_end + need) >= need)) /* GCOVR_EXCL_BR_LINE */
    {
        ABlk *b = (ABlk *)(a->base + a->persist_end);
        b->size = n;
        b->used = 1;
        void *pl = a->base + a->persist_end + AHDR;
        a->persist_end += need;
        if (a->persist_end > a->persist_hw)
        {
            a->persist_hw = a->persist_end;
        }
        a->persist_used += n;
        memor.set(pl, 0, n);
        return pl;
    }
    return NULL;
}

/**
 * @brief Merge every run of adjacent free blocks into one.
 * @param c In/out. The take.
 */
MMGR_INLINE void confin_coalesce(ConfinCtx *c)
{
    mmgr_confin *const a = c->a;
    size_t off = 0;

    while (off < a->persist_end)
    {
        ABlk *cur = (ABlk *)(a->base + off);
        const size_t next_off = off + AHDR + cur->size;
        if (!cur->used && (next_off < a->persist_end))
        {
            const ABlk *nxt = (const ABlk *)(a->base + next_off);
            if (!nxt->used)
            {
                cur->size += AHDR + nxt->size;
                continue;
            }
        }
        off = next_off;
    }
}

/**
 * @brief Wind the up-growing end back over a trailing free block.
 * @param c In/out. The take.
 */
MMGR_INLINE void confin_trim(ConfinCtx *c)
{
    mmgr_confin *const a = c->a;
    size_t off = 0;
    size_t last = 0;

    while (off < a->persist_end)
    {
        last = off;
        const ABlk *cur = (const ABlk *)(a->base + off);
        off += AHDR + cur->size;
    }
    if ((a->persist_end > 0) && !((ABlk *)(a->base + last))->used)
    {
        a->persist_end = last;
    }
}

/**
 * @brief Give back a persist take.
 * @param c In/out. The take.
 */
MMGR_INLINE void confin_persist_reddo(ConfinCtx *c)
{
    if (c->p == NULL)
    {
        return;
    }

    ABlk *b = (ABlk *)((uint8_t *)c->p - AHDR);
    if (b->used)
    {
        b->used = 0;
        /* persist_used is the sum of the sizes of the blocks in use and this block is one of them,
           so it cannot be the smaller of the two. Kept so a corrupted header cannot underflow the
           tally. */
        if (c->a->persist_used >= b->size) /* GCOVR_EXCL_BR_LINE */
        {
            c->a->persist_used -= b->size;
        }
    }

    confin_coalesce(c);
    confin_trim(c);
}

/**
 * @brief How much the two ends still have between them.
 * @param a The region.
 * @return Byte count, less what a header would cost.
 */
MMGR_INLINE size_t confin_octas_praesto(const ConfinCtx *c)
{
    mmgr_confin *const a = c->a;
    const size_t mid = (a->scratch_top > a->persist_end) ? (a->scratch_top - a->persist_end) : 0;

    return (mid > AHDR) ? (mid - AHDR) : 0;
}

/**
 * @brief How much the up-growing end holds.
 * @param a The region.
 * @return Byte count.
 */
MMGR_INLINE size_t confin_persist_used(const ConfinCtx *c)
{
    return c->a->persist_used;
}

/**
 * @brief Start a set with no regions in it.
 * @param s In/out. The set.
 */
MMGR_INLINE void confin_set_init(mmgr_confin_set *const s)
{
    s->count = 0;
}

/**
 * @brief Add a region to a set.
 * @param c In/out. The set.
 * @return MMGR_FALSE if the set is full or the buffer is too small to hold anything.
 */
MMGR_INLINE mmgr_bool confin_set_add(ConfinCtx *c)
{
    if (c->s->count >= MMGR_CONFIN_MAX_REGIONS)
    {
        return MMGR_FALSE;
    }

    mmgr_confin *const r = &c->s->region[c->s->count];
    mmgr_confin_init(r, c->base, c->n);
    if (r->size < (AHDR + MMGR_CONFIN_ALIGN))
    {
        return MMGR_FALSE;
    }
    c->s->count++;
    return MMGR_TRUE;
}

/**
 * @brief Take from the first region in the set that can serve it.
 * @param c In/out. The set.
 * @return The bytes, or NULL if none could.
 */
MMGR_INLINE void *confin_set_persist_capio(ConfinCtx *c)
{
    for (size_t i = 0; i < c->s->count; i++)
    {
        void *p = mmgr_confin_persist_capio(&c->s->region[i], c->n);
        if (p != NULL)
        {
            return p;
        }
    }
    return NULL;
}

/**
 * @brief Give back to whichever region the pointer came from.
 * @param c In/out. The set.
 */
MMGR_INLINE void confin_set_persist_reddo(ConfinCtx *c)
{
    if (c->p == NULL)
    {
        return;
    }

    const uint8_t *b = (const uint8_t *)c->p;
    for (size_t i = 0; i < c->s->count; i++)
    {
        mmgr_confin *const r = &c->s->region[i];
        if ((b >= r->base) && (b < (r->base + r->size)))
        {
            mmgr_confin_persist_reddo(r, c->p);
            return;
        }
    }
}

/**
 * @brief Take from the down-growing end of the first region that can serve it.
 * @param c In/out. The set.
 * @return The bytes, or NULL if none could.
 */
MMGR_INLINE void *confin_set_interim_capio_aligned(ConfinCtx *c)
{
    for (size_t i = 0; i < c->s->count; i++)
    {
        void *p = mmgr_confin_interim_capio_aligned(&c->s->region[i], c->n, c->align);
        if (p != NULL)
        {
            return p;
        }
    }
    return NULL;
}

/**
 * @brief Where every region's down-growing end is now.
 * @param c The set.
 * @return The mark.
 */
MMGR_INLINE mmgr_confin_mark confin_set_interim_mark(const ConfinCtx *c)
{
    mmgr_confin_mark m;

    m.count = c->s->count;
    for (size_t i = 0; i < c->s->count; i++)
    {
        m.top[i] = c->s->region[i].scratch_top;
    }
    return m;
}

/**
 * @brief Wind every region back to where the mark was taken.
 * @param c In/out. The set.
 */
MMGR_INLINE void confin_set_interim_reddo(ConfinCtx *c)
{
    const size_t n = (c->mark->count < c->s->count) ? c->mark->count : c->s->count;

    for (size_t i = 0; i < n; i++)
    {
        mmgr_confin_interim_reddo(&c->s->region[i], c->mark->top[i]);
    }
}

/**
 * @brief Empty every region's down-growing end.
 * @param c In/out. The set.
 */
MMGR_INLINE void confin_set_interim_reset(mmgr_confin_set *const s)
{
    for (size_t i = 0; i < s->count; i++)
    {
        mmgr_confin_interim_reset(&s->region[i]);
    }
}

/**
 * @brief What every region still has between its ends, added up.
 * @param c The set.
 * @return Byte count.
 */
MMGR_INLINE size_t confin_set_octas_praesto(const ConfinCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_confin_octas_praesto(&c->s->region[i]);
    }
    return t;
}

/**
 * @brief What every region's up-growing end holds, added up.
 * @param c The set.
 * @return Byte count.
 */
MMGR_INLINE size_t confin_set_persist_used(const ConfinCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_confin_persist_used(&c->s->region[i]);
    }
    return t;
}

/**
 * @brief What every region's down-growing end holds, added up.
 * @param c The set.
 * @return Byte count.
 */
MMGR_INLINE size_t confin_set_interim_used(const ConfinCtx *c)
{
    size_t t = 0;

    for (size_t i = 0; i < c->s->count; i++)
    {
        t += mmgr_confin_interim_used(&c->s->region[i]);
    }
    return t;
}

void mmgr_confin_init(mmgr_confin *const a, void *base, size_t size)
{
    MMGR_CALL(confin_init, ConfinCtx, .a = a, .base = base, .n = size);
}

void *mmgr_confin_persist_capio(mmgr_confin *const a, size_t n)
{
    return MMGR_CALL(confin_persist_capio, ConfinCtx, .a = a, .n = n);
}

void mmgr_confin_persist_reddo(mmgr_confin *const a, void *p)
{
    MMGR_CALL(confin_persist_reddo, ConfinCtx, .a = a, .p = p);
}

void *mmgr_confin_interim_capio(mmgr_confin *const a, size_t n)
{
    return mmgr_confin_interim_capio_aligned(a, n, MMGR_CONFIN_ALIGN);
}

size_t mmgr_confin_octas_praesto(mmgr_confin *const a)
{
    return MMGR_CALL(confin_octas_praesto, ConfinCtx, .a = a);
}

size_t mmgr_confin_persist_used(mmgr_confin *const a)
{
    return MMGR_CALL(confin_persist_used, ConfinCtx, .a = a);
}

void mmgr_confin_set_init(mmgr_confin_set *const s)
{
    confin_set_init(s);
}

mmgr_bool mmgr_confin_set_add(mmgr_confin_set *const s, void *base, size_t size)
{
    return MMGR_CALL(confin_set_add, ConfinCtx, .s = s, .base = base, .n = size);
}

void *mmgr_confin_set_persist_capio(mmgr_confin_set *const s, size_t n)
{
    return MMGR_CALL(confin_set_persist_capio, ConfinCtx, .s = s, .n = n);
}

void mmgr_confin_set_persist_reddo(mmgr_confin_set *const s, void *p)
{
    MMGR_CALL(confin_set_persist_reddo, ConfinCtx, .s = s, .p = p);
}

void *mmgr_confin_set_interim_capio_aligned(mmgr_confin_set *const s, size_t n, size_t align)
{
    return MMGR_CALL(confin_set_interim_capio_aligned, ConfinCtx, .s = s, .n = n, .align = align);
}

void *mmgr_confin_set_interim_capio(mmgr_confin_set *const s, size_t n)
{
    return mmgr_confin_set_interim_capio_aligned(s, n, MMGR_CONFIN_ALIGN);
}

mmgr_confin_mark mmgr_confin_set_interim_mark(mmgr_confin_set *const s)
{
    return MMGR_CALL(confin_set_interim_mark, ConfinCtx, .s = s);
}

void mmgr_confin_set_interim_reddo(mmgr_confin_set *const s, const mmgr_confin_mark *m)
{
    MMGR_CALL(confin_set_interim_reddo, ConfinCtx, .s = s, .mark = m);
}

void mmgr_confin_set_interim_reset(mmgr_confin_set *const s)
{
    confin_set_interim_reset(s);
}

size_t mmgr_confin_set_octas_praesto(mmgr_confin_set *const s)
{
    return MMGR_CALL(confin_set_octas_praesto, ConfinCtx, .s = s);
}

size_t mmgr_confin_set_persist_used(mmgr_confin_set *const s)
{
    return MMGR_CALL(confin_set_persist_used, ConfinCtx, .s = s);
}

size_t mmgr_confin_set_interim_used(mmgr_confin_set *const s)
{
    return MMGR_CALL(confin_set_interim_used, ConfinCtx, .s = s);
}
