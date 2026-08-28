/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Leading zero count: its argument type, the call, and the clz dispatch table.
 */
#ifndef MMGR_CLZ_H
#define MMGR_CLZ_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Argument for the leading zero count.
 */
typedef struct
{
    const mmgr_u64 val; /**< Value whose leading zeros are counted. */
} ClzCfg;

/**
 * @brief Type of the clz dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the two members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_iword (*lead)(const ClzCfg *args);  /**< Leading zero count of val. */
    mmgr_iword (*trail)(const ClzCfg *args); /**< Trailing zero count of val. */
} ClzNs;
MMGR_NS_LAYOUT(ClzNs, lead, trail);

/**
 * @brief Counts the zero bits above the highest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
 * @return      Leading zero count, 0 through 63.
 * @note Runs in a fixed number of steps, none of which branches on the value.
 * @warning A args->val of 0 returns 63, the same answer as an args->val of 1.
 */
mmgr_iword mmgr_clz_lead(const ClzCfg *args);

/**
 * @brief Counts the zero bits below the lowest set bit of args->val.
 *
 * @param[in] args Value to measure [BORROWS].
 * @return      Trailing zero count, 0 through 63.
 * @note Runs in a fixed number of steps, none of which branches on the value.
 * @warning A args->val of 0 returns 63, the same answer mmgr_clz_trail reports for an args->val of 2^63.
 */
mmgr_iword mmgr_clz_trail(const ClzCfg *args);

/**
 * @brief Dispatch table instance named clz; each member calls the matching mmgr_clz_ function.
 */
MMGR_NS ClzNs clz MMGR_UNUSED = {
    .lead = mmgr_clz_lead,
    .trail = mmgr_clz_trail,
};

MMGR_FINIS_DECLS

#endif
