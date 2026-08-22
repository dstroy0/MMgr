cmake_minimum_required(VERSION 3.20)
project(MMgr VERSION 0.1.0 LANGUAGES C)

# Everything here is host-buildable and host-testable by design. There is no cross-toolchain step
# and no board in the loop: the widths that make a target different are compile-time knobs, so a
# 16-bit SWAR lane is exercised on this machine by defining it, not by owning the hardware.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Exported for clangd, for the CodeQL and Sonar runs, and for tools/dev_env.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MMGR_BUILD_TESTS "Build the unit tests" ON)
option(MMGR_BUILD_BENCH "Build the benchmarks" OFF)
option(MMGR_WERROR "Treat warnings as errors" OFF)
option(MMGR_LTO "Link-time optimization" ON)

# A hardware capability, like the DMA engine: a part either has somewhere to defer work to or it
# does not. Off, the ring carries no reservation code at all - not a branch that is always false.
#
# Unlike MMGR_ENABLE_DMA this reaches the compiler as well as the suite map. A capability that only
# gates which suites are built decides what is tested and not what is compiled, which is the wrong
# half: the code under test is the same either way.
option(MMGR_ENABLE_KEEPOUT "Build the ring's keepout reservations" OFF)

# Test only. Points every namespace entry that has a libc equivalent at libc, so the whole suite
# runs against libc unchanged and any disagreement is a finding. libc is the oracle: a str or mem
# entry that has shipped for decades has been beaten on harder than anything written here.
#
# It is the only thing that makes src/ include a header outside the budget, which is why it is a
# separate option and never a default - the library exists so a target with no libc still has these
# entries.

# ------------------------------------------------------------------------------------------------
# Optimization
# ------------------------------------------------------------------------------------------------
# LTO, because this library is seventeen small modules that call each other and the calls between
# them are the cost. Measured in test/bench: mmgr_memor_chr over 512 bytes is 610 cycles when its
# SWAR entries are out-of-line calls and 187 when the linker may inline them. The hot entries are
# MMGR_INLINE in their headers now, so LTO is no longer load-bearing for those - it still is for
# every cross-module call that is not.
if(MMGR_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT _ipo OUTPUT _ipo_why)
  if(_ipo)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(STATUS "MMgr: LTO unavailable, continuing without it (${_ipo_why})")
  endif()
endif()

# MinSizeRel is -Os by default, and -Os is the wrong trade for this library: it declines to inline
# the small branch-free entries that ARE the library, so the calls it saves space on are the calls
# that cost the most. An embedded build reaching for MinSizeRel wants small, not slow-and-small.
foreach(_lang C)
  string(REPLACE "-Os" "-O2" CMAKE_${_lang}_FLAGS_MINSIZEREL "${CMAKE_${_lang}_FLAGS_MINSIZEREL}")
endforeach()

# One place, so a warning that matters is on for the library, the tests and the benches alike.
add_library(mmgr_flags INTERFACE)
target_compile_options(mmgr_flags INTERFACE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion;-Wcast-align;-Wcast-qual;-Wstrict-prototypes;-Wmissing-prototypes;-Wpointer-arith;-Wvla>
  $<$<AND:$<BOOL:${MMGR_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<AND:$<BOOL:${MMGR_WERROR}>,$<C_COMPILER_ID:MSVC>>:/WX>
)

# ------------------------------------------------------------------------------------------------
# Environments
# ------------------------------------------------------------------------------------------------
# An "environment" is one set of compile-time widths. The whole library and the whole test suite are
# built once per environment, so a change that only breaks the 16-bit SWAR lane fails a build rather
# than waiting for someone to own that hardware.
#
# Each entry is  name|definitions  with definitions separated by semicolons escaped as commas below,
# because a CMake list cannot nest. Kept as a flat list so adding an environment is one line.
#
#   host      whatever this machine is: MMGR_WORD_BITS derives from UINTPTR_MAX. The default build.
#   swar32    a 32-bit scan lane on a 64-bit word - the case where the lane is narrower than the
#             register, which is how a wide host models a narrow machine's ingestion path.
#   swar16    the narrowest lane. Most likely to expose an off-by-one in a tail.
#   word32    a 32-bit word on a 64-bit host. Catches anything that assumed a word holds a pointer.
#   idx16     a 16-bit index against a 64-bit word, which is the pairing the static_assert in
#             mmgr_types.h exists to police.
#   checks    debug checks compiled in, with MMGR_ASSERT wired to abort so a violated contract
#             fails a test rather than being a no-op nobody notices.
set(MMGR_ENVIRONMENTS
  "host|"
  "word32|MMGR_WORD_BITS=32"
  "word16|MMGR_WORD_BITS=16"
  "idx16|MMGR_INDEX_BITS=16"
  "checks|MMGR_DEBUG_CHECKS=1"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MMgrModule)

# No list of modules here. Each directory declares itself with mmgr_add_module(), so adding one is
# a new directory plus a line in src/mmgr/CMakeLists.txt, and nothing central has to be kept in step.
add_subdirectory(src)

if(MMGR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Added here rather than from test/CMakeLists.txt, so it does not depend on the tests being on.
# Nested under MMGR_BUILD_TESTS it silently produced nothing for -DMMGR_BUILD_BENCH=ON alone, which
# is exactly how a bench gets measured against a build nobody meant to measure.
if(MMGR_BUILD_BENCH)
  add_subdirectory(test/bench)
endif()
