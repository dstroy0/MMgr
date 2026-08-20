// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// What a SWAR word scan costs, per lane width.
//
// This is the module the width knob exists for: MMGR_SWAR_BITS picks how many bytes one register
// operation answers for, and the whole claim of the design is that a wider lane answers for more
// bytes at the same cost. That claim is worth measuring rather than asserting, and the number that
// shows it is cycles-per-byte - which falls as the lane widens if the claim holds, and does not if
// the compiler stopped vectorising or the tail handling swallowed the gain.
//
// Read the results as a comparison BETWEEN lanes of the same build, never as an absolute: the
// harness reports best-of-7 and the machine is not quiet.
//
// test/ is exempt from the src/ style rules, so this reads as plain host C.

#include <stdio.h>
#include <string.h>

#include "bench_harness.h"

#include "verbum_scrutor/verbum_scrutor.h"

// Big enough that the buffer does not sit in L1 alone, and a power of two so the index mask below
// is a single AND rather than a modulo - a modulo in the timed loop would be a large part of the
// reading at these durations.
#define SPREAD 4096u
#define ITERS 2000000ul

static uint8_t g_buf[SPREAD + 64u];

// The nop row. It runs the loop, the index derivation and the barrier, and no module entry at all,
// so whatever it reports is the harness's own cost. Subtract it mentally from every row below: if a
// row is not clearly above this one, the measurement is noise rather than a result.
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

// The argument is derived from bench_i_ on purpose. A loop-invariant argument to an entry the
// compiler can prove is const is hoisted straight out of the loop DESPITE the memory clobber, and
// the loop left behind counts to ITERS while the work happens once. See bench_harness.h.
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
    // 0xA5 has no zero byte, so has_zero takes its not-found path every iteration rather than
    // returning early on the first lane. An early return would measure the best case and report it
    // as the cost.
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
