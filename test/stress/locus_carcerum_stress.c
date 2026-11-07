/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_stress.c
 * @brief Threads hammering one cellblock through LocusCarcerum's entries, each on an owner of its own, with the
 *        cellblock's storage and its state each laid between pages that fault on any touch: a write that leaves its
 *        cell or its word faults at once instead of landing on a neighbor. Every payload is filled with its holder's
 *        mark and read back before its give, and the cellblock is read idle once every thread has ended.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#if !defined(_WIN32)
#define _GNU_SOURCE
#endif
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "host_threads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

/** @brief The most threads, the most cells one thread holds at once, and the sizes a take asks. */
#define STRESS_THREADS_MOST MMGR_HOST_THREADS_MOST
#define STRESS_HELD_MOST 32u
#define STRESS_SIZE_LEAST 1u
#define STRESS_SIZE_MOST 512u

#define STRESS_OWNERS(owner_)                                                                                          \
    owner_(s_o0, "o0") owner_(s_o1, "o1") owner_(s_o2, "o2") owner_(s_o3, "o3") owner_(s_o4, "o4") owner_(s_o5, "o5")  \
        owner_(s_o6, "o6") owner_(s_o7, "o7") owner_(s_o8, "o8") owner_(s_o9, "o9") owner_(s_o10, "o10")            \
            owner_(s_o11, "o11") owner_(s_o12, "o12") owner_(s_o13, "o13") owner_(s_o14, "o14") owner_(s_o15, "o15")
MMGR_OWNERS(STRESS_OWNERS)

/** @brief One thread's live cells: the payload, its bytes, and whether it lies in the temporary tier. */
typedef struct
{
    uint8_t *payload;
    size_t bytes;
    int temporary;
} StressHeld;

/** @brief One step of the trace: the thread, what it did, the cell's offset and bytes, and the boundaries after. */
typedef struct
{
    uint32_t seq;
    uint8_t thread;
    uint8_t op;
    uint16_t size;
    uint32_t off;
    uint32_t end;
    uint32_t top;
    uint32_t over_end;
    uint32_t over_top;
} StressTrace;

#define STRESS_TRACE_MOST (1u << 20)
#define STRESS_TRACE_SHOWN 40000u
#define STRESS_TRACE_NEAR 2048u
#define STRESS_OP_PERSISTENT 1u
#define STRESS_OP_TEMPORARY 2u
#define STRESS_OP_RELEASE 3u
#define STRESS_OP_REFUSED_PERSISTENT 4u
#define STRESS_OP_REFUSED_TEMPORARY 5u
#define STRESS_OP_BEGUN_PERSISTENT 6u
#define STRESS_OP_BEGUN_TEMPORARY 7u
#define STRESS_TRACE_WHOLE 1500u
#define STRESS_OFF_NONE 0xffffffffu

static const CellOwner *s_own[STRESS_THREADS_MOST];
static SeriesCellularum *s_block;
static unsigned int s_rounds;
static uint64_t s_seed;
static embed_atomic_u32 s_next = EMBED_ATOMIC_INIT(0u);
static StressTrace s_trace[STRESS_TRACE_MOST];
static embed_atomic_u32 s_trace_next = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_broken = EMBED_ATOMIC_INIT(0u);
/** @brief One entry a word of the storage: the trace step of the take that holds it, plus one, or 0 for a free word. */
#define STRESS_SHADOW_MOST (1u << 19)
static embed_atomic_u32 s_shadow[STRESS_SHADOW_MOST];
static embed_atomic_usize s_refused = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_taken = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_damaged = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_misnamed = EMBED_ATOMIC_INIT(0u);

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    (void)embed_atomic_usize_fetch_add(&s_misnamed, 1u);
}

/** @brief The system's page in bytes. */
static size_t page_bytes(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (size_t)info.dwPageSize;
#else
    const long page = sysconf(_SC_PAGESIZE);
    return (page > 0) ? (size_t)page : 4096u;
#endif
}

/** @brief `bytes` of writable storage with a page that faults on any touch before it and another after it. */
static uint8_t *guarded(size_t bytes)
{
    const size_t page = page_bytes();
    const size_t inner = ((bytes + page - 1u) / page) * page;
    const size_t whole = inner + (2u * page);
#if defined(_WIN32)
    uint8_t *const base = VirtualAlloc(NULL, whole, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD was = 0u;
    if (base == NULL)
    {
        return NULL;
    }
    (void)VirtualProtect(base, page, PAGE_NOACCESS, &was);
    (void)VirtualProtect(base + page + inner, page, PAGE_NOACCESS, &was);
#else
    uint8_t *const base = mmap(NULL, whole, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
    {
        return NULL;
    }
    (void)mprotect(base, page, PROT_NONE);
    (void)mprotect(base + page + inner, page, PROT_NONE);
#endif
    // the storage ends against the page that follows, and a write past its last byte faults
    return base + page + (inner - bytes);
}

/** @brief The next reading of a thread's own generator. */
static uint64_t next(uint64_t *state)
{
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

/** @brief The payload at `held` read back against its holder's mark: 1 where every byte stands. */
static int intact(const StressHeld *held, uint8_t mark)
{
    int whole = 1;
    for (size_t at = 0u; at < held->bytes; at += 1u)
    {
        whole = whole && (held->payload[at] == mark);
    }
    return whole;
}

/** @brief The offset of a payload's header from the storage's start. */
static uint32_t off_of(const uint8_t *payload)
{
    return (uint32_t)(payload - s_block->base) - 8u;
}

/** @brief The header word a payload stands behind, read as the library reads it. */
static uint64_t header_of(const uint8_t *payload)
{
    return *(const volatile uint64_t *)(const void *)(payload - 8u);
}

/** @brief One step appended to the trace with the boundaries as they stand after it: its sequence number. */
static uint32_t traced(unsigned int thread, unsigned int op, uint32_t off, size_t size)
{
    const uint32_t seq = embed_atomic_u32_fetch_add(&s_trace_next, 1u);
    StressTrace *const step = &s_trace[seq & (STRESS_TRACE_MOST - 1u)];

    step->thread = (uint8_t)thread;
    step->op = (uint8_t)op;
    step->size = (uint16_t)size;
    step->off = off;
    step->end = (uint32_t)mmgr_series_cellularum_persistent_end(s_block);
    step->top = (uint32_t)mmgr_series_cellularum_temporary_top(s_block);
    step->over_end = (uint32_t)embed_atomic_usize_load(&s_block->persistent_overrun);
    step->over_top = (uint32_t)embed_atomic_usize_load(&s_block->temporary_overrun);
    step->seq = seq;
    return seq;
}

/** @brief Whether a header word is a busy cell's of at least `bytes` and within the storage. */
static int header_fits(uint64_t word, size_t bytes)
{
    const size_t rounded = (bytes + 7u) & ~(size_t)7u;
    const size_t size = (size_t)(word >> 2u);

    return ((word & 3u) == 1u) && (size >= rounded) && (size <= s_block->size);
}

/** @brief A broken cell found: the record, the cell and the trace's steps near it printed once, then the program ends. */
static void dumped(unsigned int thread, const StressHeld *held, uint64_t word)
{
    if (embed_atomic_u32_fetch_add(&s_broken, 1u) != 0u)
    {
        for (;;)
        {
        }
    }
    const uint32_t off = off_of(held->payload);
    const uint32_t now = embed_atomic_u32_load(&s_trace_next);
    const uint32_t from = (now > STRESS_TRACE_SHOWN) ? (now - STRESS_TRACE_SHOWN) : 0u;
    static const char *const names[8] = {"?", "P", "T", "R", "p!", "t!", "p?", "t?"};

    fprintf(stderr, "BROKEN thread %u cell off=%#x bytes=%zu tier=%s header=%#llx end=%#zx top=%#zx over_end=%#zx over_top=%#zx\n",
            thread, off, held->bytes, held->temporary ? "temporary" : "persistent", (unsigned long long)word,
            (size_t)mmgr_series_cellularum_persistent_end(s_block), (size_t)mmgr_series_cellularum_temporary_top(s_block),
            (size_t)embed_atomic_usize_load(&s_block->persistent_overrun), (size_t)embed_atomic_usize_load(&s_block->temporary_overrun));
    for (uint32_t seq = from; seq < now; seq += 1u)
    {
        const StressTrace *const step = &s_trace[seq & (STRESS_TRACE_MOST - 1u)];
        const int nearby = (step->off != STRESS_OFF_NONE) && (step->off + STRESS_TRACE_NEAR >= off) && (step->off <= off + STRESS_TRACE_NEAR);
        const int refused = (step->op == STRESS_OP_REFUSED_PERSISTENT) || (step->op == STRESS_OP_REFUSED_TEMPORARY);
        const int last = (seq + STRESS_TRACE_WHOLE) >= now;

        if ((step->seq == seq) && (nearby || refused || last))
        {
            fprintf(stderr, "%u t%u %s off=%#x size=%u end=%#x top=%#x over_end=%#x over_top=%#x\n", seq, step->thread,
                    names[step->op < 8u ? step->op : 0u], step->off, step->size, step->end, step->top, step->over_end, step->over_top);
        }
    }
    fflush(stderr);
    abort();
}

/** @brief A take that landed on a word another live take holds: both steps and every step between them printed once. */
static void overlapped(unsigned int thread, uint32_t seq, uint32_t off, size_t bytes, uint32_t other)
{
    if (embed_atomic_u32_fetch_add(&s_broken, 1u) != 0u)
    {
        for (;;)
        {
        }
    }
    const uint32_t now = embed_atomic_u32_load(&s_trace_next);
    const uint32_t oldest = (now > STRESS_TRACE_MOST - 64u) ? (now - (STRESS_TRACE_MOST - 64u)) : 0u;
    const uint32_t from = (other > oldest) ? other : oldest;
    static const char *const names[8] = {"?", "P", "T", "R", "p!", "t!", "p?", "t?"};

    fprintf(stderr, "OVERLAP thread %u step %u cell off=%#x bytes=%zu lands on the cell of step %u; end=%#zx top=%#zx over_end=%#zx over_top=%#zx\n",
            thread, seq, off, bytes, other, (size_t)mmgr_series_cellularum_persistent_end(s_block),
            (size_t)mmgr_series_cellularum_temporary_top(s_block), (size_t)embed_atomic_usize_load(&s_block->persistent_overrun),
            (size_t)embed_atomic_usize_load(&s_block->temporary_overrun));
    for (uint32_t at = from; at < now; at += 1u)
    {
        const StressTrace *const step = &s_trace[at & (STRESS_TRACE_MOST - 1u)];

        if (step->seq == at)
        {
            fprintf(stderr, "%u t%u %s off=%#x size=%u end=%#x top=%#x over_end=%#x over_top=%#x\n", at, step->thread,
                    names[step->op < 8u ? step->op : 0u], step->off, step->size, step->end, step->top, step->over_end, step->over_top);
        }
    }
    fflush(stderr);
    abort();
}

/** @brief The words of a cell just taken marked with its step: a word marked already is another live cell's. */
static void shadowed(unsigned int thread, uint32_t seq, uint32_t off, size_t bytes)
{
    const size_t rounded = (bytes + 7u) & ~(size_t)7u;
    const uint32_t first = off / 8u;
    const uint32_t words = (uint32_t)((16u + rounded) / 8u);

    if ((size_t)first + words > STRESS_SHADOW_MOST)
    {
        return;
    }
    for (uint32_t at = first; at < first + words; at += 1u)
    {
        const uint32_t was = embed_atomic_u32_exchange(&s_shadow[at], seq + 1u);

        if (was != 0u)
        {
            overlapped(thread, seq, off, bytes, was - 1u);
        }
    }
}


/** @brief The words of a cell about to go back cleared. */
static void unshadowed(uint32_t off, size_t bytes)
{
    const size_t rounded = (bytes + 7u) & ~(size_t)7u;
    const uint32_t first = off / 8u;
    const uint32_t words = (uint32_t)((16u + rounded) / 8u);

    if ((size_t)first + words > STRESS_SHADOW_MOST)
    {
        return;
    }
    for (uint32_t at = first; at < first + words; at += 1u)
    {
        embed_atomic_u32_store(&s_shadow[at], 0u);
    }
}

/** @brief Every taken cell's header read: a header that is not a busy cell's of its bytes is a broken cell. */
static void checked_all(unsigned int thread, const StressHeld *held, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const uint64_t word = header_of(held[at].payload);

        if (!header_fits(word, held[at].bytes))
        {
            dumped(thread, &held[at], word);
        }
    }
}

static void given(unsigned int thread, StressHeld *held, unsigned int *count, unsigned int at, const CellOwner *owner, uint8_t mark)
{
    const uint64_t word = header_of(held[at].payload);

    if (!header_fits(word, held[at].bytes))
    {
        dumped(thread, &held[at], word);
    }
    if (!intact(&held[at], mark))
    {
        (void)embed_atomic_usize_fetch_add(&s_damaged, 1u);
    }
    const uint32_t off = off_of(held[at].payload);
    unshadowed(off, held[at].bytes);
    mmgr_persistent_buf_release(s_block, held[at].payload, owner);
    (void)traced(thread, STRESS_OP_RELEASE, off, held[at].bytes);
    held[at] = held[*count - 1u];
    *count -= 1u;
}

static void each(void)
{
    const unsigned int index = embed_atomic_u32_fetch_add(&s_next, 1u);
    const CellOwner *const owner = s_own[index];
    const uint8_t mark = (uint8_t)(index + 1u);
    uint64_t state = s_seed ^ (0x9E3779B97F4A7C15ull * (uint64_t)(index + 1u));
    StressHeld held[STRESS_HELD_MOST];
    unsigned int count = 0u;

    for (unsigned int round = 0u; round < s_rounds; round += 1u)
    {
        const uint64_t roll = next(&state);
        const unsigned int choice = (unsigned int)(roll % 100u);
        const size_t bytes = STRESS_SIZE_LEAST + (size_t)((roll >> 8) % (STRESS_SIZE_MOST - STRESS_SIZE_LEAST + 1u));
        if ((choice < 55u) && (count < STRESS_HELD_MOST))
        {
            (void)traced(index, STRESS_OP_BEGUN_PERSISTENT, STRESS_OFF_NONE, bytes);
            uint8_t *const payload = mmgr_persistent_buf_alloc(s_block, bytes, owner);
            if (payload == NULL)
            {
                (void)embed_atomic_usize_fetch_add(&s_refused, 1u);
                (void)traced(index, STRESS_OP_REFUSED_PERSISTENT, STRESS_OFF_NONE, bytes);
                continue;
            }
            shadowed(index, traced(index, STRESS_OP_PERSISTENT, off_of(payload), bytes), off_of(payload), bytes);
            memset(payload, mark, bytes);
            held[count] = (StressHeld){payload, bytes, 0};
            count += 1u;
            (void)embed_atomic_usize_fetch_add(&s_taken, 1u);
            checked_all(index, held, count);
        }
        else if ((choice < 70u) && (count < STRESS_HELD_MOST))
        {
            (void)traced(index, STRESS_OP_BEGUN_TEMPORARY, STRESS_OFF_NONE, bytes);
            uint8_t *const payload = mmgr_temporary_buf_alloc(s_block, bytes, owner);
            if (payload == NULL)
            {
                (void)embed_atomic_usize_fetch_add(&s_refused, 1u);
                (void)traced(index, STRESS_OP_REFUSED_TEMPORARY, STRESS_OFF_NONE, bytes);
                continue;
            }
            shadowed(index, traced(index, STRESS_OP_TEMPORARY, off_of(payload), bytes), off_of(payload), bytes);
            memset(payload, mark, bytes);
            held[count] = (StressHeld){payload, bytes, 1};
            count += 1u;
            (void)embed_atomic_usize_fetch_add(&s_taken, 1u);
            checked_all(index, held, count);
        }
        else if ((choice < 96u) && (count != 0u))
        {
            given(index, held, &count, (unsigned int)((roll >> 32) % count), owner, mark);
        }
        else
        {
            (void)mmgr_buf_available(s_block);
        }
    }
    while (count != 0u)
    {
        given(index, held, &count, count - 1u, owner, mark);
    }
}

int main(int argc, char **argv)
{
    const unsigned int threads = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 16u;
    const size_t bytes = (argc > 3) ? (size_t)strtoull(argv[3], NULL, 10) : (size_t)(64u * 1024u);
    const unsigned int runs = (argc > 4) ? (unsigned int)strtoul(argv[4], NULL, 10) : 3u;
    s_rounds = (argc > 2) ? (unsigned int)strtoul(argv[2], NULL, 10) : 20000u;
    s_seed = (argc > 5) ? strtoull(argv[5], NULL, 10) : 0x0123456789ABCDEFull;
    if ((threads == 0u) || (threads > STRESS_THREADS_MOST) || (bytes < 1024u))
    {
        fprintf(stderr, "locus_carcerum_stress [threads<=%u] [rounds] [bytes>=1024] [runs] [seed]\n", STRESS_THREADS_MOST);
        return 2;
    }
    const CellOwner *const owners[STRESS_THREADS_MOST] = {s_o0, s_o1, s_o2,  s_o3,  s_o4,  s_o5,  s_o6,  s_o7,
                                                          s_o8, s_o9, s_o10, s_o11, s_o12, s_o13, s_o14, s_o15};
    memcpy(s_own, owners, sizeof(owners));
    uint8_t *const storage = guarded(bytes);
    uint8_t *const state_page = guarded(sizeof(SeriesCellularum) + 64u);
    if ((storage == NULL) || (state_page == NULL))
    {
        fprintf(stderr, "no guarded storage\n");
        return 2;
    }
    // the state laid at the highest 64-aligned place of its page, and its last word stands against the fault page
    s_block = (SeriesCellularum *)(void *)(state_page + ((sizeof(SeriesCellularum) + 64u - sizeof(SeriesCellularum)) & ~(size_t)63u));
    const SeriesCellularum made = MMGR_CARCER_STATE(storage, bytes);
    memcpy(s_block, &made, sizeof(made));
    int failed = 0;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        embed_atomic_u32_store(&s_next, 0u);
        embed_atomic_usize_store(&s_refused, 0u);
        embed_atomic_usize_store(&s_taken, 0u);
        embed_atomic_usize_store(&s_damaged, 0u);
        embed_atomic_usize_store(&s_misnamed, 0u);
        if (!mmgr_host_threads_run(each, threads))
        {
            fprintf(stderr, "run %u: the threads did not all start\n", run);
            return 2;
        }
        size_t standing = 0u;
        for (unsigned int at = 0u; at < threads; at += 1u)
        {
            standing += mmgr_owner_in_use(s_own[at]);
        }
        const size_t end = mmgr_series_cellularum_persistent_end(s_block);
        const size_t top = mmgr_series_cellularum_temporary_top(s_block);
        const size_t available = mmgr_buf_available(s_block);
        const int clean = (standing == 0u) && (end == 0u) && (top == bytes) && (available == bytes) &&
                          (embed_atomic_usize_load(&s_damaged) == 0u) && (embed_atomic_usize_load(&s_misnamed) == 0u);
        printf("run %u: taken=%llu refused=%llu damaged=%llu misnamed=%llu standing=%llu end=%llu top=%llu available=%llu %s\n",
               run, (unsigned long long)embed_atomic_usize_load(&s_taken), (unsigned long long)embed_atomic_usize_load(&s_refused),
               (unsigned long long)embed_atomic_usize_load(&s_damaged), (unsigned long long)embed_atomic_usize_load(&s_misnamed),
               (unsigned long long)standing, (unsigned long long)end, (unsigned long long)top, (unsigned long long)available,
               clean ? "clean" : "BROKEN");
        fflush(stdout);
        failed = failed || !clean;
        mmgr_series_cellularum_emptied(s_block);
        memset(s_shadow, 0, sizeof(s_shadow));
        s_seed += 0x9E3779B97F4A7C15ull;
    }
    return failed ? 1 : 0;
}
