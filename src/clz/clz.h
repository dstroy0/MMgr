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
 * @note MMGR_NS_LAYOUT asserts the lead member is at offset 0 and that the struct holds nothing else.
 */
typedef struct
{
    mmgr_iword (*lead)(const ClzCfg *c); /**< Leading zero count of val. */
} ClzNs;
MMGR_NS_LAYOUT(ClzNs, lead);

/**
 * @brief Counts the zero bits above the highest set bit of c->val.
 *
 * @param[in] c Value to measure [BORROWS].
 * @return      Leading zero count, 0 through 63.
 * @note Runs in a fixed number of steps, none of which branches on the value.
 * @warning A c->val of 0 returns 63, the same answer as a c->val of 1.
 */
mmgr_iword mmgr_clz_lead(const ClzCfg *c);

/**
 * @brief Dispatch table instance named clz; its lead member calls mmgr_clz_lead.
 */
MMGR_NS ClzNs clz MMGR_UNUSED = {
    .lead = mmgr_clz_lead,
};

MMGR_FINIS_DECLS

#endif
