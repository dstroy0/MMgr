# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# One CTest target per suite per environment. A suite is a directory holding exactly one .c with
# file-scope `void test_<name>(void)` cases; harness.py turns that into unity_runner.c through
# Unity's own generator, and refuses a suite whose cases the generator would walk past.
#
# Ported from idemIP's test/CMakeLists.txt. The one structural addition is the environment loop: a
# unit suite is built and registered once per entry in MMGR_ENVIRONMENTS, against that environment's
# own library targets, so a defect that only shows at a 16-bit scan lane fails a run here.

include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

set(MMGR_UNITY_RB "${unity_SOURCE_DIR}/auto/generate_test_runner.rb")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MMGR_HARNESS "${CMAKE_CURRENT_SOURCE_DIR}/harness.py")

# test/ is exempt from the src/ style rules, so a suite reads as plain host C.
file(GLOB_RECURSE MMGR_SUITE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/unit/*/test_*/test_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/environment/test_*/test_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_*/test_*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/interop/test_*/test_*.c")

if(NOT MMGR_SUITE_SOURCES)
    message(WARNING "MMgr: no test suites found under test/unit, environment, integration or interop")
endif()

# Which capabilities each suite needs. A suite missing any of them is not built, because the
# capability names translation units that are not in the library and the suite would fail to link.
# It is SKIPPED LOUDLY: silently dropping it would leave a passing run that tested less than it
# looks like.
#
# Unity's runner generator reads the case names out of the source text and does not see a
# preprocessor conditional, so a case cannot be compiled out of a suite: the whole suite is what a
# capability gates.
#
# Only two modules are capability-gated, and both for the same reason - they talk to hardware a host
# does not have, so the code has to be absent rather than merely unlinked. Every other module is
# selected by CMake alone and needs no gate.
set(MMGR_SUITE_CAP_test_dma                DMA)
set(MMGR_SUITE_CAP_test_confinium_externum PSRAM_POOL)

set(MMGR_SKIPPED "")
set(MMGR_BUILT "")

foreach(suite_src ${MMGR_SUITE_SOURCES})
    get_filename_component(suite_dir "${suite_src}" DIRECTORY)
    get_filename_component(suite_name "${suite_dir}" NAME)
    set(runner "${suite_dir}/unity_runner.c")

    set(missing "")
    foreach(cap IN LISTS MMGR_SUITE_CAP_${suite_name})
        if(NOT MMGR_ENABLE_${cap})
            list(APPEND missing "MMGR_ENABLE_${cap}=OFF")
        endif()
    endforeach()
    if(missing)
        list(JOIN missing ", " missing_text)
        list(APPEND MMGR_SKIPPED "${suite_name} (${missing_text})")
        continue()
    endif()

    # One runner per suite, not per environment: the cases are the same source either way.
    # Regenerated whenever they change, so a new case is registered without re-configuring.
    #
    # The custom command is wrapped in a target, and the executables depend on the TARGET rather
    # than each listing the OUTPUT. With the Makefile generator, an OUTPUT consumed by several
    # targets is emitted as a rule in each of their build.make files - so the six environment builds
    # of one suite race to write the same unity_runner.c, and a target that links it mid-write gets
    # a file with no main(). That surfaced as `undefined reference to WinMain`, because the linker
    # falls back to looking for an entry point when main() is absent.
    #
    # idemIP does not need this: it builds one executable per suite, so its OUTPUT has exactly one
    # consumer. The environment loop is what makes the wrapper necessary here.
    add_custom_command(
        OUTPUT "${runner}"
        COMMAND ${Python3_EXECUTABLE} "${MMGR_HARNESS}" runners gen "${suite_dir}" --unity "${MMGR_UNITY_RB}"
        DEPENDS "${suite_src}" "${MMGR_HARNESS}"
        COMMENT "MMgr: generating Unity runner for ${suite_name}"
        VERBATIM
    )
    add_custom_target(${suite_name}_runner DEPENDS "${runner}")
    set_source_files_properties("${runner}" PROPERTIES GENERATED TRUE)

    # An environment suite asserts the widths of ONE environment, so it is built in that one only.
    # Building test_swar16 against the host library would assert a 16-bit lane that was never
    # selected, and fail for the right reason at the wrong target.
    set(is_env_suite FALSE)
    if("${suite_dir}" MATCHES "/environment/")
        set(is_env_suite TRUE)
    endif()
    string(REGEX REPLACE "^test_" "" suite_env "${suite_name}")

    foreach(entry IN LISTS MMGR_ENVIRONMENTS)
        mmgr_env_name("${entry}" env defs)
        if(is_env_suite AND NOT env STREQUAL suite_env)
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
            "${CMAKE_CURRENT_SOURCE_DIR}/support/platform_host.c")
        add_dependencies(${target} ${suite_name}_runner)
        target_include_directories(${target} PRIVATE
            "${unity_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/support"
        )
        # mmgr_<env> carries this environment's definitions and its include root, so the suite is
        # compiled with exactly the widths the library under it was compiled with. mmgr_flags is
        # deliberately not linked: test/ is exempt from the src/ warning set, and Unity's assertion
        # macros do not survive -Wconversion.
        target_link_libraries(${target} PRIVATE mmgr_${env})
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/${env}")

        add_test(NAME ${target} COMMAND ${target})
        list(APPEND MMGR_BUILT "${target}")
    endforeach()
endforeach()

list(LENGTH MMGR_BUILT MMGR_BUILT_COUNT)
message(STATUS "MMgr: ${MMGR_BUILT_COUNT} test targets")
if(MMGR_SKIPPED)
    list(LENGTH MMGR_SKIPPED MMGR_SKIPPED_COUNT)
    message(STATUS "MMgr: ${MMGR_SKIPPED_COUNT} suites NOT built, their capability is off:")
    foreach(s ${MMGR_SKIPPED})
        message(STATUS "MMgr:   ${s}")
    endforeach()
endif()
