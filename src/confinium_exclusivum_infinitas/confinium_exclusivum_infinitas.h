/**
 * @brief Ring buffer with segment holds, drain runs and one exclusive writer.
 *
 * @note The ring state is opaque; callers declare an mmgr_ring and pass its address.
 * @warning The ring is unusable until mmgr_infin_init has returned MMGR_TRUE for it.
 */
#ifndef MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H
#define MMGR_CONFINIUM_EXCLUSIVUM_INFINITAS_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Reports whether cap is a power of two.
 *
 * @param[in] cap Value to test.
 * @return        Non-zero when cap has at most one bit set.
 * @warning Also reports true for 0; mmgr_infin_init rejects 0 separately.
 */
#define MMGR_RING_POW2(cap) (((cap) & ((cap) - 1)) == 0)

/**
 * @brief Wraps an index into a ring of cap bytes.
 *
 * @param[in] i   Index to wrap.
 * @param[in] cap Ring size.
 * @return        The index masked into range.
 * @warning Correct only because cap is a power of two, which mmgr_infin_init enforces.
 */
#define MMGR_RING_WRAP(i, cap) ((i) & ((cap) - 1))

/**
 * @brief Largest number of segments a ring may have, one per bit of the held word.
 */
#define MMGR_RING_LOCULI_MAX MMGR_WORD_BITS

/**
 * @brief Size of the opaque ring storage, counted in size_t units.
 *
 * @note The implementation asserts its state fits inside this; raise it if that assertion fires.
 */
#define MMGR_RING_WORDS 40u

/**
 * @brief Largest granule an exclusive writer may attach with.
 */
#define MMGR_SING_GRANULE_MAX ((size_t)sizeof(mmgr_word))

/**
 * @brief Status flags a singularitas call reports, all within the low octet.
 *
 * @note READY and ATTACHED are exclusive: the first means no writer is attached, the second that one is.
 * @note GRANTED, FULL, ALIEN, STALE and REFUSED may each appear alongside either of those.
 */
#define MMGR_SING_READY ((mmgr_u16)1 << 0)    /**< No writer is attached. */
#define MMGR_SING_ATTACHED ((mmgr_u16)1 << 1) /**< A writer is attached. */
#define MMGR_SING_GRANTED ((mmgr_u16)1 << 2)  /**< A grant is outstanding. */
#define MMGR_SING_FULL ((mmgr_u16)1 << 3)     /**< Not even one granule would fit. */
#define MMGR_SING_ALIEN ((mmgr_u16)1 << 4)    /**< The caller is not the attached writer. */
#define MMGR_SING_STALE ((mmgr_u16)1 << 5)    /**< The tessera no longer names a live grant. */
#define MMGR_SING_REFUSED ((mmgr_u16)1 << 6)  /**< The request was not carried out. */

/**
 * @brief Width of the flag field, which is also where the segment index starts.
 */
#define MMGR_SING_FLAG_BITS 8u

/**
 * @brief Takes the flags out of a status word.
 *
 * @param[in] st Status word from a singularitas or detach call.
 * @return       The low octet, holding the MMGR_SING_ flags.
 */
#define MMGR_SING_FLAGS(st) ((mmgr_u16)((st) & (mmgr_u16)(((mmgr_u16)1 << MMGR_SING_FLAG_BITS) - 1u)))

/**
 * @brief Takes the segment index out of a status word.
 *
 * @param[in] st Status word from a singularitas or detach call.
 * @return       Index of the segment the grant sits in, or the one at the head when none is outstanding.
 */
#define MMGR_SING_SEG(st) ((size_t)((mmgr_u16)(st) >> MMGR_SING_FLAG_BITS))

/**
 * @brief Storage a caller declares for one ring, whose contents are private to the implementation.
 *
 * @note Aligned to size_t, which is what the state laid inside it needs.
 * @warning Never read opaque directly; every field is reached through the iteratio_infinita calls.
 */
typedef struct
{
    MMGR_ALIGN(sizeof(size_t)) uint8_t opaque[MMGR_RING_WORDS * sizeof(size_t)]; /**< Private ring state. */
} mmgr_ring;

/**
 * @brief A reader's position in the ring, defined only in the implementation.
 */
struct MmgrCursor;

/**
 * @brief Arguments for mmgr_infin_init.
 *
 * @warning Every one of these must outlive the ring, which keeps the buffer and the held word.
 */
typedef struct
{
    mmgr_ring *const ring;        /**< Storage to lay the ring into [BORROWS]. */
    uint8_t *const buf;           /**< Ring bytes [BORROWS]. */
    const size_t cap;             /**< Bytes in buf; must be a non-zero power of two. */
    const size_t nsegs;           /**< Segments to divide the ring into; a power of two, at most cap. */
    _Atomic mmgr_word *const held; /**< One bit per segment, shared with every holder [BORROWS]. */
} RingCfg;

/**
 * @brief Identity and granule of an exclusive writer.
 *
 * @note The owner is compared by address only; nothing it points at is read.
 */
typedef struct
{
    const void *const owner; /**< Writer's identity [BORROWS]. */
    const size_t gran;       /**< Granule size; a power of two, at most MMGR_SING_GRANULE_MAX. */
} SingularitasCfg;

/**
 * @brief Arguments for every iteratio_infinita call bar init; each reads only what it needs.
 *
 * @note Members left unset are zero, and the calls that ignore them never read them.
 */
typedef struct
{
    mmgr_ring *const ring;             /**< Ring to act on [BORROWS]. */
    struct MmgrCursor *const cur;      /**< Cursor for seek [BORROWS]. */
    uint8_t *const dst;                /**< Destination for read_byte and peek [BORROWS]. */
    const uint8_t *const src;          /**< Bytes for singularitas to publish at once [BORROWS]. */
    const size_t bytes;                /**< Byte count, or granule count on the singularitas path. */
    const size_t off;                  /**< Peek offset, seek position, or granules written on commit. */
    const size_t from;                 /**< First byte of a drain run. */
    const size_t to;                   /**< One past the last byte of a drain run. */
    size_t *const tessera;             /**< Caller's tessera, read and rewritten in place [BORROWS]. */
    const void *const owner;           /**< Identity opening the cursor [BORROWS]. */
    const SingularitasCfg *const sing; /**< Attachment request for singularitas and detach [BORROWS]. */
    size_t *const units;               /**< Set to the granules actually granted [BORROWS]. */
    mmgr_u16 *const status;            /**< Set to the packed status word [BORROWS]. */
} InfinCfg;

/**
 * @brief Type of the iteratio_infinita dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the twelve members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_bool (*init)(const RingCfg *c);           /**< Lays a fresh ring into caller storage. */
    struct MmgrCursor *(*open)(const InfinCfg *c); /**< Hands out the ring's one cursor. */
    const uint8_t *(*drain)(const InfinCfg *c);    /**< Starts or steps a drain run. */
    size_t (*available)(const InfinCfg *c);        /**< Bytes waiting to be read. */
    size_t (*vacant)(const InfinCfg *c);           /**< Bytes still free to write. */
    mmgr_bool (*read_byte)(const InfinCfg *c);     /**< Takes one byte and advances the tail. */
    const uint8_t *(*read)(const InfinCfg *c);     /**< Points at readable bytes without consuming. */
    void (*peek)(const InfinCfg *c);               /**< Copies bytes out without consuming. */
    void (*consume)(const InfinCfg *c);            /**< Advances the tail. */
    uint8_t *(*singularitas)(const InfinCfg *c);   /**< Drives the exclusive writer. */
    mmgr_bool (*detach)(const InfinCfg *c);        /**< Releases the exclusive writer. */
    void (*seek)(const InfinCfg *c);               /**< Moves a cursor within its frame. */
} InfinitasNs;
MMGR_NS_LAYOUT(InfinitasNs, init, open, drain, available, vacant, read_byte, read, peek, consume, singularitas, detach,
               seek);

/**
 * @brief Lays a fresh ring into c->ring and clears every counter.
 *
 * @param[in,out] c Storage, buffer, sizes and the held word [BORROWS].
 * @return          MMGR_TRUE when the ring is ready, MMGR_FALSE when a size was rejected.
 * @note Refuses a cap or nsegs that is 0 or not a power of two, an nsegs above cap,
 *       and an nsegs above MMGR_RING_LOCULI_MAX.
 * @warning c->buf and c->held are kept by the ring, so both must outlive it [BORROWS].
 */
mmgr_bool mmgr_infin_init(const RingCfg *c);

/**
 * @brief Hands out the ring's cursor, rewound to the start of its frame.
 *
 * @param[in,out] c Ring and the identity taking the cursor [BORROWS].
 * @return          The cursor, or NULL when it is already out [BORROWS].
 * @warning There is one cursor per ring, and no call gives it back.
 */
struct MmgrCursor *mmgr_infin_open(const InfinCfg *c);

/**
 * @brief Starts a drain run over c->from to c->to, or steps the one the tessera names.
 *
 * @param[in,out] c Ring, the range, and the caller's tessera [BORROWS].
 * @return          Start of the segment handed out, or NULL [BORROWS].
 * @note A zero tessera starts a run; a non-zero one steps it and clears the tessera when it finishes.
 * @note The run holds its segments until it finishes, keeping other writers out of them.
 * @warning Returns NULL and touches nothing when MMGR_ENABLE_KEEPOUT is 0.
 */
const uint8_t *mmgr_infin_drain(const InfinCfg *c);

/**
 * @brief Returns the bytes waiting to be read.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      Distance from the tail to the head, wrapped into the ring.
 * @warning Only a snapshot: a concurrent producer may add more before the caller acts on it.
 */
size_t mmgr_infin_available(const InfinCfg *c);

/**
 * @brief Returns the bytes still free to write.
 *
 * @param[in] c Ring to inspect [BORROWS].
 * @return      Free bytes, less one so the head and tail stay apart, less any outstanding grant.
 * @warning Only a snapshot: a concurrent consumer may free more before the caller acts on it.
 */
size_t mmgr_infin_vacant(const InfinCfg *c);

/**
 * @brief Takes one byte into c->dst and advances the tail past it.
 *
 * @param[in,out] c Ring and the destination byte [BORROWS].
 * @return          MMGR_TRUE when a byte was taken, MMGR_FALSE when the ring was empty.
 * @note Writes through c->dst only when it returns MMGR_TRUE.
 */
mmgr_bool mmgr_infin_read_byte(const InfinCfg *c);

/**
 * @brief Points at c->bytes contiguous readable bytes, leaving the tail alone.
 *
 * @param[in] c Ring and the byte count wanted [BORROWS].
 * @return      Address inside the ring buffer, or NULL [BORROWS].
 * @note Returns NULL when the ring is empty, when fewer bytes are available, or when the run would wrap.
 * @warning The bytes stay valid only until the tail advances; call mmgr_infin_consume when done with them.
 */
const uint8_t *mmgr_infin_read(const InfinCfg *c);

/**
 * @brief Copies c->bytes from c->off past the tail into c->dst, leaving the tail alone.
 *
 * @param[in,out] c Ring, destination, byte count and starting offset [BORROWS].
 * @note Wraps at the end of the buffer, so unlike mmgr_infin_read it handles a split run.
 * @warning Copies c->bytes whether or not that many are available; check mmgr_infin_available first.
 */
void mmgr_infin_peek(const InfinCfg *c);

/**
 * @brief Advances the tail past c->bytes.
 *
 * @param[in,out] c Ring and the byte count to drop [BORROWS].
 * @warning Advances whether or not that many were available; check mmgr_infin_available first.
 */
void mmgr_infin_consume(const InfinCfg *c);

/**
 * @brief Drives the exclusive writer: attach, grant, commit and report, in one call.
 *
 * @param[in,out] c Ring, plus whichever of sing, src, tessera, bytes, off, units and status apply [BORROWS].
 * @return          Start of a granted or written run, or NULL [BORROWS].
 * @note With c->src set, the bytes are copied in and published at once, and nothing is granted.
 * @note With a live c->tessera, c->off granules are committed first; a fresh grant follows only when c->sing is also set.
 * @note A successful grant writes a new tessera through c->tessera and the granule count through c->units.
 * @note With neither src nor tessera, nothing changes and only the status is reported.
 * @warning c->status receives the packed state on every path, including refusals; read it with MMGR_SING_FLAGS.
 * @warning A returned run is valid only until it is committed by the next call carrying the tessera.
 */
uint8_t *mmgr_infin_singularitas(const InfinCfg *c);

/**
 * @brief Releases the exclusive writer attachment.
 *
 * @param[in,out] c Ring, the attachment to release, and where to report state [BORROWS].
 * @return          MMGR_TRUE when the attachment was released.
 * @note Refuses with ALIEN and REFUSED when no writer is attached or c->sing->owner does not match.
 * @note Refuses with REFUSED while a grant is still outstanding; commit it first.
 * @warning c->status receives the packed state on every path, including refusals.
 */
mmgr_bool mmgr_infin_detach(const InfinCfg *c);

/**
 * @brief Moves c->cur to c->off within its frame.
 *
 * @param[in,out] c Ring, the cursor and the new position [BORROWS].
 * @warning c->off must not exceed the cursor's span.
 */
void mmgr_infin_seek(const InfinCfg *c);

/**
 * @brief Dispatch table instance named iteratio_infinita; each member calls the matching mmgr_infin_ function.
 */
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
