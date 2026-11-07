/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_movable_prove.c
 * @brief Take.lean's mover count, Bounds.lean's span test and Entry.lean proved on the C: every sequence of
 *        PROVE_STEPS operations on a cellblock with the movable option, each a movable take at any of four sizes, a
 *        persistent take and give that leave a run below, a read or a write at any offset and length, a move, or a
 *        release. A span the module takes lies inside its cell and one it refuses runs past it; every cell keeps
 *        its bytes through every move; no two cells share an entry; and once every cell goes back the owner's count
 *        is 0 and the cellblock whole.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 */
#include "locus_carcerum/locus_carcerum.h"

#include "prove.h"

#ifndef PROVE_STEPS
#define PROVE_STEPS 3u
#endif

#define PROVE_BYTES 512u
#define PROVE_CELLS 4u
#define PROVE_SPAN_MOST 40u

#define PROVE_OWNERS(owner_) owner_(s_mover, "mover")
MMGR_OWNERS(PROVE_OWNERS)
PROVE_NO_MISNAMED()

ParsMemoriaeInternae(moving, PROVE_BYTES);
ParsMemoriaeInternae(moving_option, MMGR_MOVABLE_STORAGE_BYTES(PROVE_CELLS, 1u, 0u));
LocusCarcerum(prove, MMGR_MINIMUM_SECURITY(moving));

#define BLOCK (&prove_moving_ctx)

typedef struct
{
    const CarcerMovableEntry *entry;
    size_t bytes;
    uint8_t first;
} ProveMovable;

static ProveMovable s_cell[PROVE_CELLS];
static unsigned int s_count;

static void checked(void)
{
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        uint8_t first = 0u;
        PROVE_ASSERT(
            mmgr_movable_buf_read(BLOCK, &(CarcerMovableCfg){.cell = s_cell[at].entry, .dst = &first, .bytes = 1u}),
            "a standing movable cell reads");
        PROVE_ASSERT(first == s_cell[at].first, "a movable cell keeps its bytes through every move");
        for (unsigned int other = at + 1u; other < s_count; other += 1u)
        {
            PROVE_ASSERT(s_cell[at].entry != s_cell[other].entry, "no two cells name one entry");
        }
    }
}

static void spanned(uint8_t choice)
{
    if (s_count == 0u)
    {
        return;
    }
    ProveMovable *const cell = &s_cell[choice % s_count];
    const size_t offset = nondet_size();
    const size_t bytes = nondet_size();
    PROVE_ASSUME(bytes <= PROVE_SPAN_MOST);
    PROVE_ASSUME(offset <= PROVE_SPAN_MOST);
    uint8_t span[PROVE_SPAN_MOST];
    const size_t carried = mmgr_align_up_buf(cell->bytes);
    const embed_bool fits = mmgr_movable_buf_read(
        BLOCK, &(CarcerMovableCfg){.cell = cell->entry, .offset = offset, .dst = span, .bytes = bytes});
    // the cell carries its rounded request, and a header and an owner word more where a move kept a run whole
    PROVE_ASSERT(!fits || (offset + bytes <= carried + 2u * PROVE_CELL_HEADER),
                 "a span the module takes lies inside the cell");
    PROVE_ASSERT(fits || (offset + bytes > cell->bytes), "a span inside the request is taken");
}

int main(void)
{
    PROVE_ASSERT(mmgr_movable_init(BLOCK,
                                   &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_moving_option,
                                                           .storage_bytes = sizeof(mmgr_pars_storage_moving_option),
                                                           .cells = PROVE_CELLS,
                                                           .width = 1u,
                                                           .rows = 0u},
                                   EMBED_FALSE) != 0,
                 "the option is assigned");
    for (unsigned int step = 0u; step < PROVE_STEPS; step += 1u)
    {
        const uint8_t choice = nondet_byte();
        switch (choice & 3u)
        {
        case 0:
            if (s_count < PROVE_CELLS)
            {
                const size_t bytes = 8u * (size_t)(1u + ((choice >> 2) & 3u));
                const CarcerMovableEntry *const entry = mmgr_movable_buf_alloc(BLOCK, bytes, s_mover);
                if (entry != NULL)
                {
                    const uint8_t first = (uint8_t)(0x30u + s_count);
                    PROVE_ASSERT(
                        mmgr_movable_buf_write(BLOCK, &(CarcerMovableCfg){.cell = entry, .src = &first, .bytes = 1u}),
                        "a movable cell takes a write at its start");
                    s_cell[s_count] = (ProveMovable){entry, bytes, first};
                    s_count += 1u;
                }
            }
            break;
        case 1:
            if (s_count != 0u)
            {
                const unsigned int at = (unsigned int)((choice >> 2) % s_count);
                PROVE_ASSERT(mmgr_movable_buf_release(BLOCK, s_cell[at].entry, s_mover) != 0,
                             "a movable cell goes back");
                s_cell[at] = s_cell[s_count - 1u];
                s_count -= 1u;
            }
            break;
        case 2: {
            // a run left below every movable cell, of a size the move may keep whole or split
            void *const low = mmgr_persistent_buf_alloc(BLOCK, 8u * (size_t)(1u + ((choice >> 2) & 7u)), s_mover);
            if (low != NULL)
            {
                mmgr_persistent_buf_release(BLOCK, low, s_mover);
            }
            (void)mmgr_movable_buf_move(BLOCK);
            break;
        }
        default:
            spanned(choice >> 2);
            break;
        }
        checked();
    }
    while (s_count != 0u)
    {
        PROVE_ASSERT(mmgr_movable_buf_release(BLOCK, s_cell[s_count - 1u].entry, s_mover) != 0,
                     "a movable cell goes back");
        s_count -= 1u;
    }
    PROVE_ASSERT(mmgr_owner_in_use(s_mover) == 0u, "the owner's count returns to 0 through every move");
    PROVE_ASSERT(mmgr_buf_available(BLOCK) == PROVE_BYTES, "the emptied cellblock is whole");
    PROVE_DONE();
    return 0;
}
