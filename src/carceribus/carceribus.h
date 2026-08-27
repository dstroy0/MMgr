/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Double-ended pool: its region machinery, its state, its arguments, and the carcer table.
 *
 * @note Holds the region machinery, the two ends, and the accessors. Spans over those bytes are
 *       spatium's.
 * @note Two calls give a tenancy back and differ in one thing: secura_reddo zeroes the bytes first,
 *       persist_reddo does not. The guarantee is in the name, not a flag.
 * @note The pool's address identifies whoever holds it, so a tenancy carries nothing beside it.
 */
#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Set to 1 to track each end's high-water figure.
 *
 * @note Adds CarcerCtx::persist_hw and CarcerCtx::interim_hw, and the blend that raises them in
 *       carcer_grow, which both takes reach. One figure per end, so neither is a maximum over the other.
 * @note A build sets this before including this header, the way it sets any other knob here.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif

/**
 * @brief Largest number of pools one region may be carved into.
 *
 * @note A ceiling only. Each region sizes its pool array from its own count, so raising this costs a
 *       region that does not use it nothing.
 * @note Eight is where MMGR_NARG runs out: each MMGR_POOL pair is two arguments, the table reaches
 *       24, and eight is the largest power of two under that.
 */
#ifndef MMGR_CARCER_MAX_REGIONS
#define MMGR_CARCER_MAX_REGIONS 8u
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
 * @param[in] count_ Pools this region carves, which is what sizes its pool array.
 * @note init records the whole region. The array is sized by count_, not by MMGR_CARCER_MAX_REGIONS,
 *       so a one pool region carries one CarcerCtx.
 * @note No mark array: an interim mark is a value the caller holds, so savepoints nest and the region
 *       stores nothing for them.
 */
#define MMGR_CARCER_MACHINERY(count_)                                                                                  \
    const CarcerInit init;                                                                                             \
    CarcerCtx pool[count_]

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
 * @note Requires a power of two size of at least two MMGR_ALIGN_BYTES, and off_ plus n_ within bounds.
 * @note The power of two lets an offset inside a pool be masked rather than divided, and puts each
 *       pool's base on its own size, so the next pool's offset needs no rounding.
 */
#define MMGR_CARCER_CHECK(region_, name_, off_, n_)                                                                    \
    MMGR_STATIC_ASSERT((n_) >= (2u * MMGR_ALIGN_BYTES), #name_ " is too small to hold a block");                       \
    MMGR_STATIC_ASSERT(((n_) & ((n_) - 1u)) == 0u, #name_ " is not a power of two");                                   \
    MMGR_STATIC_ASSERT(((n_) & (MMGR_ALIGN_BYTES - 1u)) == 0u, #name_ " is not a whole number of aligned units");      \
    MMGR_STATIC_ASSERT(((off_) + (n_)) <= sizeof(((region_##_layout *)0)->bytes),                                      \
                       #name_ " does not fit inside " #region_)

/**
 * @brief One pool's entry in a region's pool array.
 *
 * @param[in] region_ Region the pool is carved from.
 * @param[in] off_    Byte offset of the pool within the region.
 * @param[in] n_      Bytes given to the pool.
 * @note interim_top starts at the pool's own size, so the interim end begins empty.
 */
#define MMGR_CARCER_SEAT(region_, off_, n_)                                                                            \
    {                                                                                                                  \
        .base = region_.bytes + (off_), .size = (n_), .interim_top = (n_)                                              \
    }

/**
 * @brief The layout type, the storage assertions and the pool count assertion every region shares.
 *
 * @param[in] region_ Name of the region object being defined.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] count_  Pools it carves.
 */
#define MMGR_CARCER_HEAD(region_, n_, count_)                                                                          \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_MACHINERY(count_);                                                                                 \
        MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[(n_)];                                                              \
    } region_##_layout;                                                                                                \
    MMGR_STATIC_ASSERT((count_) <= MMGR_CARCER_MAX_REGIONS, #region_ " carves past MMGR_CARCER_MAX_REGIONS");          \
    MMGR_CARCER_EXISTS(region_, n_)

/**
 * @brief Refuses a pool count that is not a power of two, by name.
 *
 * @param[in] count_ The count that was asked for.
 * @note The counts between the powers of two are defined to land here, so the build reports the real
 *       problem instead of an undeclared MMGR_CARCER_Rn.
 */
#define MMGR_CARCER_POOLS_NOT_P2(count_)                                                                               \
    MMGR_STATIC_ASSERT(0, "a region carves a power of two number of pools, and " #count_ " is not one")

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
    MMGR_CARCER_HEAD(region_, n_, 2);                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        b_ = 1,                                                                                                        \
        region_##_count = 2                                                                                            \
    };                                                                                                                 \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    MMGR_CARCER_CHECK(region_, b_, an_, bn_);                                                                          \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {MMGR_CARCER_SEAT(region_, 0, an_),                                            \
                                         MMGR_CARCER_SEAT(region_, (an_), bn_)}}

/**
 * @brief Defines a region carved into four pools, with its layout type, enumerators and storage.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] a_      Enumerator name for the first pool.
 * @param[in] an_     Bytes in the first pool.
 * @param[in] b_      Enumerator name for the second pool.
 * @param[in] bn_     Bytes in the second pool.
 * @param[in] c_      Enumerator name for the third pool.
 * @param[in] cn_     Bytes in the third pool.
 * @param[in] d_      Enumerator name for the fourth pool.
 * @param[in] dn_     Bytes in the fourth pool.
 * @note Each pool starts at the sum of the sizes ahead of it, and every size is a power of two, so
 *       no offset here needs rounding.
 * @note Selected by mmgr_carcer_init when it is given four MMGR_POOL pairs, which is eight arguments.
 */
#define MMGR_CARCER_R8(region_, n_, a_, an_, b_, bn_, c_, cn_, d_, dn_)                                                \
    MMGR_CARCER_HEAD(region_, n_, 4);                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        b_ = 1,                                                                                                        \
        c_ = 2,                                                                                                        \
        d_ = 3,                                                                                                        \
        region_##_count = 4                                                                                            \
    };                                                                                                                 \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    MMGR_CARCER_CHECK(region_, b_, an_, bn_);                                                                          \
    MMGR_CARCER_CHECK(region_, c_, (an_) + (bn_), cn_);                                                                \
    MMGR_CARCER_CHECK(region_, d_, (an_) + (bn_) + (cn_), dn_);                                                        \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {MMGR_CARCER_SEAT(region_, 0, an_),                                            \
                                         MMGR_CARCER_SEAT(region_, (an_), bn_),                                        \
                                         MMGR_CARCER_SEAT(region_, (an_) + (bn_), cn_),                                \
                                         MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_), dn_)}}

/**
 * @brief Defines a region carved into eight pools, with its layout type, enumerators and storage.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] a_      Enumerator name for the first pool.
 * @param[in] an_     Bytes in the first pool.
 * @param[in] b_      Enumerator name for the second pool.
 * @param[in] bn_     Bytes in the second pool.
 * @param[in] c_      Enumerator name for the third pool.
 * @param[in] cn_     Bytes in the third pool.
 * @param[in] d_      Enumerator name for the fourth pool.
 * @param[in] dn_     Bytes in the fourth pool.
 * @param[in] e_      Enumerator name for the fifth pool.
 * @param[in] en_     Bytes in the fifth pool.
 * @param[in] f_      Enumerator name for the sixth pool.
 * @param[in] fn_     Bytes in the sixth pool.
 * @param[in] g_      Enumerator name for the seventh pool.
 * @param[in] gn_     Bytes in the seventh pool.
 * @param[in] h_      Enumerator name for the eighth pool.
 * @param[in] hn_     Bytes in the eighth pool.
 * @note Each pool starts at the sum of the sizes ahead of it, and every size is a power of two, so
 *       no offset here needs rounding.
 * @note The largest carve: MMGR_NARG's table reaches 24, and sixteen arguments is the largest power
 *       of two count under it.
 * @note Selected by mmgr_carcer_init when it is given eight MMGR_POOL pairs, which is sixteen arguments.
 */
#define MMGR_CARCER_R16(region_, n_, a_, an_, b_, bn_, c_, cn_, d_, dn_, e_, en_, f_, fn_, g_, gn_, h_, hn_)           \
    MMGR_CARCER_HEAD(region_, n_, 8);                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        b_ = 1,                                                                                                        \
        c_ = 2,                                                                                                        \
        d_ = 3,                                                                                                        \
        e_ = 4,                                                                                                        \
        f_ = 5,                                                                                                        \
        g_ = 6,                                                                                                        \
        h_ = 7,                                                                                                        \
        region_##_count = 8                                                                                            \
    };                                                                                                                 \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    MMGR_CARCER_CHECK(region_, b_, an_, bn_);                                                                          \
    MMGR_CARCER_CHECK(region_, c_, (an_) + (bn_), cn_);                                                                \
    MMGR_CARCER_CHECK(region_, d_, (an_) + (bn_) + (cn_), dn_);                                                        \
    MMGR_CARCER_CHECK(region_, e_, (an_) + (bn_) + (cn_) + (dn_), en_);                                                \
    MMGR_CARCER_CHECK(region_, f_, (an_) + (bn_) + (cn_) + (dn_) + (en_), fn_);                                        \
    MMGR_CARCER_CHECK(region_, g_, (an_) + (bn_) + (cn_) + (dn_) + (en_) + (fn_), gn_);                                \
    MMGR_CARCER_CHECK(region_, h_, (an_) + (bn_) + (cn_) + (dn_) + (en_) + (fn_) + (gn_), hn_);                        \
    region_##_layout region_ = {                                                                                       \
        .init = {.at = region_.bytes, .size = (n_)},                                                                   \
        .pool = {MMGR_CARCER_SEAT(region_, 0, an_),                                                                    \
                 MMGR_CARCER_SEAT(region_, (an_), bn_),                                                                \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_), cn_),                                                        \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_), dn_),                                                \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_) + (dn_), en_),                                        \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_) + (dn_) + (en_), fn_),                                \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_) + (dn_) + (en_) + (fn_), gn_),                        \
                 MMGR_CARCER_SEAT(region_, (an_) + (bn_) + (cn_) + (dn_) + (en_) + (fn_) + (gn_), hn_)}}

/**
 * @brief The pool counts between the powers of two, each refusing itself with a message.
 *
 * @note Three, five, six and seven pools.
 */
#define MMGR_CARCER_R6(...) MMGR_CARCER_POOLS_NOT_P2(3)
#define MMGR_CARCER_R10(...) MMGR_CARCER_POOLS_NOT_P2(5)
#define MMGR_CARCER_R12(...) MMGR_CARCER_POOLS_NOT_P2(6)
#define MMGR_CARCER_R14(...) MMGR_CARCER_POOLS_NOT_P2(7)

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
    MMGR_CARCER_HEAD(region_, n_, 1);                                                                                  \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        region_##_count = 1                                                                                            \
    };                                                                                                                 \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {MMGR_CARCER_SEAT(region_, 0, an_)}}

/**
 * @brief Defines a region and carves it into pools, picking the shape from the argument count.
 *
 * @param[in] region_ Name of the region object to define.
 * @param[in] n_      Bytes in the whole region.
 * @param[in] ...     One, two, four or eight MMGR_POOL pairs, giving two, four, eight or sixteen arguments.
 * @note The pair count selects MMGR_CARCER_R2, R4, R8 or R16 through MMGR_CAT and MMGR_NARG.
 * @note The pool count must be a power of two, and so must every pool size. See MMGR_CARCER_CHECK.
 * @note A count between the powers of two lands on a refusing macro that names it. Above eight there
 *       is no macro, which is where MMGR_NARG's table runs out.
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
    const void *const at;   /**< Address owns tests; only its value is read, never its target [BORROWS]. */
    void *const tenancy;    /**< Tenancy a wipe clears [BORROWS], or a reddo reclaims [TAKES OWNERSHIP]. */
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
    void *(*persist_capio)(const CarcerCfg *c);  /**< Takes size bytes from the bottom, not zeroed. */
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
 * @brief Takes c->size bytes from the persistent end.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the tenancy, or NULL when the pool cannot meet it [BORROWS].
 * @note First fit over the blocks already in the chain, splitting one large enough to leave another
 *       header and a payload behind; otherwise a fresh block is carved from the free middle.
 * @note A c->size of 0 is taken as one machine word, so every tenancy has an address of its own.
 * @note Fails closed: a request that would cross the interim end returns NULL rather than trespassing.
 * @warning The bytes are not zeroed. A reused block still holds what the last tenant left, so release
 *          anything sensitive with mmgr_carcer_secura_reddo.
 */
void *mmgr_carcer_persist_capio(const CarcerCfg *c);

/**
 * @brief Gives the tenancy at c->tenancy back, leaving its bytes as they are.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS]; c->tenancy [TAKES OWNERSHIP].
 * @note Frees by address, not by count: the block's header carries its size, so releases need not
 *       unwind in order.
 * @note Adjacent free blocks are merged, and a free block at the end of the chain returns to the free
 *       middle, so the two ends recover the space between them.
 * @note A NULL c->tenancy does nothing.
 * @warning c->tenancy is dead once this returns; the pool may hand those bytes out again.
 * @warning Leaves the bytes untouched; mmgr_carcer_secura_reddo is the call that guarantees a wipe.
 */
void mmgr_carcer_persist_reddo(const CarcerCfg *c);

/**
 * @brief Zeroes the tenancy at c->tenancy, then gives it back.
 *
 * @param[in,out] c Pool and the tenancy to release [BORROWS]; c->tenancy [TAKES OWNERSHIP].
 * @note The only difference from mmgr_carcer_persist_reddo is the wipe. The guarantee is in the name,
 *       not a flag, so a caller cannot ask for a wipe and not get one.
 * @note The extent wiped is the block's own, read from its header, so a caller cannot under-wipe a
 *       tenancy by naming fewer bytes than it holds.
 * @note A NULL c->tenancy does nothing.
 * @warning c->tenancy is dead once this returns; the pool may hand those bytes out again.
 */
void mmgr_carcer_secura_reddo(const CarcerCfg *c);

/**
 * @brief Lowers the interim end by c->size, rounded up to a whole machine word.
 *
 * @param[in,out] c Pool and the byte count wanted [BORROWS].
 * @return          Start of the lowered region, or NULL when it would cross the persistent end [BORROWS].
 * @note Each block carries a header, as at the persistent end, but nothing here is released one at a
 *       time: the whole run comes back through a mark or a reset.
 * @note No fit walk, so a take is O(1). Nothing is reused because nothing is released singly.
 * @warning The bytes are not zeroed. Neither end zeroes on hand-out, so a tenant that must not read
 *          what the last one left clears them itself.
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
 * @warning Every interim tenancy taken since c->mark is dead once this returns. Nothing is scrubbed,
 *          so such a pointer still dereferences and returns whatever the next take put there. Keep a
 *          mark and its reddo in the same function.
 */
void mmgr_carcer_interim_reddo(const CarcerCfg *c);

/**
 * @brief Gives the whole interim end back at once.
 *
 * @param[in,out] c Pool to act on [BORROWS].
 * @note mmgr_carcer_interim_reddo against the pool's own size. Named separately so the end of a
 *       dispatch need not reach into the pool to say it.
 * @note The persistent end is not written.
 * @warning Every interim tenancy the pool has handed out is dead once this returns.
 */
void mmgr_carcer_interim_reset(const CarcerCfg *c);

/**
 * @brief Returns whether c->at lies in the pool's bytes.
 *
 * @param[in] c Pool and the address to test [BORROWS].
 * @return      MMGR_TRUE when c->at is at or after base and before base plus size.
 * @note Writes nothing.
 * @warning A range test, not a liveness test. It answers the same for a released tenancy, for an
 *          address inside one, and for a header, so it belongs in an assert rather than in a check
 *          that a pointer is still good.
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
 * @note Any alignment is accepted. Byte edges cover an address or a length that is not a whole word,
 *       and the middle goes a word at a time.
 * @warning c->tenancy must be writable for c->size bytes.
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
