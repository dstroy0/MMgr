/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_fuzz.c
 * @brief A libFuzzer harness over every LocusCarcerum entry. On one cellblock: takes of both tiers at any size,
 *        a size past the cellblock's among them, gives plain and zeroing, growths plain and zeroing, gives and
 *        growths that name the wrong owner, marks, rewinds plain and zeroing, and resets, against a model of the
 *        cells standing, every payload filled with its cell's mark and read back before it goes; the owner
 *        lookups, the gap, the fragmentation, the rounding and the zeroing beside them. On a second cellblock
 *        with the movable option: movable takes, writes, reads, compares, measures, gives and moves against a
 *        copy of each cell's bytes. Each cellblock read idle once every cell is given back.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "locus_carcerum/locus_carcerum.h"

#include "fuzz_input.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_BYTES 16384u
#define FUZZ_LIVE_MOST 64u
#define FUZZ_MOVING_BYTES 8192u
#define FUZZ_MOVING_CELLS 16u
#define FUZZ_MOVING_MOST 512u

#define FUZZ_OWNERS(owner_) owner_(s_f0, "f0") owner_(s_f1, "f1") owner_(s_f2, "f2") owner_(s_f3, "f3")
MMGR_OWNERS(FUZZ_OWNERS)

ParsMemoriaeInternae(fuzzed, FUZZ_BYTES);
ParsMemoriaeInternae(moving, FUZZ_MOVING_BYTES);
ParsMemoriaeInternae(moving_option, MMGR_MOVABLE_STORAGE_BYTES(FUZZ_MOVING_CELLS, 1u, 0u));
LocusCarcerum(fuzz, MMGR_MINIMUM_SECURITY(fuzzed));
LocusCarcerum(mover, MMGR_MINIMUM_SECURITY(moving));

#define BLOCK (&fuzz_fuzzed_ctx)
#define MOVING (&mover_moving_ctx)

/** @brief One cell the model stands: its payload, its bytes, its mark, its owner and whether it is temporary. */
typedef struct
{
    uint8_t *payload;
    size_t bytes;
    uint8_t mark;
    unsigned int owner;
    int temporary;
} FuzzLive;

/** @brief One movable cell the model stands: its identity, its bytes and a copy of what they hold. */
typedef struct
{
    const CarcerMovableSlot *slot;
    size_t bytes;
    unsigned int owner;
    uint8_t copy[FUZZ_MOVING_MOST];
} FuzzMovable;

static const CellOwner *s_owner[4];
static FuzzLive s_live[FUZZ_LIVE_MOST];
static unsigned int s_count;
static size_t s_marks[8];
static unsigned int s_mark_count;
static FuzzMovable s_movable[FUZZ_MOVING_CELLS];
static unsigned int s_movable_count;
static int s_option_assigned;
static int s_misnamed_expected;
static unsigned int s_misnamed_told;

/** @brief Told of a give or growth naming the wrong owner: expected where the harness named one, a stop where not. */
void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)bytes;
    fuzz_check(s_misnamed_expected && (named != cell_owner), "a give or growth is told misnamed only where it was");
    s_misnamed_told += 1u;
}

static void checked(const FuzzLive *live)
{
    for (size_t at = 0u; at < live->bytes; at += 1u)
    {
        fuzz_check(live->payload[at] == live->mark, "a cell's payload holds its mark until it goes");
    }
}

static int zeroed(const uint8_t *bytes, size_t size)
{
    for (size_t at = 0u; at < size; at += 1u)
    {
        if (bytes[at] != 0u)
        {
            return 0;
        }
    }
    return 1;
}

static void removed(unsigned int at)
{
    s_live[at] = s_live[s_count - 1u];
    s_count -= 1u;
}

static void gone(unsigned int at, int wiping)
{
    FuzzLive *const live = &s_live[at];
    checked(live);
    fuzz_check(mmgr_owner_of_buf(BLOCK, live->payload) == s_owner[live->owner], "a cell counts to the owner that took it");
    if (wiping)
    {
        mmgr_persistent_max_security_buf_release(BLOCK, live->payload, s_owner[live->owner]);
        fuzz_check(zeroed(live->payload, live->bytes), "a zeroing give zeroes the cell");
    }
    else
    {
        mmgr_persistent_buf_release(BLOCK, live->payload, s_owner[live->owner]);
    }
    removed(at);
}

/** @brief A cell's header word, as the module lays it: a word rounded up to the alignment, ahead of the payload. */
#define FUZZ_CELL_HEADER ((sizeof(embed_atomic_usize) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief Every temporary cell whose header the model stands below `mark` dropped from the model: the rewind took
 *        them, the cell a mark lies inside included, as the rewind walks whole cells.
 */
static void rewound(size_t mark)
{
    for (unsigned int at = 0u; at < s_count;)
    {
        const size_t off = (size_t)(s_live[at].payload - BLOCK->base) - FUZZ_CELL_HEADER;
        if (s_live[at].temporary && (off < mark))
        {
            checked(&s_live[at]);
            removed(at);
            continue;
        }
        at += 1u;
    }
}

/** @brief A request size: mostly a fit, sometimes zero, sometimes past the cellblock's. */
static size_t drawn_size(FuzzInput *input, uint8_t arg)
{
    const uint8_t kind = fuzz_byte(input);
    switch (kind & 15u)
    {
    case 0:
        return SIZE_MAX - (size_t)(kind >> 4);
    case 1:
        return FUZZ_BYTES + 1u + (size_t)(kind >> 4);
    case 2:
        return 0u;
    default:
        return (size_t)arg * 4u + 1u;
    }
}

/** @brief The state a refused call must leave: both boundaries, the gap and every owner's count. */
typedef struct
{
    size_t end;
    size_t top;
    size_t available;
    size_t in_use[4];
} BlockState;

static BlockState state_of(void)
{
    BlockState state;
    state.end = mmgr_series_cellularum_persistent_end(BLOCK);
    state.top = mmgr_series_cellularum_temporary_top(BLOCK);
    state.available = mmgr_buf_available(BLOCK);
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        state.in_use[each] = mmgr_owner_in_use(s_owner[each]);
    }
    return state;
}

static void unchanged(const BlockState *before, const char *what)
{
    const BlockState after = state_of();
    fuzz_check(memcmp(before, &after, sizeof after) == 0, what);
}

static void taken(FuzzInput *input, uint8_t op, uint8_t arg, unsigned int owner, uint8_t mark)
{
    if (s_count >= FUZZ_LIVE_MOST)
    {
        return;
    }
    const size_t bytes = drawn_size(input, arg);
    const int temporary = (op & 0x40u) != 0u;
    const int wiping = (op & 0x80u) != 0u;
    const BlockState before = state_of();
    uint8_t *payload;
    if (temporary)
    {
        payload = wiping ? mmgr_temporary_max_security_buf_alloc(BLOCK, bytes, s_owner[owner])
                         : mmgr_temporary_buf_alloc(BLOCK, bytes, s_owner[owner]);
    }
    else
    {
        payload = wiping ? mmgr_persistent_max_security_buf_alloc(BLOCK, bytes, s_owner[owner])
                         : mmgr_persistent_buf_alloc(BLOCK, bytes, s_owner[owner]);
    }
    if (bytes > FUZZ_BYTES)
    {
        fuzz_check(payload == NULL, "a take past the cellblock's size is refused");
    }
    if (payload == NULL)
    {
        unchanged(&before, "a refused take changes nothing");
        return;
    }
    fuzz_check(mmgr_who_owns_buf(BLOCK, payload) != 0, "a cell lies in its cellblock's bytes");
    fuzz_check(((uintptr_t)payload % MMGR_CARCER_ALIGN) == 0u, "a cell's payload is aligned");
    fuzz_check(mmgr_owner_of_buf(BLOCK, payload) == s_owner[owner], "a cell counts to the owner that took it");
    fuzz_check(mmgr_owner_in_use(s_owner[owner]) >= before.in_use[owner] + bytes, "a take counts its bytes to its owner");
    memset(payload, mark, bytes);
    s_live[s_count] = (FuzzLive){payload, bytes, mark, owner, temporary};
    s_count += 1u;
}

static void grown(FuzzInput *input, uint8_t op, uint8_t arg, unsigned int owner)
{
    if (s_count == 0u)
    {
        return;
    }
    FuzzLive *const live = &s_live[arg % s_count];
    const size_t size = drawn_size(input, fuzz_byte(input));
    const int wiping = (op & 0x80u) != 0u;
    const int misnamed = (op & 0x40u) != 0u;
    const unsigned int named = misnamed ? ((live->owner + 1u + owner % 3u) % 4u) : live->owner;
    const BlockState before = state_of();
    const unsigned int told = s_misnamed_told;

    s_misnamed_expected = misnamed;
    uint8_t *const grew = wiping ? mmgr_max_security_buf_grow(BLOCK, live->payload, size, s_owner[named])
                                 : mmgr_buf_grow(BLOCK, live->payload, size, s_owner[named]);
    s_misnamed_expected = 0;
    if (misnamed)
    {
        fuzz_check((grew == NULL) && (s_misnamed_told == told + 1u), "a growth naming the wrong owner is told and refused");
    }
    if (size > FUZZ_BYTES)
    {
        fuzz_check(grew == NULL, "a growth past the cellblock's size is refused");
    }
    if (grew == NULL)
    {
        unchanged(&before, "a refused growth changes nothing");
        checked(live);
        return;
    }
    fuzz_check(live->temporary ? (grew <= live->payload) : (grew == live->payload),
               "a persistent cell grows where it is, a temporary one moves down");
    for (size_t at = 0u; at < live->bytes; at += 1u)
    {
        fuzz_check(grew[at] == live->mark, "a growth keeps the bytes the cell carried");
    }
    if (size > live->bytes)
    {
        // A cell carries its request rounded to a word, and one word for a request of nothing
        const size_t carried = (live->bytes == 0u) ? MMGR_CARCER_ALIGN : mmgr_align_up_buf(live->bytes);
        if (wiping && (size > carried) && !zeroed(grew + carried, size - carried))
        {
            size_t first = carried;
            while (grew[first] == 0u)
            {
                first += 1u;
            }
            fprintf(stderr, "%s growth from %zu bytes (%zu carried) to %zu, moved by %td: byte %zu reads 0x%02x\n",
                    live->temporary ? "temporary" : "persistent", live->bytes, carried, size,
                    (ptrdiff_t)(live->payload - grew), first, grew[first]);
            fuzz_failed("a zeroing growth zeroes the bytes it adds");
        }
        memset(grew + live->bytes, live->mark, size - live->bytes);
        live->bytes = size;
    }
    live->payload = grew;
    fuzz_check(mmgr_owner_of_buf(BLOCK, grew) == s_owner[live->owner], "a grown cell counts to its owner");
}

static void misnamed_give(unsigned int at, unsigned int shift)
{
    FuzzLive *const live = &s_live[at];
    const unsigned int named = (live->owner + 1u + shift % 3u) % 4u;
    const BlockState before = state_of();
    const unsigned int told = s_misnamed_told;
    s_misnamed_expected = 1;
    mmgr_persistent_buf_release(BLOCK, live->payload, s_owner[named]);
    s_misnamed_expected = 0;
    fuzz_check(s_misnamed_told == told + 1u, "a give naming the wrong owner is told");
    unchanged(&before, "a give naming the wrong owner changes nothing");
    checked(live);
}

static void lookups(FuzzInput *input)
{
    const uint8_t local = 0u;
    fuzz_check(mmgr_who_owns_buf(BLOCK, &local) == 0, "bytes outside the cellblock are not its");
    fuzz_check(mmgr_owner_of_buf(BLOCK, NULL) == NULL, "no prisoner counts to no owner");
    fuzz_check(mmgr_buf_available(BLOCK) ==
                   mmgr_series_cellularum_temporary_top(BLOCK) - mmgr_series_cellularum_persistent_end(BLOCK),
               "the gap is the top less the end");
    fuzz_check((mmgr_series_cellularum_persistent_hw(BLOCK) == 0u) ||
                   (mmgr_series_cellularum_persistent_hw(BLOCK) >= mmgr_series_cellularum_persistent_end(BLOCK)),
               "the persistent mark, where the build tracks one, is at or past the end");
    fuzz_check(mmgr_series_cellularum_fragmentation(BLOCK) <= 100u, "fragmentation reads as a percent");
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        fuzz_check(mmgr_owner_hw(s_owner[each]) >= mmgr_owner_in_use(s_owner[each]), "an owner's mark is at or past its count");
        fuzz_check(mmgr_owner_listed(s_owner[each]) != 0, "a registered owner is listed");
    }
    const size_t size = (size_t)fuzz_word(input) % (SIZE_MAX - MMGR_CARCER_ALIGN);
    const size_t rounded = mmgr_align_up_buf(size);
    fuzz_check((rounded >= size) && (rounded - size < MMGR_CARCER_ALIGN) && ((rounded % MMGR_CARCER_ALIGN) == 0u),
               "the rounding takes a size up to the next whole word");
    uint8_t scratch[64];
    memset(scratch, 0xEE, sizeof scratch);
    const size_t wiped = fuzz_below(input, sizeof scratch + 1u);
    mmgr_zero_buf(scratch, wiped);
    fuzz_check(zeroed(scratch, wiped) && ((wiped == sizeof scratch) || (scratch[wiped] == 0xEEu)),
               "zero_buf zeroes the bytes it is told and no others");
}

static void movable_assigned(void)
{
    if (s_option_assigned)
    {
        return;
    }
    memset(mmgr_pars_storage_moving_option, 0, moving_option_bytes);
    fuzz_check(mmgr_movable_init(MOVING,
                                 &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_moving_option,
                                                         .storage_bytes = moving_option_bytes,
                                                         .cells = FUZZ_MOVING_CELLS,
                                                         .width = 1u,
                                                         .rows = 0u},
                                 EMBED_FALSE) != 0,
               "the movable option is assigned over its storage");
    s_option_assigned = 1;
}

/** @brief A cell's header and owner word, as the module lays them: two words each rounded up to the alignment. */
#define FUZZ_CELL_WORDS (2u * FUZZ_CELL_HEADER)

/**
 * @brief The bytes a movable cell carries, read off the longest span a read from its start takes: its request
 *        rounded to the alignment, and up to a header and an owner word more where the take kept a free run whole.
 */
static size_t movable_capacity(const FuzzMovable *cell)
{
    static uint8_t probe[FUZZ_MOVING_MOST + 2u * FUZZ_CELL_WORDS + MMGR_CARCER_ALIGN];
    const size_t least = mmgr_align_up_buf(cell->bytes);
    size_t carried = 0u;

    for (size_t bytes = least; bytes <= least + FUZZ_CELL_WORDS + MMGR_CARCER_ALIGN; bytes += 1u)
    {
        if (mmgr_movable_buf_read(MOVING, &(CarcerMovableCfg){.cell = cell->slot, .dst = probe, .bytes = bytes}))
        {
            carried = bytes;
        }
    }
    fuzz_check((carried >= least) && (carried <= least + FUZZ_CELL_WORDS),
               "a movable cell carries its rounded request and at most a header more");
    return carried;
}

static void movable_step(FuzzInput *input, uint8_t op, unsigned int owner)
{
    switch (op % 7u)
    {
    case 0:
        if (s_movable_count < FUZZ_MOVING_CELLS)
        {
            const size_t bytes = 1u + fuzz_below(input, FUZZ_MOVING_MOST);
            const CarcerMovableSlot *const slot = mmgr_movable_buf_alloc(MOVING, bytes, s_owner[owner]);
            if (slot != NULL)
            {
                FuzzMovable *const cell = &s_movable[s_movable_count];
                cell->slot = slot;
                cell->bytes = bytes;
                cell->owner = owner;
                for (size_t at = 0u; at < bytes; at += 1u)
                {
                    cell->copy[at] = (uint8_t)(0x5Au ^ (at * 7u) ^ s_movable_count);
                }
                fuzz_check(mmgr_movable_buf_write(MOVING, &(CarcerMovableCfg){.cell = slot, .src = cell->copy, .bytes = bytes}),
                           "a movable cell takes its whole span");
                s_movable_count += 1u;
            }
        }
        break;
    case 1:
        if (s_movable_count != 0u)
        {
            const unsigned int which = (unsigned int)fuzz_below(input, s_movable_count);
            FuzzMovable *const cell = &s_movable[which];
            fuzz_check(mmgr_movable_buf_release(MOVING, cell->slot, s_owner[cell->owner]) != 0, "a movable cell goes back");
            s_movable[which] = s_movable[s_movable_count - 1u];
            s_movable_count -= 1u;
        }
        break;
    case 2:
    case 3:
        if (s_movable_count != 0u)
        {
            FuzzMovable *const cell = &s_movable[fuzz_below(input, s_movable_count)];
            const size_t offset = fuzz_below(input, cell->bytes + 16u);
            const size_t bytes = fuzz_below(input, cell->bytes + 16u);
            uint8_t span[FUZZ_MOVING_MOST + 32u];
            const int inside = (offset <= cell->bytes) && (bytes <= cell->bytes - offset);
            const int past = offset + bytes > movable_capacity(cell);
            if ((op % 7u) == 2u)
            {
                memset(span, 0xC3, sizeof span);
                const embed_bool copied = mmgr_movable_buf_read(
                    MOVING, &(CarcerMovableCfg){.cell = cell->slot, .offset = offset, .dst = span, .bytes = bytes});
                if (inside)
                {
                    fuzz_check(copied && (memcmp(span, cell->copy + offset, bytes) == 0), "a read copies the cell's bytes");
                }
                if (past)
                {
                    fuzz_check(!copied, "a read past the cell's end is refused");
                }
            }
            else
            {
                for (size_t at = 0u; at < bytes && at < sizeof span; at += 1u)
                {
                    span[at] = fuzz_byte(input);
                }
                const embed_bool copied = mmgr_movable_buf_write(
                    MOVING, &(CarcerMovableCfg){.cell = cell->slot, .offset = offset, .src = span, .bytes = bytes});
                if (inside)
                {
                    fuzz_check(copied != 0, "a write inside the cell is taken");
                }
                // A write the cell takes may reach into the word its request was rounded to; the bytes of it that
                // lie within the request are the copy's
                if (copied && (offset < cell->bytes))
                {
                    const size_t within = (bytes < cell->bytes - offset) ? bytes : (cell->bytes - offset);
                    memcpy(cell->copy + offset, span, within);
                }
                if (past)
                {
                    fuzz_check(!copied, "a write past the cell's end is refused");
                }
            }
        }
        break;
    case 4:
        if (s_movable_count != 0u)
        {
            FuzzMovable *const one = &s_movable[fuzz_below(input, s_movable_count)];
            FuzzMovable *const two = &s_movable[fuzz_below(input, s_movable_count)];
            const size_t shortest = (one->bytes < two->bytes) ? one->bytes : two->bytes;
            const size_t bytes = fuzz_below(input, shortest + 1u);
            const embed_iword order = mmgr_movable_buf_compare(
                MOVING, &(CarcerMovableCfg){.cell = one->slot, .other_cell = two->slot, .bytes = bytes});
            const int expected = memcmp(one->copy, two->copy, bytes);
            fuzz_check(((order < 0) == (expected < 0)) && ((order > 0) == (expected > 0)),
                       "compare orders two cells as their bytes do");
            const size_t offset = fuzz_below(input, one->bytes);
            const size_t measured = mmgr_movable_buf_measure(MOVING, &(CarcerMovableCfg){.cell = one->slot, .offset = offset});
            const uint8_t *const zero = memchr(one->copy + offset, 0, one->bytes - offset);
            if (zero != NULL)
            {
                fuzz_check(measured == (size_t)(zero - (one->copy + offset)), "measure counts to the next zero");
            }
            else
            {
                fuzz_check(measured >= one->bytes - offset, "measure with no zero counts to the cell's end");
            }
        }
        break;
    case 5:
        for (unsigned int step = 0u; (step < 2u * FUZZ_MOVING_CELLS) && mmgr_movable_buf_move(MOVING); step += 1u)
        {
        }
        break;
    default:
        fuzz_check(mmgr_movable_queue_depth(MOVING) == 0u, "no context is queued while none runs");
        break;
    }
    for (unsigned int each = 0u; each < s_movable_count; each += 1u)
    {
        uint8_t span[FUZZ_MOVING_MOST];
        fuzz_check(mmgr_movable_buf_read(MOVING, &(CarcerMovableCfg){.cell = s_movable[each].slot, .dst = span,
                                                                     .bytes = s_movable[each].bytes}) != 0,
                   "a standing movable cell reads whole");
        fuzz_check(memcmp(span, s_movable[each].copy, s_movable[each].bytes) == 0,
                   "a movable cell keeps its bytes through every move");
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    s_owner[0] = s_f0;
    s_owner[1] = s_f1;
    s_owner[2] = s_f2;
    s_owner[3] = s_f3;
    s_count = 0u;
    s_mark_count = 0u;
    s_movable_count = 0u;
    movable_assigned();
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        const uint8_t arg = fuzz_byte(&input);
        const unsigned int owner = op & 3u;
        const uint8_t mark = (uint8_t)(0x11u * (owner + 1u) + arg + 1u);
        switch ((op >> 2) % 10u)
        {
        case 0:
        case 1:
            taken(&input, op, arg, owner, mark);
            break;
        case 2:
            if (s_count != 0u)
            {
                gone(arg % s_count, (op & 0x40u) != 0u);
            }
            break;
        case 3:
            grown(&input, op, arg, owner);
            break;
        case 4:
            if (s_mark_count < 8u)
            {
                s_marks[s_mark_count] = mmgr_temporary_buf_mark(BLOCK);
                s_mark_count += 1u;
            }
            break;
        case 5:
            if (s_mark_count != 0u)
            {
                s_mark_count -= 1u;
                const size_t top = mmgr_series_cellularum_temporary_top(BLOCK);
                const int wiping = (op & 0x40u) != 0u;
                rewound(s_marks[s_mark_count]);
                if (wiping)
                {
                    mmgr_max_security_buf_return(BLOCK, s_marks[s_mark_count]);
                    if (s_marks[s_mark_count] > top)
                    {
                        fuzz_check(zeroed(BLOCK->base + top, s_marks[s_mark_count] - top), "a zeroing rewind zeroes what it returns");
                    }
                }
                else
                {
                    mmgr_temporary_buf_release(BLOCK, s_marks[s_mark_count]);
                }
            }
            break;
        case 6:
            if (s_count != 0u)
            {
                misnamed_give(arg % s_count, op);
            }
            break;
        case 7:
            lookups(&input);
            break;
        default:
            movable_step(&input, arg, owner);
            break;
        }
        for (unsigned int each = 0u; each < s_count; each += 1u)
        {
            checked(&s_live[each]);
        }
    }
    while (s_count != 0u)
    {
        gone(s_count - 1u, 0);
    }
    while (s_movable_count != 0u)
    {
        FuzzMovable *const cell = &s_movable[s_movable_count - 1u];
        fuzz_check(mmgr_movable_buf_release(MOVING, cell->slot, s_owner[cell->owner]) != 0, "a movable cell goes back");
        s_movable_count -= 1u;
    }
    mmgr_temporary_buf_reset(BLOCK);
    fuzz_check(mmgr_series_cellularum_persistent_end(BLOCK) == 0u, "every give leaves the persistent end at 0");
    fuzz_check(mmgr_series_cellularum_temporary_top(BLOCK) == FUZZ_BYTES, "a reset leaves the temporary top at the size");
    fuzz_check(mmgr_buf_available(BLOCK) == FUZZ_BYTES, "an emptied cellblock has every byte between its tiers");
    fuzz_check(mmgr_series_cellularum_idle(BLOCK) != 0, "an emptied cellblock reads idle");
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        fuzz_check(mmgr_owner_in_use(s_owner[each]) == 0u, "every owner's count returns to 0");
    }
    mmgr_series_cellularum_emptied(BLOCK);
    return 0;
}
