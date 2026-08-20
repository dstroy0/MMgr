# One module, built once per environment.
#
# A module directory says what it is and what it needs, and nothing about how it is built:
#
#   mmgr_add_module(spatium SOURCES spatium.c DEPS)
#   mmgr_add_module(confinium SOURCES confinium.c DEPS memoria_operor)
#   mmgr_add_module(confinium_exclusivum_infinitas HEADER_ONLY DEPS proximus_operor spatium)
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
  cmake_parse_arguments(ARG "HEADER_ONLY" "" "SOURCES;DEPS" ${ARGN})

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
    endif()

    # src/ is the include root, so every header is reached as <mmgr/<module>/<module>.h> and
    # mmgr_config.h resolves with the same single -I. A consumer of this library adds one directory.
    target_include_directories(${target} ${scope} "${MMGR_INCLUDE_DIR}")

    if(NOT defs STREQUAL "")
      target_compile_definitions(${target} ${scope} ${defs})
    endif()

    foreach(dep IN LISTS ARG_DEPS)
      target_link_libraries(${target} ${scope} mmgr_${dep}_${env})
    endforeach()

    # Collected so the per-environment aggregate can link every module without listing them.
    set_property(GLOBAL APPEND PROPERTY MMGR_TARGETS_${env} ${target})
  endforeach()
endfunction()
