#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"

#include <stdatomic.h>


#define MMGR_ATOMIC_LOAD(p) atomic_load_explicit((p), memory_order_acquire)

#define MMGR_ATOMIC_STORE(p, v) atomic_store_explicit((p), (v), memory_order_release)

#define MMGR_ATOMIC_CLEAR(p, bits) \
    ((void)atomic_fetch_and_explicit((p), (mmgr_word) ~(bits), memory_order_release))

struct MmgrCursor
{
    size_t base;     size_t span;     size_t off;  };

typedef struct
{
    size_t first;     size_t last;      size_t next;      size_t gen;   } Drain;

#define MMGR_TESSERA_SLOTS (MMGR_RING_DRAINS + 1u)

#define MMGR_TESSERA_SING MMGR_RING_DRAINS

#define MMGR_TESSERA(idx, gen) (((gen) * MMGR_TESSERA_SLOTS) + (idx) + 1u)

#define MMGR_TESSERA_IDX(t) ((((t) - 1u)) % MMGR_TESSERA_SLOTS)

#define MMGR_TESSERA_GEN(t) ((((t) - 1u)) / MMGR_TESSERA_SLOTS)

typedef struct
{
    const void *owner;     size_t gran;           size_t at;             size_t span;           size_t gen;            int open;          } Singularitas;

typedef struct
{
    uint8_t *buf;                size_t cap;                  size_t nsegs;                size_t seg;                  _Atomic mmgr_word *held;     _Atomic size_t head;         _Atomic size_t tail;         struct MmgrCursor ord;       const void *owner;           int open;                    _Atomic mmgr_word slots;     Singularitas sing;           Drain drains[MMGR_RING_DRAINS];
} RingState;

MMGR_STATIC_ASSERT(sizeof(RingState) <= sizeof(mmgr_ring),
                   "MMGR_RING_WORDS is short: a consumer cannot declare room for the ring");

MMGR_STATIC_ASSERT((size_t)MMGR_RING_LOCULI_MAX <= (size_t)0xFF,
                   "a segment index has to fit the upper octet of a status");
MMGR_STATIC_ASSERT(MMGR_SING_REFUSED < ((mmgr_u16)1 << MMGR_SING_FLAG_BITS),
                   "a status flag has to fit the lower octet of a status");

MMGR_INLINE RingState *ring_of(mmgr_ring *r)
{
    return (RingState *)(void *)r->opaque;
}

typedef struct
{
    RingState *s;               struct MmgrCursor *cur;     uint8_t *dst;               const uint8_t *src;         size_t n;                   size_t off;                 size_t from;                size_t to;                  size_t *tessera;            const void *owner;          size_t *units;              int rearm;              } InfinCtx;

MMGR_INLINE size_t ring_arrived(const RingState *s)
{
    return MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&s->head) - MMGR_ATOMIC_LOAD(&s->tail), s->cap);
}

MMGR_INLINE size_t ring_vacant(const RingState *s)
{
    return (s->cap - 1u) - ring_arrived(s) - s->sing.span;
}

MMGR_INLINE size_t infin_available(const InfinCtx *c)
{
    return ring_arrived(c->s);
}

MMGR_INLINE size_t infin_free(const InfinCtx *c)
{
    return (c->s->cap - 1u) - infin_available(c) - c->s->sing.span;
}

MMGR_INLINE mmgr_bool infin_read_byte(const InfinCtx *c)
{
    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    if (t == MMGR_ATOMIC_LOAD(&c->s->head))
    {
        return MMGR_FALSE;
    }
    *c->dst = c->s->buf[t];
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(t + 1u, c->s->cap));
    return MMGR_TRUE;
}

MMGR_INLINE const uint8_t *infin_read(const InfinCtx *c)
{
    const size_t have = infin_available(c);
    if ((have == 0u) || (c->n > have))
    {
        return NULL;
    }

    const size_t t = MMGR_ATOMIC_LOAD(&c->s->tail);
    if (c->n > (c->s->cap - t))
    {
        return NULL;
    }
    return &c->s->buf[t];
}

MMGR_INLINE void infin_peek(const InfinCtx *c)
{
    size_t at = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->off, c->s->cap);

    for (size_t i = 0; i < c->n; i++)
    {
        c->dst[i] = c->s->buf[at];
        at = MMGR_RING_WRAP(at + 1u, c->s->cap);
    }
}

MMGR_INLINE void infin_consume(const InfinCtx *c)
{
    MMGR_ATOMIC_STORE(&c->s->tail, MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&c->s->tail) + c->n, c->s->cap));
}

MMGR_INLINE size_t infin_room(const RingState *s, size_t at, size_t want)
{
    const mmgr_word bits = MMGR_ATOMIC_LOAD(s->held);

    if (bits == 0u)
    {
        return want;
    }

    size_t room = 0;
    while (room < want)
    {
        const size_t seg = at / s->seg;
        if ((bits & (mmgr_word)((mmgr_word)1 << seg)) != 0u)
        {
            break;
        }
        size_t step = s->seg - (at % s->seg);
        if (step > (want - room))
        {
            step = want - room;
        }
        room += step;
        at = MMGR_RING_WRAP(at + step, s->cap);
    }
    return room;
}

MMGR_INLINE void infin_seek(const InfinCtx *c)
{
    MMGR_ASSERT(c->off <= c->cur->span, "a cursor cannot be moved outside its frame");
    c->cur->off = c->off;
}

MMGR_INLINE mmgr_word seg_mask(size_t first, size_t last)
{
    mmgr_word m = 0;

    for (size_t i = first; i < last; i++)
    {
        m |= (mmgr_word)((mmgr_word)1 << i);
    }
    return m;
}

MMGR_INLINE Drain *drain_of(RingState *s, size_t tessera)
{
    if (tessera == 0u)
    {
        return NULL;
    }

    const size_t idx = MMGR_TESSERA_IDX(tessera);
    if (idx >= MMGR_RING_DRAINS)
    {
        return NULL;     }

    Drain *const d = &s->drains[idx];

    if ((d->first == d->last) || (d->gen != MMGR_TESSERA_GEN(tessera)))
    {
        return NULL;
    }
    return d;
}

MMGR_INLINE void drain_retire(RingState *s, Drain *d)
{
    const size_t slot = (size_t)(d - &s->drains[0]);

    MMGR_ATOMIC_CLEAR(s->held, seg_mask(d->first, d->last));
    d->first = 0u;
    d->last = 0u;
    d->next = 0u;
    d->gen++;
    MMGR_ATOMIC_CLEAR(&s->slots, (mmgr_word)((mmgr_word)1 << slot));
}

MMGR_INLINE mmgr_bool segs_claim(RingState *s, size_t at, size_t len)
{
    const size_t first = at / s->seg;
    const size_t last = (at + len + s->seg - 1u) / s->seg;
    const mmgr_word want = seg_mask(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);

    if ((prev & want) != 0)
    {
                MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return MMGR_FALSE;
    }
    return MMGR_TRUE;
}

MMGR_INLINE void segs_drop(RingState *s, size_t at, size_t len)
{
    MMGR_ATOMIC_CLEAR(s->held, seg_mask(at / s->seg, (at + len + s->seg - 1u) / s->seg));
}

MMGR_INLINE uint8_t *sing_claim(RingState *s, size_t units, size_t *given)
{
    if (s->sing.span != 0u)
    {
        return NULL;     }
    if ((units == 0u) && (given == NULL))
    {
        return NULL;     }

    const size_t head = MMGR_ATOMIC_LOAD(&s->head);
    size_t len = ((s->cap - 1u) - MMGR_RING_WRAP(head - MMGR_ATOMIC_LOAD(&s->tail), s->cap));

    if (units != 0u)
    {
        const size_t want = units * s->sing.gran;
        if (want > len)
        {
            return NULL;         }
        len = want;
    }
    if (len > (s->cap - head))
    {
        len = s->cap - head;
    }
    len = infin_room(s, head, len);
    len -= (len % s->sing.gran);

    if ((len == 0u) || ((units != 0u) && (len != (units * s->sing.gran))))
    {
        return NULL;
    }
    if (!segs_claim(s, head, len))
    {
        return NULL;
    }
    s->sing.at = head;
    s->sing.span = len;
    if (given != NULL)
    {
        *given = len / s->sing.gran;
    }
    return &s->buf[head];
}

MMGR_INLINE void sing_commit(RingState *s, size_t bytes)
{
    if (bytes > s->sing.span)
    {
        bytes = s->sing.span;     }
    MMGR_ATOMIC_STORE(&s->head, MMGR_RING_WRAP(s->sing.at + bytes, s->cap));
    segs_drop(s, s->sing.at, s->sing.span);
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen++;
}

MMGR_INLINE uint8_t *sing_put(const InfinCtx *c)
{
    RingState *const s = c->s;
    const size_t n = c->n * s->sing.gran;
    size_t at = MMGR_ATOMIC_LOAD(&s->head);

    if ((n == 0u) || (s->sing.span != 0u) || (n > ((s->cap - 1u) - infin_available(c))))
    {
        return NULL;
    }
    if (infin_room(s, at, n) < n)
    {
        return NULL;
    }

    uint8_t *const first = &s->buf[at];
    const uint8_t *src = c->src;
    size_t left = n;

    while (left > 0u)
    {
        size_t chunk = s->cap - at;
        if (chunk > left)
        {
            chunk = left;
        }
        proxim.read(&s->buf[at], src, chunk);
        at = MMGR_RING_WRAP(at + chunk, s->cap);
        src += chunk;
        left -= chunk;
    }
    MMGR_ATOMIC_STORE(&s->head, at);
    return first;
}

MMGR_INLINE uint8_t *sing_body(const InfinCtx *c)
{
    RingState *const s = c->s;

    if (c->src != NULL)
    {
        return sing_put(c);
    }

    if ((c->tessera != NULL) && (*c->tessera != 0u))
    {
        sing_commit(s, c->off * s->sing.gran);
        *c->tessera = 0u;
        if (c->rearm == 0)
        {
            return NULL;
        }
    }

    uint8_t *const at = sing_claim(s, c->n, c->units);
    if ((at != NULL) && (c->tessera != NULL))
    {
        *c->tessera = MMGR_TESSERA(MMGR_TESSERA_SING, s->sing.gen);
    }
    return at;
}

mmgr_bool mmgr_infin_init(mmgr_ring *r, const RingCfg *c)
{
    MMGR_ASSERT(r != NULL, "a ring needs storage");
    MMGR_ASSERT(c->buf != NULL, "a ring needs a buffer");

    if ((c->cap == 0u) || !MMGR_RING_POW2(c->cap))
    {
        return MMGR_FALSE;
    }
    if ((c->nsegs == 0u) || !MMGR_RING_POW2(c->nsegs) || (c->nsegs > c->cap))
    {
        return MMGR_FALSE;
    }
        if (c->nsegs > (size_t)MMGR_RING_LOCULI_MAX)
    {
        return MMGR_FALSE;
    }

    RingState *const s = ring_of(r);
    s->buf = c->buf;
    s->cap = c->cap;
    s->nsegs = c->nsegs;
    s->seg = c->cap / c->nsegs;
    s->held = c->held;
    atomic_init(&s->head, 0u);
    atomic_init(&s->tail, 0u);
    MMGR_ATOMIC_STORE(s->held, (mmgr_word)0);
    s->ord.base = 0u;
    s->ord.span = c->cap;
    s->ord.off = 0u;
    s->owner = NULL;
    s->open = 0;
    atomic_init(&s->slots, (mmgr_word)0);
    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.at = 0u;
    s->sing.span = 0u;
    s->sing.gen = 0u;
    s->sing.open = 0;
    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        s->drains[i].first = 0u;
        s->drains[i].last = 0u;
        s->drains[i].next = 0u;
        s->drains[i].gen = 0u;
    }
    return MMGR_TRUE;
}

struct MmgrCursor *mmgr_infin_open(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);

        if (s->open != 0)
    {
        return NULL;
    }
    s->ord.off = 0u;
    s->open = 1;
    s->owner = c->owner;
    return &s->ord;
}

const uint8_t *mmgr_infin_drain(const InfinCfg *c)
{
#if !MMGR_ENABLE_KEEPOUT
    (void)c;
    return NULL;
#else
    RingState *const s = ring_of(c->r);

    if ((c->tessera != NULL) && (*c->tessera != 0u))
    {
        Drain *const d = drain_of(s, *c->tessera);
        if (d == NULL)
        {
            return NULL;
        }
        if (d->next >= d->last)
        {
            drain_retire(s, d);
            *c->tessera = 0u;
            return NULL;
        }
        const size_t give = d->next;
        d->next++;
        return &s->buf[give * s->seg];
    }

    if (c->to <= c->from)
    {
        return NULL;
    }

        {
        const size_t tail = MMGR_ATOMIC_LOAD(&s->tail);
        const size_t arrived = MMGR_RING_WRAP(MMGR_ATOMIC_LOAD(&s->head) - tail, s->cap);
        const size_t rel = MMGR_RING_WRAP(c->from - tail, s->cap);

        if ((arrived == 0u) || (rel >= arrived) || ((c->to - c->from) > (arrived - rel)))
        {
            return NULL;
        }
    }

    const size_t first = c->from / s->seg;
    const size_t last = (c->to + s->seg - 1u) / s->seg;
    if (last > s->nsegs)
    {
        return NULL;
    }

    const mmgr_word want = seg_mask(first, last);
    const mmgr_word prev = atomic_fetch_or_explicit(s->held, want, memory_order_acquire);
    if ((prev & want) != 0)
    {
                MMGR_ATOMIC_CLEAR(s->held, want & (mmgr_word)~prev);
        return NULL;
    }

    for (size_t i = 0; i < MMGR_RING_DRAINS; i++)
    {
        const mmgr_word bit = (mmgr_word)((mmgr_word)1 << i);
        const mmgr_word had = atomic_fetch_or_explicit(&s->slots, bit, memory_order_acquire);
        if ((had & bit) != 0)
        {
            continue;
        }
        Drain *const d = &s->drains[i];
        d->first = first;
        d->last = last;
        d->next = first + 1u;
        if (c->tessera != NULL)
        {
            *c->tessera = MMGR_TESSERA(i, d->gen);
        }
        return &s->buf[first * s->seg];
    }

    MMGR_ATOMIC_CLEAR(s->held, want);
    return NULL;
#endif
}

size_t mmgr_infin_available(const InfinCfg *c)
{
    return MMGR_CALL(infin_available, InfinCtx, .s = ring_of(c->r));
}

size_t mmgr_infin_free(const InfinCfg *c)
{
    return MMGR_CALL(infin_free, InfinCtx, .s = ring_of(c->r));
}

mmgr_bool mmgr_infin_read_byte(const InfinCfg *c)
{
    return MMGR_CALL(infin_read_byte, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .dst = c->dst);
}

const uint8_t *mmgr_infin_read(const InfinCfg *c)
{
    return MMGR_CALL(infin_read, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .n = c->n);
}

void mmgr_infin_peek(const InfinCfg *c)
{
    MMGR_CALL(infin_peek, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .dst = c->dst, .n = c->n, .off = c->off);
}

void mmgr_infin_consume(const InfinCfg *c)
{
    MMGR_CALL(infin_consume, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .n = c->n);
}

MMGR_INLINE mmgr_bool sing_admit(RingState *s, const SingularitasCfg *cfg)
{
    if (s->sing.open != 0)
    {
        return (mmgr_bool)(cfg->owner == s->sing.owner);
    }
    if ((cfg->gran == 0u) || !MMGR_RING_POW2(cfg->gran) || (cfg->gran > MMGR_SING_GRANULE_MAX) || (cfg->gran > s->cap))
    {
        return MMGR_FALSE;
    }
    s->sing.owner = cfg->owner;
    s->sing.gran = cfg->gran;
    s->sing.open = 1;
    return MMGR_TRUE;
}

MMGR_INLINE mmgr_bool sing_token_live(const RingState *s, size_t tessera)
{
    return (mmgr_bool)((s->sing.span != 0u) && (MMGR_TESSERA_IDX(tessera) == MMGR_TESSERA_SING) &&
                       (MMGR_TESSERA_GEN(tessera) == s->sing.gen));
}

MMGR_INLINE mmgr_u16 sing_state(const RingState *s, mmgr_u16 why)
{
    const size_t at = (s->sing.span != 0u) ? s->sing.at : MMGR_ATOMIC_LOAD(&s->head);
    mmgr_u16 st = why | ((s->sing.open != 0) ? MMGR_SING_ATTACHED : MMGR_SING_READY);

    if (s->sing.span != 0u)
    {
        st |= MMGR_SING_GRANTED;
    }
    if (ring_vacant(s) < s->sing.gran)
    {
        st |= MMGR_SING_FULL;
    }
    return (mmgr_u16)(st | (mmgr_u16)((at / s->seg) << MMGR_SING_FLAG_BITS));
}

uint8_t *mmgr_infin_singularitas(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);
    const int live = ((c->tessera != NULL) && (*c->tessera != 0u));
        const int asking = ((c->src != NULL) || (c->tessera != NULL));
    mmgr_u16 why = 0u;
    uint8_t *at = NULL;

    if (live && !sing_token_live(s, *c->tessera))
    {
        *c->tessera = 0u;
        why = MMGR_SING_STALE | MMGR_SING_REFUSED;
    }
    else if (!live && ((c->sing == NULL) || !sing_admit(s, c->sing)))
    {
                why = (mmgr_u16)(MMGR_SING_ALIEN | (asking ? MMGR_SING_REFUSED : 0u));
    }
    else if (asking)
    {
        at = MMGR_CALL(sing_body, InfinCtx, .s = s, .src = c->src, .n = c->n, .off = c->off, .tessera = c->tessera,
                       .units = c->units, .rearm = (c->sing != NULL));
        if ((at == NULL) && !(live && (c->sing == NULL)))
        {
            why = MMGR_SING_REFUSED;         }
    }

    if (c->status != NULL)
    {
        *c->status = sing_state(s, why);
    }
    return at;
}

mmgr_bool mmgr_infin_detach(const InfinCfg *c)
{
    RingState *const s = ring_of(c->r);
    mmgr_bool let_go = MMGR_FALSE;

    if ((s->sing.open == 0) || (c->sing == NULL) || (c->sing->owner != s->sing.owner))
    {
        let_go = MMGR_FALSE;
        if (c->status != NULL)
        {
            *c->status = sing_state(s, MMGR_SING_ALIEN | MMGR_SING_REFUSED);
        }
        return let_go;
    }
    if (s->sing.span != 0u)
    {
        if (c->status != NULL)
        {
            *c->status = sing_state(s, MMGR_SING_REFUSED);
        }
        return MMGR_FALSE;
    }

    s->sing.owner = NULL;
    s->sing.gran = 0u;
    s->sing.open = 0;
    s->sing.gen++;
    let_go = MMGR_TRUE;

        if (c->status != NULL)
    {
        *c->status = sing_state(s, 0u);
    }
    return let_go;
}

void mmgr_infin_seek(const InfinCfg *c)
{
    MMGR_CALL(infin_seek, InfinCtx, .s = ring_of(c->r), .cur = c->cur, .off = c->off);
}
