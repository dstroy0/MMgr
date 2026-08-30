/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief locus_carcerum against ProtoCore's arena, span, secure and plaintext, each through its own
 *        API.
 *
 * @note Built against ProtoCore's real sources rather than a copy, so what is measured is its code
 *       and not a transcription of it.
 *
 * Three of these rows are not like for like, and the report says so rather than pretending:
 *
 *  - ProtoCore's persist_alloc zeroes the payload it hands back; locus_carcerum does not, because it
 *    clears on release instead. The alloc rows therefore price different work, and the release rows
 *    are where locus_carcerum pays what ProtoCore already paid.
 *  - ProtoCore's scratch end is a pure bump with no header. The temporary tier in locus_carcerum is
 *    the same cell allocator as its persistent tier, so it carries a header and looks for a fit
 *    first. The temporary rows are what that unification costs.
 *  - ProtoCore's secure and plaintext arenas resolve the calling worker's seat themselves and take
 *    no arena argument. locus_carcerum is handed the cellblock, because the cellblock address is the
 *    identity. The call shapes differ by one argument and a lookup.
 */
#include <stdio.h>

#include "bench_harness.h"

#include "locus_carcerum/locus_carcerum.h"
#include "spatium/spatium.h"

// span.h is reached through these rather than named here. A quoted include searches the including
// file's own directory first, and test/bench carries a span shim for the ring A/B, so naming it
// here would pull the shim in beside ProtoCore's own and declare protocore_cspan twice.
#include "mmgr/arena/arena.h"
#include "mmgr/plaintext/plaintext.h"
#include "mmgr/secure/secure.h"

#define ITERS 200000ul
#define ARENA_BYTES 65536u
#define CHAIN 16u

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(general, ARENA_BYTES));

static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_proto_bytes[ARENA_BYTES];
static protocore_arena g_arena;

static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_buf[1024];

/**
 * @brief Prints one row, turning cycles per call into a rate.
 *
 * @note bytes is the payload a case moved, where it moved one; a case that only walks pointers
 *       reports its rate alone.
 */
static void report(const char *impl, const char *name, size_t bytes, double cycles)
{
    const double ops = bench_cycles_per_s() / cycles;

    if (bytes == 0u)
    {
        printf("%s,%s,0,%.3f,\n", impl, name, ops / 1e6);
    }
    else
    {
        printf("%s,%s,%u,%.3f,%.1f\n", impl, name, (unsigned)bytes, ops / 1e6, ((double)bytes * ops) / 1e6);
    }
    fflush(stdout);
}

static void carcer_fresh(void)
{
    ram_general_ctx.persistent_end = 0u;
    ram.general.temporary_buf_reset();
}

static void proto_fresh(void)
{
    protocore_arena_init(&g_arena, g_proto_bytes, sizeof g_proto_bytes);
}

/**
 * @brief One take and one give back at each end, which is the shape a dispatch has.
 */
#define AB_PERSIST(N)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        proto_fresh();                                                                                                 \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            void *p_ = protocore_arena_persist_alloc(&g_arena, (N));                                                   \
            BENCH_KEEP(p_ != NULL);                                                                                    \
            protocore_arena_persist_free(&g_arena, p_);                                                                \
        });                                                                                                            \
        report("protocore", "persist alloc+free", (N), cy_);                                                           \
                                                                                                                       \
        carcer_fresh();                                                                                                \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            void *p_ = ram.general.persistent_buf_alloc((N));                                                          \
            BENCH_KEEP(p_ != NULL);                                                                                    \
            ram.general.persistent_buf_release(p_);                                                                    \
        });                                                                                                            \
        report("locus_carcerum", "persist alloc+free", (N), cy_);                                                      \
    } while (0)

/**
 * @brief A run of takes, then the whole run given back at once, which is what a mark is for.
 */
#define AB_SCRATCH(N)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        proto_fresh();                                                                                                 \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t m_ = protocore_arena_scratch_mark(&g_arena);                                                  \
            for (unsigned k_ = 0; k_ < CHAIN; k_++)                                                                    \
            {                                                                                                          \
                BENCH_KEEP(protocore_arena_scratch_alloc(&g_arena, (N)) != NULL);                                      \
            }                                                                                                          \
            protocore_arena_scratch_release(&g_arena, m_);                                                             \
        });                                                                                                            \
        report("protocore", "transient run+release", (N) * CHAIN, cy_);                                                \
                                                                                                                       \
        carcer_fresh();                                                                                                \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t m_ = ram.general.temporary_buf_mark();                                                        \
            for (unsigned k_ = 0; k_ < CHAIN; k_++)                                                                    \
            {                                                                                                          \
                BENCH_KEEP(ram.general.temporary_buf_alloc((N)) != NULL);                                              \
            }                                                                                                          \
            ram.general.temporary_buf_release(m_);                                                                     \
        });                                                                                                            \
        report("locus_carcerum", "transient run+release", (N) * CHAIN, cy_);                                           \
    } while (0)

/**
 * @brief The wipe both call a guarantee, at the same extent.
 */
#define AB_WIPE(N)                                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            protocore_secure_wipe(g_buf, (N));                                                                         \
            BENCH_KEEP(g_buf[0]);                                                                                      \
        });                                                                                                            \
        report("protocore", "wipe", (N), cy_);                                                                         \
                                                                                                                       \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            mmgr_zero_buf(g_buf, (N));                                                                                 \
            BENCH_KEEP(g_buf[0]);                                                                                      \
        });                                                                                                            \
        report("locus_carcerum", "zero", (N), cy_);                                                                    \
    } while (0)

/**
 * @brief A take and a wiped give back, which is the whole point of the secure end.
 */
#define AB_SECURE(N)                                                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t m_ = protocore_secure_mark();                                                                 \
            BENCH_KEEP(protocore_secure_alloc((N), sizeof(void *)) != NULL);                                           \
            protocore_secure_release(m_);                                                                              \
        });                                                                                                            \
        report("protocore", "secure take+wiped release", (N), cy_);                                                    \
                                                                                                                       \
        carcer_fresh();                                                                                                \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            void *p_ = ram.general.persistent_buf_alloc((N));                                                          \
            BENCH_KEEP(p_ != NULL);                                                                                    \
            ram.general.persistent_buf_release(p_);                                                                    \
        });                                                                                                            \
        report("locus_carcerum", "max security alloc+zeroing release", (N), cy_);                                      \
                                                                                                                       \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t m_ = protocore_plaintext_mark();                                                              \
            BENCH_KEEP(protocore_plaintext_alloc((N), sizeof(void *)) != NULL);                                        \
            protocore_plaintext_release(m_);                                                                           \
        });                                                                                                            \
        report("protocore", "plain take+release", (N), cy_);                                                           \
                                                                                                                       \
        carcer_fresh();                                                                                                \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            void *p_ = ram.general.persistent_buf_alloc((N));                                                          \
            BENCH_KEEP(p_ != NULL);                                                                                    \
            ram.general.persistent_buf_release(p_);                                                                    \
        });                                                                                                            \
        report("locus_carcerum", "min security alloc+release", (N), cy_);                                              \
    } while (0)

int main(void)
{
    (void)bench_cycles_per_s();

    printf("impl,case,bytes_per_op,mops_per_s,mb_per_s\n");
    fflush(stdout);

    {
        double cy = 0.0;

        BENCH_TIME_CYCLES(cy, ITERS, { BENCH_KEEP(bench_i_); });
        printf("harness,nop,0,%.3f,\n", (bench_cycles_per_s() / cy) / 1e6);
        fflush(stdout);
    }

    AB_PERSIST(32u);
    AB_PERSIST(256u);

    AB_SCRATCH(64u);

    AB_WIPE(64u);
    AB_WIPE(1024u);

    AB_SECURE(64u);

    /* The span walk: build one, narrow it twice, and read back what it holds. */
    {
        double cy = 0.0;

        BENCH_TIME_CYCLES(cy, ITERS, {
            protocore_span s_ = protocore_span_from(g_buf, sizeof g_buf);
            s_.pos = 128u;
            BENCH_KEEP(protocore_span_ok(protocore_span_first(protocore_span_after(s_, 8u), 64u)));
            BENCH_KEEP(protocore_cspan_ok(protocore_span_produced(s_)));
        });
        report("protocore", "span walk", 0u, cy);

        BENCH_TIME_CYCLES(cy, ITERS, {
            mmgr_span s_ = MMGR_CALL(spat.from, SpatiumCfg, .buf = g_buf, .cap = sizeof g_buf);
            s_.pos = 128u;
            BENCH_KEEP(MMGR_CALL(spat.ok, SpatiumCfg,
                                 .span = MMGR_CALL(spat.first, SpatiumCfg,
                                                   .span = MMGR_CALL(spat.after, SpatiumCfg, .span = s_, .count = 8u),
                                                   .count = 64u)));
            BENCH_KEEP(MMGR_CALL(spat.cok, SpatiumCfg, .cspan = MMGR_CALL(spat.produced, SpatiumCfg, .span = s_)));
        });
        report("locus_carcerum", "span walk", 0u, cy);
    }

    return 0;
}
