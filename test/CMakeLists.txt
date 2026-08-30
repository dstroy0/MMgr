# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Sets up Unity and the harness, then hands off. Every suite declares itself in its own directory,
# so adding one is a new directory plus a line in its parent, and nothing central has to be kept in
# step - the same rule src/ follows.
#
#   unit/<module>/test_<name>/   one per translation unit, mirroring src/<module>/
#   integration/test_<name>/     cross translation unit, and combination logic
#   interop/test_<name>/         this library against another implementation
#   environment/test_<env>/      one per entry in MMGR_ENVIRONMENTS, asserting that environment's
#                                widths, and built in that one only
#
# The one structural feature worth naming is the environment loop inside mmgr_add_suite: a suite is
# built and registered once per environment, against that environment's own library targets, so a
# defect that only shows at a 16-bit word fails a run here.

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
set(MMGR_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

include(MMgrSuite)

set_property(GLOBAL PROPERTY MMGR_SUITES_BUILT "")
set_property(GLOBAL PROPERTY MMGR_SUITES_SKIPPED "")

add_subdirectory(unit)
add_subdirectory(accuracy)
add_subdirectory(environment)
add_subdirectory(integration)
add_subdirectory(interop)

get_property(MMGR_BUILT GLOBAL PROPERTY MMGR_SUITES_BUILT)
get_property(MMGR_SKIPPED GLOBAL PROPERTY MMGR_SUITES_SKIPPED)

list(LENGTH MMGR_BUILT MMGR_BUILT_COUNT)
if(MMGR_BUILT_COUNT EQUAL 0)
    message(WARNING "MMgr: no test suites were built")
endif()
message(STATUS "MMgr: ${MMGR_BUILT_COUNT} test targets")

if(MMGR_SKIPPED)
    list(LENGTH MMGR_SKIPPED MMGR_SKIPPED_COUNT)
    message(STATUS "MMgr: ${MMGR_SKIPPED_COUNT} suites NOT built, their capability is off:")
    foreach(s ${MMGR_SKIPPED})
        message(STATUS "MMgr:   ${s}")
    endforeach()
endif()
