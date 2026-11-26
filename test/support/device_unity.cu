/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file device_unity.cu
 * @brief The assertion calls of Unity for both passes of nvcc, and the host loop that runs each case of a device
 *        suite in its own kernel.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-26
 *
 * @note unity_internals.h declares each call for the host. Each definition here adds the device, and nvcc then
 *       takes the call as a call of both passes (warning 20040).
 * @note A failure prints the line that Unity prints, file:line:case:FAIL: and the reason, through printf. A kernel
 *       prints into the buffer of the device, and the host gets the text when the kernel ends.
 */
#include "device_unity.h"

#include <cuda_runtime.h>
#include <math.h>
#include <stdio.h>

/** @brief The record of the case that runs, in the memory of the pass that runs it. */
static EMBED_PASS_MEMORY MmgrDeviceCase *s_case;

/** @brief Room for the value of UnityNumToPtr, UnityFloatToPtr and UnityDoubleToPtr, one for each pass. */
static EMBED_PASS_MEMORY union {
    UNITY_INT8 i8;
    UNITY_INT16 i16;
    UNITY_INT32 i32;
    UNITY_INT64 i64;
    float f;
    double d;
} s_value;

extern "C" EMBED_HOST_DEVICE void mmgr_device_case_begin(MmgrDeviceCase *record, const char *file, const char *name)
{
    s_case = record;
    record->file = file;
    record->name = name;
    record->failed = 0u;
    record->ignored = 0u;
    record->failure_line = 0u;
}

/**
 * @brief The start of the line of a failure: the file, the line and the case, as Unity prints them.
 *
 * @param[in] line The line of the assertion.
 * @return         1 where the case had no failure before, and the caller prints the rest of the line. 0 where the
 *                 case failed before, and the caller prints nothing.
 */
static EMBED_HOST_DEVICE int device_unity_failure(UNITY_LINE_TYPE line)
{
    if (s_case->failed != 0u)
    {
        return 0;
    }
    s_case->failed = 1u;
    s_case->failure_line = (unsigned int)line;
    printf("%s:%u:%s:FAIL: ", s_case->file, (unsigned int)line, s_case->name);
    return 1;
}

/** @brief The message of the assertion after the reason, and the end of the line. */
static EMBED_HOST_DEVICE void device_unity_message_end(const char *msg)
{
    if (msg != NULL)
    {
        printf(" %s", msg);
    }
    printf("\n");
}

/** @brief The bits of number that a value of style holds, the rest cleared. */
static EMBED_HOST_DEVICE UNITY_UINT device_unity_masked(UNITY_INT number, UNITY_DISPLAY_STYLE_T style)
{
    const unsigned int bytes = (unsigned int)style & 0x0Fu;
    if ((bytes == 0u) || (bytes >= sizeof(UNITY_UINT)))
    {
        return (UNITY_UINT)number;
    }
    return (UNITY_UINT)number & (((UNITY_UINT)1u << (bytes * 8u)) - 1u);
}

/** @brief A number as Unity prints a value of style: hex, a character, signed or unsigned. */
static EMBED_HOST_DEVICE void device_unity_number(UNITY_INT number, UNITY_DISPLAY_STYLE_T style)
{
    const unsigned int range = (unsigned int)style & 0xF0u;
    if ((range & UNITY_DISPLAY_RANGE_HEX) != 0u)
    {
        printf("0x%0*llX", (int)(((unsigned int)style & 0x0Fu) * 2u),
               (unsigned long long)device_unity_masked(number, style));
    }
    else if ((range & UNITY_DISPLAY_RANGE_CHAR) != 0u)
    {
        if ((number >= 32) && (number < 127))
        {
            printf("'%c'", (char)number);
        }
        else
        {
            printf("0x%02X", (unsigned int)(number & 0xFF));
        }
    }
    else if ((range & UNITY_DISPLAY_RANGE_INT) != 0u)
    {
        printf("%lld", (long long)number);
    }
    else
    {
        printf("%llu", (unsigned long long)device_unity_masked(number, style));
    }
}

/** @brief A value of the bytes and style of an array element at element, read as Unity reads it. */
static EMBED_HOST_DEVICE UNITY_INT device_unity_element(UNITY_INTERNAL_PTR element, UNITY_DISPLAY_STYLE_T style)
{
    const unsigned int bytes = (unsigned int)style & 0x0Fu;
    const int is_signed = (((unsigned int)style & UNITY_DISPLAY_RANGE_INT) != 0u);
    switch (bytes)
    {
    case 1u:
        return is_signed ? (UNITY_INT)(*(const UNITY_INT8 *)element) : (UNITY_INT)(*(const UNITY_UINT8 *)element);
    case 2u:
        return is_signed ? (UNITY_INT)(*(const UNITY_INT16 *)element) : (UNITY_INT)(*(const UNITY_UINT16 *)element);
    case 4u:
        return is_signed ? (UNITY_INT)(*(const UNITY_INT32 *)element) : (UNITY_INT)(*(const UNITY_UINT32 *)element);
    default:
        return (UNITY_INT)(*(const UNITY_INT64 *)element);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualNumber(const UNITY_INT expected, const UNITY_INT actual,
                                                         const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                         const UNITY_DISPLAY_STYLE_T style)
{
    if ((expected != actual) && device_unity_failure(lineNumber))
    {
        printf("Expected ");
        device_unity_number(expected, style);
        printf(" Was ");
        device_unity_number(actual, style);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertGreaterOrLessOrEqualNumber(
    const UNITY_INT threshold, const UNITY_INT actual, const UNITY_COMPARISON_T compare, const char *msg,
    const UNITY_LINE_TYPE lineNumber, const UNITY_DISPLAY_STYLE_T style)
{
    int failed = (threshold == actual) && ((compare & UNITY_EQUAL_TO) == 0);
    if ((((unsigned int)style & UNITY_DISPLAY_RANGE_INT) != 0u))
    {
        failed |= (actual > threshold) && ((compare & UNITY_SMALLER_THAN) != 0);
        failed |= (actual < threshold) && ((compare & UNITY_GREATER_THAN) != 0);
    }
    else
    {
        const UNITY_UINT unsigned_actual = device_unity_masked(actual, style);
        const UNITY_UINT unsigned_threshold = device_unity_masked(threshold, style);
        failed |= (unsigned_actual > unsigned_threshold) && ((compare & UNITY_SMALLER_THAN) != 0);
        failed |= (unsigned_actual < unsigned_threshold) && ((compare & UNITY_GREATER_THAN) != 0);
    }
    if (failed && device_unity_failure(lineNumber))
    {
        printf("Expected ");
        device_unity_number(actual, style);
        printf(" against the threshold ");
        device_unity_number(threshold, style);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual,
                                                           const UNITY_UINT32 num_elements, const char *msg,
                                                           const UNITY_LINE_TYPE lineNumber,
                                                           const UNITY_DISPLAY_STYLE_T style, const UNITY_FLAGS_T flags)
{
    const unsigned int bytes =
        (((unsigned int)style & 0x0Fu) == 0u) ? (unsigned int)sizeof(UNITY_INT) : ((unsigned int)style & 0x0Fu);
    if (num_elements == 0u)
    {
        if (device_unity_failure(lineNumber))
        {
            printf("You asked me to compare nothing, which was pointless.");
            device_unity_message_end(msg);
        }
        return;
    }
    if ((expected == NULL) || (actual == NULL))
    {
        if ((expected != actual) && device_unity_failure(lineNumber))
        {
            printf("Expected pointer to be NULL");
            device_unity_message_end(msg);
        }
        return;
    }
    const unsigned char *expected_at = (const unsigned char *)expected;
    const unsigned char *actual_at = (const unsigned char *)actual;
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        const UNITY_INT expected_value = device_unity_element(expected_at, style);
        const UNITY_INT actual_value = device_unity_element(actual_at, style);
        if (device_unity_masked(expected_value, style) != device_unity_masked(actual_value, style))
        {
            if (device_unity_failure(lineNumber))
            {
                printf("Element %u Expected ", (unsigned int)element);
                device_unity_number(expected_value, style);
                printf(" Was ");
                device_unity_number(actual_value, style);
                device_unity_message_end(msg);
            }
            return;
        }
        if (flags == UNITY_ARRAY_TO_ARRAY)
        {
            expected_at += bytes;
        }
        actual_at += bytes;
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertBits(const UNITY_INT mask, const UNITY_INT expected,
                                                  const UNITY_INT actual, const char *msg,
                                                  const UNITY_LINE_TYPE lineNumber)
{
    if (((mask & expected) != (mask & actual)) && device_unity_failure(lineNumber))
    {
        printf("Expected bits 0x%llX of 0x%llX Was 0x%llX", (unsigned long long)mask, (unsigned long long)expected,
               (unsigned long long)actual);
        device_unity_message_end(msg);
    }
}

/** @brief 1 where the first length bytes of two texts agree, or where length is 0 and both texts end together. */
static EMBED_HOST_DEVICE int device_unity_texts_agree(const char *expected, const char *actual, UNITY_UINT32 length,
                                                      int bounded)
{
    for (UNITY_UINT32 at = 0u; !bounded || (at < length); at++)
    {
        if (expected[at] != actual[at])
        {
            return 0;
        }
        if (expected[at] == '\0')
        {
            return 1;
        }
    }
    return 1;
}

/** @brief The check of two texts that UnityAssertEqualString and UnityAssertEqualStringLen share. */
static EMBED_HOST_DEVICE void device_unity_texts(const char *expected, const char *actual, UNITY_UINT32 length,
                                                 int bounded, const char *msg, UNITY_LINE_TYPE lineNumber)
{
    int agree = 0;
    if ((expected == NULL) || (actual == NULL))
    {
        agree = (expected == actual);
    }
    else
    {
        agree = device_unity_texts_agree(expected, actual, length, bounded);
    }
    if (!agree && device_unity_failure(lineNumber))
    {
        printf("Expected '%s' Was '%s'", (expected != NULL) ? expected : "NULL", (actual != NULL) ? actual : "NULL");
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualString(const char *expected, const char *actual, const char *msg,
                                                         const UNITY_LINE_TYPE lineNumber)
{
    device_unity_texts(expected, actual, 0u, 0, msg, lineNumber);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualStringLen(const char *expected, const char *actual,
                                                            const UNITY_UINT32 length, const char *msg,
                                                            const UNITY_LINE_TYPE lineNumber)
{
    device_unity_texts(expected, actual, length, 1, msg, lineNumber);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualStringArray(UNITY_INTERNAL_PTR expected, const char **actual,
                                                              const UNITY_UINT32 num_elements, const char *msg,
                                                              const UNITY_LINE_TYPE lineNumber,
                                                              const UNITY_FLAGS_T flags)
{
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        const char *const expected_text =
            (flags == UNITY_ARRAY_TO_ARRAY) ? ((const char *const *)expected)[element] : (const char *)expected;
        device_unity_texts(expected_text, actual[element], 0u, 0, msg, lineNumber);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertEqualMemory(UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual,
                                                         const UNITY_UINT32 length, const UNITY_UINT32 num_elements,
                                                         const char *msg, const UNITY_LINE_TYPE lineNumber,
                                                         const UNITY_FLAGS_T flags)
{
    if ((num_elements == 0u) || (length == 0u))
    {
        if (device_unity_failure(lineNumber))
        {
            printf("You asked me to compare nothing, which was pointless.");
            device_unity_message_end(msg);
        }
        return;
    }
    const unsigned char *expected_at = (const unsigned char *)expected;
    const unsigned char *actual_at = (const unsigned char *)actual;
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        for (UNITY_UINT32 at = 0u; at < length; at++)
        {
            if (expected_at[at] != actual_at[at])
            {
                if (device_unity_failure(lineNumber))
                {
                    printf("Element %u Byte %u Expected 0x%02X Was 0x%02X", (unsigned int)element, (unsigned int)at,
                           (unsigned int)expected_at[at], (unsigned int)actual_at[at]);
                    device_unity_message_end(msg);
                }
                return;
            }
        }
        if (flags == UNITY_ARRAY_TO_ARRAY)
        {
            expected_at += length;
        }
        actual_at += length;
    }
}

/** @brief The distance of two values of style, as an unsigned count. */
static EMBED_HOST_DEVICE UNITY_UINT device_unity_distance(UNITY_INT expected, UNITY_INT actual,
                                                          UNITY_DISPLAY_STYLE_T style)
{
    if (((unsigned int)style & UNITY_DISPLAY_RANGE_INT) != 0u)
    {
        return (actual > expected) ? ((UNITY_UINT)actual - (UNITY_UINT)expected)
                                   : ((UNITY_UINT)expected - (UNITY_UINT)actual);
    }
    const UNITY_UINT unsigned_expected = device_unity_masked(expected, style);
    const UNITY_UINT unsigned_actual = device_unity_masked(actual, style);
    return (unsigned_actual > unsigned_expected) ? (unsigned_actual - unsigned_expected)
                                                 : (unsigned_expected - unsigned_actual);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertNumbersWithin(const UNITY_UINT delta, const UNITY_INT expected,
                                                           const UNITY_INT actual, const char *msg,
                                                           const UNITY_LINE_TYPE lineNumber,
                                                           const UNITY_DISPLAY_STYLE_T style)
{
    if ((device_unity_distance(expected, actual, style) > delta) && device_unity_failure(lineNumber))
    {
        printf("Values Not Within Delta %llu Expected ", (unsigned long long)delta);
        device_unity_number(expected, style);
        printf(" Was ");
        device_unity_number(actual, style);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertNumbersArrayWithin(
    const UNITY_UINT delta, UNITY_INTERNAL_PTR expected, UNITY_INTERNAL_PTR actual, const UNITY_UINT32 num_elements,
    const char *msg, const UNITY_LINE_TYPE lineNumber, const UNITY_DISPLAY_STYLE_T style, const UNITY_FLAGS_T flags)
{
    const unsigned int bytes =
        (((unsigned int)style & 0x0Fu) == 0u) ? (unsigned int)sizeof(UNITY_INT) : ((unsigned int)style & 0x0Fu);
    const unsigned char *expected_at = (const unsigned char *)expected;
    const unsigned char *actual_at = (const unsigned char *)actual;
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        UnityAssertNumbersWithin(delta, device_unity_element(expected_at, style),
                                 device_unity_element(actual_at, style), msg, lineNumber, style);
        if (flags == UNITY_ARRAY_TO_ARRAY)
        {
            expected_at += bytes;
        }
        actual_at += bytes;
    }
}

extern "C" EMBED_HOST_DEVICE void UnityFail(const char *message, const UNITY_LINE_TYPE line)
{
    if (device_unity_failure(line))
    {
        printf("%s\n", (message != NULL) ? message : "");
    }
}

extern "C" EMBED_HOST_DEVICE void UnityIgnore(const char *message, const UNITY_LINE_TYPE line)
{
    s_case->ignored = 1u;
    printf("%s:%u:%s:IGNORE: %s\n", s_case->file, (unsigned int)line, s_case->name, (message != NULL) ? message : "");
}

extern "C" EMBED_HOST_DEVICE void UnityMessage(const char *message, const UNITY_LINE_TYPE line)
{
    printf("%s:%u:%s:INFO: %s\n", s_case->file, (unsigned int)line, s_case->name, (message != NULL) ? message : "");
}

/** @brief 1 where two doubles agree within delta, both NaN, or both the same infinity. */
static EMBED_HOST_DEVICE int device_unity_doubles_within(double delta, double expected, double actual)
{
    if (isnan(expected) || isnan(actual))
    {
        return isnan(expected) && isnan(actual);
    }
    if (isinf(expected) || isinf(actual))
    {
        return expected == actual;
    }
    return fabs(expected - actual) <= fabs(delta);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertDoublesWithin(const UNITY_DOUBLE delta, const UNITY_DOUBLE expected,
                                                           const UNITY_DOUBLE actual, const char *msg,
                                                           const UNITY_LINE_TYPE lineNumber)
{
    if (!device_unity_doubles_within(delta, expected, actual) && device_unity_failure(lineNumber))
    {
        printf("Expected %.17g Was %.17g", expected, actual);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertDoublesNotWithin(const UNITY_DOUBLE delta, const UNITY_DOUBLE expected,
                                                              const UNITY_DOUBLE actual, const char *msg,
                                                              const UNITY_LINE_TYPE lineNumber)
{
    if (device_unity_doubles_within(delta, expected, actual) && device_unity_failure(lineNumber))
    {
        printf("Expected Not %.17g Was %.17g", expected, actual);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertGreaterOrLessDouble(const UNITY_DOUBLE threshold,
                                                                 const UNITY_DOUBLE actual,
                                                                 const UNITY_COMPARISON_T compare, const char *msg,
                                                                 const UNITY_LINE_TYPE linenumber)
{
    int failed = (threshold == actual) && ((compare & UNITY_EQUAL_TO) == 0);
    failed |= (actual > threshold) && ((compare & UNITY_SMALLER_THAN) != 0);
    failed |= (actual < threshold) && ((compare & UNITY_GREATER_THAN) != 0);
    if (failed && device_unity_failure(linenumber))
    {
        printf("Expected %.17g against the threshold %.17g", actual, threshold);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertWithinDoubleArray(const UNITY_DOUBLE delta, const UNITY_DOUBLE *expected,
                                                               const UNITY_DOUBLE *actual,
                                                               const UNITY_UINT32 num_elements, const char *msg,
                                                               const UNITY_LINE_TYPE lineNumber,
                                                               const UNITY_FLAGS_T flags)
{
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        const UNITY_DOUBLE expected_value = (flags == UNITY_ARRAY_TO_ARRAY) ? expected[element] : expected[0];
        UnityAssertDoublesWithin(delta, expected_value, actual[element], msg, lineNumber);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertDoubleSpecial(const UNITY_DOUBLE actual, const char *msg,
                                                           const UNITY_LINE_TYPE lineNumber,
                                                           const UNITY_FLOAT_TRAIT_T style)
{
    int holds = 0;
    switch (style)
    {
    case UNITY_FLOAT_IS_INF:
        holds = isinf(actual) && (actual > 0.0);
        break;
    case UNITY_FLOAT_IS_NOT_INF:
        holds = !(isinf(actual) && (actual > 0.0));
        break;
    case UNITY_FLOAT_IS_NEG_INF:
        holds = isinf(actual) && (actual < 0.0);
        break;
    case UNITY_FLOAT_IS_NOT_NEG_INF:
        holds = !(isinf(actual) && (actual < 0.0));
        break;
    case UNITY_FLOAT_IS_NAN:
        holds = isnan(actual);
        break;
    case UNITY_FLOAT_IS_NOT_NAN:
        holds = !isnan(actual);
        break;
    case UNITY_FLOAT_IS_DET:
        holds = !isnan(actual) && !isinf(actual);
        break;
    case UNITY_FLOAT_IS_NOT_DET:
        holds = isnan(actual) || isinf(actual);
        break;
    default:
        holds = 0;
        break;
    }
    if (!holds && device_unity_failure(lineNumber))
    {
        printf("Expected the trait %d Was %.17g", (int)style, actual);
        device_unity_message_end(msg);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertFloatsWithin(const UNITY_FLOAT delta, const UNITY_FLOAT expected,
                                                          const UNITY_FLOAT actual, const char *msg,
                                                          const UNITY_LINE_TYPE lineNumber)
{
    UnityAssertDoublesWithin((double)delta, (double)expected, (double)actual, msg, lineNumber);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertFloatsNotWithin(const UNITY_FLOAT delta, const UNITY_FLOAT expected,
                                                             const UNITY_FLOAT actual, const char *msg,
                                                             const UNITY_LINE_TYPE lineNumber)
{
    UnityAssertDoublesNotWithin((double)delta, (double)expected, (double)actual, msg, lineNumber);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertGreaterOrLessFloat(const UNITY_FLOAT threshold, const UNITY_FLOAT actual,
                                                                const UNITY_COMPARISON_T compare, const char *msg,
                                                                const UNITY_LINE_TYPE linenumber)
{
    UnityAssertGreaterOrLessDouble((double)threshold, (double)actual, compare, msg, linenumber);
}

extern "C" EMBED_HOST_DEVICE void UnityAssertWithinFloatArray(const UNITY_FLOAT delta, const UNITY_FLOAT *expected,
                                                              const UNITY_FLOAT *actual,
                                                              const UNITY_UINT32 num_elements, const char *msg,
                                                              const UNITY_LINE_TYPE lineNumber,
                                                              const UNITY_FLAGS_T flags)
{
    for (UNITY_UINT32 element = 0u; element < num_elements; element++)
    {
        const UNITY_FLOAT expected_value = (flags == UNITY_ARRAY_TO_ARRAY) ? expected[element] : expected[0];
        UnityAssertFloatsWithin(delta, expected_value, actual[element], msg, lineNumber);
    }
}

extern "C" EMBED_HOST_DEVICE void UnityAssertFloatSpecial(const UNITY_FLOAT actual, const char *msg,
                                                          const UNITY_LINE_TYPE lineNumber,
                                                          const UNITY_FLOAT_TRAIT_T style)
{
    UnityAssertDoubleSpecial((double)actual, msg, lineNumber, style);
}

extern "C" EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityNumToPtr(const UNITY_INT num, const UNITY_UINT8 size)
{
    switch (size)
    {
    case 1u:
        s_value.i8 = (UNITY_INT8)num;
        return &s_value.i8;
    case 2u:
        s_value.i16 = (UNITY_INT16)num;
        return &s_value.i16;
    case 4u:
        s_value.i32 = (UNITY_INT32)num;
        return &s_value.i32;
    default:
        s_value.i64 = (UNITY_INT64)num;
        return &s_value.i64;
    }
}

extern "C" EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityFloatToPtr(const float num)
{
    s_value.f = num;
    return &s_value.f;
}

extern "C" EMBED_HOST_DEVICE UNITY_INTERNAL_PTR UnityDoubleToPtr(const double num)
{
    s_value.d = num;
    return &s_value.d;
}

/** @brief The bytes of the stack of each device thread. The frame of cellul.find alone takes 1344 on sm_86. */
#define DEVICE_UNITY_STACK_BYTES (64u * 1024u)

/** @brief The bytes of the buffer that printf of a kernel writes into. */
#define DEVICE_UNITY_PRINT_BYTES (16u * 1024u * 1024u)

extern "C" int mmgr_device_suite_run(const char *file, const MmgrDeviceSuiteCase *cases, unsigned int count)
{
    MmgrDeviceCase *record = NULL;
    if ((cudaDeviceSetLimit(cudaLimitStackSize, DEVICE_UNITY_STACK_BYTES) != cudaSuccess) ||
        (cudaDeviceSetLimit(cudaLimitPrintfFifoSize, DEVICE_UNITY_PRINT_BYTES) != cudaSuccess) ||
        (cudaMallocManaged((void **)&record, sizeof(*record), cudaMemAttachGlobal) != cudaSuccess))
    {
        printf("%s: the device refused the limits or the record: %s\n", file, cudaGetErrorString(cudaGetLastError()));
        return (int)count;
    }
    unsigned int failures = 0u;
    unsigned int ignored = 0u;
    unsigned int ran = 0u;
    for (unsigned int place = 0u; place < count; place++)
    {
        cases[place].launch(record);
        const cudaError_t launched = cudaGetLastError();
        const cudaError_t ended = (launched == cudaSuccess) ? cudaDeviceSynchronize() : launched;
        ran += 1u;
        if (ended != cudaSuccess)
        {
            printf("%s:0:%s:FAIL: the kernel of the case ended with %s\n", file, cases[place].name,
                   cudaGetErrorString(ended));
            failures += count - place;
            break;
        }
        if (record->failed != 0u)
        {
            failures += 1u;
        }
        else if (record->ignored != 0u)
        {
            ignored += 1u;
        }
        else
        {
            printf("%s:0:%s:PASS\n", file, cases[place].name);
        }
    }
    printf("\n-----------------------\n%u Tests %u Failures %u Ignored\n%s\n", ran, failures, ignored,
           (failures == 0u) ? "OK" : "FAIL");
    return (int)failures;
}
