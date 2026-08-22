# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# src/ is the include root: a module header is reached as <module>/<module>.h and mmgr_config.h
# resolves with the same single -I, so a consumer adds one directory.
set(MMGR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "include root")

# One directory per module, each declaring only what it is and what it needs.
# Ordered so a dependency is defined before whatever links it.
add_subdirectory(impensa_ancorae_acus)
add_subdirectory(ascii_persona_bitorum)
add_subdirectory(bitorum_introitus_exitus)
add_subdirectory(endian)
add_subdirectory(pow5)
add_subdirectory(proximus_operor)
add_subdirectory(verbum_scrutor)
add_subdirectory(memoria_operor)
add_subdirectory(cellularum_laboro)
add_subdirectory(spatium)
add_subdirectory(octetus_introitus_exitus)
add_subdirectory(carceribus)
add_subdirectory(custodia_soluta)
add_subdirectory(confinium_exclusivum_infinitas)
add_subdirectory(confinium_externum)
add_subdirectory(transformo)
add_subdirectory(dma)
add_subdirectory(fractio)
add_subdirectory(verba_scribo)
add_subdirectory(numeros_scribo)
add_subdirectory(custodia_secura)

# One aggregate per environment, so a test or a consumer links `mmgr_<env>` rather than naming
# seventeen modules and getting the list wrong.
foreach(entry IN LISTS MMGR_ENVIRONMENTS)
  mmgr_env_name("${entry}" env defs)
  get_property(_mods GLOBAL PROPERTY MMGR_TARGETS_${env})
  add_library(mmgr_${env} INTERFACE)
  target_link_libraries(mmgr_${env} INTERFACE ${_mods})
endforeach()
