/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdio.h>

#include "bench_harness.h"

#include "mmgr/ring/ring.h"

#include "memoria_anularis/memoria_anularis.h"

#define ITERS 200000ul
#define CAP 65536u
#define NSEGS 8u

static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_proto_buf[CAP];
static _Atomic size_t g_proto_head;
static _Atomic size_t g_proto_tail;

static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_mmgr_buf[CAP];
static mmgr_ring g_mmgr_ring;

static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_src[512];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_dst[512];

static unsigned long g_refused;

/**
 * @brief Prints one row, turning cycles per call into throughput.
 *
 * @note A case that moves no bytes reports rate alone, since there is no byte count to divide by.
 * @note refused is carried through so a run whose calls were turned away cannot read as throughput.
 */
static void report(const char *impl, const char *name, size_t bytes, double cycles)
{
    const double ops = bench_cycles_per_s() / cycles;

    if (bytes == 0u)
    {
        printf("%s,%s,0,%.3f,,%lu\n", impl, name, ops / 1e6, g_refused);
    }
    else
    {
        printf("%s,%s,%u,%.3f,%.1f,%lu\n", impl, name, (unsigned)bytes, ops / 1e6, ((double)bytes * ops) / 1e6,
               g_refused);
    }
    fflush(stdout);
    g_refused = 0ul;
}

static void mmgr_reset(void)
{
    (void)MMGR_CALL(anularis.init, AnularisCfg, .ring = &g_mmgr_ring, .buf = g_mmgr_buf, .capacity = CAP,
                    .segment_count = NSEGS);
}

static void proto_reset(void)
{
    PROTO_ATOMIC_STORE(&g_proto_head, (size_t)0);
    PROTO_ATOMIC_STORE(&g_proto_tail, (size_t)0);
}

/**
 * @brief Fills the ring to just short of full, so a drain case measures draining alone.
 */
static void mmgr_fill(void)
{
    while (MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_mmgr_ring, .src = g_src, .bytes = 512u))
    {
    }
}

#define AB_PUT(N)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        proto_reset();                                                                                                 \
        g_refused = 0ul;                                                                                               \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            if (protocore_ring_free(&g_proto_head, &g_proto_tail, CAP) >= (N))                                         \
            {                                                                                                          \
                const size_t h_ =                                                                                      \
                    protocore_ring_write_span(g_proto_buf, CAP, PROTO_ATOMIC_LOAD(&g_proto_head), g_src, (N));         \
                PROTO_ATOMIC_STORE(&g_proto_head, h_);                                                                 \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                g_refused++;                                                                                           \
            }                                                                                                          \
            protocore_ring_consume(&g_proto_tail, CAP, (N));                                                           \
            BENCH_KEEP(g_proto_buf[bench_i_ & (CAP - 1u)]);                                                            \
        });                                                                                                            \
        report("protocore", "put", (N), cy_);                                                                          \
                                                                                                                       \
        mmgr_reset();                                                                                                  \
        g_refused = 0ul;                                                                                               \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            if (!MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_mmgr_ring, .src = g_src, .bytes = (N)))               \
            {                                                                                                          \
                g_refused++;                                                                                           \
            }                                                                                                          \
            MMGR_CALL(anularis.consume, AnularisCfg, .ring = &g_mmgr_ring, .bytes = (N));                              \
            BENCH_KEEP(g_mmgr_buf[bench_i_ & (CAP - 1u)]);                                                             \
        });                                                                                                            \
        report("mmgr", "put", (N), cy_);                                                                               \
    } while (0)

#define AB_PEEK(N)                                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            protocore_ring_peek(g_proto_buf, CAP, &g_proto_tail, (size_t)(bench_i_ & 63u), g_dst, (N));                \
            BENCH_KEEP(g_dst[0]);                                                                                      \
        });                                                                                                            \
        report("protocore", "peek", (N), cy_);                                                                         \
                                                                                                                       \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            MMGR_CALL(anularis.peek, AnularisCfg, .ring = &g_mmgr_ring, .dst = g_dst, .bytes = (N),                    \
                      .offset = (size_t)(bench_i_ & 63u));                                                             \
            BENCH_KEEP(g_dst[0]);                                                                                      \
        });                                                                                                            \
        report("mmgr", "peek", (N), cy_);                                                                              \
                                                                                                                       \
        proto_reset();                                                                                                 \
        PROTO_ATOMIC_STORE(&g_proto_head, (size_t)(CAP - 1u));                                                         \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            if (protocore_ring_read(g_proto_buf, CAP, &g_proto_head, &g_proto_tail, g_dst, (N)) == 0u)                 \
            {                                                                                                          \
                PROTO_ATOMIC_STORE(&g_proto_head, (size_t)(CAP - 1u));                                                 \
                PROTO_ATOMIC_STORE(&g_proto_tail, (size_t)0);                                                          \
            }                                                                                                          \
            BENCH_KEEP(g_dst[0]);                                                                                      \
        });                                                                                                            \
        report("protocore", "read", (N), cy_);                                                                         \
                                                                                                                       \
        mmgr_reset();                                                                                                  \
        mmgr_fill();                                                                                                   \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            if (MMGR_CALL(anularis.read, AnularisCfg, .ring = &g_mmgr_ring, .dst = g_dst, .bytes = (N)) == 0u)         \
            {                                                                                                          \
                mmgr_reset();                                                                                          \
                mmgr_fill();                                                                                           \
            }                                                                                                          \
            BENCH_KEEP(g_dst[0]);                                                                                      \
        });                                                                                                            \
        report("mmgr", "read", (N), cy_);                                                                              \
    } while (0)

int main(void)
{
    for (unsigned i = 0; i < sizeof g_src; i++)
    {
        g_src[i] = (uint8_t)(i * 7u + 1u);
    }

    (void)bench_cycles_per_s();

    proto_reset();
    mmgr_reset();

    printf("impl,case,bytes_per_op,mops_per_s,mb_per_s,refused\n");
    fflush(stdout);

    {
        double cy = 0.0;

        BENCH_TIME_CYCLES(cy, ITERS, { BENCH_KEEP(bench_i_); });
        printf("harness,nop,0,%.3f,,0\n", (bench_cycles_per_s() / cy) / 1e6);
        fflush(stdout);

        BENCH_TIME_CYCLES(cy, ITERS, { BENCH_KEEP(protocore_ring_available(&g_proto_head, &g_proto_tail, CAP)); });
        report("protocore", "available", 0u, cy);

        BENCH_TIME_CYCLES(cy, ITERS, { BENCH_KEEP(MMGR_CALL(anularis.available, AnularisCfg, .ring = &g_mmgr_ring)); });
        report("mmgr", "available", 0u, cy);
    }

    AB_PUT(64u);
    AB_PUT(512u);

    AB_PEEK(64u);
    AB_PEEK(512u);

    {
        double cy = 0.0;
        uint8_t b = 0;

        proto_reset();
        PROTO_ATOMIC_STORE(&g_proto_head, (size_t)(CAP - 1u));
        BENCH_TIME_CYCLES(cy, ITERS, {
            if (!protocore_ring_read_byte(g_proto_buf, CAP, &g_proto_head, &g_proto_tail, &b))
            {
                PROTO_ATOMIC_STORE(&g_proto_head, (size_t)(CAP - 1u));
                PROTO_ATOMIC_STORE(&g_proto_tail, (size_t)0);
            }
            BENCH_KEEP(b);
        });
        report("protocore", "read_byte", 1u, cy);

        mmgr_reset();
        (void)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_mmgr_ring, .src = g_src, .bytes = 512u);
        BENCH_TIME_CYCLES(cy, ITERS, {
            if (!MMGR_CALL(anularis.read_byte, AnularisCfg, .ring = &g_mmgr_ring, .dst = &b))
            {
                mmgr_reset();
                (void)MMGR_CALL(anularis.put, AnularisCfg, .ring = &g_mmgr_ring, .src = g_src, .bytes = 512u);
            }
            BENCH_KEEP(b);
        });
        report("mmgr", "read_byte", 1u, cy);
    }

    return 0;
}
