#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "device_bench.h"

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "endian/endian.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "locus_carcerum/locus_carcerum.h"
#include "memoria_anularis/memoria_anularis.h"
#include "memoria_externa/memoria_externa.h"
#include "memoria_operor/memoria_operor.h"
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "spatium/spatium.h"
#include "verbum_scrutor/verbum_scrutor.h"

#define ARENA_BYTES 4096u

ParsMemoriaeInternae(general, ARENA_BYTES);

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(general));

static volatile size_t g_take = 64u;

static volatile size_t g_wire_n = 8u;

static volatile size_t g_ext_small = 256u;
static volatile size_t g_ext_large = 65536u;

static volatile size_t g_cmp_len = 8u;

// The distance between the two ends of a backward move, held in a volatile so the collision check
// cannot be settled at compile time. Read as a constant, the branch folds and the arm measures a
// decision the library would have to make at run time
static volatile size_t g_move_gap = 64u;

static const uint8_t *volatile g_cmp_a;
static const uint8_t *volatile g_cmp_b;

static volatile uint64_t g_swap_val = 0x0123456789ABCDEFull;
static volatile unsigned g_swap_off = 0u;

#define CAP 4096u

static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_a[CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_b[CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_d[CAP];

static void bitor_put_ref(mmgr_bitor *writer, uint64_t val, embed_word bit_count)
{
    const uint64_t mask = (bit_count >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << bit_count) - 1u);
    const size_t whole = (size_t)((writer->bit_count + bit_count) / 8u);
    uint64_t work = val & mask;
    embed_word left = bit_count;

    if (whole > (writer->cap - writer->bytes_written))
    {
        writer->overflow = EMBED_TRUE;
        writer->bit_count = 0;
        writer->residue = 0;
        return;
    }

    for (size_t i = 0; i < whole; i++)
    {
        const embed_word take = 8u - writer->bit_count;
        const uint8_t chunk = (uint8_t)(work & 0xFFu);

        writer->out[writer->bytes_written + i] = (uint8_t)(writer->residue | (uint8_t)(chunk << writer->bit_count));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->bit_count = 0;
    }

    if (left != 0u)
    {

        const uint8_t tail = (uint8_t)(work & ((1u << left) - 1u));

        writer->residue = (uint8_t)(writer->residue | (uint8_t)(tail << writer->bit_count));
        writer->bit_count += left;
    }
    writer->bytes_written += whole;
}

static void bitor_put_split(mmgr_bitor *writer, uint64_t val, embed_word bit_count)
{
    const uint64_t mask = (bit_count >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << bit_count) - 1u);
    const size_t whole = (size_t)((writer->bit_count + bit_count) / 8u);
    uint64_t work = val & mask;
    embed_word left = bit_count;

    if (whole > (writer->cap - writer->bytes_written))
    {
        writer->overflow = EMBED_TRUE;
        writer->bit_count = 0;
        writer->residue = 0;
        return;
    }

    uint8_t *const to = writer->out + writer->bytes_written;
    size_t index = 0u;

    if (whole != 0u)
    {
        const embed_word take = 8u - writer->bit_count;

        to[0] = (uint8_t)(writer->residue | (uint8_t)((uint8_t)(work & 0xFFu) << writer->bit_count));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->bit_count = 0;
        index = 1u;

        for (; index < whole; index++)
        {
            to[index] = (uint8_t)(work & 0xFFu);
            work >>= 8u;
            left -= 8u;
        }
    }

    if (left != 0u)
    {
        const uint8_t tail = (uint8_t)(work & ((1u << left) - 1u));

        writer->residue = (uint8_t)(writer->residue | (uint8_t)(tail << writer->bit_count));
        writer->bit_count += left;
    }
    writer->bytes_written += whole;
}

static uint32_t bitor_is_correct(void)
{
    static uint8_t one[512];
    static uint8_t two[512];
    uint32_t bad = 0u;

    mmgr_bitor a = EMBED_CALL(bitio.init, BitorumCfg, .out = one, .cap = sizeof one);
    mmgr_bitor b = EMBED_CALL(bitio.init, BitorumCfg, .out = two, .cap = sizeof two);
    uint64_t seed = 0x9E3779B97F4A7C15ull;

    for (embed_word width = 1u; width <= 64u; width++)
    {
        bitor_put_ref(&a, seed, width);
        bitor_put_split(&b, seed, width);
        seed = (seed * 6364136223846793005ull) + 1442695040888963407ull;
    }

    if ((a.bytes_written != b.bytes_written) || (a.bit_count != b.bit_count) || (a.residue != b.residue))
    {
        bad++;
    }

    for (size_t index = 0; index < a.bytes_written; index++)
    {
        if (one[index] != two[index])
        {
            bad++;
        }
    }
    return bad;
}

static uint64_t take_shape_all(const uint8_t *at, size_t bytes)
{
    uint64_t v = 0u;
    size_t sh = 0u;

    if ((bytes & 8u) != 0u)
    {
        v = EMBED_CALL(proxim.load64, ProximusCfg, .at = at);
    }
    if ((bytes & 4u) != 0u)
    {
        v |= (uint64_t)EMBED_CALL(proxim.load32, ProximusCfg, .at = at) << sh;
        at += 4;
        sh += 32u;
    }
    if ((bytes & 2u) != 0u)
    {
        v |= (uint64_t)EMBED_CALL(proxim.load16, ProximusCfg, .at = at) << sh;
        at += 2;
        sh += 16u;
    }
    if ((bytes & 1u) != 0u)
    {
        v |= (uint64_t)(*at) << sh;
    }
    return EMBED_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)bytes);
}

static uint64_t take_shape_exit(const uint8_t *at, size_t bytes)
{
    uint64_t v = 0u;

    if ((bytes & 8u) != 0u)
    {
        v = EMBED_CALL(proxim.load64, ProximusCfg, .at = at);
    }
    else
    {
        size_t sh = 0u;

        if ((bytes & 4u) != 0u)
        {
            v |= (uint64_t)EMBED_CALL(proxim.load32, ProximusCfg, .at = at) << sh;
            at += 4;
            sh += 32u;
        }
        if ((bytes & 2u) != 0u)
        {
            v |= (uint64_t)EMBED_CALL(proxim.load16, ProximusCfg, .at = at) << sh;
            at += 2;
            sh += 16u;
        }
        if ((bytes & 1u) != 0u)
        {
            v |= (uint64_t)(*at) << sh;
        }
    }
    return EMBED_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)bytes);
}

static embed_iword cmp_unaligned(const uint8_t *a, const uint8_t *b, size_t bytes)
{
    const size_t full = (bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (EMBED_CALL(word.load, ScrutWordCfg, .at = a + at) != EMBED_CALL(word.load, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

typedef struct
{
    const uint8_t *src;
    const uint8_t *other;
    size_t bytes;
} BenchCmpCtx;

static embed_iword cmp_via_ctx(const BenchCmpCtx *args)
{
    const size_t full = (args->bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (EMBED_CALL(word.load, ScrutWordCfg, .at = args->src + at) !=
            EMBED_CALL(word.load, ScrutWordCfg, .at = args->other + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

static embed_iword cmp_ctx_hoisted(const BenchCmpCtx *args)
{
    const uint8_t *const a = args->src;
    const uint8_t *const b = args->other;
    const size_t full = (args->bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (EMBED_CALL(word.load, ScrutWordCfg, .at = a + at) != EMBED_CALL(word.load, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

static embed_iword cmp_aligned(const uint8_t *a, const uint8_t *b, size_t bytes)
{

    if (((((uintptr_t)a) | ((uintptr_t)b)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) != 0u)
    {
        return cmp_unaligned(a, b, bytes);
    }

    const size_t full = (bytes / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    size_t at = 0u;

    while (at != full)
    {
        if (EMBED_CALL(word.load_al, ScrutWordCfg, .at = a + at) !=
            EMBED_CALL(word.load_al, ScrutWordCfg, .at = b + at))
        {
            return 1;
        }
        at += MMGR_SWAR_BYTES;
    }
    return 0;
}

/**
 * @brief Arguments for the backward move arms.
 *
 * @note Mirrors MemorMoveCtx in memoria_operor.c, down to neither pointer being restrict qualified.
 *       move_up is the entry for regions that overlap, which is the thing restrict promises does not
 *       happen, so an arm that added it would time a move the library is not allowed to make.
 */
typedef struct
{
    uint8_t *dst;       /**< Destination [BORROWS]. */
    const uint8_t *src; /**< Source [BORROWS]. */
    size_t bytes;       /**< Bytes to move. */
} BenchMoveCtx;

/**
 * @brief Moves args->bytes backward, one word an iteration, as memor_move_up does today.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note The control. It reproduces the shipping word loop step for step - decrement both pointers,
 *       then one aligned load and one aligned store - so the row against memor.move_up itself shows
 *       whether this reproduction is faithful before the unrolled arms are read against it.
 * @note Advances both pointers to the end and walks them back, which is what the entry does.
 * @warning args->dst must be writable and args->src readable for args->bytes, and both must be
 *          word aligned. The word loop goes through proxim.al_load and proxim.al_put.
 */
EMBED_INLINE void move_up_one_word(BenchMoveCtx *args)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t tail_bytes = args->bytes & (size_t)(sizeof(embed_word) - 1u);
    size_t word_bytes = args->bytes - tail_bytes;

    args->dst += args->bytes;
    args->src += args->bytes;

    while (tail_bytes != 0u)
    {
        args->dst--;
        args->src--;
        *args->dst = *args->src;
        tail_bytes--;
    }
    while (word_bytes != 0u)
    {
        args->dst -= sizeof(embed_word);
        args->src -= sizeof(embed_word);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        word_bytes -= sizeof(embed_word);
    }
}

/**
 * @brief Moves args->bytes backward, four words an iteration, loading and storing each in turn.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note The unroll memor_cpy already has walking forward, turned around. Its comment records what
 *       the width is worth there: a one word loop measured 1.02 cycles a byte against 0.65 on the
 *       ESP32-S3. This arm is what says whether the backward walk gets the same thing.
 * @note Each word is loaded and stored before the next is loaded, which is the order memor_cpy
 *       uses. move_up_four_loads is the same unroll with all four loads taken first, and the row
 *       between the two is what separates the width from the ordering.
 * @note Safe on an overlap with the destination above the source. Every store lands at an address
 *       above every source byte a later pass reads, because the walk descends and the destination
 *       is the higher of the two.
 * @warning args->dst must be writable and args->src readable for args->bytes, and both must be
 *          word aligned.
 */
EMBED_INLINE void move_up_four_words(BenchMoveCtx *args)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t tail_bytes = args->bytes & (size_t)(sizeof(embed_word) - 1u);
    size_t word_bytes = args->bytes - tail_bytes;

    args->dst += args->bytes;
    args->src += args->bytes;

    while (tail_bytes != 0u)
    {
        args->dst--;
        args->src--;
        *args->dst = *args->src;
        tail_bytes--;
    }
    while (word_bytes >= (4u * sizeof(embed_word)))
    {
        args->dst -= 4u * sizeof(embed_word);
        args->src -= 4u * sizeof(embed_word);

        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (3u * sizeof(embed_word)),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + (3u * sizeof(embed_word))));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (2u * sizeof(embed_word)),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + (2u * sizeof(embed_word))));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + sizeof(embed_word),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + sizeof(embed_word)));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));

        word_bytes -= 4u * sizeof(embed_word);
    }
    while (word_bytes != 0u)
    {
        args->dst -= sizeof(embed_word);
        args->src -= sizeof(embed_word);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        word_bytes -= sizeof(embed_word);
    }
}

/**
 * @brief Moves args->bytes backward, four words an iteration, taking all four loads first.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note Same width as move_up_four_words and a different order. Neither pointer is restrict
 *       qualified, so the compiler has to assume a store may land on the next word to be loaded and
 *       cannot lift a load above a store on its own. Writing the four loads out first is how the
 *       four become independent without promising something move_up cannot promise.
 * @note Still safe on an overlap with the destination above the source, and for a wider margin than
 *       the interleaved form: nothing is written until all four words are in registers.
 * @warning args->dst must be writable and args->src readable for args->bytes, and both must be
 *          word aligned.
 */
EMBED_INLINE void move_up_four_loads(BenchMoveCtx *args)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t tail_bytes = args->bytes & (size_t)(sizeof(embed_word) - 1u);
    size_t word_bytes = args->bytes - tail_bytes;

    args->dst += args->bytes;
    args->src += args->bytes;

    while (tail_bytes != 0u)
    {
        args->dst--;
        args->src--;
        *args->dst = *args->src;
        tail_bytes--;
    }
    while (word_bytes >= (4u * sizeof(embed_word)))
    {
        args->dst -= 4u * sizeof(embed_word);
        args->src -= 4u * sizeof(embed_word);

        const embed_word third = EMBED_CALL(proxim.al_load, ProximusCfg,
                                            .at = args->src + (3u * sizeof(embed_word)));
        const embed_word second = EMBED_CALL(proxim.al_load, ProximusCfg,
                                             .at = args->src + (2u * sizeof(embed_word)));
        const embed_word first = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + sizeof(embed_word));
        const embed_word zeroth = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src);

        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (3u * sizeof(embed_word)), .val = third);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (2u * sizeof(embed_word)), .val = second);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + sizeof(embed_word), .val = first);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst, .val = zeroth);

        word_bytes -= 4u * sizeof(embed_word);
    }
    while (word_bytes != 0u)
    {
        args->dst -= sizeof(embed_word);
        args->src -= sizeof(embed_word);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        word_bytes -= sizeof(embed_word);
    }
}

/**
 * @brief Moves args->bytes backward by copying disjoint chunks down from the far end.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note The distance between the two pointers is the collision point, and one subtraction finds it
 *       before anything moves. Nothing here has to be told what the caller is doing - a coalesce, a
 *       header insert, anything else - only that a move of args->bytes from one address to the other
 *       runs into itself.
 * @note Where the destination sits at least args->bytes above the source the regions never touch,
 *       and the whole move is one memor.cpy - the forward copy, four words an iteration, both
 *       pointers restrict qualified.
 * @note Where they do touch, the move splits into chunks of at most the collision point, taken from
 *       the top down. Each chunk's destination is offset from its own source by exactly that
 *       distance, which is not less than the chunk, so no chunk overlaps itself and each one is
 *       another legal memor.cpy. Every write also lands above every byte a later chunk reads,
 *       because the walk descends, so no chunk clobbers another's source.
 * @note A collision point below four words falls through to move_up_one_word, since chunks that
 *       small cost more in per call work than the wide copy returns.
 * @warning args->dst must be writable and args->src readable for args->bytes, and args->dst must be
 *          at or above args->src. A destination below the source is what memor.cpy already handles.
 */
EMBED_INLINE void move_up_collision(BenchMoveCtx *args)
{
    // Explicit casts read both addresses as integers, so the distance is ordinary unsigned
    // arithmetic and never a comparison of pointers into two different objects
    const size_t apart = (size_t)((uintptr_t)args->dst - (uintptr_t)args->src);

    if (apart >= args->bytes)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = args->dst, .src = args->src, .bytes = args->bytes);
        return;
    }
    if (apart >= (4u * sizeof(embed_word)))
    {
        size_t left = args->bytes;

        while (left != 0u)
        {
            const size_t take = (apart < left) ? apart : left;

            left -= take;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = args->dst + left, .src = args->src + left, .bytes = take);
        }
        return;
    }
    move_up_one_word(args);
}

/**
 * @brief Moves bytes backward one at a time, with no library call in it.
 *
 * @param[out] dst   Destination [BORROWS].
 * @param[in]  src   Source [BORROWS].
 * @param[in]  bytes Bytes to move.
 * @note The oracle the three arms are checked against. A byte loop written here, because an
 *       expectation taken from the entry under test agrees with that entry however wrong it is.
 */
static void move_up_reference(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    size_t offset = bytes;

    while (offset != 0u)
    {
        offset--;
        dst[offset] = src[offset];
    }
}

/**
 * @brief Checks every backward move form against the byte reference over the overlaps that matter.
 *
 * @return Count of forms that disagreed with the reference, over every offset and length tried.
 * @note Runs before the timing. A form that moves the wrong bytes still produces a number, and the
 *       number reads as a result.
 * @note The worst overlap is a destination one byte above the source, where a forward walk would
 *       overwrite the byte it is about to read. A whole word and four whole words are the offsets
 *       where the unrolled block straddles its own source.
 */
static uint32_t move_up_is_correct(void)
{
    static const size_t offsets[4] = {1u, 4u, 16u, 64u};
    static const size_t lengths[4] = {8u, 15u, 64u, 2048u};
    uint32_t bad = 0u;

    for (unsigned offset_index = 0u; offset_index < 4u; offset_index++)
    {
        for (unsigned length_index = 0u; length_index < 4u; length_index++)
        {
            const size_t at = offsets[offset_index];
            const size_t bytes = lengths[length_index];

            for (unsigned form = 0u; form < 4u; form++)
            {
                for (size_t fill_index = 0u; fill_index < (bytes + at); fill_index++)
                {
                    // The mix makes a byte carry its own offset, so a byte taken from the wrong
                    // place shows up as itself instead of only as a mismatch
                    g_a[fill_index] = (uint8_t)((((fill_index * 31u) + 17u) & 0xFFu) | 1u);
                    g_d[fill_index] = g_a[fill_index];
                }
                move_up_reference(g_a + at, g_a, bytes);

                if (form == 0u)
                {
                    move_up_one_word(&(BenchMoveCtx){.dst = g_d + at, .src = g_d, .bytes = bytes});
                }
                else if (form == 1u)
                {
                    move_up_four_words(&(BenchMoveCtx){.dst = g_d + at, .src = g_d, .bytes = bytes});
                }
                else if (form == 2u)
                {
                    move_up_four_loads(&(BenchMoveCtx){.dst = g_d + at, .src = g_d, .bytes = bytes});
                }
                else
                {
                    move_up_collision(&(BenchMoveCtx){.dst = g_d + at, .src = g_d, .bytes = bytes});
                }

                for (size_t check_index = 0u; check_index < (bytes + at); check_index++)
                {
                    if (g_d[check_index] != g_a[check_index])
                    {
                        bad++;
                        break;
                    }
                }
            }
        }
    }
    return bad;
}

/**
 * @brief Arguments for the forward copy arms.
 *
 * @note Mirrors MemorCpyCtx in memoria_operor.c, restrict on both pointers included. That
 *       qualifier is what tells the compiler no store here can land on a word still to be loaded,
 *       which it is not told about the backward move. Whether it acts on the permission is the
 *       question copy_four_loads exists to answer.
 * @warning Both pointers are restrict qualified, so the two regions must not overlap.
 */
typedef struct
{
    uint8_t *restrict dst;       /**< Destination [BORROWS]. */
    const uint8_t *restrict src; /**< Source [BORROWS]. */
    size_t bytes;                /**< Bytes to copy. */
} BenchCopyCtx;

/**
 * @brief Copies args->bytes forward, four words an iteration, loading and storing each in turn.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note The control. It reproduces memor_cpy's word loop step for step - four moves against
 *       constant offsets, then one advance of all four - so the row against memor.cpy itself shows
 *       whether this reproduction is faithful before copy_four_loads is read against it.
 * @warning args->dst must be writable and args->src readable for args->bytes, the two must not
 *          overlap, and both must be word aligned.
 */
EMBED_INLINE void copy_four_words(BenchCopyCtx *args)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t tail_bytes = args->bytes & (size_t)(sizeof(embed_word) - 1u);
    size_t word_bytes = args->bytes - tail_bytes;

    while (word_bytes >= (4u * sizeof(embed_word)))
    {
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + sizeof(embed_word),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + sizeof(embed_word)));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (2u * sizeof(embed_word)),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + (2u * sizeof(embed_word))));
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (3u * sizeof(embed_word)),
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + (3u * sizeof(embed_word))));

        args->dst += 4u * sizeof(embed_word);
        args->src += 4u * sizeof(embed_word);
        word_bytes -= 4u * sizeof(embed_word);
    }
    while (word_bytes != 0u)
    {
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        args->dst += sizeof(embed_word);
        args->src += sizeof(embed_word);
        word_bytes -= sizeof(embed_word);
    }
    while (tail_bytes != 0u)
    {
        *args->dst = *args->src;
        args->dst++;
        args->src++;
        tail_bytes--;
    }
}

/**
 * @brief Copies args->bytes forward, four words an iteration, taking all four loads first.
 *
 * @param[in,out] args Destination, source and count [BORROWS].
 * @note Same width as copy_four_words and a different order. restrict already permits the compiler
 *       to reach this arrangement from the interleaved one, so a row that reads 1.00 says it
 *       already does and there is nothing here to take. On the backward move, where the qualifier
 *       is absent, writing the loads out first measured 1.27x on an ESP32-C6 and nothing on an
 *       ESP32-S3.
 * @warning args->dst must be writable and args->src readable for args->bytes, the two must not
 *          overlap, and both must be word aligned.
 */
EMBED_INLINE void copy_four_loads(BenchCopyCtx *args)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t tail_bytes = args->bytes & (size_t)(sizeof(embed_word) - 1u);
    size_t word_bytes = args->bytes - tail_bytes;

    while (word_bytes >= (4u * sizeof(embed_word)))
    {
        const embed_word word_zero = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src);
        const embed_word word_one = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src + sizeof(embed_word));
        const embed_word word_two = EMBED_CALL(proxim.al_load, ProximusCfg,
                                               .at = args->src + (2u * sizeof(embed_word)));
        const embed_word word_three = EMBED_CALL(proxim.al_load, ProximusCfg,
                                                 .at = args->src + (3u * sizeof(embed_word)));

        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst, .val = word_zero);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + sizeof(embed_word), .val = word_one);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (2u * sizeof(embed_word)), .val = word_two);
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + (3u * sizeof(embed_word)), .val = word_three);

        args->dst += 4u * sizeof(embed_word);
        args->src += 4u * sizeof(embed_word);
        word_bytes -= 4u * sizeof(embed_word);
    }
    while (word_bytes != 0u)
    {
        EMBED_CALL(proxim.al_put, ProximusCfg, .dst = args->dst,
                   .val = EMBED_CALL(proxim.al_load, ProximusCfg, .at = args->src));
        args->dst += sizeof(embed_word);
        args->src += sizeof(embed_word);
        word_bytes -= sizeof(embed_word);
    }
    while (tail_bytes != 0u)
    {
        *args->dst = *args->src;
        args->dst++;
        args->src++;
        tail_bytes--;
    }
}

/**
 * @brief Checks both forward copy forms against a byte loop written here.
 *
 * @return Count of forms that disagreed with the reference, over every length tried.
 * @note Runs before the timing, for the reason move_up_is_correct gives. The lengths straddle the
 *       four word block: below it, one past it, and a long run with an odd tail.
 */
static uint32_t copy_is_correct(void)
{
    static const size_t lengths[5] = {8u, 15u, 64u, 129u, 2048u};
    uint32_t bad = 0u;

    for (unsigned length_index = 0u; length_index < 5u; length_index++)
    {
        const size_t bytes = lengths[length_index];

        for (unsigned form = 0u; form < 2u; form++)
        {
            for (size_t fill_index = 0u; fill_index < bytes; fill_index++)
            {
                // The mix makes a byte carry its own offset, so a byte taken from the wrong place
                // shows up as itself instead of only as a mismatch
                g_a[fill_index] = (uint8_t)((((fill_index * 31u) + 17u) & 0xFFu) | 1u);
                g_d[fill_index] = 0u;
            }

            if (form == 0u)
            {
                copy_four_words(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = bytes});
            }
            else
            {
                copy_four_loads(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = bytes});
            }

            for (size_t check_index = 0u; check_index < bytes; check_index++)
            {
                if (g_d[check_index] != g_a[check_index])
                {
                    bad++;
                    break;
                }
            }
        }
    }
    return bad;
}

#define RING_CAP 1024u

#define RING_SEGS 4u

#define RING_SPAN 64u

static mmgr_ring g_ring;
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_ring_buf[RING_CAP];
static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_ring_out[RING_CAP];

static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_hand_buf[RING_CAP];
static size_t g_hand_head;
static size_t g_hand_tail;

static _Atomic size_t g_ord_head;
static _Atomic size_t g_ord_tail;

static embed_bool hand_put(const uint8_t *src, size_t bytes)
{
    if ((RING_CAP - (g_hand_head - g_hand_tail)) < bytes)
    {
        return EMBED_FALSE;
    }

    const size_t at = g_hand_head & (RING_CAP - 1u);
    const size_t first = ((at + bytes) > RING_CAP) ? (RING_CAP - at) : bytes;

    memcpy(g_hand_buf + at, src, first);
    if (first != bytes)
    {
        memcpy(g_hand_buf, src + first, bytes - first);
    }
    g_hand_head += bytes;
    return EMBED_TRUE;
}

static size_t hand_read(uint8_t *dst, size_t bytes)
{
    const size_t held = g_hand_head - g_hand_tail;
    const size_t take = (held < bytes) ? held : bytes;
    const size_t at = g_hand_tail & (RING_CAP - 1u);
    const size_t first = ((at + take) > RING_CAP) ? (RING_CAP - at) : take;

    memcpy(dst, g_hand_buf + at, first);
    if (first != take)
    {
        memcpy(dst + first, g_hand_buf, take - first);
    }
    g_hand_tail += take;
    return take;
}

static uintptr_t ring_segment_cycle(void)
{
    size_t idx = 0u;
    uintptr_t seen = 0u;

    if (EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &g_ring, .out_index = &idx))
    {
        seen |= (uintptr_t)EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &g_ring, .index = idx);
        EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &g_ring);
    }
    if (EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &g_ring, .out_index = &idx))
    {
        seen |= idx + 1u;
        EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &g_ring);
    }
    return seen;
}

static uintptr_t ring_loculus_cycle(void)
{
    const embed_word ready = EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &g_ring);
    const embed_iword slot = EMBED_CALL(anularis.loculus_next, AnularisCfg, .ring = &g_ring, .mask = ready);
    uintptr_t seen = (uintptr_t)ready;

    if (slot >= 0)
    {

        const size_t idx = (size_t)slot;

        if (EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &g_ring, .index = idx, .src = g_a,
                       .bytes = RING_SPAN))
        {
            seen |= (uintptr_t)EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &g_ring, .index = idx);
            EMBED_CALL(anularis.loculus_drop, AnularisCfg, .ring = &g_ring, .index = idx);
            EMBED_CALL(anularis.loculus_mark, AnularisCfg, .ring = &g_ring, .index = idx);
        }
    }
    return seen;
}

static uintptr_t ring_byte_mmgr(void)
{
    uint8_t held = 0u;

    (void)EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_a, .bytes = 1u);
    (void)EMBED_CALL(anularis.read_byte, AnularisCfg, .ring = &g_ring, .dst = &held);
    return (uintptr_t)held;
}

static uintptr_t ring_byte_hand(void)
{
    uint8_t held = 0u;

    (void)hand_put(g_a, 1u);
    (void)hand_read(&held, 1u);
    return (uintptr_t)held;
}

static uintptr_t ring_peek_mmgr(void)
{
    (void)EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_a, .bytes = RING_SPAN);
    EMBED_CALL(anularis.peek, AnularisCfg, .ring = &g_ring, .dst = g_ring_out, .bytes = RING_SPAN, .offset = 0u);
    EMBED_CALL(anularis.consume, AnularisCfg, .ring = &g_ring, .bytes = RING_SPAN);
    return (uintptr_t)g_ring_out[0];
}

static uintptr_t ring_peek_hand(void)
{
    (void)hand_put(g_a, RING_SPAN);

    const size_t at = g_hand_tail & (RING_CAP - 1u);
    const size_t first = ((at + RING_SPAN) > RING_CAP) ? (RING_CAP - at) : RING_SPAN;

    memcpy(g_ring_out, g_hand_buf + at, first);
    if (first != RING_SPAN)
    {
        memcpy(g_ring_out + first, g_hand_buf, RING_SPAN - first);
    }
    g_hand_tail += RING_SPAN;
    return (uintptr_t)g_ring_out[0];
}

static uintptr_t ring_round_mmgr(void)
{
    (void)EMBED_CALL(anularis.put, AnularisCfg, .ring = &g_ring, .src = g_a, .bytes = RING_SPAN);
    return (uintptr_t)EMBED_CALL(anularis.read, AnularisCfg, .ring = &g_ring, .dst = g_ring_out, .bytes = RING_SPAN);
}

static uintptr_t ring_round_hand(void)
{
    (void)hand_put(g_a, RING_SPAN);
    return (uintptr_t)hand_read(g_ring_out, RING_SPAN);
}

static uintptr_t cellblock_persistent_alloc_release(void)
{
    void *const held = ram.general.persistent_buf_alloc(g_take);

    ram.general.persistent_buf_release(held);
    return (uintptr_t)held;
}

static uintptr_t heap_take_give(void)
{
    void *const held = malloc(g_take);

    free(held);
    return (uintptr_t)held;
}

static uintptr_t cellblock_temporary_mark_run(void)
{
    const size_t mark = ram.general.temporary_buf_mark();
    uintptr_t seen = 0u;

    for (unsigned index = 0; index < 8u; index++)
    {
        seen |= (uintptr_t)ram.general.temporary_buf_alloc(g_take);
    }
    ram.general.temporary_buf_release(mark);
    return seen;
}

static uintptr_t heap_run_free(void)
{
    void *held[8];
    uintptr_t seen = 0u;

    for (unsigned index = 0; index < 8u; index++)
    {
        held[index] = malloc(g_take);
        seen |= (uintptr_t)held[index];
    }
    for (unsigned index = 0; index < 8u; index++)
    {
        free(held[index]);
    }
    return seen;
}

static void fill(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {

        g_a[i] = (uint8_t)('a' + (i % 15));
        g_b[i] = (uint8_t)('a' + (i % 15));
    }
}

void dbench_run(void)
{
    static const size_t lens[] = {8u, 16u, 32u, 64u, 128u, 512u, 2048u};

    for (;;)
    {
        DBENCH_BANNER("memoria vs libc");

        printf("DB bitor_check     disagreements=%u\n", (unsigned)bitor_is_correct());

        printf("DB move_up_check   disagreements=%u\n", (unsigned)move_up_is_correct());

        printf("DB copy_check      disagreements=%u\n", (unsigned)copy_is_correct());

        for (unsigned li = 0; li < (sizeof lens / sizeof lens[0]); li++)
        {
            const size_t n = lens[li];
            const uint32_t iters = (n <= 64u) ? 20000u : ((n <= 512u) ? 4000u : 1000u);

            fill(n);

            DBENCH_AB("cmp", iters, n,
                      DBENCH_KEEP(EMBED_CALL(memor.cmp, MemoriaCfg, .src = g_a, .other = g_b, .bytes = n)),
                      DBENCH_KEEP(memcmp(g_a, g_b, n)));

            g_cmp_len = n;

            DBENCH_AB("cmp_entry", iters, n,
                      DBENCH_KEEP(EMBED_CALL(memor.cmp, MemoriaCfg, .src = g_a, .other = g_b, .bytes = g_cmp_len)),
                      DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)));

            DBENCH_AB("cmp_align", iters, n, DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)),
                      DBENCH_KEEP(cmp_aligned(g_a, g_b, g_cmp_len)));

            {
                const BenchCmpCtx ctx = {.src = g_a, .other = g_b, .bytes = g_cmp_len};

                DBENCH_AB("cmp_ctx", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)),
                          DBENCH_KEEP(cmp_unaligned(g_a, g_b, g_cmp_len)));

                DBENCH_AB("cmp_hoist", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)), DBENCH_KEEP(cmp_ctx_hoisted(&ctx)));

                g_cmp_a = g_a;
                g_cmp_b = g_b;
                DBENCH_AB("cmp_opaque", iters, n, DBENCH_KEEP(cmp_via_ctx(&ctx)),
                          DBENCH_KEEP(cmp_unaligned(g_cmp_a, g_cmp_b, g_cmp_len)));

                DBENCH_AB("cmp_al_opaque", iters, n, DBENCH_KEEP(cmp_unaligned(g_cmp_a, g_cmp_b, g_cmp_len)),
                          DBENCH_KEEP(cmp_aligned(g_cmp_a, g_cmp_b, g_cmp_len)));
            }

            DBENCH_AB("chr", iters, n,
                      DBENCH_KEEP(EMBED_CALL(memor.chr, MemoriaCfg, .src = g_a, .bytes = n, .val = 0xFFu)),
                      DBENCH_KEEP(memchr(g_a, 0xFF, n)));

            DBENCH_AB(
                "cpy", iters, n,
                DBENCH_KEEP((EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_d, .src = g_a, .bytes = n), (uintptr_t)g_d)),
                DBENCH_KEEP(memcpy(g_d, g_a, n)));

            DBENCH_AB(
                "set", iters, n,
                DBENCH_KEEP((EMBED_CALL(memor.set, MemoriaCfg, .dst = g_d, .bytes = n, .val = 0x5Au), (uintptr_t)g_d)),
                DBENCH_KEEP(memset(g_d, 0x5A, n)));

            DBENCH_AB("move_up", iters, n,
                      DBENCH_KEEP(
                          (EMBED_CALL(memor.move_up, MemoriaCfg, .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n),
                           (uintptr_t)g_d)),
                      DBENCH_KEEP(memmove(g_d + MMGR_ALIGN_BYTES, g_d, n)));

            DBENCH_AB("move_down", iters, n,
                      DBENCH_KEEP((EMBED_CALL(memor.move_down, MemoriaCfg, .dst = g_d, .src = g_d + MMGR_ALIGN_BYTES,
                                              .bytes = n),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP(memmove(g_d, g_d + MMGR_ALIGN_BYTES, n)));

            // The entry against a step for step reproduction of it. Both walk one word an
            // iteration, so this row is the cost of reaching the entry through the dispatch table
            // and nothing else. It has to read near 1.00 before either row below means anything
            DBENCH_AB("mv_entry", iters, n,
                      DBENCH_KEEP((EMBED_CALL(memor.move_up, MemoriaCfg, .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d,
                                              .bytes = n),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("mv_entry_swapped", iters, n,
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((EMBED_CALL(memor.move_up, MemoriaCfg, .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d,
                                              .bytes = n),
                                   (uintptr_t)g_d)));

            // One word an iteration against four, the same width memor_cpy takes walking forward.
            // Only the width differs between these two: both load and store each word in turn
            DBENCH_AB("mv_roll", iters, n,
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_four_words(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("mv_roll_swapped", iters, n,
                      DBENCH_KEEP((move_up_four_words(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            // Four words either way, and only the order differs: the second arm takes all four
            // loads before any store. Without restrict the compiler cannot make that move itself
            DBENCH_AB("mv_hoist", iters, n,
                      DBENCH_KEEP((move_up_four_words(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_four_loads(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("mv_hoist_swapped", iters, n,
                      DBENCH_KEEP((move_up_four_loads(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_four_words(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            // A collision point of one word, which is below the threshold, so the second arm finds
            // the chunks would be too small and walks backward exactly as the first one does. This
            // row is what the check costs when it routes nowhere
            DBENCH_AB("mv_tight", iters, n,
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_collision(&(BenchMoveCtx){
                                       .dst = g_d + MMGR_ALIGN_BYTES, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            // A collision point of g_move_gap bytes. Below that length the two regions never touch
            // and the whole move is one forward copy; above it the move goes as chunks of that size,
            // taken from the top down. This row is the one the question is about
            DBENCH_AB("mv_collide", iters, n,
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + g_move_gap, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_collision(&(BenchMoveCtx){
                                       .dst = g_d + g_move_gap, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("mv_collide_swapped", iters, n,
                      DBENCH_KEEP((move_up_collision(&(BenchMoveCtx){
                                       .dst = g_d + g_move_gap, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((move_up_one_word(&(BenchMoveCtx){
                                       .dst = g_d + g_move_gap, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)));

            // The collision arm against the forward copy it routes to, at the same collision point.
            // memor.cpy on regions this far apart is the floor a backward move could reach
            DBENCH_AB("mv_vs_cpy", iters, n,
                      DBENCH_KEEP((move_up_collision(&(BenchMoveCtx){
                                       .dst = g_d + g_move_gap, .src = g_d, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_b, .src = g_d, .bytes = n),
                                   (uintptr_t)g_d)));

            // memor.cpy against a step for step reproduction of its word loop. Both are four words
            // an iteration loading and storing each in turn, so this row is the cost of reaching
            // the entry and nothing else. It has to read near 1.00 before the row below means
            // anything
            DBENCH_AB("cp_entry", iters, n,
                      DBENCH_KEEP((EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_d, .src = g_a, .bytes = n),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((copy_four_words(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("cp_entry_swapped", iters, n,
                      DBENCH_KEEP((copy_four_words(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_d, .src = g_a, .bytes = n),
                                   (uintptr_t)g_d)));

            // Four words either way and only the order differs: the second arm takes all four loads
            // before any store. Both pointers are restrict qualified here, which already permits
            // the compiler to reach the second arrangement from the first, so 1.00 says it does
            DBENCH_AB("cp_hoist", iters, n,
                      DBENCH_KEEP((copy_four_words(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((copy_four_loads(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)));

            DBENCH_AB("cp_hoist_swapped", iters, n,
                      DBENCH_KEEP((copy_four_loads(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)),
                      DBENCH_KEEP((copy_four_words(&(BenchCopyCtx){.dst = g_d, .src = g_a, .bytes = n}),
                                   (uintptr_t)g_d)));
        }

        {
            const uint32_t iters = 20000u;

            DBENCH_AB(
                "rev32", iters, 4u,
                DBENCH_KEEP(EMBED_CALL(magna_extremitas.rev, EndianCfg, .val = g_swap_val, .width = MMGR_ENDIAN_32)),
                DBENCH_KEEP(__builtin_bswap32((uint32_t)g_swap_val)));

            DBENCH_AB(
                "rev64", iters, 8u,
                DBENCH_KEEP(EMBED_CALL(magna_extremitas.rev, EndianCfg, .val = g_swap_val, .width = MMGR_ENDIAN_64)),
                DBENCH_KEEP(__builtin_bswap64(g_swap_val)));

            DBENCH_AB("wr32", iters, 4u,
                      DBENCH_KEEP(EMBED_CALL(magna_extremitas.wr, EndianCfg, .dst = g_d + g_swap_off, .val = g_swap_val,
                                             .width = MMGR_ENDIAN_32)),
                      DBENCH_KEEP((memcpy(g_d + g_swap_off, &(uint32_t){__builtin_bswap32((uint32_t)g_swap_val)}, 4u),
                                   (uintptr_t)g_d)));

            DBENCH_AB("rd32", iters, 4u,
                      DBENCH_KEEP(
                          EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = g_d + g_swap_off, .width = MMGR_ENDIAN_32)),
                      DBENCH_KEEP(__builtin_bswap32(*(const uint32_t *)(g_d + g_swap_off))));
        }

        {
            const uint32_t iters = 5000u;

            DBENCH_AB("cellblock_persistent", iters, 64u, DBENCH_KEEP(cellblock_persistent_alloc_release()),
                      DBENCH_KEEP(heap_take_give()));

            DBENCH_AB("cellblock_temporary", iters, 64u, DBENCH_KEEP(cellblock_temporary_mark_run()),
                      DBENCH_KEEP(heap_run_free()));
        }

        {
            const uint32_t iters = 5000u;

            (void)EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_ring_buf, .capacity = RING_CAP,
                             .segment_count = RING_SEGS);
            g_hand_head = 0u;
            g_hand_tail = 0u;

            DBENCH_AB("ring_avail", iters, 8u,
                      DBENCH_KEEP(EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring)),
                      DBENCH_KEEP(g_hand_head - g_hand_tail));

            DBENCH_AB("ring_avail_ord", iters, 8u,
                      DBENCH_KEEP(EMBED_CALL(anularis.available, AnularisCfg, .ring = &g_ring)),
                      DBENCH_KEEP(atomic_load_explicit(&g_ord_head, memory_order_acquire) -
                                  atomic_load_explicit(&g_ord_tail, memory_order_acquire)));

            DBENCH_AB("ring_vacant", iters, 8u, DBENCH_KEEP(EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &g_ring)),
                      DBENCH_KEEP(RING_CAP - (g_hand_head - g_hand_tail)));

            DBENCH_AB("ring_round", iters, RING_SPAN, DBENCH_KEEP(ring_round_mmgr()), DBENCH_KEEP(ring_round_hand()));

            DBENCH_AB("ring_byte", iters, 1u, DBENCH_KEEP(ring_byte_mmgr()), DBENCH_KEEP(ring_byte_hand()));

            DBENCH_AB("ring_peek", iters, RING_SPAN, DBENCH_KEEP(ring_peek_mmgr()), DBENCH_KEEP(ring_peek_hand()));

            DBENCH_AB("ring_init", iters, 8u,
                      DBENCH_KEEP(EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_ring_buf,
                                             .capacity = RING_CAP, .segment_count = RING_SEGS)),
                      DBENCH_KEEP((g_hand_head = 0u, g_hand_tail = 0u, (uintptr_t)1)));

            (void)EMBED_CALL(anularis.init, AnularisCfg, .ring = &g_ring, .buf = g_ring_buf, .capacity = RING_CAP,
                             .segment_count = RING_SEGS);
            DBENCH_OP("ring_segment", iters, DBENCH_KEEP(ring_segment_cycle()));
            DBENCH_OP("ring_loculus", iters, DBENCH_KEEP(ring_loculus_cycle()));
        }

        {
            const uint32_t iters = 20000u;

            DBENCH_OP("span_from", iters,
                      DBENCH_KEEP(EMBED_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = g_take).cap));

            DBENCH_OP("span_after", iters,
                      DBENCH_KEEP(EMBED_CALL(spat.after, SpatiumCfg,
                                             .span = EMBED_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP),
                                             .count = g_take)
                                      .cap));

            DBENCH_OP("span_ok", iters,
                      DBENCH_KEEP(EMBED_CALL(spat.ok, SpatiumCfg,
                                             .span = EMBED_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP))));

            DBENCH_OP("cellblock_who_owns_buf", iters,
                      DBENCH_KEEP(ram.general.who_owns_buf((const void *)(g_d + g_swap_off))));

            DBENCH_OP("cellblock_buf_available", iters, DBENCH_KEEP(ram.general.buf_available()));
        }

        {
            const uint32_t iters = 20000u;

            DBENCH_OP("ancorae_cost", iters,
                      DBENCH_KEEP(EMBED_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)g_swap_off)));
        }

        {
            const uint32_t iters = 20000u;
            PingPong pingpong;

            EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pingpong);

            DBENCH_OP("ext_place_dram", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.place, ExternaCfg, .size = g_ext_small, .dma_required = EMBED_FALSE,
                                             .free_dram = 65536u, .free_psram = 1048576u, .psram_threshold = 4096u,
                                             .dram_reserve = 8192u)));

            DBENCH_OP("ext_place_psram", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.place, ExternaCfg, .size = g_ext_large, .dma_required = EMBED_FALSE,
                                             .free_dram = 65536u, .free_psram = 1048576u, .psram_threshold = 4096u,
                                             .dram_reserve = 8192u)));

            DBENCH_OP("ext_place_dma", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.place, ExternaCfg, .size = g_ext_small, .dma_required = EMBED_TRUE,
                                             .free_dram = 65536u, .free_psram = 1048576u, .psram_threshold = 4096u,
                                             .dram_reserve = 8192u)));

            DBENCH_OP("ext_pp_fill", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pingpong)));

            DBENCH_OP("ext_pp_drain", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pingpong)));

            DBENCH_OP("ext_pp_swap", iters,
                      DBENCH_KEEP(EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pingpong)));
        }

        {
            const uint32_t iters = 20000u;
            mmgr_bitor bw = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP);

            DBENCH_OP("bit_init", iters, DBENCH_KEEP(EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP).cap));

            DBENCH_OP("bit_put8", iters,
                      (bw = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       EMBED_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 8u),
                       DBENCH_KEEP(bw.bytes_written)));

            DBENCH_OP("bit_put64", iters,
                      (bw = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       EMBED_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 64u),
                       DBENCH_KEEP(bw.bytes_written)));

            DBENCH_OP("bit_put13", iters,
                      (bw = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       EMBED_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 13u),
                       DBENCH_KEEP(bw.bytes_written)));

            DBENCH_OP("bit_align", iters,
                      (bw = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       EMBED_CALL(bitio.put, BitorumCfg, .writer = &bw, .val = g_swap_val, .bit_count = 13u),
                       EMBED_CALL(bitio.align, BitorumCfg, .writer = &bw), DBENCH_KEEP(bw.bytes_written)));

            mmgr_bitor one;
            mmgr_bitor two;

            DBENCH_AB("bit_shape64", iters, 8u,
                      (one = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_ref(&one, g_swap_val, 64u), DBENCH_KEEP(one.bytes_written)),
                      (two = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_split(&two, g_swap_val, 64u), DBENCH_KEEP(two.bytes_written)));

            DBENCH_AB("bit_shape8", iters, 1u,
                      (one = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_ref(&one, g_swap_val, 8u), DBENCH_KEEP(one.bytes_written)),
                      (two = EMBED_CALL(bitio.init, BitorumCfg, .out = g_d, .cap = CAP),
                       bitor_put_split(&two, g_swap_val, 8u), DBENCH_KEEP(two.bytes_written)));
        }

        {
            const uint32_t iters = 20000u;

            g_wire_n = 8u;
            DBENCH_AB("wire_take8_shape", iters, 8u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));

            g_wire_n = 4u;
            DBENCH_AB("wire_take4_shape", iters, 4u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));

            g_wire_n = 7u;
            DBENCH_AB("wire_take7_shape", iters, 7u, DBENCH_KEEP(take_shape_all(g_a, g_wire_n)),
                      DBENCH_KEEP(take_shape_exit(g_a, g_wire_n)));
        }

        {
            const uint32_t iters = 20000u;
            const size_t field = 32u;
            const size_t vlen = 20u;

            DBENCH_AB("wire_mpint_fill", iters, (unsigned)field,
                      (EMBED_CALL(memor.set, MemoriaCfg, .dst = g_b, .val = (uint8_t)0, .bytes = field),
                       EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_b + (field - vlen), .src = g_a, .bytes = vlen),
                       DBENCH_KEEP(g_b)),
                      (EMBED_CALL(memor.set, MemoriaCfg, .dst = g_b, .val = (uint8_t)0, .bytes = field - vlen),
                       EMBED_CALL(memor.cpy, MemoriaCfg, .dst = g_b + (field - vlen), .src = g_a, .bytes = vlen),
                       DBENCH_KEEP(g_b)));
        }

        {
            const uint32_t iters = 20000u;
            mmgr_span w = EMBED_CALL(spat.from, SpatiumCfg, .buf = g_d, .cap = CAP);
            mmgr_cspan r = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = g_a, .cap = CAP);
            uint64_t got = 0u;
            const uint8_t *blob = NULL;
            size_t blen = 0u;

            DBENCH_OP("wire_put", iters,
                      (EMBED_CALL(spat.reset, SpatiumCfg, .at = &w),
                       EMBED_CALL(byteio.put, OctetusCfg, .write_span = &w, .byte = 0x5Au), DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_put_be4", iters,
                      (EMBED_CALL(spat.reset, SpatiumCfg, .at = &w),
                       EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 4u),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_put_be8", iters,
                      (EMBED_CALL(spat.reset, SpatiumCfg, .at = &w),
                       EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 8u),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_put_be7", iters,
                      (EMBED_CALL(spat.reset, SpatiumCfg, .at = &w),
                       EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = g_swap_val, .bytes = 7u),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_raw", iters,
                      (EMBED_CALL(spat.reset, SpatiumCfg, .at = &w),
                       EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = g_a, .bytes = g_take),
                       DBENCH_KEEP(w.pos)));

            DBENCH_OP("wire_take_be4", iters,
                      (r.pos = 0u, r.err = EMBED_FALSE,
                       DBENCH_KEEP(EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = 4u))));

            DBENCH_OP("wire_take_be8", iters,
                      (r.pos = 0u, r.err = EMBED_FALSE,
                       DBENCH_KEEP(EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = 8u))));

            EMBED_CALL(spat.reset, SpatiumCfg, .at = &w);
            EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = 64u, .bytes = 4u);
            EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = g_a, .bytes = 64u);

            mmgr_cspan rs = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = g_d, .cap = CAP);

            DBENCH_OP("wire_rd_str", iters,
                      (rs.pos = 0u, rs.err = EMBED_FALSE,
                       DBENCH_KEEP(EMBED_CALL(byteio.rd_str, OctetusCfg, .read_span = &rs, .blob = &blob,
                                              .blob_bytes = &blen))));

            mmgr_span field = EMBED_CALL(spat.from, SpatiumCfg, .buf = g_b, .cap = 32u);

            DBENCH_OP("wire_mpint", iters,
                      DBENCH_KEEP(
                          EMBED_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &field, .src = g_a, .bytes = 20u)));
        }

        DBENCH_DONE();
    }
}

DBENCH_MAIN("memoria")
