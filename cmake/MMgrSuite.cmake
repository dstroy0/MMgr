# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# mmgr_add_suite() - one CTest target per environment for the suite in the current directory.
#
# A suite is a directory holding exactly one .c with file-scope `void test_<name>(void)` cases.
# harness.py turns that into unity_runner.c through Unity's own generator and refuses a suite whose
# cases the generator would walk past.
#
#   mmgr_add_suite(test_endian)
#   mmgr_add_suite(test_dma CAPABILITY DMA)
#   mmgr_add_suite(test_word16 ENVIRONMENT word16)
#
# CAPABILITY names a config option the suite cannot be built without. The capability names
# translation units that are not in the library when it is off, so the suite would fail to link.
# It is skipped loudly - silently dropping it leaves a passing run that tested less than it looks
# like. Unity's generator reads case names out of the source text and does not see a preprocessor
# conditional, so a case cannot be compiled out of a suite: the whole suite is what a capability
# gates.
#
# ENVIRONMENT pins the suite to one entry in MMGR_ENVIRONMENTS. An environment suite asserts the
# widths of one environment, so building it against the others would assert a lane that was never
# selected and fail for the right reason at the wrong target.

function(mmgr_add_suite suite_name)
    cmake_parse_arguments(ARG "" "CAPABILITY;ENVIRONMENT" "" ${ARGN})

    set(suite_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(suite_src "${suite_dir}/${suite_name}.c")
    set(runner "${suite_dir}/unity_runner.c")

    if(NOT EXISTS "${suite_src}")
        message(FATAL_ERROR "MMgr: ${suite_name} has no ${suite_name}.c - a suite directory holds exactly one")
    endif()

    if(ARG_CAPABILITY AND NOT MMGR_ENABLE_${ARG_CAPABILITY})
        set_property(GLOBAL APPEND PROPERTY MMGR_SUITES_SKIPPED
                     "${suite_name} (MMGR_ENABLE_${ARG_CAPABILITY}=OFF)")
        return()
    endif()

    # One runner per suite, not per environment: the cases are the same source either way.
    #
    # The custom command is wrapped in a target, and the executables depend on the TARGET rather
    # than each listing the OUTPUT. With the Makefile generator, an OUTPUT consumed by several
    # targets is emitted as a rule in each of their build.make files - so the environment builds of
    # one suite race to write the same unity_runner.c, and a target that links it mid-write gets a
    # file with no main(). That surfaced as `undefined reference to WinMain`, because the linker
    # falls back to looking for an entry point when main() is absent.
    add_custom_command(
        OUTPUT "${runner}"
        COMMAND ${Python3_EXECUTABLE} "${MMGR_HARNESS}" runners gen "${suite_dir}" --unity "${MMGR_UNITY_RB}"
        DEPENDS "${suite_src}" "${MMGR_HARNESS}"
        COMMENT "MMgr: generating Unity runner for ${suite_name}"
        VERBATIM
    )
    add_custom_target(${suite_name}_runner DEPENDS "${runner}")
    set_source_files_properties("${runner}" PROPERTIES GENERATED TRUE)

    foreach(entry IN LISTS MMGR_ENVIRONMENTS)
        mmgr_env_name("${entry}" env defs)
        if(ARG_ENVIRONMENT AND NOT env STREQUAL ARG_ENVIRONMENT)
            continue()
        endif()

        set(target ${suite_name}_${env})

        # support/platform_host.c supplies mmgr_platform_context_id(), which the library declares
        # and does not define. Linked into every suite rather than only the ones that need it: which
        # environments call it is MMGR_NEEDS_CONTEXT_ID's business, and a suite list that has to
        # track that condition is a second copy of it waiting to disagree. Where nothing calls it,
        # the linker drops it.
        add_executable(${target}
            "${suite_src}"
            "${runner}"
            "${unity_SOURCE_DIR}/src/unity.c"
            "${MMGR_TEST_ROOT}/support/platform_host.c")
        add_dependencies(${target} ${suite_name}_runner)
        target_include_directories(${target} PRIVATE
            "${unity_SOURCE_DIR}/src"
            "${MMGR_TEST_ROOT}"
            "${MMGR_TEST_ROOT}/support"
        )
        # mmgr_<env> carries this environment's definitions and its include root, so the suite is
        # compiled with exactly the widths the library under it was compiled with. mmgr_flags is
        # deliberately not linked: test/ is exempt from the src/ warning set, and Unity's assertion
        # macros do not survive -Wconversion.
        target_link_libraries(${target} PRIVATE mmgr_${env})
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/${env}")

        add_test(NAME ${target} COMMAND ${target})
        set_property(GLOBAL APPEND PROPERTY MMGR_SUITES_BUILT "${target}")
    endforeach()
endfunction()
