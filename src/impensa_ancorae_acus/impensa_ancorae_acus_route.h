// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_IMPENSA_ANCORAE_ACUS_ROUTE_H
#define MMGR_IMPENSA_ANCORAE_ACUS_ROUTE_H

/**
 * @file
 * @brief Anchor cost profile.
 *
 * HTTP route patterns. RFC 3986 path-abempty plus the {param} and :param template syntax
 * routers use. The slash is over half of all structural bytes.
 *
 * Cost is how common a byte is, so the picker takes the minimum. A byte that cannot occur under
 * this profile scores best, because a needle containing one is filtered on the first row.
 *
 * Generated, not hand tuned. See tools/dev_env/gen_ancorae_formae.py.
 */
#define MMGR_IMPENSA_ANCORAE_ACUS_ROUTE                                                                                    \
    255,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1, 118, 118, 146,   1,   1, 204, 176, 255,              \
    176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 181,   1, 128,   1, 128, 136,              \
      1, 136,  94, 111, 116, 145, 102, 100, 122, 132,  43,  76, 118, 105, 132, 133,              \
    100,  23, 129, 130, 137, 107,  83,  97,  40,  96,  26,   1,   1,   1,   1, 194,              \
      1, 218, 175, 193, 198, 227, 183, 182, 204, 214, 125, 158, 200, 187, 214, 215,              \
    182, 105, 210, 212, 219, 189, 165, 179, 121, 177, 107, 187,   1, 187,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,              \
      1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1

#endif
