# The libFuzzer harnesses, one for each module, built where MMGR_FUZZ is on and the compiler is clang, which carries
# the fuzzer runtime. Each one is also a test: a fixed seed and MMGR_FUZZ_RUNS inputs, and the suite runs every
# harness; harness.py fuzz runs them for as long as it is asked, over a corpus under build/.
#
# Most harnesses link the host library. Three build their module in themselves, with the switches the host library
# is built without: memoria_externa with external memory on, memoriam_praetereo with DMA on and its knobs, once
# with recovery and once lean, and impensa_ancorae_acus once for each of its five cost tables, of which a build
# links one.
if(NOT MMGR_FUZZ)
    return()
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "MMgr: the fuzz harnesses need clang, not built")
    return()
endif()

set(MMGR_FUZZ_RUNS 20000 CACHE STRING "Inputs each fuzz harness runs as a test")

set(MMGR_FUZZ_SANITIZERS -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined)
# The fuzzer runtime clang carries for Windows defines no stack-depth counters, and so that coverage stays off there
if(WIN32)
    list(APPEND MMGR_FUZZ_SANITIZERS -fno-sanitize-coverage=stack-depth)
endif()

# The address sanitizer's runtime DLL is placed beside the programs, where Windows looks first: a Visual Studio
# folder on the PATH carries an older copy, and a program loading it stops at once on an entry point it lacks
set(MMGR_FUZZ_RUNTIME "")
if(WIN32)
    execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir OUTPUT_VARIABLE resource OUTPUT_STRIP_TRAILING_WHITESPACE)
    file(GLOB MMGR_FUZZ_RUNTIME "${resource}/lib/windows/clang_rt.asan_dynamic-x86_64.dll")
    if(MMGR_FUZZ_RUNTIME)
        file(COPY ${MMGR_FUZZ_RUNTIME} DESTINATION "${CMAKE_BINARY_DIR}/test/host")
    else()
        message(WARNING "MMgr: clang's address sanitizer runtime DLL is not under ${resource}/lib/windows")
    endif()
endif()

# One harness: its executable from SOURCES, its test, and the label harness.py fuzz finds it by.
function(mmgr_fuzz_harness name)
    cmake_parse_arguments(ARG "HOST" "" "SOURCES;DEFINITIONS" ${ARGN})
    set(target "${name}_fuzz_host")
    add_executable(${target} ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${MMGR_TEST_ROOT}/support"
                                                 "${MMGR_INCLUDE_DIR}" "${MMgr_SOURCE_DIR}/include")
    target_compile_definitions(${target} PRIVATE MMGR_CARCER_OWNERS=1024u ${ARG_DEFINITIONS})
    target_compile_options(${target} PRIVATE ${MMGR_FUZZ_SANITIZERS})
    target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    if(ARG_HOST)
        target_sources(${target} PRIVATE "${MMGR_TEST_ROOT}/support/host_wait.c")
        if(NOT WIN32)
            set_source_files_properties("${MMGR_TEST_ROOT}/support/host_wait.c" TARGET_DIRECTORY ${target}
                                        PROPERTIES COMPILE_DEFINITIONS "_GNU_SOURCE=1")
        endif()
        target_link_libraries(${target} PRIVATE mmgr_host)
        if(WIN32)
            target_link_libraries(${target} PRIVATE synchronization)
        endif()
    else()
        target_link_libraries(${target} PRIVATE embedded_types::embedded_types)
    endif()
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/host")
    add_test(NAME ${name}_fuzz COMMAND ${target} -seed=1 -runs=${MMGR_FUZZ_RUNS} -max_len=4096)
    set_tests_properties(${name}_fuzz PROPERTIES LABELS fuzz)
endfunction()

set(MMGR_FUZZ_HOST_HARNESSES
    ascii_persona_bitorum
    bitorum_introitus_exitus
    cellularum_laboro
    clz
    endian
    fractio
    locus_carcerum
    memoria_anularis
    memoria_operor
    numeros_scribo
    octetus_introitus_exitus
    pow5
    proximus_operor
    spatium
    transformo
    verba_scribo
    verbum_scrutor)

foreach(harness IN LISTS MMGR_FUZZ_HOST_HARNESSES)
    mmgr_fuzz_harness(${harness} HOST SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${harness}_fuzz.c")
endforeach()

mmgr_fuzz_harness(memoria_externa
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/memoria_externa_fuzz.c" "${MMGR_INCLUDE_DIR}/memoria_externa/memoria_externa.c"
    DEFINITIONS MMGR_ENABLE_EXTRAM=1 MMGR_ENABLE_DMA=0)

set(MMGR_FUZZ_PRAET_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/memoriam_praetereo_fuzz.c"
    "${MMGR_INCLUDE_DIR}/memoriam_praetereo/memoriam_praetereo.c"
    "${MMGR_INCLUDE_DIR}/memoriam_praetereo/praet_ordo.c"
    "${MMGR_INCLUDE_DIR}/memoriam_praetereo/praet_descriptor.c")
set(MMGR_FUZZ_PRAET_KNOBS
    PRAET_CHANNELS=8u PRAET_KEEPALIVE_MICROS=250u PRAET_CLOCK_HZ=240000000u PRAET_CLOCK_SOURCE=PRAET_CLOCK_CALLER)
mmgr_fuzz_harness(memoriam_praetereo
    SOURCES ${MMGR_FUZZ_PRAET_SOURCES}
    DEFINITIONS MMGR_ENABLE_DMA=1 MMGR_ENABLE_EXTRAM=0 ${MMGR_FUZZ_PRAET_KNOBS} PRAET_SETTLE_MICROS=40u PRAET_RECOVERY=1)
mmgr_fuzz_harness(memoriam_praetereo_lean
    SOURCES ${MMGR_FUZZ_PRAET_SOURCES}
    DEFINITIONS MMGR_ENABLE_DMA=1 MMGR_ENABLE_EXTRAM=0 ${MMGR_FUZZ_PRAET_KNOBS} PRAET_SETTLE_MICROS=0u PRAET_RECOVERY=0)

# Each cost table and the second byte it bars beside the NUL.
set(MMGR_FUZZ_ANCORAE english:32 generic:32 inet:58 route:47 uri:47)
foreach(pair IN LISTS MMGR_FUZZ_ANCORAE)
    string(REPLACE ":" ";" parts "${pair}")
    list(GET parts 0 forma)
    list(GET parts 1 barred)
    mmgr_fuzz_harness(impensa_ancorae_acus_${forma}
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/impensa_ancorae_acus_fuzz.c"
                "${MMGR_INCLUDE_DIR}/impensa_ancorae_acus/impensa_ancorae_acus_${forma}.c"
        DEFINITIONS FUZZ_ANCORAE_BARRED=${barred}u MMGR_ENABLE_DMA=0 MMGR_ENABLE_EXTRAM=0)
endforeach()
