#include <stdio.h>
#include <string.h>

#include "bench_harness.h"

#include "memoria_operor/memoria_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

#define SPREAD 65536u
#define ITERS 500000ul

#define FILLER 0xA5u
#define TARGET 0x5Au

static uint8_t g_buf[SPREAD + 128u];

static size_t find_bytewise(const uint8_t *p, size_t n, uint8_t c)
{
    for (size_t i = 0; i < n; i++)
    {
        if (p[i] == c)
        {
            return i;
        }
    }
    return n;
}

static size_t find_swar(const uint8_t *p, size_t n, uint8_t c)
{
    const size_t step = MMGR_SWAR_BYTES;
    size_t i = 0;
    while (i + step <= n)
    {
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = p + i);
        const mmgr_word m = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = c, .ci = MMGR_FALSE);
        if (m)
        {
            return i + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
        i += step;
    }
    for (; i < n; i++)
    {
        if (p[i] == c)
        {
            return i;
        }
    }
    return n;
}

#define SWEEP(STRIDE)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        memset(g_buf, FILLER, sizeof g_buf);                                                                           \
        for (size_t k = (STRIDE) - 1u; k < sizeof g_buf; k += (STRIDE))                                                \
        {                                                                                                              \
            g_buf[k] = TARGET;                                                                                         \
        }                                                                                                              \
        const double avg_ = (double)(STRIDE) / 2.0;                                                                    \
        double cb_ = 0.0;                                                                                              \
        double cs_ = 0.0;                                                                                              \
        double cm_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cb_, ITERS, {                                                                                \
            const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                      \
            const uint64_t v_ = (uint64_t)find_bytewise(g_buf + r_, (STRIDE) + 8u, TARGET);                            \
            BENCH_KEEP(v_);                                                                                            \
        });                                                                                                            \
        BENCH_TIME_CYCLES(cs_, ITERS, {                                                                                \
            const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                      \
            const uint64_t v_ = (uint64_t)find_swar(g_buf + r_, (STRIDE) + 8u, TARGET);                                \
            BENCH_KEEP(v_);                                                                                            \
        });                                                                                                            \
                                                                                                                       \
        BENCH_TIME_CYCLES(cm_, ITERS, {                                                                                \
            const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                      \
            const void *q_ = MMGR_CALL(memor.chr, MemoriaCfg, .src = g_buf + r_, .bytes = (size_t)((STRIDE) + 8u),     \
                                       .val = (uint8_t)TARGET);                                                        \
            BENCH_KEEP(q_);                                                                                            \
        });                                                                                                            \
        printf("eq_strategy,%u,%u,%.1f,%.4f,%.4f,%.4f,%.4f,%s\n", MMGR_SWAR_BITS, (unsigned)(STRIDE), avg_, cb_, cs_,  \
               cm_, cb_ / cm_, (cb_ < cm_) ? "bytewise" : "memor.chr");                                                \
        fflush(stdout);                                                                                                \
    } while (0)

int main(void)
{
    printf("bench,lane_bits,stride,avg_dist,cyc_bytewise,cyc_swar,cpb_bytewise,cpb_swar,winner\n");
    fflush(stdout);

    SWEEP(2u);
    SWEEP(4u);
    SWEEP(8u);
    SWEEP(16u);
    SWEEP(32u);
    SWEEP(64u);
    SWEEP(256u);
    SWEEP(1024u);

    return 0;
}
