// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BENCH_HARNESS_H
#define MMGR_BENCH_HARNESS_H

// The measurement, and nothing else in the measured path.
//
// A bench that defends its timing loop with a function pointer, a volatile store or a memory sink is
// timing those. A function pointer stops the call inlining, so the code measured is not the code the
// library ships - and this library ships MMGR_INLINE entries whose whole point is that they inline.
// A volatile store adds a load and a store per iteration. A sink accumulates. Each is a bottleneck
// the bench brought with it, and at these durations - a span helper or a SWAR word scan is a handful
// of nanoseconds - they are the whole reading.
//
// What is used instead is an empty asm with a memory clobber. It emits no instruction at all. It
// tells the compiler that memory may have changed and that the value handed to it has escaped, so a
// pure call cannot be hoisted out of the loop and its result cannot be discarded, and the loop is
// left holding the work and the loop counter and nothing else.
//
// test/ is exempt from the src/ style rules, so this reads as plain host C.
//
// ---------------------------------------------------------------------------------------------
// HOW TO CALL IT, AND WHY THE BARRIER ALONE IS NOT ENOUGH
// ---------------------------------------------------------------------------------------------
// Derive the argument from bench_i_, the loop counter BENCH_TIME declares:
//
//     BENCH_TIME_CYCLES(out, 20000000ul, {
//         const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));
//         const uint64_t v_ = (uint64_t)fn_under_test(buf + r_, r_);
//         BENCH_KEEP(v_);
//     });
//
// The clobber says MEMORY changed. It does not say anything about a function GCC has proved to be
// const - one that reads no memory and depends only on its arguments. Given a loop-invariant
// argument, such a call is still hoisted straight out of the loop despite the barrier, and the loop
// left behind counts to n while the work happens once. Measured on gcc 13 -O2, all three cases:
//
//     no barrier,   invariant arg  ->  loop deleted, tail-call: the bench times nothing
//     barrier,      invariant arg  ->  call hoisted above the loop: the bench times a counter
//     barrier,      variant arg    ->  one call per iteration, and the barrier emits no instruction
//
// So the loop-variant argument is what defeats the hoist and the barrier is what stops the result
// being discarded. Both are needed. A bench whose reading does not move when SPREAD changes is
// almost certainly the middle case.

#include <stdint.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
// The value has escaped: it cannot be folded away and the call producing it cannot be deleted.
// "r,m" lets the compiler satisfy it from a register or from memory, so it never has to spill a
// value that was already where it needed to be just to satisfy the constraint.
#define BENCH_KEEP(v) __asm__ __volatile__("" : : "r,m"(v) : "memory")
// Memory may have changed: what was read before this cannot be reused after it, so a call whose
// arguments are loop-invariant is still made once per iteration.
#define BENCH_CLOBBER() __asm__ __volatile__("" : : : "memory")
#else
// No such barrier here, so the fallback pays for one volatile store per iteration. The nop row in
// the results is what that costs; on a compiler that reaches the arm above it costs nothing.
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

// A cycle count is what makes a SWAR result readable: "3.2 ns" moves with the clock, "11 cycles for
// 8 bytes" does not. lfence on both sides because rdtsc is not ordered against surrounding
// instructions and without them the count picks up whatever the scheduler moved across it.
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

// Best-of rather than mean. The distribution is one-sided: nothing makes a run faster than the work
// takes, while a scheduler preemption, an interrupt or a migration can make one arbitrarily slower.
// The minimum is the closest estimate of the work itself; averaging folds the noise back in.
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

#endif // MMGR_BENCH_HARNESS_H
