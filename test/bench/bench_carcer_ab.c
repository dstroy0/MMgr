#include <stdio.h>

#include "bench_harness.h"

#include "locus_carcerum/locus_carcerum.h"
#include "spatium/spatium.h"

#include "mmgr/arena/arena.h"
#include "mmgr/plaintext/plaintext.h"
#include "mmgr/secure/secure.h"

#define ITERS 200000ul
#define ARENA_BYTES 65536u
#define CHAIN 16u

ParsMemoriaeInternae(general, ARENA_BYTES);

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(general));

static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_proto_bytes[ARENA_BYTES];
static protocore_arena g_arena;

static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t g_buf[1024];

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
            mmgr_span s_ = EMBED_CALL(spat.from, SpatiumCfg, .buf = g_buf, .cap = sizeof g_buf);
            s_.pos = 128u;
            BENCH_KEEP(EMBED_CALL(
                spat.ok, SpatiumCfg,
                .span = EMBED_CALL(spat.first, SpatiumCfg,
                                   .span = EMBED_CALL(spat.after, SpatiumCfg, .span = s_, .count = 8u), .count = 64u)));
            BENCH_KEEP(EMBED_CALL(spat.cok, SpatiumCfg, .cspan = EMBED_CALL(spat.produced, SpatiumCfg, .span = s_)));
        });
        report("locus_carcerum", "span walk", 0u, cy);
    }

    return 0;
}
