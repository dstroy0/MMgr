#ifndef MMGR_BENCH_PROTO_RAWMEMCPY_H
#define MMGR_BENCH_PROTO_RAWMEMCPY_H

#include "config/mmgr_config.h"
#include "proximus_operor/proximus_operor.h"

static inline void proto_raw_read(void *dst, const void *p, size_t sz)
{
    EMBED_CALL(proxim.read, ProximusCfg, .dst = dst, .at = p, .size = sz);
}

#endif
