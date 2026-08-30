# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
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

add_subdirectory(test_pow5_accuracy)
