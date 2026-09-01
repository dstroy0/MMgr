/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_port_default.c
 * @brief The weak refusing default for praet_hw_progress, in a translation unit of its own.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note A file of its own because a weak default and a strong definition of one name cannot share a
 *       translation unit. test_praet_correctness.c includes praet_engine.c, which puts the port's
 *       definition inside the suite's unit, and a weak one beside it is a redefinition.
 * @note Linked into every suite, the way platform_host.c and guard_page.c are. Which suites want it
 *       is their business. Where nothing calls it the linker drops it.
 * @note Declares the prototype here instead of including praet_ordo.h. That header reaches the whole
 *       configuration chain, and pulling it in would make every suite in the tree declare the praet
 *       knobs to compile this one function.
 */
#include "embed_types.h"

uint16_t praet_hw_progress(embed_word channel);

/**
 * @brief Weak default for the progress hook, which reports no bytes moved.
 *
 * @param[in] channel Channel the caller is asking about.
 * @return            0 always.
 * @note EMBED_WEAK marks this weak where EMBED_HAS_ATTRIBUTE(weak) is non-zero. A port's definition
 *       replaces it, which is the arrangement the four mmgr_praet_hw_ hooks already have in src.
 * @note praet_ordo_take_progress reads zero as no movement. An unported build then walks every
 *       channel the way it would with no progress reporting at all, and the watchdog still marks a
 *       channel stalled once its keepalive window elapses.
 * @note The (void)channel discards the argument, since this body reads nothing.
 */
EMBED_WEAK uint16_t praet_hw_progress(embed_word channel)
{
    (void)channel;
    return 0u;
}
