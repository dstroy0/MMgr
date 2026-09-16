# The bench application component: the bench source, the library's own sources, and the shared bench
# macros on the include path (performance_benching/common holds device_bench.h).
#
# The library is compiled straight into this component rather than pulled in as an IDF component,
# because MMgr's root CMakeLists is a plain CMake project and does not register itself as one. That
# keeps the desktop build untouched: nothing here reaches back into it.
get_filename_component(MMGR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

file(GLOB_RECURSE MMGR_SRCS "${MMGR_ROOT}/src/*.c")

# One cost table, as src/impensa_ancorae_acus/CMakeLists.txt does. A glob takes all five, and the
# five define the same symbol, so the four that are not selected have to come back out.
if(NOT DEFINED MMGR_ANCORAE_FORMA)
  set(MMGR_ANCORAE_FORMA generic)
endif()
string(TOLOWER "${MMGR_ANCORAE_FORMA}" _forma)
foreach(_other english generic inet route uri)
  if(NOT _other STREQUAL _forma)
    list(FILTER MMGR_SRCS EXCLUDE REGEX "impensa_ancorae_acus_${_other}\\.c$")
  endif()
endforeach()

idf_component_register(
    SRCS "main.c" ${MMGR_SRCS}
    INCLUDE_DIRS "." "${MMGR_ROOT}/src" "${MMGR_ROOT}/include" "${MMGR_ROOT}/deps/embedded_types/include"
                 "${CMAKE_CURRENT_LIST_DIR}/../../common"
)

# Link-time optimization, because without it this bench measures the wrong thing. The SWAR
# primitives are generated in verbum_scrutor.c and called by name from every other module, so no
# translation unit can inline them on its own. The desktop build sets
# CMAKE_INTERPROCEDURAL_OPTIMIZATION for that reason. Built without it, a machine-word step becomes
# three out-of-line calls, each with its argument struct spilled to the stack first.
#
# Only these objects carry LTO IR; every IDF component is compiled as it always was, so the plugin
# has nothing to reorder outside this component.
# ESP-IDF appends -fno-lto to the link unconditionally (its own CMakeLists.txt). Appending -flto
# after it is not enough, so the flag is taken back out and replaced. Only this component's objects
# carry LTO IR, so the plugin has nothing to reorder in any IDF component.
target_compile_options(${COMPONENT_LIB} PRIVATE -flto=auto -ffat-lto-objects)
idf_build_get_property(_link_opts LINK_OPTIONS)
list(REMOVE_ITEM _link_opts "-fno-lto")
list(APPEND _link_opts "-flto=auto")
idf_build_set_property(LINK_OPTIONS "${_link_opts}")

# The banner's clock, taken from the configuration the image is built with rather than named here.
# sdkconfig decides the CPU frequency, and a figure typed in by hand goes stale the moment that
# changes, which reports cycle counts against a rate the part is not running at.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DBENCH_CPU_MHZ=${CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ}u
)
