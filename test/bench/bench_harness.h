/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef MMGR_BENCH_HARNESS_H
#define MMGR_BENCH_HARNESS_H

#include <stdint.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)

#define BENCH_KEEP(v) __asm__ __volatile__("" : : "r,m"(v) : "memory")

#define BENCH_CLOBBER() __asm__ __volatile__("" : : : "memory")
#else

extern volatile uint64_t mmgr_bench_sink;
#define BENCH_KEEP(v) (mmgr_bench_sink = (uint64_t)(v))
#define BENCH_CLOBBER() ((void)0)
#endif

#if defined(CLOCK_MONOTONIC)
static inline double bench_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + ((double)t.tv_nsec * 1e-9);
}
#else
#include <stdlib.h>
static inline double bench_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
static inline uint64_t bench_cycles(void)
{
    uint32_t lo;
    uint32_t hi;
    __asm__ __volatile__("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi)::"memory");
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}
#define BENCH_HAS_CYCLES 1
#elif defined(__aarch64__)
static inline uint64_t bench_cycles(void)
{
    uint64_t v;
    __asm__ __volatile__("isb; mrs %0, cntvct_el0" : "=r"(v)::"memory");
    return v;
}
#define BENCH_HAS_CYCLES 1
#else
static inline uint64_t bench_cycles(void)
{
    return 0u;
}
#define BENCH_HAS_CYCLES 0
#endif

static inline double bench_cycles_per_s(void)
{
    static double rate = 0.0;
    if (rate > 0.0)
    {
        return rate;
    }
    const double t0 = bench_now();
    const uint64_t c0 = bench_cycles();
    double t1 = t0;
    while ((t1 - t0) < 0.05)
    {
        t1 = bench_now();
    }
    const uint64_t c1 = bench_cycles();
    rate = (double)(c1 - c0) / (t1 - t0);
    return rate;
}

#define BENCH_REPEATS 7u

#define BENCH_TIME(OUT, ITERS, CALL)                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        double bench_best_ = 1e30;                                                                                     \
        for (unsigned bench_r_ = 0; bench_r_ < BENCH_REPEATS; bench_r_++)                                              \
        {                                                                                                              \
            BENCH_CLOBBER();                                                                                           \
            const double bench_t0_ = bench_now();                                                                      \
            for (unsigned long bench_i_ = 0; bench_i_ < (ITERS); bench_i_++)                                           \
            {                                                                                                          \
                BENCH_CLOBBER();                                                                                       \
                CALL;                                                                                                  \
            }                                                                                                          \
            const double bench_dt_ = bench_now() - bench_t0_;                                                          \
            if (bench_dt_ < bench_best_)                                                                               \
            {                                                                                                          \
                bench_best_ = bench_dt_;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
        (OUT) = bench_best_ / (double)(ITERS);                                                                         \
    } while (0)

#define BENCH_TIME_CYCLES(OUT, ITERS, CALL)                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        uint64_t bench_best_ = (uint64_t)-1;                                                                           \
        for (unsigned bench_r_ = 0; bench_r_ < BENCH_REPEATS; bench_r_++)                                              \
        {                                                                                                              \
            BENCH_CLOBBER();                                                                                           \
            const uint64_t bench_c0_ = bench_cycles();                                                                 \
            for (unsigned long bench_i_ = 0; bench_i_ < (ITERS); bench_i_++)                                           \
            {                                                                                                          \
                BENCH_CLOBBER();                                                                                       \
                CALL;                                                                                                  \
            }                                                                                                          \
            const uint64_t bench_dc_ = bench_cycles() - bench_c0_;                                                     \
            if (bench_dc_ < bench_best_)                                                                               \
            {                                                                                                          \
                bench_best_ = bench_dc_;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
        (OUT) = (double)bench_best_ / (double)(ITERS);                                                                 \
    } while (0)

#endif
