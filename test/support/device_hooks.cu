/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file device_hooks.cu
 * @brief The hooks of embedded_types and of the cellblock in the device pass of nvcc, for a device suite.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-26
 *
 * @note The unit of each device suite includes this source after the cases, one time. The device pass has no weak
 *       default, and the device link of the suite names each hook. A suite that defines a hook itself gives the
 *       name of the hook a macro of its own before the include, DEVICE_HOOKS_SUITE_ and the name, and this source
 *       then leaves that hook out. harness.py runners device writes those macros.
 * @note The host pass defines nothing here. host_wait.c defines the host copy of each turnstile hook, and the weak
 *       defaults of locus_carcerum.c give the host copy of the two hooks of the cellblock. No hook then has two
 *       definitions in the host link.
 * @note The wait spins on the word for its time, as a device has no address wait. The wake has nothing to do, as a
 *       spin sees the word move. The clock is the global timer of the device, in nanoseconds. The place of a thread
 *       is its index in the grid.
 */
#ifndef DEVICE_HOOKS_CU
#define DEVICE_HOOKS_CU

#include "embed_turnstile.h"
#include "locus_carcerum/locus_carcerum.h"

#if defined(__CUDA_ARCH__)

#if !defined(DEVICE_HOOKS_SUITE_embed_clock_ns)
EMBED_HOST_DEVICE embed_word embed_clock_ns(void)
{
    unsigned long long clock_ns;
    asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(clock_ns));
    // Explicit cast takes the nanoseconds modulo the word, as the contract of the clock says
    return (embed_word)clock_ns;
}
#endif

#if !defined(DEVICE_HOOKS_SUITE_embed_word_wait)
EMBED_HOST_DEVICE int embed_word_wait(const EmbedWordWaitCfg *args)
{
    const embed_word start_ns = embed_clock_ns();
    while (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) == args->old_value)
    {
        // Explicit cast takes the difference modulo the word, which is correct across a roll of the clock
        const embed_word elapsed_ns = (embed_word)(embed_clock_ns() - start_ns);
        if ((args->nanoseconds != EMBED_WAIT_INFINITE) && (elapsed_ns >= args->nanoseconds))
        {
            return 0;
        }
    }
    return 1;
}
#endif

#if !defined(DEVICE_HOOKS_SUITE_embed_word_wake)
EMBED_HOST_DEVICE void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
}
#endif

#if !defined(DEVICE_HOOKS_SUITE_embed_thread_place)
EMBED_HOST_DEVICE embed_u32 embed_thread_place(void)
{
    const embed_u32 block = blockIdx.x + (gridDim.x * (blockIdx.y + (gridDim.y * blockIdx.z)));
    const embed_u32 thread = threadIdx.x + (blockDim.x * (threadIdx.y + (blockDim.y * threadIdx.z)));
    return (block * (blockDim.x * blockDim.y * blockDim.z)) + thread;
}
#endif

#if !defined(DEVICE_HOOKS_SUITE_mmgr_carcer_hw_clock_ns)
EMBED_HOST_DEVICE uint64_t mmgr_carcer_hw_clock_ns(void)
{
    return (uint64_t)embed_clock_ns();
}
#endif

#if !defined(DEVICE_HOOKS_SUITE_mmgr_carcer_hw_misnamed)
EMBED_HOST_DEVICE void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
}
#endif

#endif

#endif
