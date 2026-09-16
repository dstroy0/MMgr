#ifndef MMGR_BENCH_PROTO_SPAN_H
#define MMGR_BENCH_PROTO_SPAN_H

#include "config/mmgr_config.h"

typedef embed_bool proto_bool;

#define PROTO_TRUE EMBED_TRUE

#define PROTO_FALSE EMBED_FALSE

typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;
    proto_bool err;
} protocore_cspan;

#endif
