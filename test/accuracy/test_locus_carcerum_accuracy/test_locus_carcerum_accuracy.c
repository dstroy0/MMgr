// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_locus_carcerum_accuracy.c
 * @brief Checks that no two live cells ever share a byte, that every cell carries the bytes it was
 *        asked for, and that a zeroing release leaves zeros, by writing a pattern into every cell
 *        and reading them all back after each operation.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note An allocator that returns a pointer inside its own storage, correctly aligned, and reports a
 *       plausible free gap can still hand the same byte to two prisoners. Nothing about the returned
 *       pointer shows it. What shows it is the byte a prisoner wrote turning up changed later, which
 *       is what every case here reads for.
 * @note The reference is a table of live cells this file keeps for itself: the address, the bytes
 *       asked for, and the pattern written into them. It is built from what the calls returned and
 *       never from the cellblock's own state, so it disagrees with the module exactly when a cell was
 *       handed out twice.
 * @note The pattern is derived from the cell's index and its offset within the cell. A byte then
 *       identifies both the cell it belongs to and where in that cell it sits, and a run copied from
 *       one cell into another is visible as itself instead of merely as a mismatch.
 * @note Two cellblocks are declared here, one at each security level, because the zeroing release
 *       exists only at the maximum level and the plain release exists only at the minimum one. There
 *       is no call that reaches both.
 * @note Contract checks on a NULL prisoner, a foreign prisoner, and a mark the cellblock never
 *       reported live in test_locus_carcerum. This file asks whether the bytes stay the prisoner's.
 */
#include <stdint.h>
#include <stdio.h>

#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

/**
 * @brief Expands to 4096u, the bytes each cellblock declared here holds.
 *
 * @note A power of two, which MMGR_CARCER_BODY asserts. Large enough that the cases below allocate
 *       dozens of cells before the gap closes, and small enough that closing it is reachable.
 */
#define MMGR_ACCURACY_CARCER_ROW 4096u

/**
 * @brief Expands to 64u, the most live cells any case here tracks at one time.
 *
 * @note Bounds the reference table. The smallest cell a case takes is one word plus a header, and a
 *       4096 byte cellblock holds more than this many, which leaves the table running out first.
 */
#define MMGR_ACCURACY_CARCER_CELLS 64u

/**
 * @brief The prison site every case here allocates from.
 *
 * @note Declared at file scope because LocusCarcerum emits static storage, and a declaration inside a
 *       case would give that case a site of its own.
 * @note plain_cells is minimum security, so its releases leave the bytes as they are. secure_cells is
 *       maximum security, so its releases zero the cell first. Both are needed, since a cellblock has
 *       one release and not both.
 * @note Each cellblock is declared over its own pool, and a cellblock is reached by that pool's name.
 *       Pool names are unique, so no two sites in this file can name a cellblock the same thing.
 */
ParsMemoriaeInternae(plain_cells, MMGR_ACCURACY_CARCER_ROW);
ParsMemoriaeInternae(secure_cells, MMGR_ACCURACY_CARCER_ROW);

LocusCarcerum(accuracy_site, MMGR_MINIMUM_SECURITY(plain_cells), MMGR_MAXIMUM_SECURITY(secure_cells));

/**
 * @brief A second prison site, declared to prove two sites hold no byte in common.
 *
 * @note Carries its own pool, since a pool name stands for one region and cannot be reused. That is
 *       what makes the two sites' cellblocks separate objects rather than a convention about names.
 * @note A cellblock is identified by the address of the pool it was declared over, which the linker
 *       resolved and which nothing after the declaration can change. Two pools are two arrays, so no
 *       address in one lies in the other.
 */
ParsMemoriaeInternae(other_cells, MMGR_ACCURACY_CARCER_ROW);

LocusCarcerum(accuracy_other_site, MMGR_MINIMUM_SECURITY(other_cells));

/**
 * @brief A third prison site, used only by the case that drives the two tiers into each other.
 *
 * @note That case fills its cellblock on purpose and the persistent tier has no reset. A case
 *       sharing a site with it would start from whatever it left, and its own site is what keeps the
 *       order the cases run in from mattering.
 * @note A failing assertion leaves a Unity case immediately. A case that fills a shared cellblock
 *       cannot be relied on to release what it took.
 */
ParsMemoriaeInternae(meeting_cells, MMGR_ACCURACY_CARCER_ROW);

LocusCarcerum(accuracy_meeting_site, MMGR_MINIMUM_SECURITY(meeting_cells));

/**
 * @brief One live cell, as this file recorded it when the allocation returned.
 *
 * @note Holds what the caller was given and nothing the cellblock keeps. The size is the bytes asked
 *       for and not the payload the cell carries, since a reused cell keeps slack the caller never
 *       named and has no claim on.
 */
typedef struct
{
    uint8_t *at; /**< First byte the allocation returned [BORROWS]. */
    size_t size; /**< Bytes asked for. */
    uint8_t tag; /**< Value mixed into this cell's pattern, unique among live cells. */
} AccuracyLiveCell;

/**
 * @brief Returns the byte a given cell carries at a given offset.
 *
 * @param[in] tag    Value identifying the cell.
 * @param[in] offset Position within the cell.
 * @return           The byte that position holds while the cell is intact.
 * @note Mixes the two, which makes a byte name both the cell it belongs to and where in it it sits.
 *       A pattern built from the tag alone survives a run copied from one offset to another within a
 *       cell, and one built from the offset alone survives a run copied between cells.
 * @note Never zero. The zeroing cases read for zeros, and a pattern byte that could be zero would
 *       pass one of those without the release having run.
 */
static uint8_t accuracy_pattern_byte(uint8_t tag, size_t offset)
{
    // Explicit cast narrows the mixed value to the byte a cell holds. The or with 1 keeps it away
    // from zero, which the zeroing cases read for
    return (uint8_t)((((unsigned)tag * 31u) + ((unsigned)offset * 7u)) | 1u);
}

/**
 * @brief Writes a cell's pattern over the bytes it was given.
 *
 * @param[out] cell Cell to fill [BORROWS].
 * @note Writes every byte the caller asked for and not one more. A write past the request would
 *       corrupt a neighbor this file then blames the allocator for.
 */
static void accuracy_fill_cell(const AccuracyLiveCell *cell)
{
    for (size_t offset = 0u; offset < cell->size; offset++)
    {
        cell->at[offset] = accuracy_pattern_byte(cell->tag, offset);
    }
}

/**
 * @brief Returns the offset of the first byte of a cell that no longer holds its pattern.
 *
 * @param[in] cell Cell to read [BORROWS].
 * @return         The offset that disagrees, or the cell's size where every byte is intact.
 * @note Reports where the damage starts instead of only that there is some, since the offset names
 *       which neighbor reached in and how far.
 */
static size_t accuracy_first_damaged_byte(const AccuracyLiveCell *cell)
{
    for (size_t offset = 0u; offset < cell->size; offset++)
    {
        if (cell->at[offset] != accuracy_pattern_byte(cell->tag, offset))
        {
            return offset;
        }
    }
    return cell->size;
}

/**
 * @brief Checks every live cell against its own pattern.
 *
 * @param[in] live  Table of live cells [BORROWS].
 * @param[in] count How many of them are live.
 * @param[in] label Text naming the operation that just ran, printed when a byte disagrees.
 * @note Called after every allocation and every release. A cell handed out twice is caught the first
 *       time the second holder writes, and running this after each step is what pins the failure to
 *       the operation that caused it.
 */
static void accuracy_expect_all_intact(const AccuracyLiveCell *live, unsigned count, const char *label)
{
    for (unsigned index = 0u; index < count; index++)
    {
        const size_t damaged = accuracy_first_damaged_byte(&live[index]);

        if (damaged != live[index].size)
        {
            char message[160];

            (void)snprintf(message, sizeof message, "%s: cell %u lost byte %u of %u, so two cells share it", label,
                           index, (unsigned)damaged, (unsigned)live[index].size);
            TEST_FAIL_MESSAGE(message);
        }
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Both cellblocks are file scope and every case leaves them wound back, so there is nothing to
 *       prepare here that a case does not do for itself.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Required alongside setUp, since the generated runner calls both around every case.
 * @note Each case releases what it took, which is what keeps the next one starting from a cellblock
 *       with room in it.
 */
void tearDown(void)
{
}

/**
 * @brief Checks the pattern helpers this suite rests on against damage introduced on purpose.
 *
 * @note Exists to catch a defect in the reference as itself. A fill and a check that agreed with each
 *       other while reading the wrong bytes would report every allocator as correct, and every case
 *       below rests on this pair.
 * @note A byte is changed by hand and the check is expected to name that exact offset. A checker that
 *       returned the size unconditionally passes every case in this file and finds nothing.
 * @note The pattern is checked to separate two cells at the same offset and two offsets in the same
 *       cell, which are the two ways a cell can be handed out twice.
 */
void test_the_pattern_helpers_this_suite_relies_on_are_themselves_right(void)
{
    uint8_t storage[32] = {0u};
    AccuracyLiveCell cell = {storage, sizeof storage, 5u};

    accuracy_fill_cell(&cell);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof storage, accuracy_first_damaged_byte(&cell),
                                     "a freshly filled cell reads as damaged");

    storage[7] = (uint8_t)(storage[7] ^ 0xFFu);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(7u, accuracy_first_damaged_byte(&cell),
                                     "the checker did not find the byte that was changed");

    accuracy_fill_cell(&cell);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(accuracy_pattern_byte(5u, 0u), accuracy_pattern_byte(6u, 0u),
                                  "two cells hold the same byte at the same offset");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(accuracy_pattern_byte(5u, 0u), accuracy_pattern_byte(5u, 1u),
                                  "one cell holds the same byte at two offsets");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0u, accuracy_pattern_byte(0u, 0u), "a pattern byte can be zero");
}

/**
 * @brief Checks that persistent cells never share a byte, across many sizes.
 *
 * @note This is the case the file exists for. Every allocation is filled and then every live cell is
 *       read back, which catches a cell handed out on top of another at the allocation that did it.
 * @note The sizes cycle through values that are not multiples of the alignment. A run of aligned
 *       sizes never exercises the rounding, and the bytes between a request and the payload it was
 *       rounded up to are exactly where a neighbor gets reached.
 * @note Every pointer is checked to lie in the cellblock and to be aligned, since a cell that fails
 *       either is not one the caller can use whatever its bytes hold.
 */
void test_persistent_cells_never_share_a_byte(void)
{
    AccuracyLiveCell live[MMGR_ACCURACY_CARCER_CELLS];
    unsigned count = 0u;

    for (unsigned index = 0u; index < MMGR_ACCURACY_CARCER_CELLS; index++)
    {
        const size_t size = (size_t)((index % 7u) + 1u) * 5u;
        uint8_t *const at = (uint8_t *)accuracy_site.plain_cells.persistent_buf_alloc(size);
        char label[96];

        if (at == NULL)
        {
            break;
        }

        (void)snprintf(label, sizeof label, "after allocating cell %u of %u bytes", index, (unsigned)size);
        TEST_ASSERT_TRUE_MESSAGE(accuracy_site.plain_cells.who_owns_buf(at),
                                 "a cell was handed out from outside the block");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ((uintptr_t)at) % MMGR_CARCER_ALIGN, "a cell was not word aligned");

        // Explicit cast narrows the loop counter to the byte the pattern is tagged with. The table
        // holds fewer than 256 cells, so no two live cells share a tag
        live[count].at = at;
        live[count].size = size;
        live[count].tag = (uint8_t)(index + 1u);
        accuracy_fill_cell(&live[count]);
        count++;

        accuracy_expect_all_intact(live, count, label);
    }

    TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(8u, count, "the cellblock met too few requests to prove anything");

    for (unsigned index = 0u; index < count; index++)
    {
        accuracy_site.plain_cells.persistent_buf_release(live[index].at);
    }
}

/**
 * @brief Checks that a released cell is handed out again without disturbing its neighbors.
 *
 * @note Reuse is where an allocator writes a header over bytes it already gave away. The cells on
 *       either side of the released one are held and read after every step, which lets the neighbor
 *       above catch a split that put its second header one word out.
 * @note The cells are released from the middle outward, which leaves the tier holding a run of empty
 *       cells with live ones on both sides. That run is what the merge walks, and a merge that
 *       swallowed a live cell shows up as that cell losing its pattern.
 */
void test_a_reused_cell_does_not_disturb_its_neighbors(void)
{
    AccuracyLiveCell live[16];
    unsigned count = 0u;

    for (unsigned index = 0u; index < 16u; index++)
    {
        uint8_t *const at = (uint8_t *)accuracy_site.plain_cells.persistent_buf_alloc(24u);

        TEST_ASSERT_NOT_NULL_MESSAGE(at, "the cellblock could not meet sixteen small requests");
        live[count].at = at;
        live[count].size = 24u;
        // Explicit cast narrows the loop counter to the byte the pattern is tagged with
        live[count].tag = (uint8_t)(index + 1u);
        accuracy_fill_cell(&live[count]);
        count++;
    }
    accuracy_expect_all_intact(live, count, "after sixteen allocations");

    // Release the middle four, leaving live cells above and below the run they leave behind
    for (unsigned index = 6u; index < 10u; index++)
    {
        accuracy_site.plain_cells.persistent_buf_release(live[index].at);
        live[index].size = 0u;
    }
    accuracy_expect_all_intact(live, count, "after releasing the middle four");

    for (unsigned round = 0u; round < 4u; round++)
    {
        uint8_t *const at = (uint8_t *)accuracy_site.plain_cells.persistent_buf_alloc(24u);
        char label[96];

        TEST_ASSERT_NOT_NULL_MESSAGE(at, "the released cells were not handed out again");
        live[6u + round].at = at;
        live[6u + round].size = 24u;
        // Explicit cast narrows the round counter to the byte the pattern is tagged with. The tags
        // start above the sixteen already used, which keeps a stale neighbor from matching a fresh
        // cell
        live[6u + round].tag = (uint8_t)(100u + round);
        accuracy_fill_cell(&live[6u + round]);

        (void)snprintf(label, sizeof label, "after reusing cell %u", round);
        accuracy_expect_all_intact(live, count, label);
    }

    for (unsigned index = 0u; index < count; index++)
    {
        accuracy_site.plain_cells.persistent_buf_release(live[index].at);
    }
}

/**
 * @brief Checks that the two tiers never hand out the same byte.
 *
 * @note The tiers grow toward each other out of one gap. They are the two callers that can reach the
 *       same byte from opposite directions, and the failure is not a wrong pointer but two correct
 *       ones that meet.
 * @note Allocations alternate between the tiers until one refuses, which is what drives them into
 *       each other. Stopping earlier leaves the gap open and tests nothing about where they meet.
 * @note The refusal itself is checked: once the gap closes, both tiers refuse instead of overrunning.
 * @note Runs against its own site, since it fills a cellblock and the persistent tier has no reset.
 * @note The cells are sized so the gap closes well inside the reference table. A smaller cell runs
 *       the table out first, and the case then reports that the tiers never met.
 */
void test_the_two_tiers_never_hand_out_the_same_byte(void)
{
    AccuracyLiveCell live[MMGR_ACCURACY_CARCER_CELLS];
    unsigned count = 0u;
    embed_bool both_refused = EMBED_FALSE;

    while (count < (MMGR_ACCURACY_CARCER_CELLS - 1u))
    {
        uint8_t *const from_bottom = (uint8_t *)accuracy_meeting_site.meeting_cells.persistent_buf_alloc(96u);
        uint8_t *const from_top = (uint8_t *)accuracy_meeting_site.meeting_cells.temporary_buf_alloc(96u);
        char label[96];

        if ((from_bottom == NULL) && (from_top == NULL))
        {
            both_refused = EMBED_TRUE;
            break;
        }

        if (from_bottom != NULL)
        {
            live[count].at = from_bottom;
            live[count].size = 96u;
            // Explicit cast narrows the running count to the byte the pattern is tagged with
            live[count].tag = (uint8_t)(count + 1u);
            accuracy_fill_cell(&live[count]);
            count++;
        }
        if (from_top != NULL)
        {
            live[count].at = from_top;
            live[count].size = 96u;
            // Explicit cast narrows the running count to the byte the pattern is tagged with
            live[count].tag = (uint8_t)(count + 1u);
            accuracy_fill_cell(&live[count]);
            count++;
        }

        (void)snprintf(label, sizeof label, "with %u cells live across both tiers", count);
        accuracy_expect_all_intact(live, count, label);
    }

    TEST_ASSERT_TRUE_MESSAGE(both_refused, "the tiers never met, so nothing about the gap closing was tested");
    TEST_ASSERT_TRUE_MESSAGE(accuracy_meeting_site.meeting_cells.buf_available() < 96u,
                             "both tiers refused while the gap still held a cell");
}

/**
 * @brief Checks that a maximum security release leaves zeros where the prisoner's bytes were.
 *
 * @note The zeroing is the whole of what the security level buys, and it is the one property that
 *       cannot be seen from the released pointer. What shows it is the next prisoner of those bytes
 *       finding them clear.
 * @note The cell is filled with a pattern that holds no zero byte, so every zero read afterwards came
 *       from the release and not from the fill.
 * @note The same size is asked for again, so the first fit walk lands on the cell just released and
 *       the bytes read are the ones that were wiped.
 */
void test_a_maximum_security_release_leaves_zeros(void)
{
    const size_t size = 48u;
    AccuracyLiveCell cell = {(uint8_t *)accuracy_site.secure_cells.persistent_buf_alloc(size), size, 9u};

    TEST_ASSERT_NOT_NULL_MESSAGE(cell.at, "the secure cellblock refused a small request");
    accuracy_fill_cell(&cell);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(size, accuracy_first_damaged_byte(&cell), "the fill did not take");

    uint8_t *const was_at = cell.at;

    accuracy_site.secure_cells.persistent_buf_release(cell.at);

    uint8_t *const again = (uint8_t *)accuracy_site.secure_cells.persistent_buf_alloc(size);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(was_at, again, "the released cell was not the one handed out again");
    for (size_t offset = 0u; offset < size; offset++)
    {
        char message[96];

        (void)snprintf(message, sizeof message, "byte %u survived a maximum security release", (unsigned)offset);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, again[offset], message);
    }
    accuracy_site.secure_cells.persistent_buf_release(again);
}

/**
 * @brief Checks that a maximum security rewind zeros every byte taken since the mark.
 *
 * @note A rewind releases a run of cells at once, and the zeroing covers the whole run and not one
 *       cell's header extent. A rewind that zeroed only the cell at the top leaves the rest of the
 *       run readable, which is the defect this looks for.
 * @note Three cells are taken above the mark and all three are filled, so the run has something in
 *       every part of it. The bytes are read back through a fresh allocation covering the same span.
 */
void test_a_maximum_security_rewind_zeros_everything_taken_since_the_mark(void)
{
    const size_t size = 32u;
    const size_t mark = accuracy_site.secure_cells.temporary_buf_mark();
    AccuracyLiveCell live[3];

    for (unsigned index = 0u; index < 3u; index++)
    {
        live[index].at = (uint8_t *)accuracy_site.secure_cells.temporary_buf_alloc(size);
        live[index].size = size;
        // Explicit cast narrows the loop counter to the byte the pattern is tagged with
        live[index].tag = (uint8_t)(index + 20u);
        TEST_ASSERT_NOT_NULL_MESSAGE(live[index].at, "the secure cellblock refused a temporary request");
        accuracy_fill_cell(&live[index]);
    }
    accuracy_expect_all_intact(live, 3u, "after three temporary allocations");

    uint8_t *const lowest = live[2].at;

    accuracy_site.secure_cells.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mark, accuracy_site.secure_cells.temporary_buf_mark(),
                                     "the rewind did not restore the mark it was given");

    for (unsigned index = 0u; index < 3u; index++)
    {
        for (size_t offset = 0u; offset < size; offset++)
        {
            char message[128];

            (void)snprintf(message, sizeof message, "byte %u of the cell at %u survived the rewind", (unsigned)offset,
                           index);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, live[index].at[offset], message);
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(accuracy_site.secure_cells.who_owns_buf(lowest),
                             "the lowest temporary cell was not in the cellblock");
}

/**
 * @brief Checks that a rewind to a mark restores the bytes the mark reported.
 *
 * @note The mark is a byte count the caller holds across a run of allocations, and the accuracy claim
 *       is that rewinding to it gives back exactly what was taken since. A rewind giving back less
 *       leaks the difference for the life of the cellblock.
 * @note The free gap is read before the mark and again after the rewind. Those two are the same
 *       number when every byte came back, and no part of that comes from the cellblock's own idea of
 *       what it holds.
 */
void test_a_rewind_to_a_mark_gives_back_every_byte_taken_since_it(void)
{
    const size_t before = accuracy_site.plain_cells.buf_available();
    const size_t mark = accuracy_site.plain_cells.temporary_buf_mark();

    for (unsigned index = 0u; index < 8u; index++)
    {
        TEST_ASSERT_NOT_NULL_MESSAGE(accuracy_site.plain_cells.temporary_buf_alloc(24u),
                                     "the cellblock refused a temporary request");
    }
    TEST_ASSERT_TRUE_MESSAGE(accuracy_site.plain_cells.buf_available() < before, "eight allocations took no bytes");

    accuracy_site.plain_cells.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, accuracy_site.plain_cells.buf_available(),
                                     "the rewind did not give back every byte taken since the mark");
}

/**
 * @brief Checks that two prison sites hold no byte in common and know their own by address.
 *
 * @note Every cellblock is identified by the address of its own storage. Two sites are two arrays the
 *       linker placed separately, and the claim under test is that neither one ever reports an
 *       address belonging to the other.
 * @note Cells are taken from both sites at once and every one of them is offered to both. A cellblock
 *       that answered on a size and an offset instead of its own base would claim its neighbor's
 *       cells as soon as the two arrays landed near each other.
 * @note Both sites are filled with patterns and read back together. A site that reached into the
 *       other's bytes is caught the same way a cell handed out twice is.
 * @note Each site holds its own pool. Two pools are two arrays the linker placed separately, and no
 *       address in one lies in the other.
 */
void test_two_prison_sites_hold_no_byte_in_common(void)
{
    AccuracyLiveCell live[24];
    unsigned count = 0u;

    for (unsigned index = 0u; index < 12u; index++)
    {
        uint8_t *const from_first = (uint8_t *)accuracy_site.plain_cells.persistent_buf_alloc(32u);
        uint8_t *const from_second = (uint8_t *)accuracy_other_site.other_cells.persistent_buf_alloc(32u);
        char label[96];

        TEST_ASSERT_NOT_NULL_MESSAGE(from_first, "the first site refused a small request");
        TEST_ASSERT_NOT_NULL_MESSAGE(from_second, "the second site refused a small request");

        TEST_ASSERT_TRUE_MESSAGE(accuracy_site.plain_cells.who_owns_buf(from_first),
                                 "the first site did not claim its own cell");
        TEST_ASSERT_FALSE_MESSAGE(accuracy_other_site.other_cells.who_owns_buf(from_first),
                                  "the second site claimed a cell belonging to the first");
        TEST_ASSERT_TRUE_MESSAGE(accuracy_other_site.other_cells.who_owns_buf(from_second),
                                 "the second site did not claim its own cell");
        TEST_ASSERT_FALSE_MESSAGE(accuracy_site.plain_cells.who_owns_buf(from_second),
                                  "the first site claimed a cell belonging to the second");

        live[count].at = from_first;
        live[count].size = 32u;
        // Explicit cast narrows the running count to the byte the pattern is tagged with
        live[count].tag = (uint8_t)(count + 1u);
        accuracy_fill_cell(&live[count]);
        count++;

        live[count].at = from_second;
        live[count].size = 32u;
        // Explicit cast narrows the running count to the byte the pattern is tagged with
        live[count].tag = (uint8_t)(count + 1u);
        accuracy_fill_cell(&live[count]);
        count++;

        (void)snprintf(label, sizeof label, "with %u cells live across two sites", count);
        accuracy_expect_all_intact(live, count, label);
    }

    for (unsigned index = 0u; index < count; index += 2u)
    {
        accuracy_site.plain_cells.persistent_buf_release(live[index].at);
        accuracy_other_site.other_cells.persistent_buf_release(live[index + 1u].at);
    }
}

/**
 * @brief Checks the rounding against a reference built from the alignment itself.
 *
 * @note The rounding is exact arithmetic with a documented result, so the reference is the smallest
 *       multiple of the alignment at or above the input, worked out by counting up.
 * @note Every size from 0 to four words is covered, which reaches every remainder more than once.
 *       A size already on a boundary returns itself, and that is the case a rounding written with
 *       the wrong constant still gets right.
 */
void test_the_rounding_lands_on_the_next_whole_word(void)
{
    for (size_t size = 0u; size <= (4u * MMGR_CARCER_ALIGN); size++)
    {
        size_t expected = 0u;
        char message[96];

        while (expected < size)
        {
            expected += MMGR_CARCER_ALIGN;
        }

        (void)snprintf(message, sizeof message, "a size of %u did not round to the next whole word", (unsigned)size);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(expected, mmgr_align_up_buf(size), message);
    }
}
