// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ANCHOR_COST_INET_H
#define MMGR_ANCHOR_COST_INET_H

/**
 * @file
 * @brief Anchor cost profile.
 *
 * IPv6 text form and dotted quad, RFC 4291. The alphabet is 0-9 a-f A-F : . / % [ ] and
 * nothing else, so any needle byte outside it is the most selective anchor there is.
 *
 * Cost is how common a byte is, so the picker takes the minimum. A byte that cannot occur under
 * this profile scores best, because a needle containing one is filtered on the first row.
 *
 * Generated, not hand tuned. See tools/dev_env/gen_anchor_profiles.py.
 */
#define MMGR_ANCHOR_COST_INET                                                                                    \
    255,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1, 137,   1,   1,   1,   1,   1,   1,   1,   1, 242, 178,              \
    240, 235, 231, 228, 228, 228, 228, 228, 228, 228, 255,   1,   1,   1,   1,   1,              \
      1, 178, 178, 178, 178, 178, 178,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 160,   1, 160,   1,   1,              \
      1, 206, 206, 206, 206, 206, 206,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1

#endif
