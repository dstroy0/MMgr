/**
 * @brief The string entries against the target's own libc, across input lengths, on silicon.
 *
 * @note Run on the part rather than on a host. A desktop libc answers these with SSE or AVX, which
 *       reads 16 to 48 bytes per instruction, and neither of these parts has anything of the kind.
 *       Comparing a machine-word SWAR against a vector unit measures the vector unit.
 * @note Lengths run from 8 bytes up, because a fixed prologue cannot show at 16 KB and this library
 *       is built for short buffers whose extents are settled before the build.
 * @note The needle length is passed rather than measured, so find is timed doing the work it was
 *       asked for instead of re-deriving what the caller already knew.
 */
#include <string.h>

#include "device_bench.h"

#include "cellularum_laboro/cellularum_laboro.h"

#define CAP 4096u

static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_a[CAP];
static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_b[CAP];

static const char *const g_needle = "qx";
#define NLEN 2u

/**
 * @brief Where every timed result lands, so no call can be deleted for having no effect.
 *
 * @note Declared in device_bench.h and defined here, one per bench image.
 */
volatile uintptr_t g_dbench_sink;

/**
 * @brief Fills both buffers with n bytes that never contain the needle or the sought byte.
 *
 * @param[in] n Bytes to fill, leaving room for the terminator.
 * @note The alphabet stops short of 'q' followed by 'x' and never reaches 'z', so every scan runs the
 *       whole length rather than stopping early on a hit.
 */
static void fill(size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        g_a[i] = (char)('a' + (i % 15));
        g_b[i] = (char)('a' + (i % 15));
    }
    g_a[n] = '\0';
    g_b[n] = '\0';
}

/**
 * @brief One pass: every case at every length, then the dispatch cost on its own.
 */
void dbench_run(void)
{
    static const size_t lens[] = {8u, 16u, 32u, 64u, 128u, 512u, 2048u};

    DBENCH_BANNER("cellularum vs libc");

    for (unsigned li = 0; li < (sizeof lens / sizeof lens[0]); li++)
    {
        const size_t n = lens[li];
        const uint32_t iters = (n <= 64u) ? 20000u : ((n <= 512u) ? 4000u : 1000u);

        fill(n);

        DBENCH_AB("len", iters, n, DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = n + 1u)),
                  DBENCH_KEEP(strnlen(g_a, n + 1u)));

        DBENCH_AB("chr", iters, n,
                  DBENCH_KEEP(MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .byte = (uint8_t)'z')),
                  DBENCH_KEEP(strchr(g_a, 'z')));

        DBENCH_AB("cmp", iters, n,
                  DBENCH_KEEP(MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = g_a, .other = g_b, .cap = n)),
                  DBENCH_KEEP(memcmp(g_a, g_b, n)));

        DBENCH_AB("find", iters, n,
                  DBENCH_KEEP(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = g_a, .cap = n + 1u, .other = g_needle,
                                        .other_cap = NLEN + 1u, .other_len = NLEN)),
                  DBENCH_KEEP(strstr(g_a, g_needle)));
    }

    // What MMGR_CALL costs before any work happens. On Cortex-M4 the compound literal became a
    // memset of the whole argument type per call rather than folding into registers; whether these
    // parts do the same is the question, and it lands on every entry in the library if they do.
    fill(8u);
    DBENCH_OP("dispatch_len8", 20000u, DBENCH_KEEP(MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = g_a, .cap = 9u)));
    DBENCH_OP("direct_len8", 20000u, DBENCH_KEEP(mmgr_cellul_len(&(CatenaFinitaCfg){.src = g_a, .cap = 9u})));

    printf("DB ==== end ====\n");
}

/**
 * @brief Runs the pass on its own task, repeating, so a capture opened at any time catches a cycle.
 *
 * @note Repeats rather than running once at boot. A single pass finishes before a serial capture
 *       opened after flashing can attach, and the run would be missed.
 * @note The first pass is delayed so the counts are not taken against app startup.
 */
void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;)
    {
        dbench_run();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
