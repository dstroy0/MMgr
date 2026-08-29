# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# A mirror of src: one directory per module, and config beside them for the headers every
# module is compiled against rather than anything a module does. Every
# translation unit has a suite, so a module that grows a second .c grows a second suite
# beside it rather than sharing one.

add_subdirectory(clz)
add_subdirectory(impensa_ancorae_acus)
add_subdirectory(ascii_persona_bitorum)
add_subdirectory(bitorum_introitus_exitus)
add_subdirectory(octetus_introitus_exitus)
add_subdirectory(cellularum_laboro)
add_subdirectory(config)
add_subdirectory(locus_carcerum)
add_subdirectory(confinium_exclusivum_infinitas)
add_subdirectory(confinium_externum)
add_subdirectory(endian)
add_subdirectory(fractio)
add_subdirectory(memoria_operor)
add_subdirectory(memoriam_praetereo)
add_subdirectory(mmgr)
add_subdirectory(numeros_scribo)
add_subdirectory(proximus_operor)
add_subdirectory(spatium)
add_subdirectory(transformo)
add_subdirectory(verba_scribo)
add_subdirectory(verbum_scrutor)
