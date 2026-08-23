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
    uint8_t fill_idx; } PingPong;

typedef struct
{
    const size_t size;                const mmgr_bool dma_required;     const size_t free_dram;           const size_t free_psram;          const size_t psram_threshold;     const size_t dram_reserve;    } ExternumCfg;

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

#define MMGR_EXTER_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0, int: 0, unsigned: 0, long: 0, unsigned long: 0))
#define MMGR_EXTER_IS_BOOL(x_) ((void)_Generic((x_), mmgr_bool: 0, int: 0, unsigned: 0))
#define MMGR_EXTER_IS_PAIR(x_) ((void)_Generic((x_), PingPong: 0))

#define mmgr_exter_place(size_, dma_, free_dram_, free_psram_, threshold_, reserve_)                                   \
    (MMGR_EXTER_IS_SIZE(size_), MMGR_EXTER_IS_BOOL(dma_), MMGR_EXTER_IS_SIZE(free_dram_),                              \
     MMGR_EXTER_IS_SIZE(free_psram_), MMGR_EXTER_IS_SIZE(threshold_), MMGR_EXTER_IS_SIZE(reserve_),                    \
     exter.place(&(ExternumCfg){.size = (size_),                                                                       \
                                .dma_required = (dma_),                                                                \
                                .free_dram = (free_dram_),                                                             \
                                .free_psram = (free_psram_),                                                           \
                                .psram_threshold = (threshold_),                                                       \
                                .dram_reserve = (reserve_)}))

#define mmgr_pingpong_init(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_init(&(pp_)))
#define mmgr_pingpong_fill_index(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_fill(&(pp_)))
#define mmgr_pingpong_drain_index(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_drain(&(pp_)))
#define mmgr_pingpong_swap(pp_) (MMGR_EXTER_IS_PAIR(pp_), exter.pingpong_swap(&(pp_)))

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
