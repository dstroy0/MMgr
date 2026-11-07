/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_fragmentation_prove.c
 * @brief Fragment.lean's reading_le held on the C: for every persistent end and every running total at or below
 *        it, mmgr_series_cellularum_fragmentation reads at most 100, and 0 below a hundred bytes.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 */
#include "locus_carcerum/locus_carcerum.h"

#include "prove.h"

#define PROVE_BYTES 4096u

#define PROVE_OWNERS(owner_) owner_(s_reader, "reader")
MMGR_OWNERS(PROVE_OWNERS)
PROVE_NO_MISNAMED()

ParsMemoriaeInternae(proved, PROVE_BYTES);
LocusCarcerum(prove, MMGR_MINIMUM_SECURITY(proved));

int main(void)
{
    SeriesCellularum *const series = &prove_proved_ctx;
    const size_t end = nondet_size();
    const size_t in_use = nondet_size();

    PROVE_ASSUME(end <= series->size);
    PROVE_ASSUME(in_use <= end);
    embed_atomic_usize_store(&series->bounds, (((embed_atomic_usize_value)series->size << MMGR_CARCER_HALF_BITS) | end));
    embed_atomic_usize_store(&series->persistent_in_use, in_use);
    PROVE_ASSERT(mmgr_series_cellularum_persistent_end(series) == end, "the harness lays the end it means");
    const size_t reading = mmgr_series_cellularum_fragmentation(series);
    PROVE_ASSERT(reading <= 100u, "fragmentation reads as a percent");
    PROVE_ASSERT((end >= 100u) || (reading == 0u), "a tier under a hundred bytes reads 0");
    PROVE_DONE();
    return 0;
}
