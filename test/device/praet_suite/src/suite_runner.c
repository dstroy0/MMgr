/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file suite_runner.c
 * @brief Pulls in the generated Unity runner, whose main takes the name praet_suite_main.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note The runner comes from the case names in the suite source. A case added there registers
 *       here with nothing written by hand. Regenerate it with harness.py build.
 * @warning A stale runner registers only the cases from its own time and the rest never
 *          run, while the suite still reports green. For exactly that reason harness.py remote
 *          regenerates before it sends, and a device build has to receive a current one.
 */
#include "../../../integration/test_praet_correctness/unity_runner.c"
