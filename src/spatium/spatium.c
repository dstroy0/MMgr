#include "spatium/spatium.h"


mmgr_spat (mmgr_spat_init)(const SpatCfg *c)
{
    MMGR_ASSERT(c->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(c->cap != 0, "a span needs a capacity");

    mmgr_spat s;
    s.buf = c->buf;
    s.cap = c->cap;
    s.pos = 0;
    return s;
}
