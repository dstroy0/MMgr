// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
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
            const mmgr_scrut_word w_ = mmgr_scrut_load(g_buf + r_);                                                    \
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
    SWEEP("has_zero", mmgr_scrut_has_zero(w_));
    SWEEP("eq", mmgr_scrut_eq(w_, (mmgr_scrut_word)0x5Au));
    SWEEP("le", mmgr_scrut_le(w_, (mmgr_scrut_word)0x7Fu));
    SWEEP("spread", mmgr_scrut_spread(w_));

    return 0;
}
