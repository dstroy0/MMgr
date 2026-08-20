// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ANCHOR_COST_GENERIC_H
#define MMGR_ANCHOR_COST_GENERIC_H

/**
 * @file
 * @brief Anchor cost profile.
 *
 * No grammar assumed. Printable ASCII is common, control bytes and the high half are not.
 * This is the default and it is a weak prior on purpose - it should never be badly wrong.
 *
 * Cost is how common a byte is, so the picker takes the minimum. A byte that cannot occur under
 * this profile scores best, because a needle containing one is filtered on the first row.
 *
 * Generated, not hand tuned. See tools/dev_env/gen_anchor_profiles.py.
 */
#define MMGR_ANCHOR_COST_GENERIC                                                                                    \
    255, 136, 136, 136, 136, 136, 136, 136, 136, 163, 199, 136, 136, 136, 136, 136,              \
    136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,              \
    255, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,              \
    193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 169, 169, 169, 169, 169, 169,              \
    169, 173, 128, 147, 152, 183, 136, 135, 159, 169,  74, 110, 154, 141, 169, 170,              \
    135,  53, 165, 167, 175, 142, 117, 132,  70, 130,  56, 169, 169, 169, 169, 169,              \
    169, 230, 184, 203, 208, 239, 193, 192, 215, 226, 131, 166, 211, 197, 225, 226,              \
    191, 109, 222, 223, 231, 198, 173, 188, 127, 186, 112, 169, 169, 169, 169, 136,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,              \
    107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107

#endif
