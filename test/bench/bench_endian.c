/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdio.h>

#include "bench_harness.h"

#include "endian/endian.h"

#define ITERS 5000000ul
#define SPREAD 256u

static uint8_t g_buf[SPREAD + 64u];

#define SWEEP(LABEL, WIDTH, EXPR)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t off_ = (size_t)(bench_i_ & (SPREAD - 1u));                                                    \
            const uint64_t r_ = (uint64_t)(EXPR);                                                                      \
            BENCH_KEEP(r_);                                                                                            \
        });                                                                                                            \
        printf("endian,%s,%u,%u,%.4f\n", LABEL, (unsigned)(WIDTH), MMGR_WORD_BITS, cy_);                                \
        fflush(stdout);                                                                                                \
    } while (0)

int main(void)
{
    for (unsigned i = 0; i < sizeof g_buf; i++)
    {
        g_buf[i] = (uint8_t)(i * 7u + 1u);
    }

    SWEEP("wr_parva", 16, parva_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_16}));
    SWEEP("wr_parva", 32, parva_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_32}));
    SWEEP("wr_parva", 64, parva_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_64}));

    SWEEP("wr_magna", 16, magna_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_16}));
    SWEEP("wr_magna", 32, magna_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_32}));
    SWEEP("wr_magna", 64, magna_extremitas.wr(&(EndianCfg){g_buf + off_, 0, (uint64_t)bench_i_, MMGR_ENDIAN_64}));

    SWEEP("rd_parva", 16, parva_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_16}));
    SWEEP("rd_parva", 32, parva_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_32}));
    SWEEP("rd_parva", 64, parva_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_64}));

    SWEEP("rd_magna", 16, magna_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_16}));
    SWEEP("rd_magna", 32, magna_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_32}));
    SWEEP("rd_magna", 64, magna_extremitas.rd(&(EndianCfg){0, g_buf + off_, 0, MMGR_ENDIAN_64}));

    return 0;
}
