# One module, built once per environment.
#
# A module directory says what it is and what it needs, and nothing about how to build it:
#
#   mmgr_add_module(spatium SOURCES spatium.c DEPS)
#   mmgr_add_module(confinium SOURCES confinium.c DEPS memoria_operor)
#   mmgr_add_module(memoria_anularis HEADER_ONLY DEPS proximus_operor spatium)
#   mmgr_add_module(verba_scribo SOURCES verba_scribo.c DEPS fractio OPTIMIZE -O2)
#
# OPTIMIZE overrides the build's level for this module alone. One level for the whole library is a
# guess that suits some of it. At -O3 the compiler unrolls and inlines, which is worth its cost
# where the work is in a loop the compiler can see through. It is dead weight where the code is
# already the shape it wants to be. The tool tools/dev_env/sizes.py measures the difference per
# unit and the numbers are in docs/quality/optimization.md. A module that names a level here should
# have a reason there.
#
# A target's own options come after the build's on the command line, and the last -O wins. So this
# needs nothing removed to take effect.
#
# Every environment in MMGR_ENVIRONMENTS gets its own target, named mmgr_<module>_<env>. That is the
# purpose of the widths as compile-time knobs: a 16-bit scan lane runs on this machine through
# one define. A change that only breaks that lane fails a build here and does not wait for
# someone to own the hardware.
#
# DEPS names modules and not targets. The environment suffix goes on here. A module never has to
# know which environment it builds for. It cannot accidentally link across two of them, which
# would mix objects compiled with different word widths into one binary.

function(mmgr_env_name entry out_name out_defs)
  # An entry is "name|DEF=1;DEF2=2". A list cannot nest in CMake, hence the separator.
  string(FIND "${entry}" "|" _pos)
  string(SUBSTRING "${entry}" 0 ${_pos} _name)
  math(EXPR _after "${_pos} + 1")
  string(SUBSTRING "${entry}" ${_after} -1 _defs)
  set(${out_name} "${_name}" PARENT_SCOPE)
  set(${out_defs} "${_defs}" PARENT_SCOPE)
endfunction()

function(mmgr_add_module name)
  cmake_parse_arguments(ARG "HEADER_ONLY" "OPTIMIZE" "SOURCES;DEPS" ${ARGN})

  foreach(entry IN LISTS MMGR_ENVIRONMENTS)
    mmgr_env_name("${entry}" env defs)
    set(target mmgr_${name}_${env})

    if(ARG_HEADER_ONLY)
      # No .c to compile, but it still carries includes, definitions and dependencies. It still
      # has to exist once per environment so a consumer can link it inside one width.
      add_library(${target} INTERFACE)
      set(scope INTERFACE)
    else()
      set(_srcs "")
      foreach(s IN LISTS ARG_SOURCES)
        list(APPEND _srcs "${CMAKE_CURRENT_SOURCE_DIR}/${s}")
      endforeach()
      add_library(${target} STATIC ${_srcs})
      set(scope PUBLIC)
      target_link_libraries(${target} PRIVATE mmgr_flags)

      if(ARG_OPTIMIZE AND MMGR_MODULE_OPTIMIZE)
        # After mmgr_flags, and so this is the last -O on the line and the one that counts.
        target_compile_options(${target} PRIVATE ${ARG_OPTIMIZE})
      endif()
    endif()

    # src/ is the include root, and so every module header has the path <module>/<module>.h. The
    # include/ directory carries mmgr.h, which every module header includes for the embed macros and the widths.
    target_include_directories(${target} ${scope} "${MMGR_INCLUDE_DIR}" "${MMgr_SOURCE_DIR}/include")

    # embedded_types rides the module target at the module's own scope, for the same reason the
    # capabilities below do. A suite links the module and deliberately does not link mmgr_flags.
    # The header include/mmgr.h includes the three embed headers and every module header includes it.
    # So every translation unit that reaches any MMgr header needs this include directory, cases included.
    target_link_libraries(${target} ${scope} embedded_types::embedded_types)

    # Capabilities ride the module target, not mmgr_flags, because a suite links the module and
    # deliberately does not link mmgr_flags. On mmgr_flags the define would reach src/ and not the
    # cases that exercise it. A suite could build in the belief that a feature is absent which the
    # library has.
    #
    # Every capability and not only the new one. A capability that reaches mmgr_add_suite and stops
    # there builds its suite, once on, against a module whose body is still compiled out. The
    # suite passes, and it tests nothing. A capability has to decide what goes into the build as
    # well as what runs.
    foreach(cap IN LISTS MMGR_CAPABILITIES)
      list(APPEND defs MMGR_ENABLE_${cap}=$<BOOL:${MMGR_ENABLE_${cap}}>)
    endforeach()

    # The DMA knobs ride the same way and for the same reason. They size the schedule's contexts in
    # the header. A suite that saw a different channel count from the library would
    # declare contexts of a different size from the ones the library walks.
    if(MMGR_ENABLE_DMA)
      list(APPEND defs ${MMGR_PRAET_KNOBS})
    endif()

    if(NOT defs STREQUAL "")
      target_compile_definitions(${target} ${scope} ${defs})
    endif()

    # checks is a test environment, and what it wants is a failed expectation that reports and stops.
    # That needs stdio and stdlib, which cannot sit in src/. A library for a part with no libc does
    # not reach for fprintf and abort because of a test flag. The report forms live in
    # test/support/mmgr_host_traps.h. They come in ahead of every other header, and so they are the
    # definitions src/ meets on this environment.
    #
    # On the module target and not only the suite, because the asserts under arm are the ones
    # compiled into src/. Carried at ${scope}. A suite that links this module inherits it.
    if(env STREQUAL "checks")
      target_compile_options(${target} ${scope}
                             "-include" "${MMgr_SOURCE_DIR}/test/support/mmgr_host_traps.h")
    endif()

    foreach(dep IN LISTS ARG_DEPS)
      target_link_libraries(${target} ${scope} mmgr_${dep}_${env})
    endforeach()

    # Collected so the per-environment aggregate can link every module with no list of them.
    set_property(GLOBAL APPEND PROPERTY MMGR_TARGETS_${env} ${target})
  endforeach()
endfunction()
