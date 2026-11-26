/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_vector.c
 * @brief The calls of a vector: a claim bit for each element, one CAS on the count, and the elements in one movable
 *        cell.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note Each step here is a step of LocusCarcerum/ConcurrentVector.lean. A claim is one fetch-or that finds the bit
 *       clear, and a give back is one fetch-and. at and set test the count after their claim. A push and a pop
 *       take effect at their CAS of the count.
 * @note A growth holds the bit of the element at the count, and claims each bit below it in order. A release claims
 *       each bit from the ceiling down. So a growth never waits for a bit that a release holds while the release
 *       waits for the bit of the growth.
 * @note No call holds a bit while it waits for another, but a growth and a release. Each claim of a held bit
 *       pauses and tries again.
 */
#include "memoria_vector/memoria_vector.h"

#include "embed_atomic.h"

/**
 * @brief The bit of element index in its word of the claim bits.
 *
 * @param[in] index The element.
 * @return          The word with that one bit set.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_atomic_usize_value vector_bit(size_t index)
{
    // Explicit cast puts the one at the width of the claim word before the shift
    return (embed_atomic_usize_value)1u << (index % MMGR_VECTOR_CLAIM_BITS);
}

/**
 * @brief The word of the claim bits that holds the bit of element index.
 *
 * @param[in] vector The vector [BORROWS].
 * @param[in] index  The element.
 * @return           The word [BORROWS].
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_atomic_usize *vector_claim_word(const VectorRecord *vector, size_t index)
{
    return &vector->claimed[index / MMGR_VECTOR_CLAIM_BITS];
}

/**
 * @brief Claims the bit of element index once.
 *
 * @param[in,out] vector The vector [BORROWS].
 * @param[in]     index  The element.
 * @return               EMBED_TRUE where the fetch-or found the bit clear, and the caller now holds it.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool vector_claim_once(const VectorRecord *vector, size_t index)
{
    const embed_atomic_usize_value bit = vector_bit(index);
    const embed_atomic_usize_value found = EMBED_CALL(embed_atomic_usize_table.fetch_or, EmbedAtomicUsizeFetchOrCfg,
                                                      .word = vector_claim_word(vector, index), .value = bit);
    // Explicit cast reads the test of the bit as the answer of the claim
    return (embed_bool)((found & bit) == 0u);
}

/**
 * @brief Claims the bit of element index, and pauses and tries again until the claim lands.
 *
 * @param[in,out] vector The vector [BORROWS].
 * @param[in]     index  The element.
 */
EMBED_HOST_DEVICE EMBED_INLINE void vector_claim(const VectorRecord *vector, size_t index)
{
    embed_bool claimed = vector_claim_once(vector, index);
    while (!claimed)
    {
        EMBED_TURNSTILE_SPIN_PAUSE();
        claimed = vector_claim_once(vector, index);
    }
}

/**
 * @brief Gives the bit of element index back.
 *
 * @param[in,out] vector The vector [BORROWS].
 * @param[in]     index  The element, whose bit the caller holds.
 */
EMBED_HOST_DEVICE EMBED_INLINE void vector_give(const VectorRecord *vector, size_t index)
{
    (void)EMBED_CALL(embed_atomic_usize_table.fetch_and, EmbedAtomicUsizeFetchAndCfg,
                     .word = vector_claim_word(vector, index), .value = ~vector_bit(index));
}

/**
 * @brief The count of elements.
 *
 * @param[in] vector The vector [BORROWS].
 * @return           The count, off one word.
 */
EMBED_HOST_DEVICE EMBED_INLINE size_t vector_count(const VectorRecord *vector)
{
    // Explicit cast reads the word as the count it holds
    return (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &vector->count);
}

/**
 * @brief The elements that the cell holds.
 *
 * @param[in] vector The vector [BORROWS].
 * @return           The capacity, off one word.
 */
EMBED_HOST_DEVICE EMBED_INLINE size_t vector_capacity(const VectorRecord *vector)
{
    // Explicit cast reads the word as the capacity it holds
    return (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &vector->capacity);
}

/**
 * @brief The identity of the movable cell of the elements.
 *
 * @param[in] vector The vector [BORROWS].
 * @return           The identity, NULL where the vector has no cell [BORROWS].
 */
EMBED_HOST_DEVICE EMBED_INLINE const CarcerMovableEntry *vector_cell(const VectorRecord *vector)
{
    // Explicit cast takes the word back to the identity that a growth stored in it as an integer
    return (const CarcerMovableEntry *)(uintptr_t)EMBED_CALL(embed_atomic_uptr_table.load, EmbedAtomicUptrLoadCfg,
                                                             .word = &vector->cell);
}

/**
 * @brief Puts a new cell and its capacity in the record.
 *
 * @param[in,out] vector   The vector [BORROWS].
 * @param[in]     cell     The identity of the new cell, NULL for none [BORROWS].
 * @param[in]     capacity The elements that the new cell holds.
 * @note The caller holds each bit that a call can hold while it reads the cell, so no call reads the two halfway.
 */
EMBED_HOST_DEVICE EMBED_INLINE void vector_place(VectorRecord *vector, const CarcerMovableEntry *cell, size_t capacity)
{
    // Explicit cast stores the identity as the integer the word holds
    EMBED_CALL(embed_atomic_uptr_table.store, EmbedAtomicUptrStoreCfg, .word = &vector->cell,
               .value = (embed_atomic_uptr_value)(uintptr_t)cell);
    // Explicit cast stores the capacity as the value the word holds
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &vector->capacity,
               .value = (embed_atomic_usize_value)capacity);
}

/**
 * @brief The capacity after a growth: twice the capacity, 1 for a capacity of 0, and never past the ceiling.
 *
 * @param[in] capacity The capacity before the growth.
 * @param[in] limit    The ceiling.
 * @return             The capacity after it, as grown of LocusCarcerum/Vector.lean gives it.
 */
EMBED_HOST_DEVICE EMBED_INLINE size_t vector_grown(size_t capacity, size_t limit)
{
    const size_t doubled = (capacity == 0u) ? 1u : (2u * capacity);
    return (doubled < limit) ? doubled : limit;
}

/**
 * @brief Copies bytes from the start of one movable cell to the start of another, a chunk at a time through a
 *        buffer on the stack.
 *
 * @param[in] args The guard [BORROWS].
 * @param[in] from The cell that the bytes come from [BORROWS].
 * @param[in] to   The cell that the bytes go to [BORROWS].
 * @param[in] bytes The bytes to copy.
 * @return          EMBED_TRUE where each chunk read and wrote, EMBED_FALSE at the first that did not.
 * @note The movable option has no copy from one cell to another, so each chunk goes out to the stack and back
 *       in. This is copyRun of LocusCarcerum/Vector.lean, which proves that it copies each byte for each chunk size.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool vector_copy(const MemoriaVectorCfg *args, const CarcerMovableEntry *from,
                                                      const CarcerMovableEntry *to, size_t bytes)
{
    uint8_t chunk[MMGR_VECTOR_CHUNK_BYTES];
    size_t offset = 0u;
    embed_bool copied = EMBED_TRUE;
    while (copied && (offset < bytes))
    {
        const size_t left = bytes - offset;
        const size_t step = (left < MMGR_VECTOR_CHUNK_BYTES) ? left : MMGR_VECTOR_CHUNK_BYTES;
        const embed_bool read =
            EMBED_CALL(args->read, CarcerMovableCfg, .cell = from, .offset = offset, .dst = chunk, .bytes = step);
        copied = read &&
                 EMBED_CALL(args->write, CarcerMovableCfg, .cell = to, .offset = offset, .src = chunk, .bytes = step);
        offset += step;
    }
    return copied;
}

/**
 * @brief Grows the cell of a full vector: a cell of the grown capacity, the elements copied to it, the old cell
 *        given back.
 *
 * @param[in,out] args  The vector and the guard [BORROWS].
 * @param[in]     count The count, whose bit the caller holds, which is the capacity.
 * @return              EMBED_TRUE where the vector has the grown cell. EMBED_FALSE where the cellblock refused
 *                      the cell, and nothing changes.
 * @note Claims each bit below the count first, so no other call reads or writes an element while the cell
 *       changes. This is the relocation of LocusCarcerum/Vector.lean, which changes nothing that the caller can see.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool vector_grow(const MemoriaVectorCfg *args, size_t count)
{
    VectorRecord *const vector = args->vector;
    for (size_t index = 0u; index < count; index++)
    {
        vector_claim(vector, index);
    }
    const CarcerMovableEntry *const old_cell = vector_cell(vector);
    const size_t grown = vector_grown(count, vector->limit);
    const CarcerMovableEntry *const new_cell = args->take(grown * vector->stride, *vector->owner);
    embed_bool placed = EMBED_FALSE;
    if (new_cell != NULL)
    {
        const embed_bool copied = (old_cell == NULL) || vector_copy(args, old_cell, new_cell, count * vector->stride);
        if (copied)
        {
            vector_place(vector, new_cell, grown);
            if (old_cell != NULL)
            {
                (void)args->give(old_cell, *vector->owner);
            }
            placed = EMBED_TRUE;
        }
        else
        {
            (void)args->give(new_cell, *vector->owner);
        }
    }
    for (size_t index = 0u; index < count; index++)
    {
        vector_give(vector, index);
    }
    return placed;
}

EMBED_HOST_DEVICE embed_bool mmgr_vector_at(const MemoriaVectorCfg *args)
{
    VectorRecord *const vector = args->vector;
    vector_claim(vector, args->index);
    const embed_bool legal = (args->index < vector_count(vector));
    const embed_bool copied = legal && EMBED_CALL(args->read, CarcerMovableCfg, .cell = vector_cell(vector),
                                                  .offset = args->index * vector->stride, .dst = args->dst,
                                                  .bytes = vector->stride);
    vector_give(vector, args->index);
    return copied;
}

EMBED_HOST_DEVICE embed_bool mmgr_vector_set(const MemoriaVectorCfg *args)
{
    VectorRecord *const vector = args->vector;
    vector_claim(vector, args->index);
    const embed_bool legal = (args->index < vector_count(vector));
    const embed_bool copied = legal && EMBED_CALL(args->write, CarcerMovableCfg, .cell = vector_cell(vector),
                                                  .offset = args->index * vector->stride, .src = args->src,
                                                  .bytes = vector->stride);
    vector_give(vector, args->index);
    return copied;
}

EMBED_HOST_DEVICE embed_bool mmgr_vector_push_back(const MemoriaVectorCfg *args)
{
    VectorRecord *const vector = args->vector;
    size_t count = vector_count(vector);
    while (count < vector->limit)
    {
        const embed_bool claimed = vector_claim_once(vector, count);
        if (claimed && (vector_count(vector) == count))
        {
            const embed_bool room = (count < vector_capacity(vector)) || vector_grow(args, count);
            if (!room)
            {
                vector_give(vector, count);
                return EMBED_FALSE;
            }
            const embed_bool written =
                EMBED_CALL(args->write, CarcerMovableCfg, .cell = vector_cell(vector),
                           .offset = count * vector->stride, .src = args->src, .bytes = vector->stride);
            if (!written)
            {
                vector_give(vector, count);
                return EMBED_FALSE;
            }
            // Explicit cast puts the count at the width of the word that the CAS compares
            embed_atomic_usize_value expected = (embed_atomic_usize_value)count;
            // Explicit cast puts the count after the push at the width of the word
            const embed_bool committed =
                EMBED_CALL(embed_atomic_usize_table.swap_if, EmbedAtomicUsizeSwapIfCfg, .word = &vector->count,
                           .expected = &expected, .value = (embed_atomic_usize_value)(count + 1u));
            vector_give(vector, count);
            if (committed)
            {
                return EMBED_TRUE;
            }
        }
        else if (claimed)
        {
            vector_give(vector, count);
        }
        else
        {
            EMBED_TURNSTILE_SPIN_PAUSE();
        }
        count = vector_count(vector);
    }
    return EMBED_FALSE;
}

EMBED_HOST_DEVICE embed_bool mmgr_vector_pop_back(const MemoriaVectorCfg *args)
{
    VectorRecord *const vector = args->vector;
    size_t count = vector_count(vector);
    while (count > 0u)
    {
        const size_t last = count - 1u;
        const embed_bool claimed = vector_claim_once(vector, last);
        if (claimed)
        {
            // Explicit cast puts the count at the width of the word that the CAS compares
            embed_atomic_usize_value expected = (embed_atomic_usize_value)count;
            // Explicit cast puts the count after the pop at the width of the word
            const embed_bool committed =
                EMBED_CALL(embed_atomic_usize_table.swap_if, EmbedAtomicUsizeSwapIfCfg, .word = &vector->count,
                           .expected = &expected, .value = (embed_atomic_usize_value)last);
            const embed_bool copied =
                committed && EMBED_CALL(args->read, CarcerMovableCfg, .cell = vector_cell(vector),
                                        .offset = last * vector->stride, .dst = args->dst, .bytes = vector->stride);
            vector_give(vector, last);
            if (committed)
            {
                return copied;
            }
        }
        else
        {
            EMBED_TURNSTILE_SPIN_PAUSE();
        }
        count = vector_count(vector);
    }
    return EMBED_FALSE;
}

EMBED_HOST_DEVICE size_t mmgr_vector_size(const MemoriaVectorCfg *args)
{
    return vector_count(args->vector);
}

EMBED_HOST_DEVICE embed_bool mmgr_vector_release(const MemoriaVectorCfg *args)
{
    VectorRecord *const vector = args->vector;
    for (size_t index = vector->limit; index > 0u; index--)
    {
        vector_claim(vector, index - 1u);
    }
    const CarcerMovableEntry *const cell = vector_cell(vector);
    const embed_bool given = (cell == NULL) || args->give(cell, *vector->owner);
    if (given)
    {
        vector_place(vector, NULL, 0u);
        EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &vector->count, .value = 0u);
    }
    for (size_t index = 0u; index < vector->limit; index++)
    {
        vector_give(vector, index);
    }
    return given;
}
