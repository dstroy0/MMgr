// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
    mmgr_bool overflow;
} mmgr_spat;

typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;
    mmgr_bool err;
} mmgr_fspat;

typedef struct
{
    mmgr_spat (*from)(uint8_t *p, size_t cap);
    mmgr_bool (*ok)(mmgr_spat s);
    mmgr_bool (*has_storage)(mmgr_spat s);
    size_t (*len)(mmgr_spat s);
    size_t (*room)(mmgr_spat s);
    void (*reset)(mmgr_spat *s);
    mmgr_spat (*after)(mmgr_spat s, size_t off);
    mmgr_spat (*first)(mmgr_spat s, size_t n);
    mmgr_fspat (*produced)(mmgr_spat s);
    mmgr_fspat (*read)(mmgr_spat s, size_t len);
    mmgr_fspat (*cfrom)(const uint8_t *p, size_t len);
    mmgr_bool (*cok)(mmgr_fspat s);
} SpatiumNs;

mmgr_spat mmgr_spat_from(uint8_t *p, size_t cap);
mmgr_bool mmgr_spat_ok(mmgr_spat s);
mmgr_bool mmgr_spat_has_storage(mmgr_spat s);
size_t mmgr_spat_len(mmgr_spat s);
size_t mmgr_spat_room(mmgr_spat s);
void mmgr_spat_reset(mmgr_spat *s);
mmgr_spat mmgr_spat_after(mmgr_spat s, size_t off);
mmgr_spat mmgr_spat_first(mmgr_spat s, size_t n);
mmgr_fspat mmgr_spat_produced(mmgr_spat s);
mmgr_fspat mmgr_spat_read(mmgr_spat s, size_t len);
mmgr_fspat mmgr_fspat_from(const uint8_t *p, size_t len);
mmgr_bool mmgr_fspat_ok(mmgr_fspat s);

static const SpatiumNs spat __attribute__((unused)) = {.from = mmgr_spat_from,
                                                       .ok = mmgr_spat_ok,
                                                       .has_storage = mmgr_spat_has_storage,
                                                       .len = mmgr_spat_len,
                                                       .room = mmgr_spat_room,
                                                       .reset = mmgr_spat_reset,
                                                       .after = mmgr_spat_after,
                                                       .first = mmgr_spat_first,
                                                       .produced = mmgr_spat_produced,
                                                       .read = mmgr_spat_read,
                                                       .cfrom = mmgr_fspat_from,
                                                       .cok = mmgr_fspat_ok};

MMGR_END_DECLS

#endif
