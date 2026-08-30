#ifndef MMGR_PERF_DEVICE_BENCH_H
#define MMGR_PERF_DEVICE_BENCH_H

#include <stdint.h>
#include <stdio.h>

#if defined(__XTENSA__) || defined(__riscv) || defined(__IMXRT1062__)
#define MMGR_BENCH_SILICON 1
#else
#define MMGR_BENCH_SILICON 0
#endif

#if defined(__XTENSA__) || defined(__riscv)
#define MMGR_BENCH_ESP 1
#else
#define MMGR_BENCH_ESP 0
#endif

#if defined(__IMXRT1062__)
#define MMGR_BENCH_TEENSY 1
#else
#define MMGR_BENCH_TEENSY 0
#endif

#if MMGR_BENCH_ESP
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#elif MMGR_BENCH_TEENSY

extern void delay(uint32_t ms);
extern void yield(void);
extern uint32_t millis(void);

#define DBENCH_ARM_DEMCR (*(volatile uint32_t *)0xE000EDFCu)

#define DBENCH_ARM_DEMCR_TRCENA (1u << 24)

#define DBENCH_ARM_DWT_CTRL (*(volatile uint32_t *)0xE0001000u)

#define DBENCH_ARM_DWT_CTRL_CYCCNTENA (1u << 0)

#define DBENCH_ARM_DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)

extern int dbench_printf(const char *fmt, ...);
extern void dbench_flush(void);
#else
#include <stdlib.h>
#include <time.h>
#endif

#if MMGR_BENCH_TEENSY
#define DBENCH_PRINTF dbench_printf
#define DBENCH_FLUSH() dbench_flush()
#else
#define DBENCH_PRINTF printf
#define DBENCH_FLUSH() fflush(stdout)
#endif

#ifndef DBENCH_CPU_MHZ
#define DBENCH_CPU_MHZ 240u
#endif

#if MMGR_BENCH_ESP

#define DBENCH_CYCLE_READ(dst) ((dst) = (uint32_t)esp_cpu_get_cycle_count())
#elif MMGR_BENCH_TEENSY

static inline void dbench_teensy_counter_start(void)
{
    DBENCH_ARM_DEMCR |= DBENCH_ARM_DEMCR_TRCENA;
    DBENCH_ARM_DWT_CTRL |= DBENCH_ARM_DWT_CTRL_CYCCNTENA;
    DBENCH_ARM_DWT_CYCCNT = 0;
}

#define DBENCH_CYCLE_READ(dst) ((dst) = (uint32_t)DBENCH_ARM_DWT_CYCCNT)
#else

static inline uint32_t dbench_host_cycles(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const double ns = ((double)ts.tv_sec * 1e9) + (double)ts.tv_nsec;

    return (uint32_t)(ns * ((double)DBENCH_CPU_MHZ / 1000.0));
}

#define DBENCH_CYCLE_READ(dst) ((dst) = dbench_host_cycles())
#endif

extern volatile uintptr_t g_dbench_sink;

#define DBENCH_KEEP(v) (g_dbench_sink = (uintptr_t)(v))

#if MMGR_BENCH_ESP
#define DBENCH_SETTLE() vTaskDelay(1)
#elif MMGR_BENCH_TEENSY
#define DBENCH_SETTLE() yield()
#else
#define DBENCH_SETTLE() ((void)0)
#endif

#define DBENCH_CYCLES(N, expr, out_cy)                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        expr;                                                                                                \
        uint32_t _c0, _c1;                                                                                             \
        DBENCH_CYCLE_READ(_c0);                                                                                        \
        for (uint32_t _i = 0; _i < (uint32_t)(N); _i++)                                                                \
        {                                                                                                              \
            expr;                                                                                                      \
        }                                                                                                              \
        DBENCH_CYCLE_READ(_c1);                                                                                        \
        (out_cy) = (double)(_c1 - _c0) / (double)(N);                                                                  \
    } while (0)

#define DBENCH_OP(label, N, expr)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        double _cy = 0.0;                                                                                              \
        DBENCH_CYCLES(N, expr, _cy);                                                                                   \
        DBENCH_PRINTF("DB %-30s cyc=%-11.2f us=%-9.2f ns=%.0f\n", label, _cy, _cy / (double)DBENCH_CPU_MHZ,                   \
               (_cy * 1000.0) / (double)DBENCH_CPU_MHZ);                                                               \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

#define DBENCH_BULK(label, N, bytes, expr)                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        double _cy = 0.0;                                                                                              \
        DBENCH_CYCLES(N, expr, _cy);                                                                                   \
        double _nspb = ((_cy * 1000.0) / (double)DBENCH_CPU_MHZ) / (double)(bytes);                                    \
        double _mbs = (_nspb > 0.0) ? (1000.0 / _nspb) : 0.0;                                                          \
        DBENCH_PRINTF("DB %-30s cyc=%-11.0f ns/B=%-8.2f MB/s=%-8.1f (%uB)\n", label, _cy, _nspb, _mbs, (unsigned)(bytes));    \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

#define DBENCH_AB(label, N, bytes, mmgr_expr, libc_expr)                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        double _m = 0.0;                                                                                               \
        double _l = 0.0;                                                                                               \
        DBENCH_CYCLES(N, mmgr_expr, _m);                                                                               \
        DBENCH_CYCLES(N, libc_expr, _l);                                                                               \
        DBENCH_PRINTF("DB %-16s n=%-6u mmgr=%-10.1f libc=%-10.1f ratio=%.2f  mmgr_c/B=%.3f libc_c/B=%.3f\n", label,           \
               (unsigned)(bytes), _m, _l, (_l > 0.0) ? (_m / _l) : 0.0, _m / (double)(bytes), _l / (double)(bytes));   \
        DBENCH_SETTLE();                                                                                               \
    } while (0)

#if MMGR_BENCH_ESP
#if defined(__XTENSA__)
#define DBENCH_COUNTER "CCOUNT, xtensa"
#else
#define DBENCH_COUNTER "mcycle, riscv"
#endif
#elif MMGR_BENCH_TEENSY
#define DBENCH_COUNTER "DWT CYCCNT, cortex-m7"
#else
#define DBENCH_COUNTER "host clock"
#endif

#define DBENCH_BANNER(label)                                                                                           \
    DBENCH_PRINTF("DB ==== " label " microbench start (" DBENCH_COUNTER " @ %u MHz) ====\n", (unsigned)DBENCH_CPU_MHZ)

#if MMGR_BENCH_ESP
#define DBENCH_DONE()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        DBENCH_PRINTF("DB ==== DONE ====\n");                                                                                 \
        vTaskDelay(5000 / portTICK_PERIOD_MS);                                                                         \
    } while (0)
#elif MMGR_BENCH_TEENSY
#define DBENCH_DONE()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        DBENCH_PRINTF("DB ==== DONE ====\n");                                                                                 \
        DBENCH_FLUSH();                                                                                                \
        delay(5000);                                                                                                   \
    } while (0)
#else
#define DBENCH_DONE()                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        DBENCH_PRINTF("DB ==== DONE ====\n");                                                                                 \
        DBENCH_FLUSH();                                                                                                \
        exit(0);                                                                                                       \
    } while (0)
#endif

void dbench_run(void);

#if MMGR_BENCH_ESP
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
        DBENCH_PRINTF("\nDB boot: " label " device microbench\n");                                                            \
        xTaskCreatePinnedToCore(dbench_task, "dbench", 16384, NULL, 24, NULL, portNUM_PROCESSORS - 1);                 \
    }
#elif MMGR_BENCH_TEENSY

#define DBENCH_MAIN(label)                                                                                             \
    volatile uintptr_t g_dbench_sink;                                                                                  \
    void setup(void)                                                                                                   \
    {                                                                                                                  \
        delay(2500);                                                                                                   \
        dbench_teensy_counter_start();                                                                                 \
        DBENCH_PRINTF("\nDB boot: " label " device microbench\n");                                                            \
    }                                                                                                                  \
    void loop(void)                                                                                                    \
    {                                                                                                                  \
        dbench_run();                                                                                                  \
    }
#else
#define DBENCH_MAIN(label)                                                                                             \
    volatile uintptr_t g_dbench_sink;                                                                                  \
    int main(void)                                                                                                     \
    {                                                                                                                  \
        DBENCH_PRINTF("\nDB boot: " label " host microbench\n");                                                              \
        dbench_run();                                                                                                  \
        return 0;                                                                                                      \
    }
#endif

#endif
