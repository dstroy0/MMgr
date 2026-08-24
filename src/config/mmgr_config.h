/**
 * @brief Build-time settings: widths, region sizes, feature switches and the region carving macros.
 *
 * @note Every setting is taken only when the build has not already defined it.
 */
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

/**
 * @brief Width in bits of mmgr_word, the register every SWAR lane operation runs in.
 *
 * @note Derived from UINTPTR_MAX as 64, 32 or 16 when the build does not set it.
 * @warning A target whose UINTPTR_MAX matches none of the three raises #error; pass -DMMGR_WORD_BITS.
 */
#ifndef MMGR_WORD_BITS
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define MMGR_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define MMGR_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define MMGR_WORD_BITS 16
#else
#error "cannot derive MMGR_WORD_BITS from UINTPTR_MAX on this target - pass -DMMGR_WORD_BITS=16|32|64"
#endif
#endif

/**
 * @brief Width in bits of mmgr_idx, the type every offset and length is carried in.
 *
 * @note Follows MMGR_WORD_BITS below 32, and is capped at 32 above that.
 * @warning mmgr_types.h accepts only 16 or 32, and asserts mmgr_idx fits inside mmgr_word.
 */
#ifndef MMGR_INDEX_BITS
#if MMGR_WORD_BITS < 32
#define MMGR_INDEX_BITS MMGR_WORD_BITS
#else
#define MMGR_INDEX_BITS 32
#endif
#endif

/**
 * @brief Alignment applied to every carved region, and the granularity each pool must be a multiple of.
 *
 * @note MMGR_CARCER_CHECK asserts each pool is a whole number of these and at least two of them.
 */
#ifndef MMGR_ALIGN_BYTES

#define MMGR_ALIGN_BYTES 16u
#endif

#include "config/mmgr_types.h"

/**
 * @brief Width in bits of one SWAR word, always equal to MMGR_WORD_BITS.
 *
 * @warning Any earlier definition is discarded, so a build cannot set the two widths apart.
 */
#ifdef MMGR_SWAR_BITS
#undef MMGR_SWAR_BITS
#endif
#define MMGR_SWAR_BITS MMGR_WORD_BITS

/**
 * @brief Runtime assertion hook, left inert unless the build supplies one.
 *
 * @param[in] cond Condition the caller expects to hold.
 * @param[in] msg  String literal describing the expectation.
 * @note The default expands to a sizeof, so cond is type checked but never evaluated.
 * @warning With the default, a failed expectation produces no diagnostic and no trap.
 */
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif

/**
 * @brief Set to 1 to enable the library's debug-only checks.
 */
#ifndef MMGR_DEBUG_CHECKS

#define MMGR_DEBUG_CHECKS 0
#endif

/**
 * @brief Bytes in the plaintext confinium.
 */
#ifndef MMGR_PLAINTEXT_CONFIN_SIZE
#define MMGR_PLAINTEXT_CONFIN_SIZE 4096u
#endif

/**
 * @brief Bytes in the secure confinium.
 */
#ifndef MMGR_SECURE_CONFIN_SIZE
#define MMGR_SECURE_CONFIN_SIZE 4096u
#endif

/**
 * @brief Bytes in the larger of the two confinia, sizing the worst case a scan must cover.
 *
 * @note verbum_scrutor.h derives MMGR_SCAN_MAX_WORDS from this, and mmgr_string_shim.h uses it as MMGR_STR_MAX.
 */
#ifndef MMGR_CARCER_MAX
#if MMGR_PLAINTEXT_CONFIN_SIZE >= MMGR_SECURE_CONFIN_SIZE
#define MMGR_CARCER_MAX ((size_t)MMGR_PLAINTEXT_CONFIN_SIZE)
#else
#define MMGR_CARCER_MAX ((size_t)MMGR_SECURE_CONFIN_SIZE)
#endif
#endif

/**
 * @brief Largest number of pools one region may be carved into.
 *
 * @note Sizes the pool and mark arrays in MMGR_CARCER_MACHINERY, and bounds the region macros.
 */
#ifndef MMGR_CARCER_MAX_REGIONS
#define MMGR_CARCER_MAX_REGIONS 2u
#endif

/**
 * @brief Members every carved region carries ahead of its bytes.
 *
 * @note init records the whole region; pool holds one CarcerCtx per carved pool.
 * @warning mark is declared but neither written nor read anywhere in the library.
 */
#define MMGR_CARCER_MACHINERY                                                                                          \
    const CarcerInit init;                                                                                             \
    CarcerCtx pool[MMGR_CARCER_MAX_REGIONS];                                                                           \
    size_t mark[MMGR_CARCER_MAX_REGIONS]

/**
 * @brief Pairs a pool name with its size for mmgr_carcer_init.
 *
 * @param[in] name_ Enumerator name to give the pool.
 * @param[in] n_    Bytes to give the pool.
 * @note Expands to two comma-separated arguments, so each pair counts as two towards MMGR_NARG.
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
 * @brief Set to 1 to build the memoriam_praetereo DMA path.
 *
 * @note mmgr.h includes memoriam_praetereo.h only when this is set, and it gates MMGR_PRAET_CHANNELS below.
 */
#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
/**
 * @brief Set to 1 to build the confinium_externum external memory path.
 *
 * @note mmgr.h includes confinium_externum.h only when this is set.
 */
#ifndef MMGR_ENABLE_EXTRAM

#define MMGR_ENABLE_EXTRAM 0
#endif
/**
 * @brief Set to 1 to build the keep-out region support.
 */
#ifndef MMGR_ENABLE_KEEPOUT

#define MMGR_ENABLE_KEEPOUT 0
#endif
/**
 * @brief Set to 1 to track each pool's peak occupancy in CarcerCtx::hw.
 *
 * @note Adds the hw member to CarcerCtx and the blend that maintains it in both carcer capio calls.
 */
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif

/**
 * @brief Number of drain passes the ring makes per service call.
 */
#ifndef MMGR_RING_DRAINS
#define MMGR_RING_DRAINS 4u
#endif

/**
 * @brief Set to 1 to build the time-based ring behaviour.
 *
 * @note MMGR_RING_ATTACH_US exists only when this is set.
 */
#ifndef MMGR_ENABLE_CLOCK
#define MMGR_ENABLE_CLOCK 0
#endif

/**
 * @brief Microseconds a ring waits before attaching.
 *
 * @warning Defined only when MMGR_ENABLE_CLOCK is set; referencing it otherwise will not compile.
 */
#if MMGR_ENABLE_CLOCK
#ifndef MMGR_RING_ATTACH_US
#define MMGR_RING_ATTACH_US 100u
#endif
#endif

/**
 * @brief DMA channels memoriam_praetereo carries, and the bytes each one buffers.
 *
 * @warning Both are defined only when MMGR_ENABLE_DMA is set; referencing them otherwise will not compile.
 */
#if MMGR_ENABLE_DMA
#ifndef MMGR_PRAET_CHANNELS

#define MMGR_PRAET_CHANNELS 2
#endif
#ifndef MMGR_PRAET_BUF_SIZE

#define MMGR_PRAET_BUF_SIZE 256
#endif
#endif

/**
 * @brief Fails the build unless x_ has type size_t.
 *
 * @param[in] x_ Expression whose type is checked.
 * @note Evaluates to void; the _Generic association list admits size_t only.
 */
#define MMGR_MEMOR_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))

/**
 * @brief Fails the build unless x_ has type uint8_t.
 *
 * @param[in] x_ Expression whose type is checked.
 * @note Evaluates to void; the _Generic association list admits uint8_t only.
 */
#define MMGR_MEMOR_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

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
 * @param[in] ...     Further designated initialisers, such as .size or .at.
 * @return            Address of the compound literal [BORROWS].
 * @warning The literal has automatic storage; the callee must not retain the address.
 */
#define MMGR_CARCER(region_, pool_, ...) (&(CarcerCfg){.pool = MMGR_CARCER_POOL(region_, pool_), __VA_ARGS__})

#endif
