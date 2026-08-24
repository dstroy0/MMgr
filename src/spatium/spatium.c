/**
 * @brief Builds a span over a buffer the caller owns.
 *
 * @note Holds no storage of its own, and returns the span by value rather than through a pointer.
 */
#include "spatium/spatium.h"

/**
 * @brief Returns an mmgr_spat over c->buf, with its position at 0.
 *
 * @note The two assertions name c->buf and c->cap; the default MMGR_ASSERT leaves both unevaluated.
 * @note Documented at the declaration in spatium.h.
 */
mmgr_spat mmgr_spat_init(const SpatCfg *c)
{
    MMGR_ASSERT(c->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(c->cap != 0, "a span needs a capacity");

    mmgr_spat s;
    s.buf = c->buf;
    s.cap = c->cap;
    s.pos = 0;
    return s;
}
