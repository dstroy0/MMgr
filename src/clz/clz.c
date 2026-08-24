/**
 * @brief Branchless count of the leading zero bits in a 64-bit value.
 */
#include "clz/clz.h"

/**
 * @brief Argument type built by MMGR_CALL in mmgr_clz_lead.
 *
 * @note Mirrors ClzCfg without its const qualifier.
 */
typedef struct
{
    mmgr_u64 val; /**< Value whose leading zeros are counted. */
} ClzCtx;

/**
 * @brief Counts the zero bits above the highest set bit of c->val.
 *
 * @param[in] c Value to measure [BORROWS].
 * @return      Leading zero count, 0 through 63.
 * @note Halves the search five times, then tests the top bit, so no step branches on the data.
 * @warning A c->val of 0 returns 63, the same answer as a c->val of 1.
 */
MMGR_INLINE mmgr_iword clz_lead(const ClzCtx *c)
{
    mmgr_u64 x = c->val;
    mmgr_u64 shift;
    mmgr_iword n = 0;

    // Each step below: the comparison gives 0 or 1, cast to mmgr_u64 so the shift builds 32, 16, 8, 4 or 2
    // Explicit cast converts that step into the signed mmgr_iword total
    shift = (mmgr_u64)((x >> 32) == 0u) << 5;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 48) == 0u) << 4;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 56) == 0u) << 3;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 60) == 0u) << 2;
    x <<= shift;
    n += (mmgr_iword)shift;
    shift = (mmgr_u64)((x >> 62) == 0u) << 1;
    x <<= shift;
    n += (mmgr_iword)shift;
    // Explicit cast keeps the last add in mmgr_iword after the comparison promotes to int
    n = (mmgr_iword)(n + ((x >> 63) == 0u));
    return n;
}

/**
 * @brief Copies c->val into a ClzCtx and returns clz_lead's result.
 *
 * @note Documented at the declaration in clz.h.
 */
mmgr_iword mmgr_clz_lead(const ClzCfg *c)
{
    return MMGR_CALL(clz_lead, ClzCtx, .val = c->val);
}
