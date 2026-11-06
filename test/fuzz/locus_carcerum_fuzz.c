/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_fuzz.c
 * @brief A libFuzzer harness over LocusCarcerum's entries: each input is a sequence of takes, gives, marks and
 *        rewinds on one small cellblock, checked against a model of the cells standing, every payload filled with
 *        its cell's mark and read back before it goes, and the cellblock read idle once every cell is given back.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "locus_carcerum/locus_carcerum.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_BYTES 16384u
#define FUZZ_LIVE_MOST 64u

#define FUZZ_OWNERS(owner_) owner_(s_f0, "f0") owner_(s_f1, "f1") owner_(s_f2, "f2") owner_(s_f3, "f3")
MMGR_OWNERS(FUZZ_OWNERS)

ParsMemoriaeInternae(fuzzed, FUZZ_BYTES);
LocusCarcerum(fuzz, MMGR_MINIMUM_SECURITY(fuzzed));

/** @brief One cell the model stands: its payload, its bytes, its mark, its owner and whether it is temporary. */
typedef struct
{
    uint8_t *payload;
    size_t bytes;
    uint8_t mark;
    unsigned int owner;
    int temporary;
} FuzzLive;

static const CellOwner *s_owner[4];
static FuzzLive s_live[FUZZ_LIVE_MOST];
static unsigned int s_count;
static size_t s_marks[8];
static unsigned int s_mark_count;

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    abort();
}

static void checked(const FuzzLive *live)
{
    for (size_t at = 0u; at < live->bytes; at += 1u)
    {
        if (live->payload[at] != live->mark)
        {
            abort();
        }
    }
}

static void gone(unsigned int at)
{
    checked(&s_live[at]);
    mmgr_persistent_buf_release(&fuzz_fuzzed_ctx, s_live[at].payload, s_owner[s_live[at].owner]);
    s_live[at] = s_live[s_count - 1u];
    s_count -= 1u;
}

/** @brief Every temporary cell the model stands at or above `mark` dropped from the model: the rewind took them. */
static void rewound(size_t mark)
{
    for (unsigned int at = 0u; at < s_count;)
    {
        const size_t off = (size_t)(s_live[at].payload - fuzz_fuzzed_ctx.base);
        if (s_live[at].temporary && (off >= mark))
        {
            checked(&s_live[at]);
            s_live[at] = s_live[s_count - 1u];
            s_count -= 1u;
            continue;
        }
        at += 1u;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    s_owner[0] = s_f0;
    s_owner[1] = s_f1;
    s_owner[2] = s_f2;
    s_owner[3] = s_f3;
    s_count = 0u;
    s_mark_count = 0u;
    for (size_t at = 0u; at + 1u < size; at += 2u)
    {
        const uint8_t op = data[at];
        const uint8_t arg = data[at + 1u];
        const unsigned int owner = op & 3u;
        const size_t bytes = (size_t)arg * 4u + 1u;
        const uint8_t mark = (uint8_t)(0x11u * (owner + 1u) + arg);
        switch ((op >> 2) % 6u)
        {
        case 0:
        case 1:
            if (s_count < FUZZ_LIVE_MOST)
            {
                uint8_t *const payload = mmgr_persistent_buf_alloc(&fuzz_fuzzed_ctx, bytes, s_owner[owner]);
                if (payload != NULL)
                {
                    memset(payload, mark, bytes);
                    s_live[s_count] = (FuzzLive){payload, bytes, mark, owner, 0};
                    s_count += 1u;
                }
            }
            break;
        case 2:
            if (s_count < FUZZ_LIVE_MOST)
            {
                uint8_t *const payload = mmgr_temporary_buf_alloc(&fuzz_fuzzed_ctx, bytes, s_owner[owner]);
                if (payload != NULL)
                {
                    memset(payload, mark, bytes);
                    s_live[s_count] = (FuzzLive){payload, bytes, mark, owner, 1};
                    s_count += 1u;
                }
            }
            break;
        case 3:
            if (s_count != 0u)
            {
                gone(arg % s_count);
            }
            break;
        case 4:
            if (s_mark_count < 8u)
            {
                s_marks[s_mark_count] = mmgr_temporary_buf_mark(&fuzz_fuzzed_ctx);
                s_mark_count += 1u;
            }
            break;
        default:
            if (s_mark_count != 0u)
            {
                s_mark_count -= 1u;
                rewound(s_marks[s_mark_count]);
                mmgr_temporary_buf_release(&fuzz_fuzzed_ctx, s_marks[s_mark_count]);
            }
            break;
        }
        for (unsigned int each = 0u; each < s_count; each += 1u)
        {
            checked(&s_live[each]);
        }
    }
    while (s_count != 0u)
    {
        gone(s_count - 1u);
    }
    if ((mmgr_series_cellularum_persistent_end(&fuzz_fuzzed_ctx) != 0u) ||
        (mmgr_series_cellularum_temporary_top(&fuzz_fuzzed_ctx) != FUZZ_BYTES) ||
        (mmgr_buf_available(&fuzz_fuzzed_ctx) != FUZZ_BYTES))
    {
        abort();
    }
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        if (mmgr_owner_in_use(s_owner[each]) != 0u)
        {
            abort();
        }
    }
    mmgr_series_cellularum_emptied(&fuzz_fuzzed_ctx);
    return 0;
}
