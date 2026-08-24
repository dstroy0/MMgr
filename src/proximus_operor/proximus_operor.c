/**
 * @brief Loads and stores through typed pointers, in an unaligned family and an aligned one.
 *
 * @note The proxim_ types carry MMGR_RAW, so their loads and stores accept any address.
 * @note The aequus_ types carry MMGR_ALIAS alone, so their loads and stores need a naturally aligned address.
 */
#include "proximus_operor/proximus_operor.h"

/**
 * @brief The unaligned access types, one per width.
 *
 * @note MMGR_RAW is MMGR_ALIGN(1) MMGR_ALIAS, so an access through these needs no particular address.
 * @warning Both halves of MMGR_RAW expand to nothing where their attributes are unavailable.
 */
typedef uint16_t mmgr_proxim_u16_t MMGR_RAW;
typedef uint32_t mmgr_proxim_u32_t MMGR_RAW;
typedef uint64_t mmgr_proxim_u64_t MMGR_RAW;

/**
 * @brief The aligned access type for sixty-four bits.
 *
 * @note MMGR_ALIAS without MMGR_ALIGN(1), so this keeps uint64_t's own alignment.
 * @warning An access through this needs an address aligned for a uint64_t.
 */
typedef uint64_t mmgr_aequus_u64_t MMGR_ALIAS;

/**
 * @brief The two word-width access types, sized by MMGR_RAW_WORD.
 *
 * @note mmgr_proxim_word_t takes any address; mmgr_aequus_word_t needs one aligned for mmgr_migro_word.
 */
typedef mmgr_migro_word mmgr_proxim_word_t MMGR_RAW;
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
 * @brief Reads two bytes from c->at in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The two bytes as a uint16_t.
 * @note Reads through mmgr_proxim_u16_t, so c->at needs no particular alignment.
 * @warning c->at must be readable for two bytes.
 */
MMGR_INLINE uint16_t proxim_load16(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u16_t *)c->at;
}

/**
 * @brief Reads four bytes from c->at in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The four bytes as a uint32_t.
 * @note Reads through mmgr_proxim_u32_t, so c->at needs no particular alignment.
 * @warning c->at must be readable for four bytes.
 */
MMGR_INLINE uint32_t proxim_load32(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u32_t *)c->at;
}

/**
 * @brief Reads eight bytes from c->at in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The eight bytes as a uint64_t.
 * @note Reads through mmgr_proxim_u64_t, so c->at needs no particular alignment.
 * @warning c->at must be readable for eight bytes.
 */
MMGR_INLINE uint64_t proxim_load64(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_u64_t *)c->at;
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from c->at in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The bytes as an mmgr_migro_word.
 * @note Reads through mmgr_proxim_word_t, so c->at needs no particular alignment.
 * @warning c->at must be readable for MMGR_RAW_WORD bytes.
 */
MMGR_INLINE mmgr_migro_word proxim_load(const ProximLoadCtx *c)
{
    return *(const mmgr_proxim_word_t *)c->at;
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from an aligned c->at, in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The bytes as an mmgr_migro_word.
 * @note Reads through mmgr_aequus_word_t, which keeps mmgr_migro_word's alignment, unlike proxim_load.
 * @warning c->at must be readable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
MMGR_INLINE mmgr_migro_word aequus_load(const ProximLoadCtx *c)
{
    return *(const mmgr_aequus_word_t *)c->at;
}

/**
 * @brief Reads eight bytes from an aligned c->at, in the target's own order.
 *
 * @param[in] c Address to read from [BORROWS].
 * @return      The eight bytes as a uint64_t.
 * @note Reads through mmgr_aequus_u64_t, which keeps uint64_t's alignment, unlike proxim_load64.
 * @warning c->at must be readable for eight bytes and aligned for a uint64_t.
 */
MMGR_INLINE uint64_t aequus_load64(const ProximLoadCtx *c)
{
    return *(const mmgr_aequus_u64_t *)c->at;
}

/**
 * @brief Writes the low two bytes of c->val to c->dst in the target's own order.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u16_t, so c->dst needs no particular alignment.
 * @warning c->dst must be writable for two bytes.
 */
MMGR_INLINE void proxim_put16(const ProximPutCtx *c)
{
    // Explicit cast narrows the 64-bit val to the uint16_t the store writes
    *(mmgr_proxim_u16_t *)c->dst = (uint16_t)c->val;
}

/**
 * @brief Writes the low four bytes of c->val to c->dst in the target's own order.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u32_t, so c->dst needs no particular alignment.
 * @warning c->dst must be writable for four bytes.
 */
MMGR_INLINE void proxim_put32(const ProximPutCtx *c)
{
    // Explicit cast narrows the 64-bit val to the uint32_t the store writes
    *(mmgr_proxim_u32_t *)c->dst = (uint32_t)c->val;
}

/**
 * @brief Writes all eight bytes of c->val to c->dst in the target's own order.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_u64_t, so c->dst needs no particular alignment.
 * @note No cast is needed here, since c->val is already a uint64_t.
 * @warning c->dst must be writable for eight bytes.
 */
MMGR_INLINE void proxim_put64(const ProximPutCtx *c)
{
    *(mmgr_proxim_u64_t *)c->dst = c->val;
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of c->val to c->dst in the target's own order.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_proxim_word_t, so c->dst needs no particular alignment.
 * @warning c->dst must be writable for MMGR_RAW_WORD bytes.
 */
MMGR_INLINE void proxim_put(const ProximPutCtx *c)
{
    // Explicit cast narrows the 64-bit val to the mmgr_migro_word the store writes
    *(mmgr_proxim_word_t *)c->dst = (mmgr_migro_word)c->val;
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of c->val to an aligned c->dst.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_aequus_word_t, which keeps mmgr_migro_word's alignment, unlike proxim_put.
 * @warning c->dst must be writable for MMGR_RAW_WORD bytes and aligned for an mmgr_migro_word.
 */
MMGR_INLINE void aequus_put(const ProximPutCtx *c)
{
    // Explicit cast narrows the 64-bit val to the mmgr_migro_word the store writes
    *(mmgr_aequus_word_t *)c->dst = (mmgr_migro_word)c->val;
}

/**
 * @brief Writes all eight bytes of c->val to an aligned c->dst.
 *
 * @param[in] c Destination and value [BORROWS].
 * @note Writes through mmgr_aequus_u64_t, which keeps uint64_t's alignment, unlike proxim_put64.
 * @warning c->dst must be writable for eight bytes and aligned for a uint64_t.
 */
MMGR_INLINE void aequus_put64(const ProximPutCtx *c)
{
    *(mmgr_aequus_u64_t *)c->dst = c->val;
}

/**
 * @brief Copies the bytes that bring c->dst up to an MMGR_RAW_WORD boundary.
 *
 * @param[in,out] c Destination, source and the count still to copy [BORROWS].
 * @note skew is the distance from c->dst up to the next boundary; it copies that many, or c->bytes if fewer.
 * @note Returns at once when c->dst already sits on a boundary, or when c->bytes is 0.
 * @note Advances c->dst and c->src past what it copied and draws that count off c->bytes.
 */
MMGR_INLINE void proxim_head(ProximReadCtx *c)
{
    // Explicit casts hold the negation and the mask at uintptr_t, then bring the byte count back to size_t
    const size_t skew = (size_t)((0u - (uintptr_t)c->dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    size_t t = (skew < c->bytes) ? skew : c->bytes;

    if (t == 0u)
    {
        return;
    }
    c->bytes -= t;

    do
    {
        *c->dst++ = *c->src++;
    } while (--t);
}

/**
 * @brief Copies whole MMGR_RAW_WORD words, leaving fewer than one word for proxim_tail.
 *
 * @param[in,out] c Destination, source and the count still to copy [BORROWS].
 * @note Stores through mmgr_aequus_word_t but loads through mmgr_proxim_word_t, since only c->dst was aligned.
 * @note Advances both pointers and draws the whole words off c->bytes.
 * @warning Depends on proxim_head having run, which is what puts c->dst on a boundary.
 */
MMGR_INLINE void proxim_words(ProximReadCtx *c)
{
    // Explicit cast holds the mask at size_t, matching the byte count whose low bits it clears
    size_t w = c->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);
    if (w == 0u)
    {
        return;
    }
    c->bytes -= w;
    do
    {
        *(mmgr_aequus_word_t *)c->dst = *(const mmgr_proxim_word_t *)c->src;
        c->dst += MMGR_RAW_WORD;
        c->src += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);
}

/**
 * @brief Copies whatever c->bytes is left, one byte at a time.
 *
 * @param[in,out] c Destination, source and the count still to copy [BORROWS].
 * @note Returns at once when nothing is left.
 * @note Advances c->dst and c->src, but leaves c->bytes as it found it, unlike the two stages before it.
 */
MMGR_INLINE void proxim_tail(ProximReadCtx *c)
{
    size_t t = c->bytes;

    if (t == 0u)
    {
        return;
    }

    do
    {
        *c->dst++ = *c->src++;
    } while (--t);
}

/**
 * @brief Copies c->bytes from c->src to c->dst, aligning the destination before the word run.
 *
 * @param[in,out] c Destination, source and count [BORROWS].
 * @note Runs proxim_head, then proxim_words, then proxim_tail.
 * @warning Copies forward, so a c->dst above c->src within one region would read bytes it has already written.
 */
MMGR_INLINE void proxim_read(ProximReadCtx *c)
{
    proxim_head(c);
    proxim_words(c);
    proxim_tail(c);
}

/**
 * @brief Reads two bytes from c->at, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
uint16_t mmgr_proxim_load16(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load16, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Reads four bytes from c->at, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
uint32_t mmgr_proxim_load32(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load32, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Reads eight bytes from c->at, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
uint64_t mmgr_proxim_load64(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load64, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Writes the low two bytes of c->val to c->dst, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_proxim_put16(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put16, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Writes the low four bytes of c->val to c->dst, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_proxim_put32(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put32, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Writes all eight bytes of c->val to c->dst, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_proxim_put64(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put64, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from c->at, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
mmgr_migro_word mmgr_proxim_load(const ProximusCfg *c)
{
    return MMGR_CALL(proxim_load, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of c->val to c->dst, at any alignment.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_proxim_put(const ProximusCfg *c)
{
    MMGR_CALL(proxim_put, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Reads MMGR_RAW_WORD bytes from an aligned c->at.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
mmgr_migro_word mmgr_aequus_load(const ProximusCfg *c)
{
    return MMGR_CALL(aequus_load, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Writes the low MMGR_RAW_WORD bytes of c->val to an aligned c->dst.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_aequus_put(const ProximusCfg *c)
{
    MMGR_CALL(aequus_put, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Reads eight bytes from an aligned c->at.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
uint64_t mmgr_aequus_load64(const ProximusCfg *c)
{
    return MMGR_CALL(aequus_load64, ProximLoadCtx, .at = c->at);
}

/**
 * @brief Writes all eight bytes of c->val to an aligned c->dst.
 *
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_aequus_put64(const ProximusCfg *c)
{
    MMGR_CALL(aequus_put64, ProximPutCtx, .dst = c->dst, .val = c->val);
}

/**
 * @brief Copies c->size bytes from c->at to c->dst.
 *
 * @note The count comes from c->size here, where every other entry leaves that member alone.
 * @note Documented at the declaration in proximus_operor.h.
 */
void mmgr_proxim_read(const ProximusCfg *c)
{
    // Explicit casts convert the void pointers to the uint8_t pointers ProximReadCtx declares
    MMGR_CALL(proxim_read, ProximReadCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->at, .bytes = c->size);
}
