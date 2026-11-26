/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_vector.h
 * @brief A vector behind a const token, with its elements in one movable cell, and its calls on many threads at once.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note The caller holds a guard and the token of the vector. The token is the address of the record, a const
 *       pointer that the linker resolves. The record holds the count, the movable cell of the elements, the
 *       capacity and one claim bit an element. No call gives back the cell or an address in it.
 * @note Each call goes through the movable entries of the guard, which MMGR_VECTOR_THROUGH names. An element
 *       moves between the cell and a buffer of the caller, and never in place.
 * @note at and set claim the bit of their element, then read the count. A push claims the bit of the element at
 *       the count, writes it, and one CAS takes the count up. A pop claims the bit of the last element, and one
 *       CAS takes the count down. LocusCarcerum/ConcurrentVector.lean proves these calls linearizable.
 * @note A growth claims each bit below the count. It copies the elements a chunk at a time through a buffer on the
 *       stack, and it gives the old cell back. LocusCarcerum/Vector.lean proves that a relocation changes nothing
 *       that the caller can see.
 * @note The ceiling bounds the capacity, and the vector refuses a push at the ceiling. A growth that the cellblock
 *       cannot meet refuses its push too.
 */
#ifndef MMGR_MEMORIA_VECTOR_H
#define MMGR_MEMORIA_VECTOR_H

/** @addtogroup mod_vector
 *  @{ */

#include "mmgr.h"

#include "locus_carcerum/locus_carcerum.h"

EMBED_BEGIN_DECLS

/**
 * @brief Bytes of the buffer on the stack that a growth copies the elements through, which a build may set before
 *        it includes this header.
 *
 * @note A growth copies the elements a chunk of these bytes at a time, and the last chunk takes the bytes left.
 */
#ifndef MMGR_VECTOR_CHUNK_BYTES

#define MMGR_VECTOR_CHUNK_BYTES 64u
#endif

/** @brief Asserts that a growth copies at least one byte a chunk. */
EMBED_STATIC_ASSERT(MMGR_VECTOR_CHUNK_BYTES > 0u, "MMGR_VECTOR_CHUNK_BYTES must be above 0");

/** @brief The bits of one word of the claim bits of a vector: one an element. */
#define MMGR_VECTOR_CLAIM_BITS ((size_t)(8u * sizeof(embed_atomic_usize)))

/**
 * @brief The words of claim bits for a ceiling of limit_ elements.
 *
 * @param[in] limit_ The ceiling of the capacity.
 * @return           One bit an element in whole words.
 */
#define MMGR_VECTOR_CLAIM_WORDS(limit_) (((size_t)(limit_) + MMGR_VECTOR_CLAIM_BITS - 1u) / MMGR_VECTOR_CLAIM_BITS)

/**
 * @brief The record of one vector, which its token names.
 *
 * @note The library's storage, positioned at compile time by MemoriaVector. Each member is MMgr's to write, and
 *       the caller reads none of them. The token is a const pointer to the record. It does not change while the
 *       program runs, however often a growth or a move puts the elements in another place.
 * @note count is the word that a push and a pop change, each with one CAS. cell is the identity of the movable
 *       cell, as an integer, and 0 before the first push. capacity is the elements that the cell holds.
 * @note claimed holds one bit an element. A call holds the bit of each element that it reads or writes, and so no
 *       two calls reach one element at once.
 */
typedef struct
{
    embed_atomic_usize count;            /**< Elements in the vector. */
    embed_atomic_uptr cell;              /**< Identity of the movable cell of the elements, 0 where none. */
    embed_atomic_usize capacity;         /**< Elements that the cell holds. */
    const size_t stride;                 /**< Bytes of one element. */
    const size_t limit;                  /**< The ceiling of the capacity, which the claim bits cover. */
    const CellOwner *const *const owner; /**< The owner that counts the cell [BORROWS]. */
    embed_atomic_usize *const claimed;   /**< One claim bit an element, in whole words [BORROWS]. */
} VectorRecord;

/**
 * @brief Declares a vector of element_ with a ceiling of limit_ elements, its cell counted to owner_.
 *
 * @param[in] name_    The token of the vector: a const pointer to its record.
 * @param[in] element_ The type of one element.
 * @param[in] limit_   The ceiling of the capacity, a constant above 0.
 * @param[in] owner_   The identity of the owner that counts the cell, as MMGR_OWNERS declares it [BORROWS].
 * @note Emits the claim bits, the record and the token as data that the linker resolves. Nothing runs before the
 *       first call, and the vector starts with no element and no cell.
 * @note The build fails where the ceiling is 0, where one cell of the ceiling passes MMGR_CARCER_BYTES_MAX, or
 *       where the alignment of the element does not divide MMGR_ALIGN_BYTES.
 * @note EMBED_PASS_MEMORY gives each pass of nvcc its own claim bits, record and token. A kernel reaches the vector in
 *       device memory, and its owner_ is the device copy of the identity.
 * @warning The vector names no cellblock. Each call names the guard that it goes through, and every call on one
 *          vector must name the same guard.
 */
#define MemoriaVector(name_, element_, limit_, owner_)                                                                 \
    EMBED_STATIC_ASSERT((size_t)(limit_) > 0u, #name_ " has a ceiling of 0 elements");                                 \
    EMBED_STATIC_ASSERT((size_t)(limit_) <= (MMGR_CARCER_BYTES_MAX / sizeof(element_)),                                \
                        #name_ " has a ceiling whose cell passes MMGR_CARCER_BYTES_MAX");                              \
    EMBED_STATIC_ASSERT((MMGR_ALIGN_BYTES % EMBED_ALIGNOF(element_)) == 0u,                                            \
                        #name_ " has an element whose alignment does not divide MMGR_ALIGN_BYTES");                    \
    static EMBED_PASS_MEMORY embed_atomic_usize mmgr_vector_claimed_##name_[MMGR_VECTOR_CLAIM_WORDS(limit_)];          \
    static EMBED_PASS_MEMORY VectorRecord mmgr_vector_record_##name_ = {                                               \
        EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u), sizeof(element_),                         \
        (size_t)(limit_),      &(owner_),             mmgr_vector_claimed_##name_};                                    \
    static EMBED_PASS_MEMORY VectorRecord *const name_ = &mmgr_vector_record_##name_

/**
 * @brief The movable entries of a guard, as the members of a MemoriaVectorCfg.
 *
 * @param[in] guard_ The guard of a cellblock with the movable option, MinimumSecurityGuard or MaxSecurityGuard.
 * @note Both guards name the four entries the same way, so either one goes here.
 */
#define MMGR_VECTOR_THROUGH(guard_)                                                                                    \
    .take = (guard_).movable_buf_alloc, .give = (guard_).movable_buf_release, .read = (guard_).movable_buf_read,       \
    .write = (guard_).movable_buf_write

/**
 * @brief Arguments for every mmgr_vector_ entry, each reading only the members it needs.
 *
 * @note Every entry reads vector, and every entry but mmgr_vector_size reads the four movable entries of the
 *       guard. at and set read index, set and push_back read src, and at and pop_back read dst.
 * @note Members left unset are zero, and the entries that ignore them never read them.
 */
typedef struct
{
    VectorRecord *const vector; /**< The token of the vector [BORROWS]. */
    const CarcerMovableEntry *(*const take)(size_t size, const CellOwner *owner); /**< The movable take of the guard. */
    embed_bool (*const give)(const CarcerMovableEntry *cell,
                             const CellOwner *owner);        /**< The movable release of the guard. */
    embed_bool (*const read)(const CarcerMovableCfg *args);  /**< The movable read of the guard. */
    embed_bool (*const write)(const CarcerMovableCfg *args); /**< The movable write of the guard. */
    const size_t index;                                      /**< The element at and set reach. */
    const void *const src;                                   /**< The element set and push_back copy in [BORROWS]. */
    void *const dst;                                         /**< Where at and pop_back copy the element [BORROWS]. */
} MemoriaVectorCfg;

/**
 * @brief Type of the vector dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts the six members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 *       nothing else.
 */
typedef struct
{
    embed_bool (*at)(const MemoriaVectorCfg *args);        /**< Copies the element at index out. */
    embed_bool (*set)(const MemoriaVectorCfg *args);       /**< Copies an element in at index. */
    embed_bool (*push_back)(const MemoriaVectorCfg *args); /**< Appends an element. */
    embed_bool (*pop_back)(const MemoriaVectorCfg *args);  /**< Removes the last element and copies it out. */
    size_t (*size)(const MemoriaVectorCfg *args);          /**< The count of elements. */
    embed_bool (*release)(const MemoriaVectorCfg *args);   /**< Gives the cell back and leaves no element. */
} MemoriaVectorNs;
EMBED_TABLE_LAYOUT(MemoriaVectorNs, at, set, push_back, pop_back, size, release);

/**
 * @brief Copies the element at args->index to args->dst.
 *
 * @param[in,out] args The vector, the guard, the index and dst [BORROWS].
 * @return             EMBED_TRUE where the element came out. EMBED_FALSE where args->index is at or past the count,
 *                     and nothing changes.
 * @note Claims the bit of the element, reads the count, copies the element, and gives the bit back. The read of
 *       the count is the point where the call takes effect.
 * @warning args->dst must be writable for one element.
 */
EMBED_HOST_DEVICE embed_bool mmgr_vector_at(const MemoriaVectorCfg *args);

/**
 * @brief Copies the element at args->src to args->index.
 *
 * @param[in,out] args The vector, the guard, the index and src [BORROWS].
 * @return             EMBED_TRUE where the element went in. EMBED_FALSE where args->index is at or past the count,
 *                     and nothing changes.
 * @note Claims the bit of the element, reads the count, copies the element, and gives the bit back.
 * @warning args->src must be readable for one element.
 */
EMBED_HOST_DEVICE embed_bool mmgr_vector_set(const MemoriaVectorCfg *args);

/**
 * @brief Appends the element at args->src.
 *
 * @param[in,out] args The vector, the guard and src [BORROWS].
 * @return             EMBED_TRUE where the element went in. EMBED_FALSE where the count is at the ceiling, where a
 *                     growth found no cell, or where the guard refused the write, and nothing changes.
 * @note Reads the count, claims the bit of the element at the count, and reads the count again. Where the cell is
 *       full, it grows the cell. It writes the element, and one CAS takes the count up by one, which is the point
 *       where the call takes effect.
 * @note A CAS that finds another count gives the bit back and starts again, as does a claim of a held bit.
 * @warning args->src must be readable for one element.
 */
EMBED_HOST_DEVICE embed_bool mmgr_vector_push_back(const MemoriaVectorCfg *args);

/**
 * @brief Removes the last element and copies it to args->dst.
 *
 * @param[in,out] args The vector, the guard and dst [BORROWS].
 * @return             EMBED_TRUE where an element came out. EMBED_FALSE where the vector has no element, and
 *                     nothing changes.
 * @note Reads the count, and claims the bit of the last element. One CAS takes the count down by one, which is the
 *       point where the call takes effect. Then it copies the element out and gives the bit back.
 * @warning args->dst must be writable for one element.
 * @warning A guard other than the guard of the cell can refuse the read after the CAS. The call then answers
 *          EMBED_FALSE, and the element is gone.
 */
EMBED_HOST_DEVICE embed_bool mmgr_vector_pop_back(const MemoriaVectorCfg *args);

/**
 * @brief Returns the count of elements.
 *
 * @param[in] args The vector [BORROWS].
 * @return         The count, off one word.
 * @warning The answer is only a snapshot. A push or a pop on another thread can change it before the caller acts.
 */
EMBED_HOST_DEVICE size_t mmgr_vector_size(const MemoriaVectorCfg *args);

/**
 * @brief Gives the cell of the vector back, and leaves the vector with no element.
 *
 * @param[in,out] args The vector and the guard [BORROWS].
 * @return             EMBED_TRUE where the cell went back or the vector had none. EMBED_FALSE where the guard
 *                     refused the release of the cell.
 * @note Claims every bit, from the ceiling down, so no other call reaches an element meanwhile. A later push
 *       takes a new cell.
 * @warning The claims are one for each element of the ceiling. A vector with a high ceiling pays that at each
 *          release.
 */
EMBED_HOST_DEVICE embed_bool mmgr_vector_release(const MemoriaVectorCfg *args);

/**
 * @brief Dispatch table instance named memoria_vector, whose members are the mmgr_vector_ entries.
 */
EMBED_TABLE_STORAGE MemoriaVectorNs memoria_vector EMBED_UNUSED = {
    .at = mmgr_vector_at,
    .set = mmgr_vector_set,
    .push_back = mmgr_vector_push_back,
    .pop_back = mmgr_vector_pop_back,
    .size = mmgr_vector_size,
    .release = mmgr_vector_release,
};

EMBED_END_DECLS

/** @} */

#endif
