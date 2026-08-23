#ifndef MMGR_CUSTODIA_SECURA_H
#define MMGR_CUSTODIA_SECURA_H

#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS



struct SecuraInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_spat (*span)(size_t n, size_t align);
    mmgr_spat (*persist_span)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);

    struct SecuraInternal *internal;
} CustodiaSecuraNs;
MMGR_NS_LAYOUT_OPEN(CustodiaSecuraNs, internal, alloc, span, persist_span, reset, mark, release, used, high_water,
                    capacity, owns);

static inline void mmgr_secura_wipe(void *ptr, size_t len)
{

    volatile uint8_t *b = (volatile uint8_t *)ptr;
    while (len != 0 && (((uintptr_t)b & (sizeof(uintptr_t) - 1)) != 0))
    {
        *b++ = 0;
        len--;
    }
    volatile uintptr_t *w = (volatile uintptr_t *)b;
    while (len >= sizeof(uintptr_t))
    {
        *w++ = 0;
        len -= sizeof(uintptr_t);
    }
    b = (volatile uint8_t *)w;
    while (len != 0)
    {
        *b++ = 0;
        len--;
    }
}

extern struct SecuraInternal mmgr_secura_state;
void *mmgr_secura_capio(size_t n, size_t align);
mmgr_spat mmgr_secura_span(size_t n, size_t align);
mmgr_spat mmgr_secura_persist_span(size_t n);
size_t mmgr_secura_mark(void);
void mmgr_secura_reddo(size_t mark);
void mmgr_secura_reset(void);
size_t mmgr_secura_used(void);
size_t mmgr_secura_high_water(void);
size_t mmgr_secura_capacity(void);
mmgr_bool mmgr_secura_owns(const void *p);

MMGR_NS CustodiaSecuraNs secura MMGR_UNUSED = {
    .alloc = mmgr_secura_capio,
    .span = mmgr_secura_span,
    .persist_span = mmgr_secura_persist_span,
    .reset = mmgr_secura_reset,
    .mark = mmgr_secura_mark,
    .release = mmgr_secura_reddo,
    .used = mmgr_secura_used,
    .high_water = mmgr_secura_high_water,
    .capacity = mmgr_secura_capacity,
    .owns = mmgr_secura_owns,
    .internal = &mmgr_secura_state,
};

MMGR_FINIS_DECLS

#endif
