// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ANCHOR_COST_URI_H
#define MMGR_ANCHOR_COST_URI_H

/**
 * @file
 * @brief Anchor cost profile.
 *
 * URIs and URLs. Character classes from RFC 3986, weighted by where they occur: the path
 * separator dominates, then the host dots, then the query delimiters.
 *
 * Cost is how common a byte is, so the picker takes the minimum. A byte that cannot occur under
 * this profile scores best, because a needle containing one is filtered on the first row.
 *
 * Generated, not hand tuned. See tools/dev_env/gen_anchor_profiles.py.
 */
#define MMGR_ANCHOR_COST_URI                                                                                    \
    255,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1, 107,   1, 155, 107, 188, 199, 107, 107, 107, 107, 163, 144, 225, 244, 255,              \
    201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 218, 136,   1, 207,   1, 193,              \
    144, 162, 117, 136, 141, 172, 126, 124, 148, 158,  63,  99, 143, 130, 158, 159,              \
    124,  42, 154, 156, 164, 131, 106, 121,  60, 119,  45,  93,   1,  93,   1, 188,              \
      1, 230, 184, 203, 208, 239, 193, 192, 215, 226, 131, 166, 211, 197, 225, 226,              \
    191, 109, 222, 223, 231, 198, 173, 188, 127, 186, 112,   1,   1,   1, 136,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1

#endif
