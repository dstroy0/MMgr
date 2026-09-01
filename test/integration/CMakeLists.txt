# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Cross translation unit, and combination logic. A unit suite proves one module keeps its
# own promises; these prove the modules still agree when stacked.

add_subdirectory(test_hostile_content)
add_subdirectory(test_praet_correctness)
add_subdirectory(test_read_bounds)
add_subdirectory(test_ring_bounds)
add_subdirectory(test_scan_reference)
add_subdirectory(test_stream_lexer)
add_subdirectory(test_tenant_lifecycle)
add_subdirectory(test_text_pipeline)
add_subdirectory(test_wire_roundtrip)
