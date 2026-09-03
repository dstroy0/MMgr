/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_target_probe.c
 * @brief Compiles the praet schedule for one target and holds the platform derivation to what that
 *        target answers.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Nothing here runs. `harness.py targets` compiles it once per part, with the expectations
 *       arriving as EXPECT_ defines from that table.
 * @note The expectations are not read out of the header, so a header that decided something else
 *       fails a row instead of agreeing with itself.
 * @note Carries no cases, so Unity's generator walks past it and mmgr_add_suite does not build it.
 */
#include "praet_ordo.h"

// The descriptor declarators, which praet_ordo.h does not reach. praet_descriptor.h includes it, not
// the other way round, so a probe that wants both names it
#include "praet_descriptor.h"

EMBED_STATIC_ASSERT(PRAET_PLATFORM_ARM == EXPECT_ARM, "the ARM flag is not what this target gives");
EMBED_STATIC_ASSERT(PRAET_PLATFORM_RISCV == EXPECT_RISCV, "the RISC-V flag is not what this target gives");
EMBED_STATIC_ASSERT(PRAET_PLATFORM_XTENSA == EXPECT_XTENSA, "the Xtensa flag is not what this target gives");
EMBED_STATIC_ASSERT(PRAET_PLATFORM_XLEN == EXPECT_XLEN, "the register width is not what this target gives");
EMBED_STATIC_ASSERT(PRAET_PLATFORM_HAS_CYCLE_COUNTER == EXPECT_COUNTER,
                    "the cycle counter capability is not what this target gives");

/**
 * @brief The context this target would emit, declared the way a caller declares one.
 */
PraetOrdoContext(s_probe_context, AD_VERBI_CONFINIUM_RESTITUE_PAULATIM_CRC_ENABLE);

/**
 * @brief A pool for the probe's channel to be over.
 */
ParsMemoriaeInternae(s_probe_pool, 256);

/**
 * @brief Binds channel zero of the probe context to that pool.
 */
PraetChannel(s_probe_context, 0, s_probe_pool);

/**
 * @brief A second pool, so a descriptor has somewhere to read from and somewhere else to write to.
 */
ParsMemoriaeInternae(s_probe_source, 256);

/**
 * @brief A register standing in for one a part would name.
 */
static volatile uint8_t s_probe_register;

/**
 * @brief The peripheral the two descriptors below reach.
 */
PraetPeripheralDeclare(s_probe_peripheral, &s_probe_register);

/**
 * @brief One memory to memory transfer, so the ordinary descriptor path is compiled here too.
 */
PraetOneShot(s_probe_one_shot, s_probe_source, 0u, s_probe_pool, 0u, 64u, PRAET_MENSURA_VERBUM);

/**
 * @brief One transfer that drains a pool into a register.
 *
 * @note Here because a static initializer that reads a const object is not a constant expression in
 *       C, and a compiler that folds it anyway hides that. GCC 13 accepts these two; GCC 5.4.1
 *       refuses them, which is what the old toolchain rows are for. Measured, on a Cortex-M7 build
 *       of the suite that reported "initializer element is not constant" and named these lines.
 */
PraetToPeripheral(s_probe_drain, s_probe_source, 0u, s_probe_peripheral, &s_probe_register, 0u, 64u,
                  PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief One transfer that fills a pool from a register.
 */
PraetFromPeripheral(s_probe_fill, s_probe_peripheral, &s_probe_register, 0u, s_probe_pool, 0u, 64u,
                    PRAET_MENSURA_VERBUM, NULL);

/**
 * @brief Reaches the attach and submit surfaces, so both declarators are compiled for this target.
 *
 * @return The channel's flag word, as an int.
 * @note Never called. It exists to make the compiler expand and generate code for the macros, which
 *       a declaration alone does not do.
 */
int praet_probe_surface(void);

int praet_probe_surface(void)
{
    (void)PraetAttach(s_probe_context, 0, s_probe_pool, PRAET_REGION_INTERNAL);
    (void)PraetSubmit(s_probe_context, 0, s_probe_pool, 0u, 64u);
    praet_ordo_poll(&s_probe_context);
    return (int)praet_ordo_flags(&s_probe_context, 0u);
}
