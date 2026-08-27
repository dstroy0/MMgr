/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file host_bench.h
 * @brief Shared host microbench macros for performance_benching/<module>/host.c.
 *
 * Times an expression over N iterations against CLOCK_MONOTONIC and prints one table row per benched
 * operation. The host number is a relative baseline on a desktop core; the figure that means
 * something for this library is the one from the matching main/main.c cycle bench (device_bench.h),
 * because a desktop libc answers the same calls with SSE or AVX and no target here has anything of
 * the kind.
 *
 * Adapted from ProtoCore's performance_benching harness.
 */
#ifndef MMGR_PERF_HOST_BENCH_H
#define MMGR_PERF_HOST_BENCH_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/**
 * @brief Monotonic nanoseconds.
 *
 * @return The reading, as a double so a difference keeps sub-microsecond resolution.
 */
static inline double hbench_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((double)ts.tv_sec * 1e9) + (double)ts.tv_nsec;
}

/**
 * @brief Runs expr N times, leaving the mean nanoseconds per iteration in out_ns (a double lvalue).
 */
#define HBENCH_NS(iters, expr, out_ns)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        double _t0 = hbench_now_ns();                                                                                  \
        for (uint64_t _i = 0; _i < (uint64_t)(iters); _i++)                                                            \
        {                                                                                                              \
            expr;                                                                                                      \
        }                                                                                                              \
        (out_ns) = (hbench_now_ns() - _t0) / (double)(iters);                                                          \
    } while (0)

/**
 * @brief Column headers for the result table.
 */
static inline void hbench_header(void)
{
    printf("| Module       | Operation                |     ns/op |    MB/s |\n");
    printf("|--------------|--------------------------|-----------|---------|\n");
}

/**
 * @brief One result row.
 *
 * @param[in] module       Module the operation belongs to [BORROWS].
 * @param[in] op           Operation name [BORROWS].
 * @param[in] ns_per_op    Mean nanoseconds per iteration.
 * @param[in] bytes_per_op Bytes one iteration covers, or 0 when the op has no byte count.
 * @note MB/s is reported as 0 when the op has no byte count, rather than left blank, so a column
 *       stays a column.
 */
static inline void hbench_row(const char *module, const char *op, double ns_per_op, double bytes_per_op)
{
    const double mbps = (bytes_per_op > 0.0) ? ((bytes_per_op / (ns_per_op * 1e-9)) / 1e6) : 0.0;

    printf("| %-12s | %-24s | %10.1f | %9.1f |\n", module, op, ns_per_op, mbps);
}

#endif // MMGR_PERF_HOST_BENCH_H
