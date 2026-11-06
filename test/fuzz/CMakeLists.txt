# The libFuzzer harness over LocusCarcerum, built where MMGR_FUZZ is on and the compiler is clang, which carries
# the fuzzer runtime; run by hand with a corpus folder and libFuzzer's own arguments.
if(NOT MMGR_FUZZ)
    return()
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "MMgr: the fuzz harness needs clang, not built")
    return()
endif()

set(MMGR_FUZZ_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/locus_carcerum_fuzz.c"
    "${MMGR_TEST_ROOT}/support/host_wait.c")
if(NOT WIN32)
    set_source_files_properties("${MMGR_TEST_ROOT}/support/host_wait.c" PROPERTIES COMPILE_DEFINITIONS "_GNU_SOURCE=1")
endif()

add_executable(locus_carcerum_fuzz_host ${MMGR_FUZZ_SOURCES})
target_include_directories(locus_carcerum_fuzz_host PRIVATE "${MMGR_TEST_ROOT}/support")
target_compile_definitions(locus_carcerum_fuzz_host PRIVATE MMGR_CARCER_OWNERS=1024u)
target_compile_options(locus_carcerum_fuzz_host PRIVATE -fsanitize=fuzzer)
target_link_options(locus_carcerum_fuzz_host PRIVATE -fsanitize=fuzzer)
target_link_libraries(locus_carcerum_fuzz_host PRIVATE mmgr_host)
if(WIN32)
    target_link_libraries(locus_carcerum_fuzz_host PRIVATE synchronization)
endif()
set_target_properties(locus_carcerum_fuzz_host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/host")
