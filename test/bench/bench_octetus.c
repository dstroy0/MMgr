/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdio.h>

#include "bench_harness.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "spatium/spatium.h"
#include "memoria_operor/memoria_operor.h"

#define ITERS 5000000ul
#define SPREAD 256u
#define ROOM (SPREAD + 64u)

static uint64_t g_store[ROOM / 8u];
static uint64_t g_srcstore[8];

#define PUT(WIDTH)                                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        uint8_t *const mem_ = (uint8_t *)g_store;                                                                      \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u)) & ~(size_t)7u;                                      \
            mmgr_span w_ = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem_ + off_, .cap = 8u);                                                                 \
            MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w_, .value = (uint64_t)bench_i_,                       \
                      .bytes = (size_t)(WIDTH));                                                                       \
            BENCH_KEEP(mem_[off_]);                                                                                    \
        });                                                                                                            \
        printf("octetus,put,%u,%.4f\n", (unsigned)(WIDTH), cy_);                                                       \
        fflush(stdout);                                                                                                \
    } while (0)

#define TAKE(WIDTH)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint8_t *const mem_ = (const uint8_t *)g_store;                                                          \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u)) & ~(size_t)7u;                                      \
            uint64_t out_ = 0;                                                                                         \
            mmgr_cspan r_ = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = mem_ + off_, .cap = 8u);                                                               \
            BENCH_KEEP(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r_, .out = &out_,                           \
                                 .bytes = (size_t)(WIDTH)));                                                           \
            BENCH_KEEP(out_);                                                                                          \
        });                                                                                                            \
        printf("octetus,take,%u,%.4f\n", (unsigned)(WIDTH), cy_);                                                      \
        fflush(stdout);                                                                                                \
    } while (0)

int main(void)
{
    uint8_t *const mem = (uint8_t *)g_store;
    uint8_t *const src = (uint8_t *)g_srcstore;

    for (unsigned i = 0; i < ROOM; i++)
    {
        mem[i] = (uint8_t)(i * 7u + 1u);
    }
    for (unsigned i = 0; i < sizeof g_srcstore; i++)
    {
        src[i] = (uint8_t)i;
    }

    PUT(1);
    PUT(2);
    PUT(4);
    PUT(8);

    TAKE(1);
    TAKE(2);
    TAKE(4);
    TAKE(8);

    {
        double cy_ = 0.0;
        BENCH_TIME_CYCLES(cy_, ITERS, {
            const size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u)) & ~(size_t)7u;
            MMGR_CALL(memor.cpy, MemoriaCfg, .dst = mem + off_, .src = src, .bytes = (size_t)16);
            BENCH_KEEP(mem[off_]);
        });
        printf("octetus,cpy,16,%.4f\n", cy_);
        fflush(stdout);
    }
    return 0;
}
