/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file device_unity.h
 * @brief The device suite: each case of a suite runs in a kernel of its own on the GPU, and Unity's assertions
 *        record into the case that runs.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-26
 *
 * @note harness.py runners device writes the unit of a device suite. The unit holds the cases of the suite, each
 *       function marked for both passes, and one kernel for each case. Its main calls mmgr_device_suite_run.
 * @note device_unity.cu defines the assertion calls of Unity for both passes, in place of unity.c. A device suite
 *       compiles with UNITY_EXCLUDE_SETJMP_H. A failed assertion then returns, and the case goes on to its end.
 *       The record keeps the first failure, and each later assertion of that case prints nothing.
 */
#ifndef DEVICE_UNITY_H
#define DEVICE_UNITY_H

#include "embed_compiler_directives.h"
#include "unity.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @name Unity's assertion calls for both passes
     * @brief unity_internals.h declares each of them for the host. Each declaration here adds the device, and a case
     *        in a kernel then calls it. device_unity.cu defines each of them.
     * @{
     */
    EMBED_HOST_DEVICE void UnityAssertEqualNumber(const UNITY_INT expected, const UNITY_INT actual, const char *msg,
                                                  const UNITY_LINE_TYPE lineNumber, const UNITY_DISPLAY_STYLE_T style);
    EMBED_HOST_DEVICE void UnityAssertGreaterOrLessOrEqualNumber(const UNITY_INT threshold, const UNITY_INT actual,
                                                                 const UNITY_COMPARISON_T compare, const char *msg,
                                                                 const UNITY_LINE_TYPE lineNumber,
                                                                 const UNITY_DISPLAY_STYLE_T style);
    EMBED_HOST_DEVICE void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual,
                                                    const UNITY_UINT32 num_elements, const char *msg,
                                                    const UNITY_LINE_TYPE lineNumber, const UNITY_DISPLAY_STYLE_T style,
                                                    const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityAssertBits(const UNITY_INT mask, const UNITY_INT expected, const UNITY_INT actual,
                                           const char *msg, const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertEqualString(const char *expected, const char *actual, const char *msg,
                                                  const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertEqualStringLen(const char *expected, const char *actual,
                                                     const UNITY_UINT32 length, const char *msg,
                                                     const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertEqualStringArray(UNITY_INTERNAL_PTR expected, const char **actual,
                                                       const UNITY_UINT32 num_elements, const char *msg,
                                                       const UNITY_LINE_TYPE lineNumber, const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual,
                                                  const UNITY_UINT32 length, const UNITY_UINT32 num_elements,
                                                  const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                  const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityAssertNumbersWithin(const UNITY_UINT delta, const UNITY_INT expected,
                                                    const UNITY_INT actual, const char *msg,
                                                    const UNITY_LINE_TYPE lineNumber,
                                                    const UNITY_DISPLAY_STYLE_T style);
    EMBED_HOST_DEVICE void UnityAssertNumbersArrayWithin(const UNITY_UINT delta, UNITY_INTERNAL_PTR expected,
                                                         UNITY_INTERNAL_PTR actual, const UNITY_UINT32 num_elements,
                                                         const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                         const UNITY_DISPLAY_STYLE_T style, const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityFail(const char *message, const UNITY_LINE_TYPE line);
    EMBED_HOST_DEVICE void UnityIgnore(const char *message, const UNITY_LINE_TYPE line);
    EMBED_HOST_DEVICE void UnityMessage(const char *message, const UNITY_LINE_TYPE line);
    EMBED_HOST_DEVICE void UnityAssertFloatsWithin(const UNITY_FLOAT delta, const UNITY_FLOAT expected,
                                                   const UNITY_FLOAT actual, const char *msg,
                                                   const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertFloatsNotWithin(const UNITY_FLOAT delta, const UNITY_FLOAT expected,
                                                      const UNITY_FLOAT actual, const char *msg,
                                                      const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertGreaterOrLessFloat(const UNITY_FLOAT threshold, const UNITY_FLOAT actual,
                                                         const UNITY_COMPARISON_T compare, const char *msg,
                                                         const UNITY_LINE_TYPE linenumber);
    EMBED_HOST_DEVICE void UnityAssertWithinFloatArray(const UNITY_FLOAT delta, const UNITY_FLOAT *expected,
                                                       const UNITY_FLOAT *actual, const UNITY_UINT32 num_elements,
                                                       const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                       const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityAssertFloatSpecial(const UNITY_FLOAT actual, const char *msg,
                                                   const UNITY_LINE_TYPE lineNumber, const UNITY_FLOAT_TRAIT_T style);
    EMBED_HOST_DEVICE void UnityAssertDoublesWithin(const UNITY_DOUBLE delta, const UNITY_DOUBLE expected,
                                                    const UNITY_DOUBLE actual, const char *msg,
                                                    const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertDoublesNotWithin(const UNITY_DOUBLE delta, const UNITY_DOUBLE expected,
                                                       const UNITY_DOUBLE actual, const char *msg,
                                                       const UNITY_LINE_TYPE lineNumber);
    EMBED_HOST_DEVICE void UnityAssertGreaterOrLessDouble(const UNITY_DOUBLE threshold, const UNITY_DOUBLE actual,
                                                          const UNITY_COMPARISON_T compare, const char *msg,
                                                          const UNITY_LINE_TYPE linenumber);
    EMBED_HOST_DEVICE void UnityAssertWithinDoubleArray(const UNITY_DOUBLE delta, const UNITY_DOUBLE *expected,
                                                        const UNITY_DOUBLE *actual, const UNITY_UINT32 num_elements,
                                                        const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                        const UNITY_FLAGS_T flags);
    EMBED_HOST_DEVICE void UnityAssertDoubleSpecial(const UNITY_DOUBLE actual, const char *msg,
                                                    const UNITY_LINE_TYPE lineNumber, const UNITY_FLOAT_TRAIT_T style);
    EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityNumToPtr(const UNITY_INT num, const UNITY_UINT8 size);
    EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityFloatToPtr(const float num);
    EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityDoubleToPtr(const double num);
    /** @} */

    /** @brief What one case did, which its kernel writes and the host reads after the kernel ends. */
    typedef struct
    {
        const char *file;          /**< The source of the suite, as the failures name it [BORROWS]. */
        const char *name;          /**< The name of the case [BORROWS]. */
        unsigned int failed;       /**< 1 where an assertion of the case failed. */
        unsigned int ignored;      /**< 1 where the case called TEST_IGNORE. */
        unsigned int failure_line; /**< The line of the first failure, or 0. */
    } MmgrDeviceCase;

    /** @brief One case of a device suite as the host launches it. */
    typedef struct
    {
        const char *name;                       /**< The name of the case [BORROWS]. */
        void (*launch)(MmgrDeviceCase *record); /**< Launches the kernel of the case on one thread. */
    } MmgrDeviceSuiteCase;

    /**
     * @brief The record of the case that runs, which the kernel of the case sets first.
     *
     * @param[in] record The record in memory that the device and the host both read [BORROWS].
     * @param[in] file   The source of the suite [BORROWS].
     * @param[in] name   The name of the case [BORROWS].
     */
    EMBED_HOST_DEVICE void mmgr_device_case_begin(MmgrDeviceCase *record, const char *file, const char *name);

    /**
     * @brief Runs each case of a suite in its own kernel, one after the other, and prints a line for each.
     *
     * @param[in] file  The source of the suite [BORROWS].
     * @param[in] cases The cases, in the order of the source [BORROWS].
     * @param[in] count The count of cases.
     * @return          The count of cases that failed, crashed or did not launch.
     * @note It sets the stack of each device thread before the first launch, as MMgr's CUDA guide says. A case that
     *       crashes ends the run, as the device then refuses each later launch.
     */
    int mmgr_device_suite_run(const char *file, const MmgrDeviceSuiteCase *cases, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif
