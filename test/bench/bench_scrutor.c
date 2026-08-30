#include <stdio.h>
#include <string.h>

#include "bench_harness.h"

#include "verbum_scrutor/verbum_scrutor.h"

#define SPREAD 4096u
#define ITERS 2000000ul

static uint8_t g_buf[SPREAD + 64u];

static void bench_nop(void)
{
    double cy = 0.0;
    BENCH_TIME_CYCLES(cy, ITERS, {
        const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));
        const uint64_t v_ = (uint64_t)r_;
        BENCH_KEEP(v_);
    });
    printf("scrut,nop,%u,%.4f,%.4f\n", MMGR_SWAR_BITS, cy, cy / (double)MMGR_SWAR_BYTES);
    fflush(stdout);
}

#define SWEEP(LABEL, EXPR)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t r_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                      \
            const mmgr_word w_ = MMGR_CALL(word.load, ScrutWordCfg, .at = g_buf + r_);                                 \
            const uint64_t v_ = (uint64_t)(EXPR);                                                                      \
            BENCH_KEEP(v_);                                                                                            \
        });                                                                                                            \
        printf("scrut,%s,%u,%.4f,%.4f\n", LABEL, MMGR_SWAR_BITS, cy_, cy_ / (double)MMGR_SWAR_BYTES);                  \
        fflush(stdout);                                                                                                \
    } while (0)

int main(void)
{

    memset(g_buf, 0xA5, sizeof g_buf);

    printf("module,case,lane_bits,cycles_per_call,cycles_per_byte\n");
    fflush(stdout);

    bench_nop();
    SWEEP("load", w_);
    SWEEP("has_zero", MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w_));
    SWEEP("eq", MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w_, .byte = 0x5Au));
    SWEEP("le", MMGR_CALL(lane.le, ScrutLaneCfg, .word = w_, .byte = 0x7Fu));
    SWEEP("spread", MMGR_CALL(mask.spread, ScrutMaskCfg, .mask = w_));

    return 0;
}
