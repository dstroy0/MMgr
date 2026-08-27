/**
 * @brief On-device cycle-counter macros for the benches under performance_benching.
 *
 * @note Reads the part's own cycle counter through esp_cpu_get_cycle_count, which is CCOUNT on Xtensa
 *       and the mcycle CSR on RISC-V. Both are JTAG-observable and neither costs a call into a driver.
 * @note A cycle count is the figure to compare between the two targets. The S3 runs at 240 MHz and the
 *       C6 at 160 MHz, so a wall time says as much about the clock as about the code.
 * @note Every bench prints one "DB " line per operation, so a runner can collect them off the serial
 *       line without parsing anything else the port carries.
 */
#ifndef MMGR_PERF_DEVICE_BENCH_H
#define MMGR_PERF_DEVICE_BENCH_H

#include <stdint.h>
#include <stdio.h>

#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief The clock the cycle counts are taken against, in MHz.
 *
 * @note Supplied per target by common.ini, since the two parts differ. Overriding it reports against
 *       a rate the part is not running at, which is only ever what a comparison wants.
 */
#ifndef MMGR_BENCH_CPU_MHZ
#define MMGR_BENCH_CPU_MHZ 240u
#endif

/**
 * @brief One cycle-counter read.
 */
#define DBENCH_CYCLE_READ(dst) ((dst) = (uint32_t)esp_cpu_get_cycle_count())

/**
 * @brief Where a timed expression's result goes so the call cannot be deleted.
 *
 * @note volatile, and written every iteration. A result that is computed and dropped is a call the
 *       optimiser may remove outright, which reports the removal rather than the work. This bit the
 *       first build of this bench: gcc refused the discarded libc results, and had it not, the libc
 *       arm would have measured an empty loop.
 */
extern volatile uintptr_t g_dbench_sink;

/**
 * @brief Keeps one value, whatever its type.
 */
#define DBENCH_KEEP(v) (g_dbench_sink = (uintptr_t)(v))

/**
 * @brief One RTOS tick between reported lines, so the serial line keeps up.
 */
#define DBENCH_SETTLE() vTaskDelay(1)

/**
 * @brief Warms once, runs N iterations, leaves the mean cycle count in out_cy.
 *
 * @note The warm pass is not counted. It pulls the code and the fixture into cache, so what follows
 *       measures the loop rather than the first miss.
 */
#define DBENCH_CYCLES(N, expr, out_cy)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        expr; /* warm */                                                                                               \
        uint32_t c0_, c1_;                                                                                             \
        DBENCH_CYCLE_READ(c0_);                                                                                        \
        for (uint32_t i_ = 0; i_ < (uint32_t)(N); i_++)                                                                \
        {                                                                                                              \
            expr;                                                                                                      \
        }                                                                                                              \
        DBENCH_CYCLE_READ(c1_);                                                                                        \
        (out_cy) = (double)(c1_ - c0_) / (double)(N);                                                                  \
    } while (0)

/**
 * @brief One A/B row: the same work through the library and through libc, over the same bytes.
 *
 * @note Prints both arms and the ratio on one line, because the ratio is the reading and computing it
 *       from two lines by hand is where a comparison goes wrong.
 */
#define DBENCH_AB(label, N, bytes, mmgr_expr, libc_expr)                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        double m_ = 0.0;                                                                                               \
        double l_ = 0.0;                                                                                               \
        DBENCH_CYCLES(N, mmgr_expr, m_);                                                                                \
        DBENCH_CYCLES(N, libc_expr, l_);                                                                                \
        printf("DB %-16s n=%-6u mmgr=%-10.1f libc=%-10.1f ratio=%.2f  mmgr_c/B=%.3f libc_c/B=%.3f\n", label,           \
               (unsigned)(bytes), m_, l_, (l_ > 0.0) ? (m_ / l_) : 0.0, m_ / (double)(bytes),                          \
               l_ / (double)(bytes));                                                                                  \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

/**
 * @brief One measurement of a single expression, for a cost that has no libc counterpart.
 */
#define DBENCH_OP(label, N, expr)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        DBENCH_CYCLES(N, expr, cy_);                                                                                   \
        printf("DB %-16s cyc=%.2f\n", label, cy_);                                                                     \
        DBENCH_SETTLE();                                                                                                \
    } while (0)

/**
 * @brief Start-of-pass line, naming the feature, the instruction set and the clock.
 */
#if defined(__XTENSA__)
#define DBENCH_ISA "xtensa"
#elif defined(__riscv)
#define DBENCH_ISA "riscv"
#else
#define DBENCH_ISA "unknown"
#endif

#define DBENCH_BANNER(label)                                                                                           \
    printf("DB ==== " label " (" DBENCH_ISA ", %u MHz) ====\n", (unsigned)MMGR_BENCH_CPU_MHZ)

/**
 * @brief Prepares the fixtures and loops the timed operations.
 */
void dbench_run(void);

#endif
