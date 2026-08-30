/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file proximus_operor.c
 * @brief Loads and stores through typed pointers, in an unaligned family and an aligned one.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note The proxim_ types carry MMGR_RAW, so their loads and stores accept any address.
 * @note The aequus_ types carry MMGR_ALIAS alone, so their loads and stores need a naturally aligned address.
 */
#include "proximus_operor/proximus_operor.h"

/**
 * @brief The sixteen-bit access type, carrying MMGR_RAW so a read or write takes any address.
 */
typedef uint16_t mmgr_proxim_u16_t MMGR_RAW;

/**
 * @brief The thirty-two-bit access type, carrying MMGR_RAW so a read or write takes any address.
 */
typedef uint32_t mmgr_proxim_u32_t MMGR_RAW;

/**
 * @brief The sixty-four-bit access type, carrying MMGR_RAW so a read or write takes any address.
 */
typedef uint64_t mmgr_proxim_u64_t MMGR_RAW;

/**
 * @brief The aligned access type for sixty-four bits.
 *
 * @note MMGR_ALIAS without MMGR_ALIGN(1), so this keeps uint64_t's own alignment.
 * @warning An access through this needs an address aligned for a uint64_t.
 */
typedef uint64_t mmgr_aequus_u64_t MMGR_ALIAS;

/**
 * @brief The word-width access type, carrying MMGR_RAW so a read or write takes any address.
 */
typedef mmgr_migro_word mmgr_proxim_word_t MMGR_RAW;

/**
 * @brief The word-width access type that keeps mmgr_migro_word's own alignment.
 *
 * @warning A read or write through this needs an address aligned for an mmgr_migro_word.
 */
typedef mmgr_migro_word mmgr_aequus_word_t MMGR_ALIAS;

/**
 * @brief Argument for every load, aligned or not.
 */
typedef struct
{
    const uint8_t *at; /**< Address to read from [BORROWS]. */
} ProximLoadCtx;

/**
 * @brief Arguments for every store, aligned or not.
 *
 * @note val is carried at sixty-four bits whatever the store's width, and each backend casts it down.
 */
typedef struct
{
    uint8_t *dst; /**< Address to write to [BORROWS]. */
    uint64_t val; /**< Value to write, taken from its low bytes. */
} ProximPutCtx;

/**
 * @brief Arguments for the byte copy.
 *
 * @note The three stages take this by non-const pointer and advance dst and src while drawing bytes down.
 */
typedef struct
{
    uint8_t *dst;       /**< Destination [BORROWS]. */
    const uint8_t *src; /**< Source [BORROWS]. */
    size_t bytes;       /**< Bytes still to copy. */
} ProximReadCtx;

/**
 * @brief Reads two bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The two bytes as a uint16_t.
 * @note Reads through mmgr_proxim_u16_t, so args->at needs no particular alignment.
 * @warning args->at must be readable for two bytes.
 */
MMGR_INLINE uint16_t proxim_load16(const ProximLoadCtx *args)
{
    return *(const mmgr_proxim_u16_t *)args->at;
}

/**
 * @brief Reads four bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The four bytes as a uint32_t.
 * @note Reads through mmgr_proxim_u32_t, so args->at needs no particular alignment.
 * @warning args->at must be readable for four bytes.
 */
MMGR_INLINE uint32_t proxim_load32(const ProximLoadCtx *args)
{
    return *(const mmgr_proxim_u32_t *)args->at;
}

/**
 * @brief Reads eight bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The eight bytes as a uint64_t.
 * @note Reads through mmgr_proxim_u64_t, so args->at needs no particular alignment.
 * @warning args->at must be readable for eight bytes.
 */
MMGR_INLINE uint64_t proxim_load64(const ProximLoadCtx *args)
{
    return *(const mmgr_proxim_u64_t *)args->at;
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from args->at in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The bytes as an mmgr_migro_word.
 * @note Reads through mmgr_proxim_word_t, so args->at needs no particular alignment.
 * @warning args->at must be readable for MMGR_RAW_WORD bytes.
 */
MMGR_INLINE mmgr_migro_word proxim_load(const ProximLoadCtx *args)
{
    return *(const mmgr_proxim_word_t *)args->at;
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from an aligned args->at, in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The bytes as an mmgr_migro_word.
 * @note Reads through mmgr_aequus_word_t, which keeps mmgr_migro_word's alignment, unlike proxim_load.
 * @warning args->at must be readable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
MMGR_INLINE mmgr_migro_word aequus_load(const ProximLoadCtx *args)
{
    return *(const mmgr_aequus_word_t *)args->at;
}

/**
 * @brief Reads eight bytes from an aligned args->at, in the target's own order.
 *
 * @param[in] args Address to read from [BORROWS].
 * @return         The eight bytes as a uint64_t.
 * @note Reads through mmgr_aequus_u64_t, which keeps uint64_t's alignment, unlike proxim_load64.
 * @warning args->at must be readable for eight bytes and aligned for a uint64_t.
 */
MMGR_INLINE uint64_t aequus_load64(const ProximLoadCtx *args)
{
    return *(const mmgr_aequus_u64_t *)args->at;
}

/**
 * @brief Writes the low two bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u16_t, so args->dst needs no particular alignment.
 * @warning args->dst must be writable for two bytes.
 */
MMGR_INLINE void proxim_put16(const ProximPutCtx *args)
{
    // Explicit cast narrows the 64-bit val to the uint16_t the store writes
    *(mmgr_proxim_u16_t *)args->dst = (uint16_t)args->val;
}

/**
 * @brief Writes the low four bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u32_t, so args->dst needs no particular alignment.
 * @warning args->dst must be writable for four bytes.
 */
MMGR_INLINE void proxim_put32(const ProximPutCtx *args)
{
    // Explicit cast narrows the 64-bit val to the uint32_t the store writes
    *(mmgr_proxim_u32_t *)args->dst = (uint32_t)args->val;
}

/**
 * @brief Writes all eight bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u64_t, so args->dst needs no particular alignment.
 * @note No cast is needed here, since args->val is already a uint64_t.
 * @warning args->dst must be writable for eight bytes.
 */
MMGR_INLINE void proxim_put64(const ProximPutCtx *args)
{
    *(mmgr_proxim_u64_t *)args->dst = args->val;
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of args->val to args->dst in the target's own order.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_word_t, so args->dst needs no particular alignment.
 * @warning args->dst must be writable for MMGR_RAW_WORD bytes.
 */
MMGR_INLINE void proxim_put(const ProximPutCtx *args)
{
    // Explicit cast narrows the 64-bit val to the mmgr_migro_word the store writes
    *(mmgr_proxim_word_t *)args->dst = (mmgr_migro_word)args->val;
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of args->val to an aligned args->dst.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_aequus_word_t, which keeps mmgr_migro_word's alignment, unlike proxim_put.
 * @warning args->dst must be writable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
MMGR_INLINE void aequus_put(const ProximPutCtx *args)
{
    // Explicit cast narrows the 64-bit val to the mmgr_migro_word the store writes
    *(mmgr_aequus_word_t *)args->dst = (mmgr_migro_word)args->val;
}

/**
 * @brief Writes all eight bytes of args->val to an aligned args->dst.
 *
 * @param[in] args Destination and value [BORROWS].
 * @note Writes through mmgr_aequus_u64_t, which keeps uint64_t's alignment, unlike proxim_put64.
 * @note No cast is needed here, since args->val is already a uint64_t.
 * @warning args->dst must be writable for eight bytes and aligned for a uint64_t.
 */
MMGR_INLINE void aequus_put64(const ProximPutCtx *args)
{
    *(mmgr_aequus_u64_t *)args->dst = args->val;
}

/**
 * @brief Copies the bytes that bring args->dst up to an MMGR_RAW_WORD boundary.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note skew is the distance from args->dst up to the next boundary. It copies that many, or args->bytes if fewer.
 * @note Returns at once when args->dst already sits on a boundary, or when args->bytes is 0.
 * @note Advances args->dst and args->src past what it copied and draws that count off args->bytes.
 * @note Both pointers step in the copy statement itself, and t counts down in the while test.
 */
MMGR_INLINE void proxim_head(ProximReadCtx *args)
{
    // Explicit casts hold the negation and the mask at uintptr_t, then bring the byte count back to size_t
    const size_t skew = (size_t)((0u - (uintptr_t)args->dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    size_t t = (skew < args->bytes) ? skew : args->bytes;

    if (t == 0u)
    {
        return;
    }
    args->bytes -= t;

    do
    {
        *args->dst++ = *args->src++;
    } while (--t);
}

/**
 * @brief Copies whole MMGR_RAW_WORD words, leaving fewer than one word for proxim_tail.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note Stores through mmgr_aequus_word_t always, since proxim_head is what put args->dst on a boundary.
 * @note Loads through mmgr_aequus_word_t as well when args->src came to rest on a boundary too, and
 *       through mmgr_proxim_word_t when it did not. Two addresses that started the copy at the same
 *       offset within a word reach the aligned run, which is one load an instruction rather than the
 *       byte sequence the unaligned type compiles to on a target with no unaligned load.
 * @note The test is one mask and one compare for the whole run, not one per word.
 * @note Advances both pointers and draws the whole words off args->bytes.
 * @note The aligned load is what this shape is for: thirty bytes took 143 cycles on an ESP32-S3
 *       before it and 91 after. Seven other shapes were measured against that 91 and every one lost
 *       or tied - four words an iteration 98, eight byte chunks 95, two words an iteration 1.01,
 *       both addresses walked in locals 1.00, a counted loop over a word index 70 against 67, one
 *       function with the head, word and tail counts settled up front 126, and eight words taken as
 *       a single dispatch into straight line moves 97 against 96.
 * @note The loop is not what costs. This run copies thirty bytes in 68 cycles and the memcpy it is
 *       measured against copies the whole thirty in 73. The twenty five cycles proxim_read carries
 *       over it are the entry, the head test and the tail. Removing every per word branch, which the
 *       dispatch row did, moved nothing.
 * @warning Depends on proxim_head having run, which is what puts args->dst on a boundary.
 */
MMGR_INLINE void proxim_words(ProximReadCtx *args)
{
    // Explicit cast holds the mask at size_t, matching the byte count whose low bits it clears
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);
    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    // Explicit cast holds the address at uintptr_t for the mask that asks whether it is on a boundary
    if ((((uintptr_t)args->src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        do
        {
            *(mmgr_aequus_word_t *)args->dst = *(const mmgr_aequus_word_t *)args->src;
            args->dst += MMGR_RAW_WORD;
            args->src += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
        return;
    }

    do
    {
        *(mmgr_aequus_word_t *)args->dst = *(const mmgr_proxim_word_t *)args->src;
        args->dst += MMGR_RAW_WORD;
        args->src += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);
}

/**
 * @brief Copies whatever args->bytes is left, one byte at a time.
 *
 * @param[in,out] args Destination, source and the count still to copy [BORROWS].
 * @note Returns at once when nothing is left.
 * @note Advances args->dst and args->src, but leaves args->bytes as it found it, unlike the two stages before it.
 * @note Both pointers step in the copy statement itself, and the local t counts down in the while test.
 */
MMGR_INLINE void proxim_tail(ProximReadCtx *args)
{
    size_t t = args->bytes;

    if (t == 0u)
    {
        return;
    }

    do
    {
        *args->dst++ = *args->src++;
    } while (--t);
}

/**
 * @brief Copies args->bytes from args->src to args->dst, aligning the destination before the word run.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note Runs proxim_head, then proxim_words, then proxim_tail.
 * @warning Copies forward, so an args->dst above args->src within one region would read bytes it has already written.
 */
MMGR_INLINE void proxim_read(ProximReadCtx *args)
{
    proxim_head(args);
    proxim_words(args);
    proxim_tail(args);
}

/**
 * @brief Binds the unaligned entries to GENERIC_ENTRY, with the context type per entry.
 *
 * @param[in] ReturnType_ Return type of the entry point.
 * @param[in] CtxType_    Context type this entry's backend takes.
 * @param[in] name_       Name after the mmgr_proxim_ and proxim_ prefixes, which the two share.
 * @param[in] ...         Initializers for the CtxType_ literal, written in terms of args.
 * @note CtxType_ is a parameter because a load carries an address, a put carries an address and a
 *       value, and a read carries two addresses and a count.
 */
#define PROXIM_ENTRY(ReturnType_, CtxType_, name_, ...)                                                                \
    GENERIC_ENTRY(mmgr_proxim_, proxim_, CtxType_, ProximusCfg, ReturnType_, name_, __VA_ARGS__)

/**
 * @brief Binds the same to GENERIC_ENTRY_V, for an unaligned entry that returns nothing.
 *
 * @param[in] CtxType_ Context type this entry's backend takes.
 * @param[in] name_    Name after the mmgr_proxim_ and proxim_ prefixes.
 * @param[in] ...      Initializers for the CtxType_ literal, written in terms of args.
 */
#define PROXIM_ENTRY_V(CtxType_, name_, ...)                                                                           \
    GENERIC_ENTRY_V(mmgr_proxim_, proxim_, CtxType_, ProximusCfg, name_, __VA_ARGS__)

/**
 * @brief Binds the aligned entries, which carry their own pair of prefixes.
 *
 * @param[in] ReturnType_ Return type of the entry point.
 * @param[in] CtxType_    Context type this entry's backend takes.
 * @param[in] name_       Name after the mmgr_aequus_ and aequus_ prefixes.
 * @param[in] ...         Initializers for the CtxType_ literal, written in terms of args.
 * @note A separate pair because the aligned strategy is a separate name, not a flag. Merging the two
 *       would emit an aligned access for an address that may not be aligned, which faults on some
 *       machines and silently reads wrong on others.
 */
#define AEQUUS_ENTRY(ReturnType_, CtxType_, name_, ...)                                                                \
    GENERIC_ENTRY(mmgr_aequus_, aequus_, CtxType_, ProximusCfg, ReturnType_, name_, __VA_ARGS__)

/**
 * @brief Binds the same to GENERIC_ENTRY_V, for an aligned entry that returns nothing.
 *
 * @param[in] CtxType_ Context type this entry's backend takes.
 * @param[in] name_    Name after the mmgr_aequus_ and aequus_ prefixes.
 * @param[in] ...      Initializers for the CtxType_ literal, written in terms of args.
 */
#define AEQUUS_ENTRY_V(CtxType_, name_, ...)                                                                           \
    GENERIC_ENTRY_V(mmgr_aequus_, aequus_, CtxType_, ProximusCfg, name_, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in proximus_operor.h.
 * @note read is the only entry that reads args->size. Every other one leaves that member alone.
 */
PROXIM_ENTRY(uint16_t, ProximLoadCtx, load16, .at = args->at)
PROXIM_ENTRY(uint32_t, ProximLoadCtx, load32, .at = args->at)
PROXIM_ENTRY(uint64_t, ProximLoadCtx, load64, .at = args->at)
PROXIM_ENTRY_V(ProximPutCtx, put16, .dst = args->dst, .val = args->val)
PROXIM_ENTRY_V(ProximPutCtx, put32, .dst = args->dst, .val = args->val)
PROXIM_ENTRY_V(ProximPutCtx, put64, .dst = args->dst, .val = args->val)
PROXIM_ENTRY(mmgr_migro_word, ProximLoadCtx, load, .at = args->at)
PROXIM_ENTRY_V(ProximPutCtx, put, .dst = args->dst, .val = args->val)
AEQUUS_ENTRY(mmgr_migro_word, ProximLoadCtx, load, .at = args->at)
AEQUUS_ENTRY_V(ProximPutCtx, put, .dst = args->dst, .val = args->val)
AEQUUS_ENTRY(uint64_t, ProximLoadCtx, load64, .at = args->at)
AEQUUS_ENTRY_V(ProximPutCtx, put64, .dst = args->dst, .val = args->val)
PROXIM_ENTRY_V(ProximReadCtx, read, .dst = (uint8_t *)args->dst, .src = (const uint8_t *)args->at, .bytes = args->size)
