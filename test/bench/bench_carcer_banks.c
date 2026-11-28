/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file bench_carcer_banks.c
 * @brief The takes and gives of one cellblock, timed on one thread and on four. The same source builds against two
 *        trees, so its rows compare the two cellblocks and nothing else.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-27
 *
 * @note Each row is the best of BENCH_REPEATS runs, in nanoseconds for one take and its give. A take and a give of
 *       the newest cell is the hot path and a trim of the boundary. A chain of takes given back newest first trims
 *       on each give. One given back oldest first trims once, at the last give.
 * @note The threads rows start every thread at one barrier and time the wall clock from the barrier to the last
 *       thread's end. Each row divides that by the takes of every thread.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

#include "bench_harness.h"

#include "locus_carcerum/locus_carcerum.h"

#define ITERS 200000ul
#define POOL_BYTES (1u << 20)
#define CHAIN 16u
#define THREADS 4u
#define THREAD_ITERS 200000ul

ParsMemoriaeInternae(general, POOL_BYTES);

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(general));

// The owners every case takes for, declared with the cellblocks as the program's table
#define BENCH_OWNERS(owner_) owner_(cells, "cells")
MMGR_OWNERS(BENCH_OWNERS)

/** @brief One row: the case, the payload of each take, and the nanoseconds of a take and its give. */
static void report(const char *name, size_t bytes, double seconds_per_op)
{
    printf("%s,%u,%.2f\n", name, (unsigned)bytes, seconds_per_op * 1e9);
    fflush(stdout);
}

/** @brief The cellblock emptied ahead of a case, so every case starts from the same state. */
static void fresh(void)
{
    mmgr_series_cellularum_emptied(&ram_general_ctx);
}

/** @brief A take of each tier and its give, the newest cell each time. */
static void bench_one(size_t bytes)
{
    double per = 0.0;

    fresh();
    BENCH_TIME(per, ITERS, {
        void *held_ = ram.general.persistent_buf_alloc(bytes + (bench_i_ & 0u), cells);
        BENCH_RETAIN(held_ != NULL);
        ram.general.persistent_buf_release(held_, cells);
    });
    report("persistent take+give", bytes, per);

    fresh();
    BENCH_TIME(per, ITERS, {
        void *held_ = ram.general.temporary_buf_alloc(bytes + (bench_i_ & 0u), cells);
        BENCH_RETAIN(held_ != NULL);
        ram.general.persistent_buf_release(held_, cells);
    });
    report("temporary take+give", bytes, per);
}

/** @brief A chain of takes of each tier, given back newest first and oldest first. */
static void bench_chain(size_t bytes)
{
    double per = 0.0;

    fresh();
    BENCH_TIME(per, ITERS / CHAIN, {
        void *held_[CHAIN];
        for (unsigned k_ = 0u; k_ < CHAIN; k_++)
        {
            held_[k_] = ram.general.persistent_buf_alloc(bytes + (bench_i_ & 0u), cells);
            BENCH_RETAIN(held_[k_] != NULL);
        }
        for (unsigned k_ = CHAIN; k_ > 0u; k_--)
        {
            ram.general.persistent_buf_release(held_[k_ - 1u], cells);
        }
    });
    report("persistent chain newest first", bytes, per / (double)CHAIN);

    fresh();
    BENCH_TIME(per, ITERS / CHAIN, {
        void *held_[CHAIN];
        for (unsigned k_ = 0u; k_ < CHAIN; k_++)
        {
            held_[k_] = ram.general.persistent_buf_alloc(bytes + (bench_i_ & 0u), cells);
            BENCH_RETAIN(held_[k_] != NULL);
        }
        for (unsigned k_ = 0u; k_ < CHAIN; k_++)
        {
            ram.general.persistent_buf_release(held_[k_], cells);
        }
    });
    report("persistent chain oldest first", bytes, per / (double)CHAIN);

    fresh();
    BENCH_TIME(per, ITERS / CHAIN, {
        void *held_[CHAIN];
        for (unsigned k_ = 0u; k_ < CHAIN; k_++)
        {
            held_[k_] = ram.general.temporary_buf_alloc(bytes + (bench_i_ & 0u), cells);
            BENCH_RETAIN(held_[k_] != NULL);
        }
        for (unsigned k_ = CHAIN; k_ > 0u; k_--)
        {
            ram.general.persistent_buf_release(held_[k_ - 1u], cells);
        }
    });
    report("temporary chain newest first", bytes, per / (double)CHAIN);
}

/** @brief What one thread of a threads row runs: its tier, its payload, and the barrier it starts at. */
typedef struct
{
    int temporary;              /**< Nonzero for the temporary tier. */
    size_t bytes;               /**< The payload of each take. */
    pthread_barrier_t *barrier; /**< Where every thread and the clock start. */
} BenchThread;

/** @brief One thread of a threads row: a take and its give, THREAD_ITERS times. */
static void *bench_thread(void *argument)
{
    const BenchThread *const job = (const BenchThread *)argument;

    pthread_barrier_wait(job->barrier);
    for (unsigned long i = 0u; i < THREAD_ITERS; i++)
    {
        void *held = (job->temporary != 0) ? ram.general.temporary_buf_alloc(job->bytes, cells)
                                           : ram.general.persistent_buf_alloc(job->bytes, cells);
        BENCH_RETAIN(held);
        ram.general.persistent_buf_release(held, cells);
    }
    return NULL;
}

/** @brief One threads row, the best of BENCH_REPEATS runs. */
static void bench_threads(const char *name, int temporary, size_t bytes)
{
    double best = 1e30;

    for (unsigned r = 0u; r < BENCH_REPEATS; r++)
    {
        pthread_t thread[THREADS];
        pthread_barrier_t barrier;
        BenchThread job = {temporary, bytes, &barrier};

        fresh();
        pthread_barrier_init(&barrier, NULL, THREADS + 1u);
        for (unsigned t = 0u; t < THREADS; t++)
        {
            pthread_create(&thread[t], NULL, bench_thread, &job);
        }
        pthread_barrier_wait(&barrier);
        const double start = bench_now();
        for (unsigned t = 0u; t < THREADS; t++)
        {
            pthread_join(thread[t], NULL);
        }
        const double took = bench_now() - start;
        pthread_barrier_destroy(&barrier);
        if (took < best)
        {
            best = took;
        }
    }
    report(name, bytes, best / (double)(THREADS * THREAD_ITERS));
}

int main(void)
{
    printf("case,bytes,ns_per_op\n");
    fflush(stdout);

    {
        double per = 0.0;

        BENCH_TIME(per, ITERS, { BENCH_RETAIN(bench_i_); });
        report("harness nop", 0u, per);
    }

    bench_one(64u);
    bench_one(1024u);
    bench_chain(64u);
    bench_threads("persistent take+give on 4 threads", 0, 64u);
    bench_threads("temporary take+give on 4 threads", 1, 64u);
    return 0;
}
