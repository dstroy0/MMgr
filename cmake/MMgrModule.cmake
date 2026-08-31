# One module, built once per environment.
#
# A module directory says what it is and what it needs, and nothing about how it is built:
#
#   mmgr_add_module(spatium SOURCES spatium.c DEPS)
#   mmgr_add_module(confinium SOURCES confinium.c DEPS memoria_operor)
#   mmgr_add_module(memoria_anularis HEADER_ONLY DEPS proximus_operor spatium)
#   mmgr_add_module(verba_scribo SOURCES verba_scribo.c DEPS fractio OPTIMIZE -O2)
#
# OPTIMIZE overrides the build's level for this module alone. One level for the whole library is a
# guess that suits some of it: -O3 unrolls and inlines, which is worth paying for where the work is
# in a loop the compiler can see through, and is dead weight where the code is already the shape it
# wants to be. tools/dev_env/sizes.py measures the difference per unit and the numbers are in
# docs/quality/optimisation.md - a module that names a level here should have a reason there.
#
# A target's own options come after the build's on the command line, and the last -O wins, so this
# needs nothing removed to take effect.
#
# Every environment in MMGR_ENVIRONMENTS gets its own target, named mmgr_<module>_<env>. That is the
# whole point of the widths being compile-time knobs: a 16-bit scan lane is exercised on this
# machine by defining it, so a change that only breaks that lane fails a build here rather than
# waiting for someone to own the hardware.
#
# DEPS names modules, not targets. The environment suffix is appended here, so a module never has to
# know which environment it is being built for and cannot accidentally link across two of them -
# which would mix objects compiled with different word widths into one binary.

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
      # No .c to compile, but it still carries includes, definitions and dependencies, and it still
      # has to exist once per environment so a consumer can link it without crossing width settings.
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

      if(ARG_OPTIMIZE)
        # After mmgr_flags, so this is the last -O on the line and the one that counts.
        target_compile_options(${target} PRIVATE ${ARG_OPTIMIZE})
      endif()
    endif()

    # src/ is the include root, so every module header is reached as <module>/<module>.h. include/
    # carries mmgr.h, which every module header includes for the embed macros and the widths.
    target_include_directories(${target} ${scope} "${MMGR_INCLUDE_DIR}" "${CMAKE_SOURCE_DIR}/include")

    # embedded_types rides the module target at the module's own scope, for the same reason the
    # capabilities below do: a suite links the module and deliberately does not link mmgr_flags.
    # include/mmgr.h includes the three embed headers and every module header includes it, so every
    # translation unit reaching any MMgr header needs this include directory, cases included.
    target_link_libraries(${target} ${scope} embedded_types::embedded_types)

    # Capabilities ride the module target, not mmgr_flags, because a suite links the module and
    # deliberately does not link mmgr_flags. On mmgr_flags the define would reach src/ and not the
    # cases that exercise it, so a suite could be built believing a feature absent that the library
    # was compiled with.
    #
    # Every capability, not only the new one. Before this they reached mmgr_add_suite and stopped
    # there, so turning one on built its suite against a module whose body was still compiled out -
    # the suite passed, and it had tested nothing. A capability has to decide what is compiled, not
    # only what is run.
    foreach(cap IN LISTS MMGR_CAPABILITIES)
      list(APPEND defs MMGR_ENABLE_${cap}=$<BOOL:${MMGR_ENABLE_${cap}}>)
    endforeach()

    if(NOT defs STREQUAL "")
      target_compile_definitions(${target} ${scope} ${defs})
    endif()

    # checks is a test environment, and what it wants is a failed expectation that reports and stops.
    # That needs stdio and stdlib, which cannot sit in src/ - a library for a part with no libc does
    # not reach for fprintf and abort because a test flag was set. The reporting forms live in
    # test/support/mmgr_host_traps.h and are forced in ahead of every other header, so they are the
    # definitions src/ meets on this environment.
    #
    # On the module target rather than only the suite, because the asserts being armed are the ones
    # compiled into src/. Carried at ${scope}, so a suite linking this module inherits it.
    if(env STREQUAL "checks")
      target_compile_options(${target} ${scope}
                             "-include" "${CMAKE_SOURCE_DIR}/test/support/mmgr_host_traps.h")
    endif()

    foreach(dep IN LISTS ARG_DEPS)
      target_link_libraries(${target} ${scope} mmgr_${dep}_${env})
    endforeach()

    # Collected so the per-environment aggregate can link every module without listing them.
    set_property(GLOBAL APPEND PROPERTY MMGR_TARGETS_${env} ${target})
  endforeach()
endfunction()
