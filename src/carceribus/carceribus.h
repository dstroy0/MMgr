/**
 * @brief Double-ended pool: its region machinery, its state, its arguments, and the carcer table.
 *
 * @note One roof: the region machinery that carves the pools, the two ends that hand bytes out, and
 *       the accessors that report on them are all here. Spans over those bytes are spatium's.
 * @note A tenancy is taken with persist_capio and given back with one of two calls. They differ in
 *       exactly one thing: secura_reddo zeroes the bytes first and persist_reddo does not. The
 *       guarantee is in the name rather than a flag, so a caller cannot ask for a wipe and not get
 *       one.
 * @note The pool's own address is the identity of whoever holds it, so a tenancy needs nothing
 *       carried beside it to say whose it is.
 */
#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Set to 1 to track each pool's high-water figure in CarcerCtx::hw.
 *
 * @note Adds the hw member to CarcerCtx and the blend that maintains it in both carcer capio calls.
 * @note A build sets this before including this header, the way it sets any other knob here.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif

/**
 * @brief Largest number of pools one region may be carved into.
 *
 * @note Sizes the pool array in MMGR_CARCER_MACHINERY, and bounds the region macros.
 */
#ifndef MMGR_CARCER_MAX_REGIONS
#define MMGR_CARCER_MAX_REGIONS 2u
#endif

/**
 * @brief Alignment every tenancy is handed out at, which is one machine word.
 *
 * @note Derived from the word rather than named as a number, so a build at another width gets the
 *       alignment that width actually needs.
 * @note The region itself arrives aligned to MMGR_ALIGN_BYTES from the caller, so what this rounds
 *       is only the running offsets inside a pool.
 */
#define MMGR_CARCER_ALIGN ((size_t)sizeof(mmgr_word))

MMGR_STATIC_ASSERT((MMGR_CARCER_ALIGN & (MMGR_CARCER_ALIGN - 1u)) == 0u,
                   "the pool rounds offsets by masking, which needs a power of two alignment");
MMGR_STATIC_ASSERT((MMGR_ALIGN_BYTES & (MMGR_ALIGN_BYTES - 1u)) == 0u,
                   "MMGR_CARCER_CHECK tests pool sizes by masking, which needs a power of two");
MMGR_STATIC_ASSERT(MMGR_ALIGN_BYTES >= MMGR_CARCER_ALIGN,
                   "a region aligned less than a tenancy would hand out addresses the pool cannot");



/**
 * @brief Address and extent of a whole region.
 *
 * @note Set by the mmgr_carcer_init macro to the region's bytes array and its size.
 */
typedef struct
{
    uint8_t *const at; /**< First byte of the region [BORROWS]. */
    const size_t size; /**< Bytes in the region. */
} CarcerInit;

/**
 * @brief One pool's state: its bytes and the two ends that grow toward each other.
 *
 * @note Set up by the mmgr_carcer_init macro, which starts interim_top at the pool size and leaves
 *       every counter at 0. The base arrives aligned to MMGR_ALIGN_BYTES from the region macro, so
 *       nothing here has to align it at run time.
 * @note persist_end bounds the block chain rather than counting what is held: a freed block inside
 *       the chain stays in it until a release trims the end.
 */
typedef struct
{
    uint8_t *const base; /**< First byte of the pool [BORROWS]. */
    const size_t size;   /**< Bytes in the pool. */
    size_t persist_end;  /**< Offset just past the last persistent block, counting up from base. */
    size_t interim_top;  /**< Offset of the lowest interim byte, counting down from size. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    size_t persist_hw;   /**< Running maximum of persist_end. */
    size_t interim_hw;   /**< Running maximum of the bytes taken from the top. */
#endif
} CarcerCtx;

/**
 * @brief Members every carved region carries ahead of its bytes.
 *
 * @note init records the whole region; pool is sized by MMGR_CARCER_MAX_REGIONS.
 * @note No mark array: an interim mark is a value the caller holds and hands back, so savepoints
 *       nest and the region carries no storage for them.
 */
#define MMGR_CARCER_MACHINERY                                                                                          \
    const CarcerInit init;                                                                                             \
    CarcerCtx pool[MMGR_CARCER_MAX_REGIONS]

/**
 * @brief Pairs a pool name with its size for mmgr_carcer_init.
 *
 * @param[in] name_ Enumerator name to give the pool.
 * @param[in] n_    Bytes to give the pool.
 * @note Expands to two comma-separated arguments, so each pair counts as two toward MMGR_NARG.
 */
#define MMGR_POOL(name_, n_) name_, n_

/**
 * @brief Asserts a region has both an address and an extent.
 *
 * @param[in] region_ Region name, whose layout type is region_##_layout.
 * @param[in] n_      Bytes the region was declared with.
 */
#define MMGR_CARCER_EXISTS(region_, n_)                                                                                \
    MMGR_STATIC_ASSERT(sizeof(((region_##_layout *)0)->bytes) != 0u, #region_ " has no address");                      \
    MMGR_STATIC_ASSERT((n_) != 0u, #region_ " has no extent")

/**
 * @brief Asserts one pool is large enough, aligned, and inside its region.
 *
 * @param[in] region_ Region the pool is carved from.
 * @param[in] name_   Pool name, used in the assertion messages.
 * @param[in] off_    Byte offset of the pool within the region.
 * @param[in] n_      Bytes given to the pool.
 * @note Requires at least two MMGR_ALIGN_BYTES, an exact multiple of MMGR_ALIGN_BYTES, and off_ plus n_ within bounds.
 */
#define MMGR_CARCER_CHECK(region_, name_, off_, n_)                                                                    \
    MMGR_STATIC_ASSERT((n_) >= (2u * MMGR_ALIGN_BYTES), #name_ " is too small to hold a block");                       \
    MMGR_STATIC_ASSERT(((n_) & (MMGR_ALIGN_BYTES - 1u)) == 0u, #name_ " is not a whole number of aligned units");      \
    MMGR_STATIC_ASSERT(((off_) + (n_)) <= sizeof(((region_##_layout *)0)->bytes),                                      \
                       #name_ " does not fit inside " #region_)

/**
 * @brief Defines a region carved into two pools, with its layout type, enumerators and storage.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] a_      Enumerator name for the first pool.
 * @param[in] an_     Bytes in the first pool.
 * @param[in] b_      Enumerator name for the second pool.
 * @param[in] bn_     Bytes in the second pool.
 * @note Each pool starts with interim_top at its own size, so the interim end begins empty.
 * @note Selected by mmgr_carcer_init when it is given two MMGR_POOL pairs, which is four arguments.
 */
#define MMGR_CARCER_R4(region_, n_, a_, an_, b_, bn_)                                                                  \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_MACHINERY;                                                                                         \
        MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[(n_)];                                                              \
    } region_##_layout;                                                                                                \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        b_ = 1,                                                                                                        \
        region_##_count = 2                                                                                            \
    };                                                                                                                 \
    MMGR_STATIC_ASSERT(region_##_count <= MMGR_CARCER_MAX_REGIONS, #region_ " carves past its limit");                 \
    MMGR_CARCER_EXISTS(region_, n_);                                                                                   \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    MMGR_CARCER_CHECK(region_, b_, an_, bn_);                                                                          \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {{.base = region_.bytes + 0, .size = (an_), .interim_top = (an_)},             \
                                         {.base = region_.bytes + (an_), .size = (bn_), .interim_top = (bn_)}}}

/**
 * @brief Defines a region carved into one pool, with its layout type, enumerator and storage.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] a_      Enumerator name for the pool.
 * @param[in] an_     Bytes in the pool.
 * @note The pool starts with interim_top at its own size, so the interim end begins empty.
 * @note Selected by mmgr_carcer_init when it is given one MMGR_POOL pair, which is two arguments.
 */
#define MMGR_CARCER_R2(region_, n_, a_, an_)                                                                           \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_MACHINERY;                                                                                         \
        MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[(n_)];                                                              \
    } region_##_layout;                                                                                                \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        region_##_count = 1                                                                                            \
    };                                                                                                                 \
    MMGR_STATIC_ASSERT(region_##_count <= MMGR_CARCER_MAX_REGIONS, #region_ " carves past its limit");                 \
    MMGR_CARCER_EXISTS(region_, n_);                                                                                   \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {{.base = region_.bytes + 0, .size = (an_), .interim_top = (an_)}}}

/**
 * @brief Defines a region and carves it into pools, picking the shape from the argument count.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] ...     One or two MMGR_POOL pairs, giving two or four arguments.
 * @note The pair count selects MMGR_CARCER_R2 or MMGR_CARCER_R4 through MMGR_CAT and MMGR_NARG.
 * @warning Defines the region object itself, so it belongs at file scope in exactly one translation unit.
 */
#define mmgr_carcer_init(region_, n_, ...) MMGR_CAT(MMGR_CARCER_R, MMGR_NARG(__VA_ARGS__))(region_, n_, __VA_ARGS__)

/**
 * @brief Arguments for every carcer call; each reads only what it needs.
 *
 * @note Members left unset are zero, and the calls that ignore them never read them.
 */
typedef struct
{
    CarcerCtx *const pool;  /**< Pool to act on [BORROWS]. */
    const size_t size;      /**< Byte count for the capio, reddo and wipe calls. */
    const void *const at;   /**< Address owns tests, which it reads the value of alone [BORROWS]. */
    void *const tenancy;    /**< Bytes a wipe clears, which it writes through [BORROWS]. */
    const size_t mark;      /**< Interim top interim_reddo restores, as interim_mark returned it. */
} CarcerCfg;

/**
 * @brief Takes the address of one pool inside a region.
 *
 * @param[in] region_ Region object defined by mmgr_carcer_init.
 * @param[in] pool_   Pool enumerator from that region.
 * @return            Address of the pool's CarcerCtx [BORROWS].
 */
#define MMGR_CARCER_POOL(region_, pool_) (&(region_).pool[pool_])

/**
 * @brief Builds a CarcerCfg for one pool, ready to pass to a carcer call.
 *
 * @param[in] region_ Region object defined by mmgr_carcer_init.
 * @param[in] pool_   Pool enumerator from that region.
 * @param[in] ...     Further designated initializers, such as .size or .at.
 * @return            Address of the compound literal [BORROWS].
 * @warning The callee receives the address of a compound literal [BORROWS].
 */
#define MMGR_CARCER(region_, pool_, ...) (&(CarcerCfg){.pool = MMGR_CARCER_POOL(region_, pool_), __VA_ARGS__})

/**
 * @brief Type of the carcer dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the eleven members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note No entry here only reads a member: CarcerCtx is a type the caller declares and holds, so a
 *       call that returned pool->persist_end would be a second way to spell what the caller can
 *       already read. What is here is what does something.
 */
typedef struct
{
    void *(*persist_capio)(const CarcerCfg *c);  /**< Takes size bytes from the bottom, zeroed. */
    void (*persist_reddo)(const CarcerCfg *c);   /**< Gives a tenancy back, unwiped. */
    void (*secura_reddo)(const CarcerCfg *c);    /**< Zeroes a tenancy, then gives it back. */
    void *(*interim_capio)(const CarcerCfg *c);  /**< Takes size bytes from the top. */
    size_t (*interim_mark)(const CarcerCfg *c);  /**< Returns the current top, for interim_reddo. */
    void (*interim_reddo)(const CarcerCfg *c);   /**< Restores the top a mark reported. */
    void (*interim_reset)(const CarcerCfg *c);   /**< Gives the whole interim end back at once. */
    mmgr_bool (*owns)(const CarcerCfg *c);       /**< Tests whether at lies in the pool's bytes. */
    size_t (*octas_praesto)(const CarcerCfg *c); /**< Bytes between the two ends. */
    void (*wipe)(const CarcerCfg *c);            /**< Zeroes size bytes at tenancy. */
    size_t (*align_up)(const CarcerCfg *c);      /**< Rounds size up to a whole machine word. */
} CarceribusNs;
MMGR_NS_LAYOUT(CarceribusNs, persist_capio, persist_reddo, secura_reddo, interim_capio, interim_mark, interim_reddo,
               interim_reset, owns, octas_praesto, wipe, align_up);


/**
 * @brief Takes c->size bytes from the persistent end and hands them back zeroed.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note First fit over the blocks already in the chain, splitting one large enough to leave another
 *       header and a payload behind; otherwise a fresh block is carved from the free middle.
 * @note The bytes come back zeroed, so a persistent tenant never reads what the last one left. The
 *       interim end does not do this, which is one of the two things that separate the ends.
 * @note A c->size of 0 is taken as one machine word, so every tenancy has an address of its own.
 * @note Fails closed: a request that would cross the interim end returns NULL rather than trespassing.
 */
void *mmgr_carcer_persist_capio(const CarcerCfg *c);

/**
 * @brief Gives the tenancy at c->tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS].
 * @note Frees by address rather than by count: the block's own header carries its size, so releases
 *       need not unwind in any particular order. That is the long life the persistent end is for.
 * @note Adjacent free blocks are merged, and a free block at the end of the chain is handed back to
 *       the free middle, so the two ends recover the space between them.
 * @note A NULL c->tenancy does nothing.
 * @warning Leaves the bytes untouched; mmgr_carcer_secura_reddo is the call that guarantees a wipe.
 */
void mmgr_carcer_persist_reddo(const CarcerCfg *c);

/**
 * @brief Zeroes the tenancy at c->tenancy, then gives it back.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS].
 * @note The only difference from mmgr_carcer_persist_reddo is the wipe, and the guarantee is in the
 *       name rather than a flag, so a caller cannot ask for a wipe and not get one.
 * @note The extent wiped is the block's own, read from its header, so a caller cannot under-wipe a
 *       tenancy by naming fewer bytes than it holds.
 * @note A NULL c->tenancy does nothing.
 */
void mmgr_carcer_secura_reddo(const CarcerCfg *c);

/**
 * @brief Lowers the interim end by c->size, rounded up to a whole machine word.
 *
 * @param[in,out] c Pool and the byte count wanted [BORROWS].
 * @return          Start of the lowered region, or NULL when it would cross the persistent end [BORROWS].
 * @note A bump, with no header and no per-tenancy release: the whole run comes back at once through
 *       a mark or a reset. That is the function-length life the interim end is for.
 * @warning The bytes are not zeroed. A tenant that must not read what the last one left belongs at
 *          the persistent end, or must clear them itself.
 */
void *mmgr_carcer_interim_capio(const CarcerCfg *c);

/**
 * @brief Returns the pool's current interim top.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The value to hand back to mmgr_carcer_interim_reddo.
 * @note The caller holds the mark, so savepoints nest: an inner one does not disturb an outer one.
 * @note Writes nothing.
 */
size_t mmgr_carcer_interim_mark(const CarcerCfg *c);

/**
 * @brief Assigns the interim top the value c->mark carries.
 *
 * @param[in,out] c Pool and the mark to restore [BORROWS].
 * @note Gives back everything taken from the top since that mark, in one step.
 */
void mmgr_carcer_interim_reddo(const CarcerCfg *c);

/**
 * @brief Gives the whole interim end back at once.
 *
 * @param[in,out] c Pool to act on [BORROWS].
 * @note The same step as mmgr_carcer_interim_reddo against the pool's own size, named because it is
 *       what the end of a dispatch does and it should not have to reach into the pool to say so.
 * @note The persistent end is not written.
 */
void mmgr_carcer_interim_reset(const CarcerCfg *c);

/**
 * @brief Returns whether c->at lies in the pool's bytes.
 *
 * @param[in] c Pool and the address to test [BORROWS].
 * @return      MMGR_TRUE when c->at is at or after base and before base plus size.
 * @note Writes nothing.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCfg *c);

/**
 * @brief Returns the bytes lying between the two ends.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      interim_top minus persist_end, or 0 when they have met.
 * @note The raw gap, which is what an interim request is measured against. A persistent request also
 *       needs a block header out of it, so ask for what you want and test the answer for NULL rather
 *       than trying to predict it from this.
 * @note Writes nothing.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCfg *c);

/**
 * @brief Writes zeros over c->size bytes at c->tenancy.
 *
 * @param[in,out] c Address and extent to clear [BORROWS].
 * @note Reached on its own for a caller that wants bytes cleared without giving them back.
 * @note The stores are volatile, so the clearing survives however dead the bytes look afterwards.
 * @warning c->tenancy must carry the alignment every address this module hands out already has.
 */
void mmgr_carcer_wipe(const CarcerCfg *c);

/**
 * @brief Rounds c->size up to a whole machine word.
 *
 * @param[in] c The count to round [BORROWS].
 * @return      c->size raised to the next multiple of MMGR_CARCER_ALIGN.
 * @note The same rounding both ends apply, exposed so a caller can size a buffer the way a pool will.
 */
size_t mmgr_carcer_align_up(const CarcerCfg *c);


/**
 * @brief Dispatch table instance named carcer; each member calls the matching mmgr_carcer_ function.
 */
MMGR_NS CarceribusNs carcer MMGR_UNUSED = {
    .persist_capio = mmgr_carcer_persist_capio,
    .persist_reddo = mmgr_carcer_persist_reddo,
    .secura_reddo = mmgr_carcer_secura_reddo,
    .interim_capio = mmgr_carcer_interim_capio,
    .interim_mark = mmgr_carcer_interim_mark,
    .interim_reddo = mmgr_carcer_interim_reddo,
    .interim_reset = mmgr_carcer_interim_reset,
    .owns = mmgr_carcer_owns,
    .octas_praesto = mmgr_carcer_octas_praesto,
    .wipe = mmgr_carcer_wipe,
    .align_up = mmgr_carcer_align_up,
};


MMGR_FINIS_DECLS

#endif
