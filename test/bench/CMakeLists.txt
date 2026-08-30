# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# One bench binary per environment, for the same reason the suites are built per environment: the
# point of the SWAR bench is the comparison BETWEEN lane widths, and that comparison does not exist
# unless each width is actually built and run.
#
# Off by default. A bench is not a gate - it reports numbers, it does not pass or fail - so building
# it on every configure costs time and answers nothing. Turn it on deliberately:
#
#     cmake -S . -B build -DMMGR_BUILD_BENCH=ON
#     cmake --build build
#     ./build/bench/swar16/bench_scrutor_swar16
#
# Built with -O2 regardless of CMAKE_BUILD_TYPE. A Debug bench measures the debug build, which is
# not a number anyone wants, and the library's own inline entries do not inline at -O0 - so a Debug
# reading would be timing function calls the shipped build does not make.
#
# IPO is off here, and that is what makes the line above true. LTO re-optimizes from IR at link
# time using the LINK line's flags, so with it on the -O2 below never reached the generated code:
# the link line carried the build type's -O3 and that is what the bench was measuring. The give
# away was the size - .text came out at the -O3 figure rather than the -O2 one - and -O3 measured
# slower on put and read while inflating .text by a third.

# The A/B benches build against ProtoCore's own sources rather than a copy of them, so what they
# measure is its code and not a transcription. It is a sibling checkout and not a dependency: when
# it is not there, those benches are skipped and the rest still build.
set(MMGR_PROTOCORE_DIR "${CMAKE_SOURCE_DIR}/../../ProtoCore" CACHE PATH "ProtoCore checkout for the A/B benches")

# The four under test, plus what they call: protomem for the moves, swar for the lane work under
# it, and the host HAL for the context id the worker slots are keyed on.
set(MMGR_PROTOCORE_SOURCES
    "${MMGR_PROTOCORE_DIR}/src/mmgr/arena/arena.c"
    "${MMGR_PROTOCORE_DIR}/src/mmgr/span/span.c"
    "${MMGR_PROTOCORE_DIR}/src/mmgr/secure/secure.c"
    "${MMGR_PROTOCORE_DIR}/src/mmgr/plaintext/plaintext.c"
    "${MMGR_PROTOCORE_DIR}/src/mmgr/protomem/protomem.c"
    "${MMGR_PROTOCORE_DIR}/src/mmgr/swar/swar.c"
    "${MMGR_PROTOCORE_DIR}/test/core_setup/hal/portable/portable_platform.c")

set(MMGR_PROTOCORE_OK TRUE)
foreach(src IN LISTS MMGR_PROTOCORE_SOURCES)
    if(NOT EXISTS "${src}")
        set(MMGR_PROTOCORE_OK FALSE)
    endif()
endforeach()

file(GLOB MMGR_BENCH_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bench_*.c")

if(NOT MMGR_PROTOCORE_OK)
    list(FILTER MMGR_BENCH_SOURCES EXCLUDE REGEX "bench_carcer_ab\\.c$")
    message(STATUS "MMgr: no ProtoCore at ${MMGR_PROTOCORE_DIR}, skipping bench_carcer_ab")
endif()

foreach(bench_src ${MMGR_BENCH_SOURCES})
    get_filename_component(bench_name "${bench_src}" NAME_WE)

    foreach(entry IN LISTS MMGR_ENVIRONMENTS)
        mmgr_env_name("${entry}" env defs)
        set(target ${bench_name}_${env})

        if(bench_name STREQUAL "bench_carcer_ab")
            add_executable(${target} "${bench_src}" ${MMGR_PROTOCORE_SOURCES})
            # ProtoCore's own three roots: its sources, its vendor detect, and the host answers that
            # detect selects. It is held to its own warning set, not this library's.
            target_include_directories(${target} PRIVATE
                "${MMGR_PROTOCORE_DIR}/src"
                "${MMGR_PROTOCORE_DIR}"
                "${MMGR_PROTOCORE_DIR}/test/core_setup/hal/host")
        else()
            add_executable(${target} "${bench_src}")
        endif()
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(${target} PRIVATE mmgr_${env})
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O2>
            $<$<C_COMPILER_ID:MSVC>:/O2>)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench/${env}"
            INTERPROCEDURAL_OPTIMIZATION FALSE)

        list(APPEND MMGR_BENCHES "${target}")
    endforeach()
endforeach()

list(LENGTH MMGR_BENCHES MMGR_BENCH_COUNT)
message(STATUS "MMgr: ${MMGR_BENCH_COUNT} bench targets")
