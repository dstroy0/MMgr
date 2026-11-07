/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_sequence_prove.c
 * @brief Layout, Take, Spill, Give and Rewind of the Lean theory proved on the C: every sequence of PROVE_STEPS
 *        operations on a small cellblock, each a take of either tier at any of four sizes, a give of any cell
 *        standing plain or zeroing, a mark, or a rewind plain or zeroing. After each step every cell standing keeps
 *        its bytes and counts to its owner and the gap is the top less the end; once every cell is given back and
 *        the tier reset, both boundaries are where they started and every count is 0.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 */
#include "locus_carcerum/locus_carcerum.h"

#include "prove.h"

#ifndef PROVE_STEPS
#define PROVE_STEPS 4u
#endif

#define PROVE_BYTES 512u
#define PROVE_LIVE_MOST 4u
#define PROVE_MARKS_MOST 2u


#define PROVE_OWNERS(owner_) owner_(s_p0, "p0") owner_(s_p1, "p1")
MMGR_OWNERS(PROVE_OWNERS)
PROVE_NO_MISNAMED()

ParsMemoriaeInternae(proved, PROVE_BYTES);
LocusCarcerum(prove, MMGR_MINIMUM_SECURITY(proved));

#define BLOCK (&prove_proved_ctx)

typedef struct
{
    uint8_t *payload;
    size_t bytes;
    unsigned int owner;
    int temporary;
} ProveLive;

static ProveLive s_live[PROVE_LIVE_MOST];
static unsigned int s_count;
static size_t s_marks[PROVE_MARKS_MOST];
static unsigned int s_mark_count;

static const CellOwner *owner_at(unsigned int which)
{
    return (which == 0u) ? s_p0 : s_p1;
}

static void dropped(unsigned int at)
{
    s_live[at] = s_live[s_count - 1u];
    s_count -= 1u;
}

static void checked(void)
{
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        const ProveLive *const live = &s_live[at];
        PROVE_ASSERT(live->payload[0] == (uint8_t)(live->bytes ^ 0x5Au), "a cell keeps its first byte");
        PROVE_ASSERT(live->payload[live->bytes - 1u] == (uint8_t)(live->bytes ^ 0xA5u), "a cell keeps its last byte");
        PROVE_ASSERT(mmgr_owner_of_buf(BLOCK, live->payload) == owner_at(live->owner), "a cell counts to its owner");
        PROVE_ASSERT(mmgr_who_owns_buf(BLOCK, live->payload) != 0, "a cell lies in its cellblock");
    }
    PROVE_ASSERT(mmgr_buf_available(BLOCK) ==
                     mmgr_series_cellularum_temporary_top(BLOCK) - mmgr_series_cellularum_persistent_end(BLOCK),
                 "the gap is the top less the end");
}

static void taken(uint8_t choice)
{
    if (s_count >= PROVE_LIVE_MOST)
    {
        return;
    }
    const size_t bytes = 8u * (size_t)(1u + ((choice >> 2) & 3u));
    const unsigned int owner = (choice >> 4) & 1u;
    const int temporary = (choice & 0x20u) != 0u;
    uint8_t *const payload = temporary ? mmgr_temporary_buf_alloc(BLOCK, bytes, owner_at(owner))
                                       : mmgr_persistent_buf_alloc(BLOCK, bytes, owner_at(owner));
    if (payload == NULL)
    {
        return;
    }
    PROVE_ASSERT(((uintptr_t)payload % MMGR_CARCER_ALIGN) == 0u, "a payload is aligned");
    payload[0] = (uint8_t)(bytes ^ 0x5Au);
    payload[bytes - 1u] = (uint8_t)(bytes ^ 0xA5u);
    s_live[s_count] = (ProveLive){payload, bytes, owner, temporary};
    s_count += 1u;
}

static void given(uint8_t choice)
{
    if (s_count == 0u)
    {
        return;
    }
    const unsigned int at = (unsigned int)((choice >> 2) % s_count);
    ProveLive *const live = &s_live[at];
    if ((choice & 0x40u) != 0u)
    {
        mmgr_persistent_max_security_buf_release(BLOCK, live->payload, owner_at(live->owner));
        PROVE_ASSERT((live->payload[0] == 0u) && (live->payload[live->bytes - 1u] == 0u),
                     "a zeroing give zeroes the payload");
    }
    else
    {
        mmgr_persistent_buf_release(BLOCK, live->payload, owner_at(live->owner));
    }
    dropped(at);
}

static void rewound(uint8_t choice)
{
    if (s_mark_count == 0u)
    {
        return;
    }
    s_mark_count -= 1u;
    const size_t mark = s_marks[s_mark_count];
    for (unsigned int at = 0u; at < s_count;)
    {
        const size_t off = (size_t)(s_live[at].payload - BLOCK->base);
        // a cell whose header lies below the mark goes with the rewind, as Rewind.walk_left_past states
        if (s_live[at].temporary && ((off - PROVE_CELL_HEADER) < mark))
        {
            dropped(at);
            continue;
        }
        at += 1u;
    }
    if ((choice & 0x40u) != 0u)
    {
        mmgr_max_security_buf_return(BLOCK, mark);
    }
    else
    {
        mmgr_temporary_buf_release(BLOCK, mark);
    }
}

int main(void)
{
    for (unsigned int step = 0u; step < PROVE_STEPS; step += 1u)
    {
        const uint8_t choice = nondet_byte();
        switch (choice & 3u)
        {
        case 0:
            taken(choice);
            break;
        case 1:
            given(choice);
            break;
        case 2:
            if (s_mark_count < PROVE_MARKS_MOST)
            {
                s_marks[s_mark_count] = mmgr_temporary_buf_mark(BLOCK);
                s_mark_count += 1u;
            }
            break;
        default:
            rewound(choice);
            break;
        }
        checked();
    }
    while (s_count != 0u)
    {
        mmgr_persistent_buf_release(BLOCK, s_live[s_count - 1u].payload, owner_at(s_live[s_count - 1u].owner));
        s_count -= 1u;
    }
    mmgr_temporary_buf_reset(BLOCK);
    PROVE_ASSERT(mmgr_series_cellularum_persistent_end(BLOCK) == 0u, "every give returns the persistent end to 0");
    PROVE_ASSERT(mmgr_series_cellularum_temporary_top(BLOCK) == PROVE_BYTES, "the reset returns the top to the size");
    PROVE_ASSERT(mmgr_owner_in_use(s_p0) == 0u, "the first owner's count returns to 0");
    PROVE_ASSERT(mmgr_owner_in_use(s_p1) == 0u, "the second owner's count returns to 0");
    PROVE_ASSERT(mmgr_series_cellularum_idle(BLOCK) != 0, "the emptied cellblock reads idle");
    PROVE_DONE();
    return 0;
}
