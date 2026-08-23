#ifndef MMGR_CUSTODIA_SOLUTA_H
#define MMGR_CUSTODIA_SOLUTA_H

#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS



struct SolutaInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_spat (*span)(size_t n, size_t align);
    mmgr_spat (*persist)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);

    struct SolutaInternal *internal;
} CustodiaSolutaNs;
MMGR_NS_LAYOUT_OPEN(CustodiaSolutaNs, internal, alloc, span, persist, reset, mark, release, used, high_water, capacity, owns);

extern struct SolutaInternal mmgr_soluta_internal;
void *mmgr_soluta_capio(size_t n, size_t align);
mmgr_spat mmgr_soluta_span(size_t n, size_t align);
mmgr_spat mmgr_soluta_persist_span(size_t n);
void mmgr_soluta_reset(void);
size_t mmgr_soluta_mark(void);
void mmgr_soluta_reddo(size_t mark);
size_t mmgr_soluta_used(void);
size_t mmgr_soluta_high_water(void);
size_t mmgr_soluta_capacity(void);
mmgr_bool mmgr_soluta_owns(const void *p);

MMGR_NS CustodiaSolutaNs soluta MMGR_UNUSED = {
    .alloc = mmgr_soluta_capio,
    .span = mmgr_soluta_span,
    .persist = mmgr_soluta_persist_span,
    .reset = mmgr_soluta_reset,
    .mark = mmgr_soluta_mark,
    .release = mmgr_soluta_reddo,
    .used = mmgr_soluta_used,
    .high_water = mmgr_soluta_high_water,
    .capacity = mmgr_soluta_capacity,
    .owns = mmgr_soluta_owns,
    .internal = &mmgr_soluta_internal,
};

MMGR_FINIS_DECLS

#endif
