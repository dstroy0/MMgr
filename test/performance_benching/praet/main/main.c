/* What it costs to pick a move direction from the two addresses, instead of being told.
 *
 * memoria_operor has two moves and the caller picks: move_up when the destination is above the
 * source, move_down when it is below, and the table points move_down at mmgr_memor_cpy because
 * copying forward is already safe that way. Nothing inspects the two addresses and chooses.
 *
 * A DMA submit cannot be told. Both endpoints arrive as addresses, so something has to decide, and
 * this measures what the deciding costs against being handed the answer.
 *
 * Three arms, all reaching the same two entries the same way:
 *
 *   known    the direction is settled at the call site. The floor, not a competitor - it does less
 *            work on purpose, and its number is what the other two are paying against.
 *   branch   compares the two addresses and branches.
 *   mask     builds a mask from the same compare and indexes a two entry table, so nothing predicts.
 *
 * Three cases, because a branch is only expensive when it is wrong:
 *
 *   up       destination above source, every call. Perfectly predicted.
 *   down     destination below source, every call. Perfectly predicted.
 *   mixed    alternating, every call. Predicted no better than a coin.
 *
 * Every offset and length is read from a volatile inside the arm. Read outside, the whole expression
 * is loop invariant and both arms report the harness floor.
 */
#include <stddef.h>
#include <stdint.h>

#include "device_bench.h"

#include "memoria_operor/memoria_operor.h"

#define POOL_BYTES 8192u

/* One pool. Both endpoints live in it, which is what an overlapping move means. */
static EMBED_ALIGN(16) uint8_t g_pool[POOL_BYTES];

/* Volatile so the addresses cannot be folded and the direction cannot be settled at compile time.
 * Without this the compiler knows which way every move goes and the deciding arms measure nothing. */
static volatile size_t g_src_off = 0u;
static volatile size_t g_dst_off = 64u;
static volatile size_t g_len = 64u;

/* Flipped inside the mixed arms so the branch has nothing to learn. */
static volatile unsigned g_alternate;

typedef void (*MoveFn)(const MemoriaCfg *args);

/* The two directions, reached through the dispatch table exactly as a caller reaches them. cpy is
 * what the table points move_down at, so this pair is the real one.
 *
 * Not const, and filled at run time. A member read of memor is not a constant expression, so this
 * cannot be initialized statically. Declaring it const and casting that away to write it puts the
 * table in read-only memory and the first store faults. */
static MoveFn g_move_table[2];

/**
 * @brief Moves the bytes with the direction settled at the call site.
 *
 * @param[in,out] dst Destination [BORROWS].
 * @param[in]     src Source [BORROWS].
 * @param[in]     bytes Bytes to move.
 * @param[in]     up  Non-zero where the destination is above the source.
 * @note The floor. It is handed the answer, so what it costs is the move and nothing else.
 */
static void dir_known(uint8_t *dst, const uint8_t *src, size_t bytes, unsigned up)
{
    if (up != 0u)
    {
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
    else
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
}

/**
 * @brief Moves the bytes, choosing the direction by comparing the two addresses.
 *
 * @param[in,out] dst Destination [BORROWS].
 * @param[in]     src Source [BORROWS].
 * @param[in]     bytes Bytes to move.
 * @note Both addresses are read as uintptr_t, as mmgr_who_owns_buf does, so the comparison is
 *       ordinary unsigned arithmetic and never a comparison of pointers into different objects.
 */
static void dir_branch(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit casts read both addresses as integers. Comparing them as pointers would only be
    // defined within one object, and the compare below has to hold for any two the caller supplies
    if ((uintptr_t)dst > (uintptr_t)src)
    {
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
    else
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
}

/**
 * @brief Moves the bytes, selecting the direction with a mask and no branch.
 *
 * @param[in,out] dst Destination [BORROWS].
 * @param[in]     src Source [BORROWS].
 * @param[in]     bytes Bytes to move.
 * @note The same compare as dir_branch, turned into an index. carcer_hw selects branchlessly the
 *       same way, building a mask from a comparison and choosing between two values with it.
 */
static void dir_mask(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit casts read both addresses as integers, then narrow the comparison's int result to the
    // index it selects with. One is move_up, zero is the forward copy
    const unsigned up = (unsigned)((uintptr_t)dst > (uintptr_t)src);

    g_move_table[up](&(MemoriaCfg){.dst = dst, .src = src, .bytes = bytes});
}

/* ---- correctness, before any timing ----------------------------------------------------------
 *
 * A fast arm that picks the wrong direction still produces a number, and a wrong direction on an
 * overlapping move is the exact defect this bench exists to weigh. So every direction function is
 * checked against a reference first, and the reference is a byte walk written here rather than
 * anything from the library. An expectation taken from the code under test proves nothing.
 */

static uint8_t g_expected[POOL_BYTES];
static uint8_t g_pattern[POOL_BYTES];

/**
 * @brief Fills a region with a pattern whose byte identifies its own offset.
 *
 * @param[out] at    First byte to fill [BORROWS].
 * @param[in]  bytes Bytes to write.
 * @note Mixed from the offset so a byte copied from the wrong place is visible as itself, not merely
 *       as a mismatch. A run of one repeated value would hide a move that overlapped itself.
 */
static void praet_fill_pattern(uint8_t *at, size_t bytes)
{
    for (size_t offset = 0u; offset < bytes; offset++)
    {
        // Explicit cast narrows the mixed value to the byte it fills. The or with 1 keeps it away
        // from zero, so a byte left untouched by a short move is distinguishable from a written one
        at[offset] = (uint8_t)((((offset * 31u) + 17u) & 0xFFu) | 1u);
    }
}

/**
 * @brief Moves bytes one at a time, walking whichever way the overlap requires.
 *
 * @param[in,out] dst   Destination [BORROWS].
 * @param[in]     src   Source [BORROWS].
 * @param[in]     bytes Bytes to move.
 * @note The oracle. A byte loop with the direction chosen from the two addresses, which is what the
 *       three arms are checked against. Nothing here reaches memoria_operor.
 */
static void praet_reference_move(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    // Explicit casts read both addresses as integers, so the comparison is ordinary unsigned
    // arithmetic and not a comparison of pointers into different objects
    if ((uintptr_t)dst > (uintptr_t)src)
    {
        size_t offset = bytes;

        while (offset != 0u)
        {
            offset--;
            dst[offset] = src[offset];
        }
    }
    else
    {
        for (size_t offset = 0u; offset < bytes; offset++)
        {
            dst[offset] = src[offset];
        }
    }
}

/**
 * @brief Returns the offset of the first byte where two regions disagree.
 *
 * @param[in] left  First region [BORROWS].
 * @param[in] right Second region [BORROWS].
 * @param[in] bytes Bytes to compare.
 * @return          The offset that disagrees, or bytes where every one matches.
 * @note Reports where rather than whether, since the offset names how far a wrong walk got before it
 *       diverged.
 */
static size_t praet_first_difference(const uint8_t *left, const uint8_t *right, size_t bytes)
{
    for (size_t offset = 0u; offset < bytes; offset++)
    {
        if (left[offset] != right[offset])
        {
            return offset;
        }
    }
    return bytes;
}

/**
 * @brief Runs one direction function over one overlap and reports whether it matched the reference.
 *
 * @param[in] label     Text naming the case, printed on a mismatch.
 * @param[in] which     0 for the branch form, 1 for the mask form, 2 for the form told the answer.
 * @param[in] dst_off   Destination offset into the pool.
 * @param[in] src_off   Source offset into the pool.
 * @param[in] bytes     Bytes to move.
 * @return              1 where the move matched the reference, 0 where it did not.
 */
static int praet_check_one(const char *label, unsigned which, size_t dst_off, size_t src_off, size_t bytes)
{
    praet_fill_pattern(g_pattern, POOL_BYTES);

    for (size_t offset = 0u; offset < POOL_BYTES; offset++)
    {
        g_expected[offset] = g_pattern[offset];
        g_pool[offset] = g_pattern[offset];
    }

    praet_reference_move(g_expected + dst_off, g_expected + src_off, bytes);

    if (which == 0u)
    {
        dir_branch(g_pool + dst_off, g_pool + src_off, bytes);
    }
    else if (which == 1u)
    {
        dir_mask(g_pool + dst_off, g_pool + src_off, bytes);
    }
    else
    {
        dir_known(g_pool + dst_off, g_pool + src_off, bytes, (unsigned)(dst_off > src_off));
    }

    const size_t differs = praet_first_difference(g_pool, g_expected, POOL_BYTES);

    if (differs != POOL_BYTES)
    {
        DBENCH_PRINTF("DB CHECK FAIL %-22s dst=%u src=%u n=%u first bad byte at %u\n", label, (unsigned)dst_off,
                      (unsigned)src_off, (unsigned)bytes, (unsigned)differs);
        return 0;
    }
    return 1;
}

/**
 * @brief Checks all three direction forms over every overlap that matters.
 *
 * @return 1 where every case matched the reference, 0 where any did not.
 * @note Runs before the timing. A number from an arm that moves the wrong bytes is worse than no
 *       number, because it reads as a result.
 */
static int praet_check(void)
{
    static const char *const names[3] = {"branch", "mask", "known"};
    static const size_t lengths[3] = {8u, 64u, 2048u};
    int ok = 1;

    for (unsigned form = 0u; form < 3u; form++)
    {
        for (unsigned index = 0u; index < 3u; index++)
        {
            const size_t bytes = lengths[index];

            /* Destination one byte above the source is the worst overlap there is: a forward walk
             * overwrites the next byte it is about to read. */
            ok &= praet_check_one(names[form], form, 1u, 0u, bytes);

            /* A whole word apart, above and below, which is where a word-at-a-time walk differs from
             * a byte one. */
            ok &= praet_check_one(names[form], form, sizeof(embed_word), 0u, bytes);
            ok &= praet_check_one(names[form], form, 0u, sizeof(embed_word), bytes);

            /* Half the length apart, so the two regions genuinely straddle each other. */
            ok &= praet_check_one(names[form], form, bytes / 2u, 0u, bytes);
            ok &= praet_check_one(names[form], form, 0u, bytes / 2u, bytes);

            /* No overlap at all, which must still be correct and is the common case. */
            ok &= praet_check_one(names[form], form, 4096u, 0u, bytes);
        }
    }
    return ok;
}

/* Each arm reads its offsets from the volatiles inside itself, so nothing lifts out of the loop. */

static void arm_known_up(void)
{
    dir_known(g_pool + g_dst_off, g_pool + g_src_off, g_len, 1u);
}

static void arm_branch_up(void)
{
    dir_branch(g_pool + g_dst_off, g_pool + g_src_off, g_len);
}

static void arm_mask_up(void)
{
    dir_mask(g_pool + g_dst_off, g_pool + g_src_off, g_len);
}

static void arm_known_down(void)
{
    dir_known(g_pool + g_src_off, g_pool + g_dst_off, g_len, 0u);
}

static void arm_branch_down(void)
{
    dir_branch(g_pool + g_src_off, g_pool + g_dst_off, g_len);
}

static void arm_mask_down(void)
{
    dir_mask(g_pool + g_src_off, g_pool + g_dst_off, g_len);
}

/* The mixed arms swap the two endpoints every call, so the direction alternates and a branch
 * predictor has nothing to learn from the last one. */

static void arm_branch_mixed(void)
{
    const unsigned flip = g_alternate;

    g_alternate = flip ^ 1u;
    if (flip != 0u)
    {
        dir_branch(g_pool + g_dst_off, g_pool + g_src_off, g_len);
    }
    else
    {
        dir_branch(g_pool + g_src_off, g_pool + g_dst_off, g_len);
    }
}

static void arm_mask_mixed(void)
{
    const unsigned flip = g_alternate;

    g_alternate = flip ^ 1u;
    if (flip != 0u)
    {
        dir_mask(g_pool + g_dst_off, g_pool + g_src_off, g_len);
    }
    else
    {
        dir_mask(g_pool + g_src_off, g_pool + g_dst_off, g_len);
    }
}

void dbench_run(void)
{
    static const size_t lengths[] = {8u, 64u, 2048u};

    DBENCH_BANNER("praet direction");

    g_move_table[0] = memor.cpy;
    g_move_table[1] = memor.move_up;

    /* Correctness before timing. A form that moves the wrong bytes still produces a number, and the
     * number reads as a result, so the run stops here rather than reporting one. */
    if (praet_check() == 0)
    {
        DBENCH_PRINTF("DB ==== CHECK FAILED, no timings taken ====\n");
        DBENCH_DONE();
        return;
    }
    DBENCH_PRINTF("DB check: all three forms match the reference over every overlap\n");

    for (unsigned index = 0u; index < (sizeof lengths / sizeof lengths[0]); index++)
    {
        const size_t bytes = lengths[index];
        const uint32_t iters = (bytes >= 2048u) ? 2000u : 20000u;

        g_len = bytes;
        g_src_off = 0u;
        g_dst_off = bytes;

        /* The floor for each case, so the two deciding arms have something to be read against. */
        DBENCH_OP("dir_known_up", iters, arm_known_up());
        DBENCH_OP("dir_known_down", iters, arm_known_down());

        /* Head to head, which is the row that answers the question. Run both orders: on the C6 the
         * arm that runs second has measured about 1.1 cycles more whatever it is, so a small split
         * that follows the position instead of the code is visible only by swapping. */
        DBENCH_AB("dir_up", iters, bytes, arm_branch_up(), arm_mask_up());
        DBENCH_AB("dir_up_swapped", iters, bytes, arm_mask_up(), arm_branch_up());

        DBENCH_AB("dir_down", iters, bytes, arm_branch_down(), arm_mask_down());
        DBENCH_AB("dir_down_swapped", iters, bytes, arm_mask_down(), arm_branch_down());

        /* The case a branch cannot predict, which is where the mask should earn its place if it
         * earns it anywhere. */
        DBENCH_AB("dir_mixed", iters, bytes, arm_branch_mixed(), arm_mask_mixed());
        DBENCH_AB("dir_mixed_swapped", iters, bytes, arm_mask_mixed(), arm_branch_mixed());
    }

    DBENCH_DONE();
}

DBENCH_MAIN("praet direction")
