/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file device_bench.h
 * @brief Shared cycle-counter microbench macros for performance_benching/<module>/main/main.c.
 *
 * Every bench includes this and prints one "DB " line per benched operation, so a runner collects
 * results off the serial line without parsing anything else the port carries. Each bench loops its
 * timing block.
 *
 * Two arms. On silicon the counter is the part's own, CCOUNT on Xtensa and the mcycle CSR on
 * RISC-V, both JTAG-observable and neither costing a call into a driver; the bench runs on its own
 * FreeRTOS task and never returns, and the lines go out over USB-Serial/JTAG. On host the counter is
 * a monotonic clock scaled to the reported rate, the bench is a plain main(), and it runs one pass
 * and exits so a runner can collect it.
 *
 * A cycle count is the figure to compare between two targets. The S3 runs at 240 MHz and the C6 at
 * 160 MHz, so a wall time says as much about the clock as about the code.
 *
 * Adapted from ProtoCore's performance_benching harness, whose shape this keeps: the same macro
 * names, the same "DB " line, the same two arms. What changed is underneath - MMgr has no clock
 * module, so the counter is read straight from the part.
 */
#ifndef MMGR_PERF_DEVICE_BENCH_H
#define MMGR_PERF_DEVICE_BENCH_H

#include <stdint.h>
#include <stdio.h>

/**
 * @brief Whether this build targets a part with a cycle counter, rather than the host.
 */
#if defined(__XTENSA__) || defined(__riscv)
#define MMGR_BENCH_SILICON 1
#else
#define MMGR_BENCH_SILICON 0
#endif

#if MMGR_BENCH_SILICON
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <stdlib.h> // exit: the host arm runs one pass, so the timing loop has to end
#include <time.h>
#endif

/**
 * @brief The clock the cycle counts are taken against, in MHz.
 *
 * @note Supplied per target by the bench's main/CMakeLists.txt, read off the configuration the image
 *       is built with. A rate named by hand goes stale the moment sdkconfig changes, which reports
 *       counts against a clock the part is not running at.
 */
#ifndef DBENCH_CPU_MHZ
#define DBENCH_CPU_MHZ 240u
#endif

#if MMGR_BENCH_SILICON
/**
 * @brief One cycle-counter read.
 */
#define DBENCH_CYCLE_READ(dst) ((dst) = (uint32_t)esp_cpu_get_cycle_count())
#else
/**
 * @brief Monotonic time as a count in the same unit the part's counter reports.
 *
 * @return Nanoseconds scaled by DBENCH_CPU_MHZ, so both arms print one number.
 */
static inline uint32_t dbench_host_cycles(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const double ns = ((double)ts.tv_sec * 1e9) + (double)ts.tv_nsec;

    // Explicit cast wraps the scaled count into the same 32-bit container the part's counter uses
    return (uint32_t)(ns * ((double)DBENCH_CPU_MHZ / 1000.0));
}

/**
 * @brief One cycle-counter read.
 */
#define DBENCH_CYCLE_READ(dst) ((dst) = dbench_host_cycles())
#endif

/**
 * @brief Where a timed expression's result goes so the call cannot be deleted.
 *
 * @note volatile, and written every iteration. A result that is computed and dropped is a call the
 *       optimiser may remove outright, which reports the removal rather than the work. Under
 *       link-time optimization it does exactly that: the first run of a dispatch measurement here
 *       reported 0.00 cycles for both arms because both loops had been removed.
 * @note Defined by DBENCH_MAIN, so exactly one translation unit per image carries it.
 */
extern volatile uintptr_t g_dbench_sink;

/**
 * @brief Keeps one value, whatever its type.
 */
#define DBENCH_KEEP(v) (g_dbench_sink = (uintptr_t)(v))

/**
 * @brief The gap between reported lines: one RTOS tick on silicon, nothing on host.
 */
#if MMGR_BENCH_SILICON
#define DBENCH_SETTLE() vTaskDelay(1)
#else
#define DBENCH_SETTLE() ((void)0)
#endif

/**
 * @brief Warms once, runs N iterations, leaves the mean cycle count in out_cy (a double lvalue).
 *
 * @note The warm pass is not counted. It pulls the code and the fixture into cache, so what follows
 *       measures the loop rather than the first miss.
 */
#define DBENCH_CYCLES(N, expr, out_cy)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        expr; /* warm */                                                                                               \
        uint32_t _c0, _c1;                                                                                             \
        DBENCH_CYCLE_READ(_c0);                                                                                        \
        for (uint32_t _i = 0; _i < (uint32_t)(N); _i++)                                                                \
        {                                                                                                              \
            expr;                                                                                                      \
        }                                                                                                              \
        DBENCH_CYCLE_READ(_c1);                                                                                        \
        (out_cy) = (double)(_c1 - _c0) / (double)(N);                                                                  \
    } while (0)

/**
 * @brief One-shot op that is not a bulk byte stream. Reports the mean in cycles, us and ns.
 */
#define DBENCH_OP(label, N, expr)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        double _cy = 0.0;                                                                                              \
        DBENCH_CYCLES(N, expr, _cy);                                                                                   \
        printf("DB %-30s cyc=%-11.2f us=%-9.2f ns=%.0f\n", label, _cy, _cy / (double)DBENCH_CPU_MHZ,                   \
               (_cy * 1000.0) / (double)DBENCH_CPU_MHZ);                                                               \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

/**
 * @brief Bulk op over `bytes`. Reports cycles per op, ns per byte and MB/s.
 */
#define DBENCH_BULK(label, N, bytes, expr)                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        double _cy = 0.0;                                                                                              \
        DBENCH_CYCLES(N, expr, _cy);                                                                                   \
        double _nspb = ((_cy * 1000.0) / (double)DBENCH_CPU_MHZ) / (double)(bytes);                                    \
        double _mbs = (_nspb > 0.0) ? (1000.0 / _nspb) : 0.0;                                                          \
        printf("DB %-30s cyc=%-11.0f ns/B=%-8.2f MB/s=%-8.1f (%uB)\n", label, _cy, _nspb, _mbs, (unsigned)(bytes));    \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

/**
 * @brief One A/B row: the same work through the library and through libc, over the same bytes.
 *
 * @note Prints both arms and the ratio on one line, because the ratio is the reading and computing
 *       it from two lines by hand is where a comparison goes wrong.
 * @note Not in ProtoCore's harness. MMgr's benches exist to be read against the target's own libc,
 *       which is ESP-ROM assembly here, so the two arms belong on one line.
 */
#define DBENCH_AB(label, N, bytes, mmgr_expr, libc_expr)                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        double _m = 0.0;                                                                                               \
        double _l = 0.0;                                                                                               \
        DBENCH_CYCLES(N, mmgr_expr, _m);                                                                               \
        DBENCH_CYCLES(N, libc_expr, _l);                                                                               \
        printf("DB %-16s n=%-6u mmgr=%-10.1f libc=%-10.1f ratio=%.2f  mmgr_c/B=%.3f libc_c/B=%.3f\n", label,           \
               (unsigned)(bytes), _m, _l, (_l > 0.0) ? (_m / _l) : 0.0, _m / (double)(bytes), _l / (double)(bytes));   \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

/**
 * @brief What the counter is on this arm, for the banner.
 */
#if MMGR_BENCH_SILICON
#if defined(__XTENSA__)
#define DBENCH_COUNTER "CCOUNT, xtensa"
#else
#define DBENCH_COUNTER "mcycle, riscv"
#endif
#else
#define DBENCH_COUNTER "host clock"
#endif

/**
 * @brief Start-of-cycle line, naming the module, the counter and the clock it is taken against.
 */
#define DBENCH_BANNER(label)                                                                                           \
    printf("DB ==== " label " microbench start (" DBENCH_COUNTER " @ %u MHz) ====\n", (unsigned)DBENCH_CPU_MHZ)

/**
 * @brief End-of-cycle marker.
 *
 * @note On silicon it pauses and the caller's loop starts the next pass, so a capture opened at any
 *       time catches a whole pass. On host it ends the process, so the one pass a runner asked for
 *       is the one it gets.
 */
#if MMGR_BENCH_SILICON
#define DBENCH_DONE()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        printf("DB ==== DONE ====\n");                                                                                 \
        vTaskDelay(5000 / portTICK_PERIOD_MS);                                                                         \
    } while (0)
#else
#define DBENCH_DONE()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        printf("DB ==== DONE ====\n");                                                                                 \
        fflush(stdout);                                                                                                \
        exit(0);                                                                                                       \
    } while (0)
#endif

/**
 * @brief Prepares the fixtures and loops the timed operations.
 */
void dbench_run(void);

/**
 * @brief The entry every bench shares: a boot line, then dbench_run().
 *
 * @note On silicon the run is pinned and given a high priority, so the system is not competing with
 *       it. The last core the scheduler has rather than core 1: the C6 pairs one HP core with a
 *       20 MHz LP coprocessor FreeRTOS does not dispatch to, so it schedules one core and a task
 *       pinned to core 1 is never run at all - the image prints its boot line and stops.
 * @note Defines g_dbench_sink, which device_bench.h only declares.
 */
#if MMGR_BENCH_SILICON
#define DBENCH_MAIN(label)                                                                                             \
    volatile uintptr_t g_dbench_sink;                                                                                  \
    static void dbench_task(void *arg)                                                                                 \
    {                                                                                                                  \
        (void)arg;                                                                                                     \
        dbench_run();                                                                                                  \
    }                                                                                                                  \
    void app_main(void);                                                                                               \
    void app_main(void)                                                                                                \
    {                                                                                                                  \
        vTaskDelay(2500 / portTICK_PERIOD_MS);                                                                         \
        printf("\nDB boot: " label " device microbench\n");                                                            \
        xTaskCreatePinnedToCore(dbench_task, "dbench", 16384, NULL, 24, NULL, portNUM_PROCESSORS - 1);                 \
    }
#else
#define DBENCH_MAIN(label)                                                                                             \
    volatile uintptr_t g_dbench_sink;                                                                                  \
    int main(void)                                                                                                     \
    {                                                                                                                  \
        printf("\nDB boot: " label " host microbench\n");                                                              \
        dbench_run();                                                                                                  \
        return 0;                                                                                                      \
    }
#endif

#endif // MMGR_PERF_DEVICE_BENCH_H
