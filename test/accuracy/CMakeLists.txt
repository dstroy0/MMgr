# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# What a conversion actually produces, checked against a reference that is right by construction.
#
# A unit suite asks whether an entry keeps the contract its header states. A table-driven converter
# can keep every one of those and still be numerically wrong, because a table that is internally
# consistent is not a table that is correct. These suites ask what the number is.
#
# A reference here is exact arithmetic, a value whose bits are known, or a round trip that must
# return what it started with. It is never this library agreeing with itself, and it is never the
# same table the code under test reads.

add_subdirectory(test_ascii_persona_bitorum_accuracy)
add_subdirectory(test_bitorum_introitus_exitus_accuracy)
add_subdirectory(test_cellularum_laboro_accuracy)
add_subdirectory(test_clz_accuracy)
add_subdirectory(test_endian_accuracy)
add_subdirectory(test_fractio_accuracy)
add_subdirectory(test_impensa_ancorae_acus_accuracy)
add_subdirectory(test_locus_carcerum_accuracy)
add_subdirectory(test_memoria_anularis_accuracy)
add_subdirectory(test_memoria_externa_accuracy)
add_subdirectory(test_memoria_operor_accuracy)
add_subdirectory(test_numeros_scribo_accuracy)
add_subdirectory(test_octetus_introitus_exitus_accuracy)
add_subdirectory(test_pow5_accuracy)
add_subdirectory(test_proximus_operor_accuracy)
add_subdirectory(test_spatium_accuracy)
add_subdirectory(test_transformo_accuracy)
add_subdirectory(test_verba_scribo_accuracy)
add_subdirectory(test_verbum_scrutor_accuracy)
