#ifndef MMGR_CONFINIUM_EXTERNUM_H
#define MMGR_CONFINIUM_EXTERNUM_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_EXTRAM

MMGR_INCIPE_DECLS

typedef enum MMGR_ENUM_PACKED
{
    PLACE_DRAM = 0,
    PLACE_PSRAM = 1,
    PLACE_FAIL = 2
} mmgr_place;

typedef struct
{
    uint8_t fill_idx;
} PingPong;

typedef struct
{
    const size_t size;
    const mmgr_bool dma_required;
    const size_t free_dram;
    const size_t free_psram;
    const size_t psram_threshold;
    const size_t dram_reserve;
} ExternumCfg;

typedef struct
{
    mmgr_place (*place)(const ExternumCfg *c);
    void (*pingpong_init)(PingPong *const pp);
    uint8_t (*pingpong_fill)(PingPong *const pp);
    uint8_t (*pingpong_drain)(PingPong *const pp);
    uint8_t (*pingpong_swap)(PingPong *const pp);
} ConfiniumExternumNs;
MMGR_NS_LAYOUT(ConfiniumExternumNs, place, pingpong_init, pingpong_fill, pingpong_drain, pingpong_swap);

mmgr_place mmgr_exter_place(const ExternumCfg *c);
void mmgr_pingpong_init(PingPong *const pp);
uint8_t mmgr_pingpong_fill_index(PingPong *const pp);
uint8_t mmgr_pingpong_drain_index(PingPong *const pp);
uint8_t mmgr_pingpong_swap(PingPong *const pp);

MMGR_NS ConfiniumExternumNs exter MMGR_UNUSED = {
    .place = mmgr_exter_place,
    .pingpong_init = mmgr_pingpong_init,
    .pingpong_fill = mmgr_pingpong_fill_index,
    .pingpong_drain = mmgr_pingpong_drain_index,
    .pingpong_swap = mmgr_pingpong_swap,
};

MMGR_FINIS_DECLS

#endif

#endif
