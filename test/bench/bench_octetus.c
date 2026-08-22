// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// What a byte field costs, through the table a caller uses.
//
// The widths are what the dispatch has to sort out, so each is timed on its own rather than mixed:
// a mean over a spread of widths hides which one the entry is actually good at, and the question
// here is whether telling the entry its width by name costs more or less than telling it in a byte.
#include <stdio.h>

#include "bench_harness.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#define ITERS 5000000ul
#define SPREAD 256u

static uint8_t g_mem[SPREAD + 64u];
static uint8_t g_src[64];

#define SWEEP(LABEL, WIDTH, SETUP, EXPR)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                    \
            SETUP;                                                                                                     \
            EXPR;                                                                                                      \
            BENCH_KEEP(w_.pos);                                                                                        \
        });                                                                                                            \
        printf("octetus,%s,%u,%.4f\n", LABEL, (unsigned)(WIDTH), cy_);                                                  \
        fflush(stdout);                                                                                                \
    } while (0)

#define READ(LABEL, WIDTH, EXPR)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                          \
            uint64_t out_ = 0;                                                                                         \
            EXPR;                                                                                                      \
            BENCH_KEEP(out_);                                                                                          \
        });                                                                                                            \
        printf("octetus,%s,%u,%.4f\n", LABEL, (unsigned)(WIDTH), cy_);                                                  \
        fflush(stdout);                                                                                                \
    } while (0)

int main(void)
{
    for (unsigned i = 0; i < sizeof g_mem; i++)
    {
        g_mem[i] = (uint8_t)(i * 7u + 1u);
    }
    for (unsigned i = 0; i < sizeof g_src; i++)
    {
        g_src[i] = (uint8_t)i;
    }

    SWEEP("put", 1, mmgr_spat w_ = spat.init(&(SpatCfg){g_mem, sizeof g_mem}),
          byteio.put(&w_, (uint8_t)bench_i_));

    SWEEP("put_be", 2, mmgr_spat w_ = spat.init(&(SpatCfg){g_mem, sizeof g_mem}),
          byteio.put_be(&w_, (uint64_t)bench_i_, 2));
    SWEEP("put_be", 4, mmgr_spat w_ = spat.init(&(SpatCfg){g_mem, sizeof g_mem}),
          byteio.put_be(&w_, (uint64_t)bench_i_, 4));
    SWEEP("put_be", 8, mmgr_spat w_ = spat.init(&(SpatCfg){g_mem, sizeof g_mem}),
          byteio.put_be(&w_, (uint64_t)bench_i_, 8));

    SWEEP("raw", 16, mmgr_spat w_ = spat.init(&(SpatCfg){g_mem, sizeof g_mem}),
          byteio.raw(&w_, g_src, 16));

    READ("take_be", 2, byteio.take_be(g_mem, sizeof g_mem, &off_, &out_, 2));
    READ("take_be", 4, byteio.take_be(g_mem, sizeof g_mem, &off_, &out_, 4));
    READ("take_be", 8, byteio.take_be(g_mem, sizeof g_mem, &off_, &out_, 8));

    {
        double cy_ = 0.0;
        BENCH_TIME_CYCLES(cy_, ITERS, {
            size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u));
            uint32_t out_ = 0;
            byteio.rd_u32(g_mem, sizeof g_mem, &off_, &out_);
            BENCH_KEEP(out_);
        });
        printf("octetus,rd_u32,4,%.4f\n", cy_);
        fflush(stdout);
    }
    return 0;
}
