# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
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

file(GLOB MMGR_BENCH_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bench_*.c")

foreach(bench_src ${MMGR_BENCH_SOURCES})
    get_filename_component(bench_name "${bench_src}" NAME_WE)

    foreach(entry IN LISTS MMGR_ENVIRONMENTS)
        mmgr_env_name("${entry}" env defs)
        set(target ${bench_name}_${env})

        add_executable(${target} "${bench_src}")
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(${target} PRIVATE mmgr_${env})
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-O2>
            $<$<C_COMPILER_ID:MSVC>:/O2>)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench/${env}")

        list(APPEND MMGR_BENCHES "${target}")
    endforeach()
endforeach()

list(LENGTH MMGR_BENCHES MMGR_BENCH_COUNT)
message(STATUS "MMgr: ${MMGR_BENCH_COUNT} bench targets")
