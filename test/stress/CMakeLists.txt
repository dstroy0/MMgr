# The thread stress of LocusCarcerum on the host environment: one program, run short by ctest and long by hand with
# its thread count, rounds, cellblock bytes, runs and seed on the command line.
find_package(Threads REQUIRED)

set(MMGR_STRESS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/locus_carcerum_stress.c"
    "${MMGR_TEST_ROOT}/support/host_threads.c"
    "${MMGR_TEST_ROOT}/support/host_wait.c")

# host_wait.c sets the GNU feature set ahead of its first include, which is too late where an environment forces a
# header in first: on the command line it is in place before any header is read
if(NOT WIN32)
    set_source_files_properties("${MMGR_TEST_ROOT}/support/host_wait.c" PROPERTIES COMPILE_DEFINITIONS "_GNU_SOURCE=1")
endif()

add_executable(locus_carcerum_stress_host ${MMGR_STRESS_SOURCES})
target_include_directories(locus_carcerum_stress_host PRIVATE "${MMGR_TEST_ROOT}/support")
target_compile_definitions(locus_carcerum_stress_host PRIVATE MMGR_CARCER_OWNERS=1024u)
target_link_libraries(locus_carcerum_stress_host PRIVATE mmgr_host Threads::Threads)
if(WIN32)
    target_link_libraries(locus_carcerum_stress_host PRIVATE synchronization)
endif()
set_target_properties(locus_carcerum_stress_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/host")

add_test(NAME locus_carcerum_stress_host COMMAND locus_carcerum_stress_host 16 2000 65536 2)

# The two cells at the temporary top given by two threads at one instant, round after round
add_executable(locus_carcerum_trim_race_host
    "${CMAKE_CURRENT_SOURCE_DIR}/locus_carcerum_trim_race.c"
    "${MMGR_TEST_ROOT}/support/host_threads.c"
    "${MMGR_TEST_ROOT}/support/host_wait.c")
target_include_directories(locus_carcerum_trim_race_host PRIVATE "${MMGR_TEST_ROOT}/support")
target_link_libraries(locus_carcerum_trim_race_host PRIVATE mmgr_host Threads::Threads)
if(WIN32)
    target_link_libraries(locus_carcerum_trim_race_host PRIVATE synchronization)
endif()
set_target_properties(locus_carcerum_trim_race_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/host")

add_test(NAME locus_carcerum_trim_race_host COMMAND locus_carcerum_trim_race_host 200000)
