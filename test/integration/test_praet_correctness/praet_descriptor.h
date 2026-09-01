/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_descriptor.h
 * @brief A transfer written down: where from, where to, how many bytes, how each address moves, and
 *        what runs after it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note The shape three vendors already agree on. An ESP32 GDMA dma_descriptor_t, an i.MX RT eDMA
 *       TCD and a Zephyr dma_block_config each carry where from, where to, how much, how the
 *       addresses move, who holds it, and what is next. Three architectures, one descriptor.
 * @note Descriptors link, and that is where the arrangements come from. Two whose next point at each
 *       other are ping-pong. One pointing at itself is circular. A chain is scatter-gather. A next of
 *       NULL is one shot. No part has a ping-pong feature and neither does this.
 * @note Who holds a descriptor is per descriptor and not per channel. A channel's own state is the
 *       quaternary core in its flag word, and that answers a different question: the core says what
 *       the channel is doing, and an owner says which link in the chain the engine is on.
 * @note A peripheral end is an address that does not advance. Where that address is comes from the
 *       vendor's own header, which the compiler has already read, so a caller names theirs and no
 *       number of any part appears in this tree.
 */
#ifndef MMGR_TEST_PRAET_DESCRIPTOR_H
#define MMGR_TEST_PRAET_DESCRIPTOR_H

#include "praet_ordo.h"

EMBED_BEGIN_DECLS

/**
 * @brief How an address behaves as a transfer runs.
 *
 * @note The one fact memcpy does not need. Memory to memory advances both sides; a peripheral is an
 *       address that stays where it is, and which side stays put is what makes a transfer a read or a
 *       write.
 * @note Token ids, the same as the statuses and the regions. A misspelling is an undeclared
 *       identifier carrying the name that was written.
 */
typedef enum
{
    PRAET_ADDRESS_ADVANCES = 0, /**< The address moves on by the transfer width each step. */
    PRAET_ADDRESS_FIXED = 1     /**< The address stays where it is, which is what a peripheral is. */
} PraetAddressing;

/**
 * @brief Bytes the engine moves in one step, which is the measure everything else is counted in.
 *
 * @note Named in full for what it is. A transfer's offsets, its length and the boundary a recovery
 *       rounds to are all counted in this, so it is the measure and not a property beside the others.
 * @note Carries the count itself rather than a code, so the assertions below divide a length by it
 *       and test an offset against it with no table in between.
 * @note octetus and verbum are the tree's own words for a byte and a word, as in
 *       octetus_introitus_exitus and verba_scribo.
 */
typedef enum
{
    PRAET_MENSURA_OCTETUS = 1,    /**< One byte a step. */
    PRAET_MENSURA_SEMIVERBUM = 2, /**< Half a word a step. */
    PRAET_MENSURA_VERBUM = 4      /**< A whole word a step. */
} EgoSumMensura;

/**
 * @brief The largest measure this module knows about.
 *
 * @note Raise this when a wider step is added, and the assertion on a descriptor's measure is what
 *       stops one being written that nothing here can carry.
 */
#define PRAET_MENSURA_MAXIMA PRAET_MENSURA_VERBUM

/**
 * @brief Which way a transfer walks the words it moves.
 *
 * @note Named in full, because a bare direction says nothing about what is moving in it. This is the
 *       direction of the movement of the word, and the word is what the engine steps by.
 * @note What memor already answers for a copy, and for the same reason. Where the two ends overlap,
 *       one direction reads words the other has already written over. Where they do not, neither
 *       direction is wrong and the engine takes whichever it likes.
 * @note Settled once and then not asked again. Here it is settled before anything runs, because both
 *       ends of a descriptor come from a declaration.
 */
typedef enum
{
    PRAET_MOTUS_NEUTRUM = 0,  /**< Neither way. The ends do not overlap, so nothing is at risk. */
    PRAET_MOTUS_SURSUM = 1,   /**< Upward, from the low word on, which is safe writing downward. */
    PRAET_MOTUS_DEORSUM = 2   /**< Downward, from the high word back, which is safe writing upward. */
} DirectioMotusVerbi;

/**
 * @brief The direction of the movement of the word, for two spans of one pool.
 *
 * @param from_  Bytes into the pool the read starts at.
 * @param to_    Bytes into the pool the write starts at.
 * @param bytes_ Bytes moved.
 * @return       A DirectioMotusVerbi.
 * @note Integers, so this is answered while compiling. Both spans are offsets into one declared
 *       object, and where they came from is what makes them comparable at all: two addresses handed
 *       in as pointers could not be ordered at compile time, and comparing pointers into separate
 *       objects is not something the language defines.
 * @note Writing upward over a source means the high words must move first, so the walk goes downward.
 *       Writing downward is the mirror of it. Spans that start at the same place move nothing over
 *       anything and take neither.
 */
#define PRAET_DIRECTIO_MOTUS_VERBI_FOR(from_, to_, bytes_)                                                             \
    ((((from_) < ((to_) + (bytes_))) && ((to_) < ((from_) + (bytes_))) && ((to_) != (from_)))                           \
         ? (((to_) > (from_)) ? PRAET_MOTUS_DEORSUM : PRAET_MOTUS_SURSUM)                                               \
         : PRAET_MOTUS_NEUTRUM)

/**
 * @brief Who holds a descriptor.
 *
 * @note Per descriptor. An ESP32 descriptor calls this owner, an eDMA carries DONE and ACTIVE in its
 *       control word, and a Zephyr block config calls it busy. They are the same fact.
 */
typedef enum
{
    PRAET_OWNER_SOFTWARE = 0, /**< The program may read it and write it. */
    PRAET_OWNER_ENGINE = 1    /**< The engine is working from it, and nothing else may touch it. */
} PraetDescriptorOwner;

/**
 * @brief One transfer, written down.
 *
 * @param source                 First byte read [BORROWS].
 * @param destination            First byte written [BORROWS].
 * @param bytes                  How many bytes the transfer moves.
 * @param next                   The descriptor that runs after this one, or NULL where nothing does
 *                               [BORROWS].
 * @param source_addressing      Whether the source address advances, as a PraetAddressing.
 * @param destination_addressing Whether the destination address advances.
 * @param ego_sum_mensura        Bytes moved in one step, as an EgoSumMensura. Every offset, every
 *                               length and the boundary a recovery rounds to are counted in it.
 * @param directio_motus_verbi   Which way the words are walked, as a DirectioMotusVerbi.
 * @param owner                  Who holds it, as a PraetDescriptorOwner.
 * @note Every field except owner is settled when the descriptor is declared. A declaration emits it
 *       as initialized data, so nothing runs to make one usable.
 * @warning source and destination belong to whoever declared the pools they came from, and this holds
 *          neither. The declarator is what proves those pools exist and that the spans fit inside
 *          them.
 */
typedef struct PraetDescriptor PraetDescriptor;

struct PraetDescriptor
{
    const uint8_t *source;
    uint8_t *destination;
    embed_word bytes;
    PraetDescriptor *next;
    uint8_t source_addressing;
    uint8_t destination_addressing;
    uint8_t ego_sum_mensura;
    uint8_t directio_motus_verbi;
    uint8_t owner;
};

/**
 * @brief Fails the build where one end of a transfer does not sit whole steps inside its pool.
 *
 * @param what_   Text naming which end this is, for the message.
 * @param pool_   Pool declared by ParsMemoriaeInternae or ParsMemoriaeExternum.
 * @param offset_ Bytes into that pool the span starts at.
 * @param bytes_  Bytes in the span.
 * @param mensura_  Bytes the engine moves in one step.
 * @note Three conditions in one assertion, because they fail for one reason and a caller fixing one
 *       wants to see the others. The span fits the pool, the offset lands on a step boundary, and the
 *       length is a whole number of steps.
 * @note Static assertions rather than the expression form PraetSubmit uses, because a declarator is
 *       at file scope and a bare expression cannot sit there. Naming the end in the message is what
 *       replaces the member name the expression form prints.
 * @warning Reads sizeof(mmgr_pars_storage_##pool_), so it holds for a span whose offset, length and
 *          width are known while compiling. Anything computed at run time is not bounded by this and
 *          is not bounded anywhere else.
 */
#define PRAET_STEPS_FIT(what_, pool_, offset_, bytes_, mensura_)                                                         \
    EMBED_STATIC_ASSERT(((offset_) + (bytes_)) <= sizeof(mmgr_pars_storage_##pool_),                                   \
                        what_ " runs past the end of " #pool_);                                                        \
    EMBED_STATIC_ASSERT(((offset_) % (mensura_)) == 0u, what_ " starts part way into a step of " #pool_);                 \
    EMBED_STATIC_ASSERT(((bytes_) % (mensura_)) == 0u, what_ " is not a whole number of steps")

/**
 * @brief Declares one memory to memory transfer, and what runs after it.
 *
 * @param name_          Name the descriptor is reached by.
 * @param from_pool_     Pool the bytes are read from.
 * @param from_offset_   Bytes into that pool the read starts at.
 * @param to_pool_       Pool the bytes are written to.
 * @param to_offset_     Bytes into that pool the write starts at.
 * @param bytes_         Bytes to move.
 * @param mensura_         Bytes moved in one step, as a EgoSumMensura.
 * @param next_          The descriptor that runs after this one, or NULL.
 * @note Both pools are named, so both spans are proved to fit before anything runs and both addresses
 *       come from a declaration rather than from a caller stating one. That is the same check
 *       PraetAttach makes, applied to each end.
 * @note Both addresses advance, which is what memory to memory means. A side that stays put is a
 *       peripheral, and naming one needs an address this has no way to take.
 * @note Emitted as initialized data with the owner in software's hands. A descriptor the engine has
 *       not been given is one the program may still write.
 * @note Two different pools are two separately declared objects, so they cannot overlap and the
 *       direction is settled as either. Naming one pool at both ends collides on the enumerators this
 *       emits, because the two carry the same prefix and differ only by the pool. A transfer inside
 *       one pool is PraetDescriptorWithin, which has offsets it can compare.
 * @warning direction_ is stated here and derived by the two macros above this one. Reaching this
 *          directly with the wrong answer is a transfer that reads bytes it has already written over.
 */
#define PRAET_DESCRIPTOR_BODY(name_, source_, source_mode_, dest_, dest_mode_, bytes_, mensura_, motus_, next_)          \
    EMBED_STATIC_ASSERT((mensura_) <= (int)PRAET_MENSURA_MAXIMA,                                                           \
                        #name_ " asks for a step wider than anything this module carries");                            \
    EMBED_STATIC_ASSERT(((mensura_) & ((mensura_) - 1)) == 0, #name_ " asks for a step that is not a power of two");        \
    static PraetDescriptor name_ EMBED_UNUSED = {                                                                      \
        .source = (source_),                                                                                           \
        .destination = (dest_),                                                                                        \
        .bytes = (embed_word)(bytes_),                                                                                 \
        .next = (next_),                                                                                               \
        .source_addressing = (uint8_t)(source_mode_),                                                                  \
        .destination_addressing = (uint8_t)(dest_mode_),                                                               \
        .ego_sum_mensura = (uint8_t)(mensura_),                                                                                    \
        .directio_motus_verbi = (uint8_t)(motus_),                                                                     \
        .owner = (uint8_t)PRAET_OWNER_SOFTWARE,                                                                        \
    }

/**
 * @brief Declares one memory to memory transfer, and what runs after it.
 *
 * @param name_          Name the descriptor is reached by.
 * @param from_pool_     Pool the bytes are read from.
 * @param from_offset_   Bytes into that pool the read starts at.
 * @param to_pool_       Pool the bytes are written to.
 * @param to_offset_     Bytes into that pool the write starts at.
 * @param bytes_         Bytes to move.
 * @param mensura_         Bytes moved in one step, as a EgoSumMensura.
 * @param motus_         Which way the words are walked, as a DirectioMotusVerbi.
 * @param next_          The descriptor that runs after this one, or NULL.
 */
#define PraetDescriptorDeclare(name_, from_pool_, from_offset_, to_pool_, to_offset_, bytes_, mensura_, motus_, next_)    \
    PRAET_STEPS_FIT(#name_ " reading from " #from_pool_, from_pool_, from_offset_, bytes_, mensura_);                     \
    PRAET_STEPS_FIT(#name_ " writing to " #to_pool_, to_pool_, to_offset_, bytes_, mensura_);                             \
    PRAET_DESCRIPTOR_BODY(name_, &mmgr_pars_storage_##from_pool_[from_offset_], PRAET_ADDRESS_ADVANCES,                 \
                          &mmgr_pars_storage_##to_pool_[to_offset_], PRAET_ADDRESS_ADVANCES, bytes_, mensura_, motus_,    \
                          next_)

/**
 * @brief Fails the build where a transfer between two pools names one pool twice.
 *
 * @param name_      Name of the descriptor this belongs to.
 * @param from_pool_ Pool the bytes are read from.
 * @param to_pool_   Pool the bytes are written to.
 * @note Two enumerators carrying one prefix and differing only by the pool, so naming one pool at
 *       both ends declares the same enumerator twice in one enum and the compiler refuses it. The
 *       same idiom MMGR_PARS_CLAIMED_ONCE uses, applied to a pair.
 * @note Worth refusing because the direction a transfer between two pools takes is settled as either,
 *       and that answer is only true because two declared pools are two objects. One pool at both
 *       ends can overlap itself, and PraetDescriptorWithin is what has the offsets to work that out.
 */
#define PRAET_DESCRIPTOR_ENDS_DIFFER(name_, from_pool_, to_pool_)                                                      \
    enum                                                                                                              \
    {                                                                                                                  \
        name_##_end_##from_pool_ = 1,                                                                                  \
        name_##_end_##to_pool_ = 2                                                                                     \
    }

/**
 * @brief Declares one transfer that runs once and stops.
 *
 * @param name_        Name the descriptor is reached by.
 * @param from_pool_   Pool the bytes are read from.
 * @param from_offset_ Bytes into that pool the read starts at.
 * @param to_pool_     Pool the bytes are written to.
 * @param to_offset_   Bytes into that pool the write starts at.
 * @param bytes_       Bytes to move.
 * @param mensura_       Bytes moved in one step.
 * @note A next of NULL, which is the whole of what makes a transfer one shot.
 */
#define PraetOneShot(name_, from_pool_, from_offset_, to_pool_, to_offset_, bytes_, mensura_)                             \
    PRAET_DESCRIPTOR_ENDS_DIFFER(name_, from_pool_, to_pool_);                                                          \
    PraetDescriptorDeclare(name_, from_pool_, from_offset_, to_pool_, to_offset_, bytes_, mensura_,                       \
                           PRAET_MOTUS_NEUTRUM, NULL)

/**
 * @brief Declares one transfer that moves bytes inside a single pool, and works out which way.
 *
 * @param name_        Name the descriptor is reached by.
 * @param pool_        Pool both ends sit in.
 * @param from_offset_ Bytes into that pool the read starts at.
 * @param to_offset_   Bytes into that pool the write starts at.
 * @param bytes_       Bytes to move.
 * @param mensura_       Bytes moved in one step, as a EgoSumMensura.
 * @param next_        The descriptor that runs after this one, or NULL.
 * @note Where the two spans overlap, the direction is what keeps the transfer from reading bytes it
 *       has already written over. It is worked out here from the offsets, once, and never asked
 *       again: a run of this descriptor has the answer in front of it.
 * @note Both spans are offsets into one declared object, which is what makes them comparable at all.
 *       The same question about two pointers could not be answered while compiling, and comparing
 *       pointers into separate objects is not something the language defines.
 */
#define PraetDescriptorWithin(name_, pool_, from_offset_, to_offset_, bytes_, mensura_, next_)                            \
    PraetDescriptorDeclare(name_, pool_, from_offset_, pool_, to_offset_, bytes_, mensura_,                               \
                           PRAET_DIRECTIO_MOTUS_VERBI_FOR(from_offset_, to_offset_, bytes_), next_)

/**
 * @brief Declares one transfer that runs itself again, forever.
 *
 * @param name_        Name the descriptor is reached by.
 * @param from_pool_   Pool the bytes are read from.
 * @param from_offset_ Bytes into that pool the read starts at.
 * @param to_pool_     Pool the bytes are written to.
 * @param to_offset_   Bytes into that pool the write starts at.
 * @param bytes_       Bytes to move.
 * @param mensura_       Bytes moved in one step.
 * @note A descriptor whose next is itself. That is the whole of circular, and no part has a feature
 *       called that either.
 * @note The forward declaration ahead of it is a tentative definition, which C merges with the real
 *       one below. It is what lets the initializer name an address the declaration is still writing.
 */
#define PraetCircular(name_, from_pool_, from_offset_, to_pool_, to_offset_, bytes_, mensura_)                            \
    PRAET_DESCRIPTOR_ENDS_DIFFER(name_, from_pool_, to_pool_);                                                          \
    static PraetDescriptor name_;                                                                                      \
    PraetDescriptorDeclare(name_, from_pool_, from_offset_, to_pool_, to_offset_, bytes_, mensura_,                       \
                           PRAET_MOTUS_NEUTRUM, &name_)

/**
 * @brief Declares two transfers that hand off to each other, forever.
 *
 * @param first_       Name the first descriptor is reached by.
 * @param second_      Name the second descriptor is reached by.
 * @param from_pool_   Pool both transfers read from.
 * @param to_pool_     Pool both transfers write to.
 * @param first_from_  Bytes into the source pool the first transfer reads at.
 * @param first_to_    Bytes into the destination pool the first transfer writes at.
 * @param second_from_ Bytes into the source pool the second transfer reads at.
 * @param second_to_   Bytes into the destination pool the second transfer writes at.
 * @param bytes_       Bytes each transfer moves.
 * @param mensura_       Bytes moved in one step.
 * @note Ping-pong, and it is two descriptors whose next point at each other. Nothing here is a mode
 *       and no part has a bit for it. What makes it double buffering is that the two write different
 *       halves, which is the caller's to arrange by the offsets they pass.
 * @note Both spans are proved against their pools separately, so a pair whose halves overlap or run
 *       off the end fails at the declaration that wrote it.
 */
#define PraetPingPong(first_, second_, from_pool_, to_pool_, first_from_, first_to_, second_from_, second_to_, bytes_,  \
                      mensura_)                                                                                          \
    PRAET_DESCRIPTOR_ENDS_DIFFER(first_, from_pool_, to_pool_);                                                         \
    PRAET_DESCRIPTOR_ENDS_DIFFER(second_, from_pool_, to_pool_);                                                        \
    static PraetDescriptor second_;                                                                                     \
    PraetDescriptorDeclare(first_, from_pool_, first_from_, to_pool_, first_to_, bytes_, mensura_,                        \
                           PRAET_MOTUS_NEUTRUM, &second_);                                                           \
    PraetDescriptorDeclare(second_, from_pool_, second_from_, to_pool_, second_to_, bytes_, mensura_,                     \
                           PRAET_MOTUS_NEUTRUM, &first_)

/**
 * @brief Declares one peripheral register a transfer may reach.
 *
 * @param name_    Name the peripheral is reached by.
 * @param address_ Where the register is, as the vendor's own header gives it.
 * @note The address is not this library's to know and it is not the caller's to invent. Every part
 *       ships a header naming its registers, the compiler has read it, and what a caller writes here
 *       is that name. An ESP32 reaches one through its GDMA peripheral struct, an i.MX RT through
 *       LPUART1->DATA, and neither number appears in this tree.
 * @note Emits a pointer under a mangled name and an enumerator carrying the peripheral's. A
 *       descriptor naming a peripheral pastes onto both, so anything that was not declared here fails
 *       on a name nobody wrote. The same proof a pool gives, for a thing that has no extent.
 * @note volatile, because a register is read and written by something other than this program and a
 *       compiler that cached one would be reading a value the part has moved on from.
 * @warning No extent, which is the point. A peripheral is one address that stays where it is, so
 *          there is nothing for a span check to measure and a transfer's length is bounded by the
 *          pool at its other end alone.
 */
#define PraetPeripheralDeclare(name_, address_)                                                                        \
    static volatile uint8_t *const mmgr_praet_peripheral_##name_ EMBED_UNUSED =                                        \
        (volatile uint8_t *)(address_);                                                                                \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_is_a_declared_peripheral = 1                                                                           \
    }

/**
 * @brief Declares one transfer that reads a pool and writes a peripheral.
 *
 * @param name_        Name the descriptor is reached by.
 * @param from_pool_   Pool the bytes are read from.
 * @param from_offset_ Bytes into that pool the read starts at.
 * @param peripheral_  Peripheral declared by PraetPeripheralDeclare.
 * @param bytes_       Bytes to move.
 * @param mensura_       Bytes moved in one step, as a EgoSumMensura.
 * @param next_        The descriptor that runs after this one, or NULL.
 * @note The source advances and the destination stays where it is. That is the whole of what makes a
 *       transfer a write to a peripheral, and it is the one fact memcpy never needs.
 * @note Neither way to walk, because a pool and a register are not the same object and a transfer
 *       between them cannot read what it has written.
 */
#define PraetToPeripheral(name_, from_pool_, from_offset_, peripheral_, bytes_, mensura_, next_)                          \
    PRAET_STEPS_FIT(#name_ " reading from " #from_pool_, from_pool_, from_offset_, bytes_, mensura_);                     \
    PRAET_DESCRIPTOR_BODY(name_, &mmgr_pars_storage_##from_pool_[from_offset_], PRAET_ADDRESS_ADVANCES,                 \
                          (uint8_t *)mmgr_praet_peripheral_##peripheral_, PRAET_ADDRESS_FIXED, bytes_, mensura_,          \
                          PRAET_MOTUS_NEUTRUM, next_);                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_writes_##peripheral_ = peripheral_##_is_a_declared_peripheral                                          \
    }

/**
 * @brief Declares one transfer that reads a peripheral and writes a pool.
 *
 * @param name_       Name the descriptor is reached by.
 * @param peripheral_ Peripheral declared by PraetPeripheralDeclare.
 * @param to_pool_    Pool the bytes are written to.
 * @param to_offset_  Bytes into that pool the write starts at.
 * @param bytes_      Bytes to move.
 * @param mensura_      Bytes moved in one step, as a EgoSumMensura.
 * @param next_       The descriptor that runs after this one, or NULL.
 * @note The mirror of PraetToPeripheral. The source stays where it is and the destination advances,
 *       which is what makes a transfer a read from a peripheral.
 */
#define PraetFromPeripheral(name_, peripheral_, to_pool_, to_offset_, bytes_, mensura_, next_)                            \
    PRAET_STEPS_FIT(#name_ " writing to " #to_pool_, to_pool_, to_offset_, bytes_, mensura_);                             \
    PRAET_DESCRIPTOR_BODY(name_, (const uint8_t *)mmgr_praet_peripheral_##peripheral_, PRAET_ADDRESS_FIXED,             \
                          &mmgr_pars_storage_##to_pool_[to_offset_], PRAET_ADDRESS_ADVANCES, bytes_, mensura_,            \
                          PRAET_MOTUS_NEUTRUM, next_);                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        name_##_reads_##peripheral_ = peripheral_##_is_a_declared_peripheral                                           \
    }

/**
 * @brief Counts the descriptors a chain reaches before it repeats or ends.
 *
 * @param[in] first First descriptor in the chain [BORROWS].
 * @param[in] limit How many links to follow before giving up.
 * @return          Descriptors reached, or @p limit where the chain is longer than that.
 * @note What tells a one shot from a cycle without following a cycle forever. A chain that ends
 *       returns its length; a chain that closes returns the number of distinct descriptors in it.
 * @note Walks by pointer identity against the first descriptor alone, so it names a cycle that
 *       returns to the head and not one that closes further along. A chain that loops back to its
 *       middle reaches the limit, which is the honest answer for a walk that cannot see where it is.
 */
embed_word praet_descriptor_chain_length(const PraetDescriptor *first, embed_word limit);

EMBED_END_DECLS

#endif
