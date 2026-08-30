#ifndef MMGR_PERF_HOST_BENCH_H
#define MMGR_PERF_HOST_BENCH_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static inline double hbench_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((double)ts.tv_sec * 1e9) + (double)ts.tv_nsec;
}

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

static inline void hbench_header(void)
{
    printf("| Module       | Operation                |     ns/op |    MB/s |\n");
    printf("|--------------|--------------------------|-----------|---------|\n");
}

static inline void hbench_row(const char *module, const char *op, double ns_per_op, double bytes_per_op)
{
    const double mbps = (bytes_per_op > 0.0) ? ((bytes_per_op / (ns_per_op * 1e-9)) / 1e6) : 0.0;

    printf("| %-12s | %-24s | %10.1f | %9.1f |\n", module, op, ns_per_op, mbps);
}

#endif
