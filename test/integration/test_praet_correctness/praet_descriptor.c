/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_descriptor.c
 * @brief Walking a chain of descriptors.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note One entry, because a descriptor is data and almost everything about it is settled where it
 *       was declared. What cannot be settled there is how long a chain is, since a chain is made by
 *       pointing descriptors at each other after each has been written down.
 * @warning Included by test_praet_correctness.c rather than compiled on its own, the same way the
 *          engine and the schedule are.
 */
#include "praet_descriptor.h"

embed_word praet_descriptor_chain_length(const PraetDescriptor *first, embed_word limit)
{
    if ((first == NULL) || (limit == 0u))
    {
        return 0u;
    }

    embed_word reached = 1u;
    const PraetDescriptor *walk = first->next;

    // Stops at the head as well as at the end, so a cycle is counted once instead of followed. The
    // limit is what covers a chain that closes somewhere other than the head
    while ((walk != NULL) && (walk != first) && (reached < limit))
    {
        reached++;
        walk = walk->next;
    }

    return reached;
}
