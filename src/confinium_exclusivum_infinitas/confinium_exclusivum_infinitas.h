#ifndef MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H
#define MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

#define MMGR_RING_POW2(cap) (((cap) & ((cap) - 1)) == 0)

#define MMGR_RING_WRAP(i, cap) ((i) & ((cap) - 1))

#define MMGR_RING_LOCULI_MAX MMGR_WORD_BITS

#define MMGR_RING_WORDS 40u

#define MMGR_SING_GRANULE_MAX ((size_t)sizeof(mmgr_word))

#define MMGR_SING_READY ((mmgr_u16)1 << 0)    /**< No auctor holds the path. It can be attached. */
#define MMGR_SING_ATTACHED ((mmgr_u16)1 << 1) /**< An auctor holds it. */
#define MMGR_SING_GRANTED ((mmgr_u16)1 << 2)  /**< A grant is out. The ground is promised. */
#define MMGR_SING_FULL ((mmgr_u16)1 << 3)     /**< Nothing is free. Wait for the consumer. */
#define MMGR_SING_ALIEN ((mmgr_u16)1 << 4)    /**< The caller is not the auctor, or opened nothing. */
#define MMGR_SING_STALE ((mmgr_u16)1 << 5)    /**< The token does not name a live grant. */
#define MMGR_SING_REFUSED ((mmgr_u16)1 << 6)  /**< The ask was declined. */

#define MMGR_SING_FLAG_BITS 8u

#define MMGR_SING_FLAGS(st) ((mmgr_u16)((st) & (mmgr_u16)(((mmgr_u16)1 << MMGR_SING_FLAG_BITS) - 1u)))
#define MMGR_SING_SEG(st) ((size_t)((mmgr_u16)(st) >> MMGR_SING_FLAG_BITS))

typedef struct
{
    MMGR_ALIGN(sizeof(size_t)) uint8_t opaque[MMGR_RING_WORDS * sizeof(size_t)];
} mmgr_ring;

struct MmgrCursor;

typedef struct
{
    mmgr_ring *const r;
    uint8_t *const buf;
    const size_t cap;
    const size_t nsegs;
    _Atomic mmgr_word *const held;
} RingCfg;

typedef struct
{
    const void *const owner;
    const size_t gran;
} SingularitasCfg;

typedef struct
{
    mmgr_ring *const r;
    struct MmgrCursor *const cur;
    uint8_t *const dst;
    const uint8_t *const src;
    const size_t n;
    const size_t off;
    const size_t from;
    const size_t to;
    size_t *const tessera;
    const void *const owner;
    const SingularitasCfg *const sing;
    size_t *const units;
    mmgr_u16 *const status;
} InfinCfg;

typedef struct
{
    mmgr_bool (*init)(const RingCfg *c);
    struct MmgrCursor *(*open)(const InfinCfg *c);
    const uint8_t *(*drain)(const InfinCfg *c);
    size_t (*available)(const InfinCfg *c);
    size_t (*vacant)(const InfinCfg *c);
    mmgr_bool (*read_byte)(const InfinCfg *c);
    const uint8_t *(*read)(const InfinCfg *c);
    void (*peek)(const InfinCfg *c);
    void (*consume)(const InfinCfg *c);
    uint8_t *(*singularitas)(const InfinCfg *c);
    mmgr_bool (*detach)(const InfinCfg *c);
    void (*seek)(const InfinCfg *c);
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, init, open, drain, available, vacant, read_byte, read, peek, consume, singularitas, detach,
               seek);

mmgr_bool mmgr_infin_init(const RingCfg *c);
struct MmgrCursor *mmgr_infin_open(const InfinCfg *c);
const uint8_t *mmgr_infin_drain(const InfinCfg *c);
size_t mmgr_infin_available(const InfinCfg *c);
size_t mmgr_infin_vacant(const InfinCfg *c);
mmgr_bool mmgr_infin_read_byte(const InfinCfg *c);
const uint8_t *mmgr_infin_read(const InfinCfg *c);
void mmgr_infin_peek(const InfinCfg *c);
void mmgr_infin_consume(const InfinCfg *c);
uint8_t *mmgr_infin_singularitas(const InfinCfg *c);
mmgr_bool mmgr_infin_detach(const InfinCfg *c);
void mmgr_infin_seek(const InfinCfg *c);

MMGR_NS InfinitasNs iteratio_infinita MMGR_UNUSED = {
    .init = mmgr_infin_init,
    .open = mmgr_infin_open,
    .drain = mmgr_infin_drain,
    .available = mmgr_infin_available,
    .vacant = mmgr_infin_vacant,
    .read_byte = mmgr_infin_read_byte,
    .read = mmgr_infin_read,
    .peek = mmgr_infin_peek,
    .consume = mmgr_infin_consume,
    .singularitas = mmgr_infin_singularitas,
    .detach = mmgr_infin_detach,
    .seek = mmgr_infin_seek,
};

MMGR_FINIS_DECLS

#endif
