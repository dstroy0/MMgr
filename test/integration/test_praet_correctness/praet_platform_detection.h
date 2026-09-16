/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_platform_detection.h
 * @brief Which architecture this is being built for, and what that architecture gives a clock.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note Nothing below names a part, a vendor or a board, and nothing should. A list of parts covers
 *       the ones somebody thought of on the day they wrote it and silently misses every part released
 *       afterwards. What is tested here is the architecture level, which every member of a family
 *       defines - an ATSAMD21 answers the ARMv6-M question the same way every other Cortex-M0+ does,
 *       and a part nobody here has heard of answers it too.
 * @note The three families are the ones this library targets: ARM, RISC-V and Xtensa, all through
 *       GCC or clang. A build that is none of them is a host build, which is where the suites run.
 * @note This block is the one place an architecture is tested. When this moves to src it goes through
 *       the config header, the way every other compiler and platform test in the tree does.
 *
 * Sources for the macros used below:
 *   Arm C Language Extensions   https://arm-software.github.io/acle/main/acle.html
 *   RISC-V C API specification  https://github.com/riscv-non-isa/riscv-c-api-doc
 *   GCC gcc/config/xtensa/xtensa.h TARGET_CPU_CPP_BUILTINS
 */
#ifndef MMGR_TEST_PRAET_PLATFORM_DETECTION_H
#define MMGR_TEST_PRAET_PLATFORM_DETECTION_H

#include "memoriam_praetereo/memoriam_praetereo.h"

/**
 * @brief Set where this is an ARM build, in either execution state.
 *
 * @note __arm__ covers every 32-bit ARM back to ARMv4, and __aarch64__ covers the 64-bit state. ACLE
 *       leaves both in place, so testing the pair catches every ARM subfamily without naming one.
 */
#if defined(__arm__) || defined(__aarch64__) || defined(__thumb__)
#define PRAET_PLATFORM_ARM 1
#else
#define PRAET_PLATFORM_ARM 0
#endif

/**
 * @brief Set where this is a RISC-V build, at any register width.
 *
 * @note The RISC-V C API defines __riscv as 1 on every RISC-V target. There is no second spelling and
 *       no per-vendor variant of it.
 */
#if defined(__riscv)
#define PRAET_PLATFORM_RISCV 1
#else
#define PRAET_PLATFORM_RISCV 0
#endif

/**
 * @brief Set where this is an Xtensa build.
 *
 * @note GCC predefines both spellings unconditionally for Xtensa. Both are tested because a clang
 *       build for Xtensa is a separate front end and there is no reason to depend on it agreeing
 *       about which one to emit.
 */
#if defined(__XTENSA__) || defined(__xtensa__)
#define PRAET_PLATFORM_XTENSA 1
#else
#define PRAET_PLATFORM_XTENSA 0
#endif

/**
 * @brief Set where this is none of the three, which is a host build.
 */
#if !PRAET_PLATFORM_ARM && !PRAET_PLATFORM_RISCV && !PRAET_PLATFORM_XTENSA
#define PRAET_PLATFORM_HOST 1
#else
#define PRAET_PLATFORM_HOST 0
#endif

EMBED_STATIC_ASSERT((PRAET_PLATFORM_ARM + PRAET_PLATFORM_RISCV + PRAET_PLATFORM_XTENSA + PRAET_PLATFORM_HOST) == 1,
                    "the platform detection selected more than one architecture family, or none");

#if PRAET_PLATFORM_ARM

/*
 * ARM subfamilies, by architecture level and profile rather than by part.
 *
 * __ARM_ARCH is the architecture level as an integer and __ARM_ARCH_PROFILE is the profile as a
 * character constant, 'A', 'R' or 'M'. Between them they name every Cortex subfamily, and a
 * pre-Cortex core defines neither profile nor a level above 6. A character constant is a legal
 * operand in a preprocessor expression, and an undefined __ARM_ARCH_PROFILE evaluates to 0, which
 * matches none of the three.
 */
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 8

/**
 * @brief What to call this architecture in a diagnostic.
 *
 * @note ARMv8-M and ARMv8.1-M, which is Cortex-M23 through M85. The baseline and mainline split is
 *       below in the cycle counter question, where it is the part that matters.
 */
#define PRAET_PLATFORM_NAME "ARMv8-M"
#elif __ARM_ARCH == 7
#define PRAET_PLATFORM_NAME "ARMv7-M"
#else
#define PRAET_PLATFORM_NAME "ARMv6-M"
#endif
#elif defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'R')
#define PRAET_PLATFORM_NAME "ARM real-time profile"
#elif defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'A')
#if defined(__ARM_64BIT_STATE)
#define PRAET_PLATFORM_NAME "ARM application profile, 64 bit"
#else
#define PRAET_PLATFORM_NAME "ARM application profile, 32 bit"
#endif
#else
#define PRAET_PLATFORM_NAME "ARM, pre-Cortex"
#endif

/**
 * @brief Set where the architecture defines a counter the port can read without the caller running
 *        one.
 *
 * @note ARMv7-M and up carry the DWT, whose cycle counter is what a port reads. ARMv6-M does not have
 *       one at all, so a Cortex-M0 or M0+ has nothing here and the caller has to supply the clock.
 * @note The application and real-time profiles have the generic timer and the performance monitors,
 *       either of which a port can read.
 * @warning Says the architecture defines one, and never that this part implemented it. DWT_CYCCNT is
 *          optional even where the DWT is present, and reading it can be disabled. Whether it is
 *          really there is the port's to find out, and this only decides which default to take.
 */
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
#if __ARM_ARCH >= 7
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 1
#else
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 0
#endif
#else
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 1
#endif

/**
 * @brief Bits in this architecture's general purpose register.
 *
 * @note ACLE reports the execution state rather than the register width, and on ARM those are the
 *       same question: the 64-bit state is what gives 64-bit registers.
 */
#if defined(__ARM_64BIT_STATE)
#define PRAET_PLATFORM_XLEN 64u
#else
#define PRAET_PLATFORM_XLEN 32u
#endif

/**
 * @brief Set where the architecture loads and stores at an address that is not aligned to the size.
 *
 * @note ACLE defines __ARM_FEATURE_UNALIGNED where the hardware does it. A Cortex-M0 and an M7 answer
 *       this differently, and both are ARM.
 * @note Only affects what the word boundary in a recovery means. A part that traps on an unaligned
 *       word makes the boundary the hardware's granularity as well as this module's.
 */
#if defined(__ARM_FEATURE_UNALIGNED)
#define PRAET_PLATFORM_UNALIGNED_LOADS 1
#else
#define PRAET_PLATFORM_UNALIGNED_LOADS 0
#endif

#elif PRAET_PLATFORM_RISCV

/*
 * RISC-V subfamilies, by register width and register file.
 *
 * __riscv_xlen is 32, 64 or 128. The embedded profiles cut the register file to sixteen and are
 * reported by __riscv_32e, __riscv_64e, or __riscv_abi_rve where the ABI is the embedded one.
 */
#if defined(__riscv_32e) || defined(__riscv_64e) || defined(__riscv_abi_rve)

/**
 * @brief What to call this architecture in a diagnostic.
 */
#if __riscv_xlen == 32
#define PRAET_PLATFORM_NAME "RV32E"
#else
#define PRAET_PLATFORM_NAME "RV64E"
#endif
#elif __riscv_xlen == 32
#define PRAET_PLATFORM_NAME "RV32"
#elif __riscv_xlen == 64
#define PRAET_PLATFORM_NAME "RV64"
#else
#define PRAET_PLATFORM_NAME "RV128"
#endif

/**
 * @brief Set where the architecture defines a counter the port can read without the caller running
 *        one.
 *
 * @note The cycle counter is the Zicntr CSR, which every profile carries. A port reads it with rdcycle
 *       or from mcycle where it runs in machine mode.
 * @warning Says the architecture defines one, and never that this build can reach it. Reading cycle
 *          from a lower privilege level is gated by mcounteren, and a platform may leave it closed.
 */
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 1

/**
 * @brief Bits in this architecture's general purpose register.
 *
 * @note Straight off __riscv_xlen, which the C API defines as 32, 64 or 128 on every RISC-V target.
 */
#define PRAET_PLATFORM_XLEN __riscv_xlen

/**
 * @brief Set where the architecture loads and stores at an address that is not aligned to the size.
 *
 * @note The three __riscv_misaligned_ macros arrived in GCC 14, so an older toolchain defines none of
 *       them and this reads as strict. That is the fail-closed answer: a build described as strict on
 *       a part that is not loses nothing, and the reverse is a fault at run time.
 */
#if defined(__riscv_misaligned_fast)
#define PRAET_PLATFORM_UNALIGNED_LOADS 1
#else
#define PRAET_PLATFORM_UNALIGNED_LOADS 0
#endif

#elif PRAET_PLATFORM_XTENSA

/*
 * Xtensa subfamilies, by calling convention.
 *
 * GCC predefines exactly one of __XTENSA_WINDOWED_ABI__ and __XTENSA_CALL0_ABI__ for every Xtensa
 * configuration. The register windows are the difference that shows up in anything reaching the
 * stack, which is why it is the split worth naming.
 */
#if defined(__XTENSA_WINDOWED_ABI__)

/**
 * @brief What to call this architecture in a diagnostic.
 */
#define PRAET_PLATFORM_NAME "Xtensa, windowed ABI"
#elif defined(__XTENSA_CALL0_ABI__)
#define PRAET_PLATFORM_NAME "Xtensa, call0 ABI"
#else
#define PRAET_PLATFORM_NAME "Xtensa"
#endif

/**
 * @brief Set where the architecture defines a counter the port can read without the caller running
 *        one.
 *
 * @note CCOUNT, the cycle count special register. A port reads it with rsr.
 * @warning Says the architecture defines one, and never that this core was configured with it. An
 *          Xtensa core is configurable and the timer option can be left out, which is the port's to
 *          find out.
 */
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 1

/**
 * @brief Bits in this architecture's general purpose register.
 *
 * @note Thirty-two on every Xtensa configuration. The register file is configurable in depth and in
 *       which options are present, and never in width.
 */
#define PRAET_PLATFORM_XLEN 32u

/**
 * @brief Set where the architecture loads and stores at an address that is not aligned to the size.
 *
 * @note Zero, which is the fail-closed answer rather than a measured one. The base ISA raises a load
 *       or store alignment exception, the unaligned option is a core configuration choice, and GCC
 *       predefines nothing that reports it. A core that does support it is described conservatively
 *       here and loses nothing by it.
 */
#define PRAET_PLATFORM_UNALIGNED_LOADS 0

#else

/**
 * @brief What to call this architecture in a diagnostic.
 *
 * @note A host build. The suites run here, and a host has a clock the caller reaches through the
 *       standard library rather than one this library would pin.
 */
#define PRAET_PLATFORM_NAME "this host"

/**
 * @brief Set where the architecture defines a counter the port can read without the caller running
 *        one.
 *
 * @note Zero, and not because a host has no counter. A host build has no core to pin a timer to and
 *       no architectural register to read, so the clock is the caller's - which is what makes a host
 *       build take the same arm a Cortex-M0 does, for the same reason.
 */
#define PRAET_PLATFORM_HAS_CYCLE_COUNTER 0

/**
 * @brief Bits in this architecture's general purpose register.
 *
 * @note Off the pointer size, which GCC and clang both predefine everywhere. A host is whatever the
 *       machine running the suites is, so there is nothing else to read it from.
 */
#define PRAET_PLATFORM_XLEN (__SIZEOF_POINTER__ * 8u)

/**
 * @brief Set where the architecture loads and stores at an address that is not aligned to the size.
 */
#define PRAET_PLATFORM_UNALIGNED_LOADS 1

#endif

// A word wider than the register it is carried in. The environments build this at 16 and 32 bits on a
// 64-bit host on purpose, so narrower is expected and only the other direction is wrong
EMBED_STATIC_ASSERT(EMBED_WORD_BITS <= PRAET_PLATFORM_XLEN,
                    "EMBED_WORD_BITS is wider than this architecture's register");

/**
 * @brief The core a platform default pins a timer to.
 *
 * @note Core zero, on every family. A part with one core has it and a part with several starts at it,
 *       so it is the number that exists everywhere rather than the number that is right anywhere in
 *       particular. Which core is right is the caller's to say, and PRAET_CLOCK_CORE is where they
 *       say it.
 */
#define PRAET_PLATFORM_CLOCK_CORE 0u

#endif
