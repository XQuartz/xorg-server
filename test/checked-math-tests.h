/**
 * Copyright © 2026 Red Hat, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice (including the next
 *  paragraph) shall be included in all copies or substantial portions of the
 *  Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

/* Warning: This file is included from the two checked-math tests with
 * different #defines set */

/* Test relies on assert() */
#undef NDEBUG

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <assert.h>
#include <stdint.h>
#include "checked-math.h"

#include "tests-common.h"

/* ---------------------------------------------------------------------
 * Overflow-checked and saturating arithmetic (include/checked-math.h)
 *
 * The expected results are computed by reference implementations
 * using independent formulations (precondition checks and wider
 * arithmetic), so that a bug in either the builtins or the fallback
 * path of the helpers shows up as a mismatch.
 * --------------------------------------------------------------------- */

/* Reference overflow checks for unsigned types of at most 64 bits,
 * max is the largest value of the type under test.
 */
static Bool
ref_uadd_ovf(uint64_t a, uint64_t b, uint64_t max)
{
    uint64_t r = a + b;

    return r > max || r < a;       /* r < a only fires for 64 bit types */
}

static Bool
ref_usub_ovf(uint64_t a, uint64_t b, uint64_t max)
{
    (void) max;                    /* underflow does not depend on the width */

    return a < b;
}

static Bool
ref_umul_ovf(uint64_t a, uint64_t b, uint64_t max)
{
    uint64_t r = a * b;

    return r > max || (a != 0 && r / a != b);
}

/* Reference overflow checks for signed types, min/max are the limits
 * of the type under test. The precondition formulations only ever
 * subtract a positive value from max or add a positive value to min,
 * so they cannot overflow themselves.
 */
static Bool
ref_sadd_ovf(int64_t a, int64_t b, int64_t min, int64_t max)
{
    return (b > 0 && a > max - b) || (b < 0 && a < min - b);
}

static Bool
ref_ssub_ovf(int64_t a, int64_t b, int64_t min, int64_t max)
{
    return (b < 0 && a > max + b) || (b > 0 && a < min + b);
}

static Bool
ref_smul_ovf(int64_t a, int64_t b, int64_t min, int64_t max)
{
    if (a == 0 || b == 0)
        return FALSE;

    if (min > INT64_MIN) {
        /* narrower than 64 bits, the product always fits int64_t */
        int64_t r = a * b;

        return r < min || r > max;
    }
    else {
        /* 64 bit type, check by division */
        if (a == -1)
            return b == INT64_MIN;
        if (b == -1)
            return a == INT64_MIN;
        if (a > 0)
            return b > max / a || b < min / a;
        return b > min / a || b < max / a;
    }
}

/* Test matrices: boundary-ish values, filtered against the range of
 * the type under test, then every operation is verified for every
 * combination of two of them.
 */
#define TEST_UNSIGNED_CHECKED_OPS(prefix_, type_, max_)                       \
    do {                                                                      \
        static const uint64_t tmpls[] = {                                     \
            0, 1, 2, 3, 5, 17, 255, 256, 65535, 65536,                        \
            2147483647u, 2147483648u, 4294967295u, 4294967296u,               \
            9223372036854775807u, 9223372036854775808u,                       \
            18446744073709551614u, 18446744073709551615u,                     \
        };                                                                    \
        type_ vals[32];                                                       \
        size_t n = 0, i, j;                                                   \
                                                                              \
        for (i = 0; i < sizeof(tmpls) / sizeof(tmpls[0]); i++)                \
            if (tmpls[i] <= (uint64_t) (max_))                                \
                vals[n++] = (type_) tmpls[i];                                 \
        vals[n++] = (type_) ((uint64_t) (max_) / 2);                          \
        vals[n++] = (type_) ((uint64_t) (max_) - 1);                          \
        vals[n++] = (type_) (max_);                                           \
                                                                              \
        for (i = 0; i < n; i++) {                                             \
            for (j = 0; j < n; j++) {                                         \
                type_ a = vals[i], b = vals[j], out;                          \
                Bool ovf;                                                     \
                                                                              \
                ovf = prefix_##_add(&out, a, b);                              \
                assert(ovf == ref_uadd_ovf(a, b, (uint64_t) (max_)));         \
                if (!ovf)                                                     \
                    assert(out == (type_) ((uint64_t) a + b));                \
                                                                              \
                ovf = prefix_##_subtract(&out, a, b);                         \
                assert(ovf == ref_usub_ovf(a, b, (uint64_t) (max_)));         \
                if (!ovf)                                                     \
                    assert(out == (type_) ((uint64_t) a - b));                \
                                                                              \
                ovf = prefix_##_multiply(&out, a, b);                         \
                assert(ovf == ref_umul_ovf(a, b, (uint64_t) (max_)));         \
                if (!ovf)                                                     \
                    assert(out == (type_) ((uint64_t) a * b));                \
            }                                                                 \
        }                                                                     \
    } while (0)

#define TEST_SIGNED_CHECKED_OPS(prefix_, type_, min_, max_)                   \
    do {                                                                      \
        static const int64_t tmpls[] = {                                      \
            0, 1, -1, 2, -2, 3, -3, 5, -5, 17, -17,                           \
            255, -255, 256, -256, 32768, -32768, 65536, -65536,               \
            INT8_MAX, INT8_MIN, INT16_MAX, INT16_MIN,                         \
            INT32_MAX, INT32_MIN, INT64_MAX, INT64_MIN,                       \
            2147483647LL, -2147483647LL, 2147483648LL, -2147483648LL,         \
            4294967296LL, -4294967296LL,                                      \
            9223372036854775807LL, -9223372036854775807LL,                    \
        };                                                                    \
        type_ vals[64];                                                       \
        size_t n = 0, i, j;                                                   \
                                                                              \
        for (i = 0; i < sizeof(tmpls) / sizeof(tmpls[0]); i++)                \
            if (tmpls[i] >= (int64_t) (min_) && tmpls[i] <= (int64_t) (max_)) \
                vals[n++] = (type_) tmpls[i];                                 \
        vals[n++] = (type_) (min_);                                           \
        vals[n++] = (type_) ((int64_t) (min_) + 1);                           \
        vals[n++] = (type_) ((int64_t) (min_) / 2);                           \
        vals[n++] = (type_) ((int64_t) (max_) / 2);                           \
        vals[n++] = (type_) ((int64_t) (max_) - 1);                           \
        vals[n++] = (type_) (max_);                                           \
                                                                              \
        for (i = 0; i < n; i++) {                                             \
            for (j = 0; j < n; j++) {                                         \
                type_ a = vals[i], b = vals[j], out;                          \
                Bool ovf;                                                     \
                                                                              \
                ovf = prefix_##_add(&out, a, b);                              \
                assert(ovf == ref_sadd_ovf(a, b, (int64_t) (min_),            \
                                           (int64_t) (max_)));                \
                if (!ovf)                                                     \
                    assert(out == (type_) ((int64_t) a + b));                 \
                                                                              \
                ovf = prefix_##_subtract(&out, a, b);                         \
                assert(ovf == ref_ssub_ovf(a, b, (int64_t) (min_),            \
                                           (int64_t) (max_)));                \
                if (!ovf)                                                     \
                    assert(out == (type_) ((int64_t) a - b));                 \
                                                                              \
                ovf = prefix_##_multiply(&out, a, b);                         \
                assert(ovf == ref_smul_ovf(a, b, (int64_t) (min_),            \
                                           (int64_t) (max_)));                \
                if (!ovf)                                                     \
                    assert(out == (type_) ((int64_t) a * b));                 \
            }                                                                 \
        }                                                                     \
    } while (0)

#define TEST_UNSIGNED_SATURATING_OPS(prefix_, type_, max_)                    \
    do {                                                                      \
        static const uint64_t tmpls[] = {                                     \
            0, 1, 2, 3, 5, 17, 255, 256, 65535, 65536,                        \
            2147483647u, 2147483648u, 4294967295u, 4294967296u,               \
            9223372036854775807u, 9223372036854775808u,                       \
            18446744073709551614u, 18446744073709551615u,                     \
        };                                                                    \
        type_ vals[32];                                                       \
        size_t n = 0, i, j;                                                   \
                                                                              \
        for (i = 0; i < sizeof(tmpls) / sizeof(tmpls[0]); i++)                \
            if (tmpls[i] <= (uint64_t) (max_))                                \
                vals[n++] = (type_) tmpls[i];                                 \
        vals[n++] = (type_) ((uint64_t) (max_) / 2);                          \
        vals[n++] = (type_) ((uint64_t) (max_) - 1);                          \
        vals[n++] = (type_) (max_);                                           \
                                                                              \
        for (i = 0; i < n; i++) {                                             \
            for (j = 0; j < n; j++) {                                         \
                type_ a = vals[i], b = vals[j];                               \
                                                                              \
                if (ref_uadd_ovf(a, b, (uint64_t) (max_)))                    \
                    assert(prefix_##_add(a, b) == (type_) (max_));            \
                else                                                          \
                    assert(prefix_##_add(a, b) ==                             \
                           (type_) ((uint64_t) a + b));                       \
                                                                              \
                if (ref_usub_ovf(a, b, (uint64_t) (max_)))                    \
                    assert(prefix_##_subtract(a, b) == 0);                    \
                else                                                          \
                    assert(prefix_##_subtract(a, b) ==                        \
                           (type_) ((uint64_t) a - b));                       \
                                                                              \
                if (ref_umul_ovf(a, b, (uint64_t) (max_)))                    \
                    assert(prefix_##_multiply(a, b) ==                        \
                           (type_) (max_));                                   \
                else                                                          \
                    assert(prefix_##_multiply(a, b) ==                        \
                           (type_) ((uint64_t) a * b));                       \
            }                                                                 \
        }                                                                     \
    } while (0)

#define TEST_SIGNED_SATURATING_OPS(prefix_, type_, min_, max_)                \
    do {                                                                      \
        static const int64_t tmpls[] = {                                      \
            0, 1, -1, 2, -2, 3, -3, 5, -5, 17, -17,                           \
            255, -255, 256, -256, 32768, -32768, 65536, -65536,               \
            INT8_MAX, INT8_MIN, INT16_MAX, INT16_MIN,                         \
            INT32_MAX, INT32_MIN, INT64_MAX, INT64_MIN,                       \
            2147483647LL, -2147483647LL, 2147483648LL, -2147483648LL,         \
            4294967296LL, -4294967296LL,                                      \
            9223372036854775807LL, -9223372036854775807LL,                    \
        };                                                                    \
        type_ vals[64];                                                       \
        size_t n = 0, i, j;                                                   \
                                                                              \
        for (i = 0; i < sizeof(tmpls) / sizeof(tmpls[0]); i++)                \
            if (tmpls[i] >= (int64_t) (min_) && tmpls[i] <= (int64_t) (max_)) \
                vals[n++] = (type_) tmpls[i];                                 \
        vals[n++] = (type_) (min_);                                           \
        vals[n++] = (type_) ((int64_t) (min_) + 1);                           \
        vals[n++] = (type_) ((int64_t) (min_) / 2);                           \
        vals[n++] = (type_) ((int64_t) (max_) / 2);                           \
        vals[n++] = (type_) ((int64_t) (max_) - 1);                           \
        vals[n++] = (type_) (max_);                                           \
                                                                              \
        for (i = 0; i < n; i++) {                                             \
            for (j = 0; j < n; j++) {                                         \
                type_ a = vals[i], b = vals[j];                               \
                                                                              \
                if (ref_sadd_ovf(a, b, (int64_t) (min_), (int64_t) (max_)))   \
                    assert(prefix_##_add(a, b) ==                             \
                           (type_) ((a < 0) ? (min_) : (max_)));              \
                else                                                          \
                    assert(prefix_##_add(a, b) ==                             \
                           (type_) ((int64_t) a + b));                        \
                                                                              \
                if (ref_ssub_ovf(a, b, (int64_t) (min_), (int64_t) (max_)))   \
                    assert(prefix_##_subtract(a, b) ==                        \
                           (type_) ((b > 0) ? (min_) : (max_)));              \
                else                                                          \
                    assert(prefix_##_subtract(a, b) ==                        \
                           (type_) ((int64_t) a - b));                        \
                                                                              \
                if (ref_smul_ovf(a, b, (int64_t) (min_), (int64_t) (max_)))   \
                    assert(prefix_##_multiply(a, b) ==                        \
                           (type_) (((a < 0) != (b < 0)) ? (min_) : (max_))); \
                else                                                          \
                    assert(prefix_##_multiply(a, b) ==                        \
                           (type_) ((int64_t) a * b));                        \
            }                                                                 \
        }                                                                     \
    } while (0)

static void
checked_arithmetic_test(void)
{
    TEST_UNSIGNED_CHECKED_OPS(checked_CARD8, CARD8, UINT8_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_CARD16, CARD16, UINT16_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_CARD32, CARD32, UINT32_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_CARD64, CARD64, UINT64_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_INT8, INT8, INT8_MIN, INT8_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_INT16, INT16, INT16_MIN, INT16_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_INT32, INT32, INT32_MIN, INT32_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_INT64, int64_t, INT64_MIN, INT64_MAX);

    TEST_SIGNED_CHECKED_OPS(checked_char, char, CHAR_MIN, CHAR_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_schar, signed char, SCHAR_MIN, SCHAR_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uchar, unsigned char, UCHAR_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_short, short, SHRT_MIN, SHRT_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_ushort, unsigned short, USHRT_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_int, int, INT_MIN, INT_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uint, unsigned int, UINT_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_long, long, LONG_MIN, LONG_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_ulong, unsigned long, ULONG_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_longlong, long long, LLONG_MIN, LLONG_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_ulonglong, unsigned long long, ULLONG_MAX);

    TEST_SIGNED_CHECKED_OPS(checked_int8, int8_t, INT8_MIN, INT8_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_int16, int16_t, INT16_MIN, INT16_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_int32, int32_t, INT32_MIN, INT32_MAX);
    TEST_SIGNED_CHECKED_OPS(checked_int64, int64_t, INT64_MIN, INT64_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uint8, uint8_t, UINT8_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uint16, uint16_t, UINT16_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uint32, uint32_t, UINT32_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_uint64, uint64_t, UINT64_MAX);

    TEST_SIGNED_CHECKED_OPS(checked_ssize, ssize_t, SSIZE_MIN, SSIZE_MAX);
    TEST_UNSIGNED_CHECKED_OPS(checked_size, size_t, SIZE_MAX);
}

static void
saturating_arithmetic_test(void)
{
    TEST_UNSIGNED_SATURATING_OPS(saturating_CARD8, CARD8, UINT8_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_CARD16, CARD16, UINT16_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_CARD32, CARD32, UINT32_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_CARD64, CARD64, UINT64_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_INT8, INT8, INT8_MIN, INT8_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_INT16, INT16, INT16_MIN, INT16_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_INT32, INT32, INT32_MIN, INT32_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_INT64, int64_t, INT64_MIN, INT64_MAX);

    TEST_SIGNED_SATURATING_OPS(saturating_char, char, CHAR_MIN, CHAR_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_schar, signed char, SCHAR_MIN, SCHAR_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uchar, unsigned char, UCHAR_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_short, short, SHRT_MIN, SHRT_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_ushort, unsigned short, USHRT_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_int, int, INT_MIN, INT_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uint, unsigned int, UINT_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_long, long, LONG_MIN, LONG_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_ulong, unsigned long, ULONG_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_longlong, long long, LLONG_MIN, LLONG_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_ulonglong, unsigned long long, ULLONG_MAX);

    TEST_SIGNED_SATURATING_OPS(saturating_int8, int8_t, INT8_MIN, INT8_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_int16, int16_t, INT16_MIN, INT16_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_int32, int32_t, INT32_MIN, INT32_MAX);
    TEST_SIGNED_SATURATING_OPS(saturating_int64, int64_t, INT64_MIN, INT64_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uint8, uint8_t, UINT8_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uint16, uint16_t, UINT16_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uint32, uint32_t, UINT32_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_uint64, uint64_t, UINT64_MAX);

    TEST_SIGNED_SATURATING_OPS(saturating_ssize, ssize_t, SSIZE_MIN, SSIZE_MAX);
    TEST_UNSIGNED_SATURATING_OPS(saturating_size, size_t, SIZE_MAX);
}

static void
checked_arithmetic_wrapped_test(void)
{
    CARD8 c8;
    INT8 i8;

    /* on overflow *out is set to the wrapped value */
    assert(checked_CARD8_add(&c8, 250, 10) == TRUE);
    assert(c8 == 4);
    assert(checked_CARD8_subtract(&c8, 3, 10) == TRUE);
    assert(c8 == 249);
    assert(checked_CARD8_multiply(&c8, 16, 16) == TRUE);
    assert(c8 == 0);
    assert(checked_INT8_add(&i8, 100, 50) == TRUE);
    assert(i8 == -106);
    assert(checked_INT8_subtract(&i8, -128, 1) == TRUE);
    assert(i8 == 127);

    /* out may alias the operands */
    c8 = 200;
    assert(checked_CARD8_add(&c8, c8, 100) == TRUE);
    assert(c8 == 44);
    i8 = 100;
    assert(checked_INT8_add(&i8, i8, i8) == TRUE);
    assert(i8 == -56);
}

static void
checked_int64_test(void)
{
    int64_t v;

    /* the int64_t helpers, including the aliasing pattern
     * used by Xext/sync.c */
    v = 5;
    assert(checked_int64_add(&v, v, v) == FALSE);
    assert(v == 10);
    v = (int64_t) 1 << 62;
    assert(checked_int64_add(&v, v, v) == TRUE);
    v = 10;
    assert(checked_int64_subtract(&v, 3, v) == FALSE);
    assert(v == -7);
    v = INT64_MIN;
    assert(checked_int64_subtract(&v, v, 1) == TRUE);
    v = INT64_MAX;
    assert(checked_int64_add(&v, v, 1) == TRUE);
}

static void
saturating_arithmetic_spot_test(void)
{
    assert(saturating_CARD8_add(250, 10) == 255);
    assert(saturating_CARD8_subtract(3, 10) == 0);
    assert(saturating_CARD8_multiply(16, 16) == 255);
    assert(saturating_INT8_add(100, 50) == 127);
    assert(saturating_INT8_add(-100, -50) == -128);
    assert(saturating_INT8_subtract(-128, 1) == -128);
    assert(saturating_INT8_subtract(127, -1) == 127);
    assert(saturating_INT8_multiply(100, 2) == 127);
    assert(saturating_INT8_multiply(-100, 2) == -128);
    assert(saturating_int_add(INT_MAX, 1) == INT_MAX);
    assert(saturating_int_subtract(INT_MIN, 1) == INT_MIN);
    assert(saturating_uint_add(UINT_MAX, 1) == UINT_MAX);
    assert(saturating_uint_subtract(0, 1) == 0);
    assert(saturating_INT64_multiply(INT64_MIN, -1) == INT64_MAX);
    assert(saturating_INT64_multiply(INT64_MAX, 2) == INT64_MAX);
    assert(saturating_CARD64_add(UINT64_MAX, UINT64_MAX) == UINT64_MAX);
    assert(saturating_CARD64_subtract(0, 1) == 0);
}

static void
checked_size_spot_test(void)
{
    size_t v;

    assert(checked_size_multiply(&v, 50, 4) == FALSE);
    assert(v == 200);
    assert(checked_size_multiply(&v, SIZE_MAX / 4, 4) == FALSE);
    assert(v == (SIZE_MAX / 4) * 4);
    assert(checked_size_multiply(&v, SIZE_MAX / 4 + 1, 4) == TRUE);
    assert(checked_size_multiply(&v, SIZE_MAX, 4) == TRUE);
    assert(checked_size_multiply(&v, 0, SIZE_MAX) == FALSE);
    assert(v == 0);
    assert(checked_size_add(&v, SIZE_MAX, 1) == TRUE);
    assert(checked_size_subtract(&v, 0, 1) == TRUE);

    /* the same for ssize_t, including the asymmetric corner cases */
    ssize_t s;

    assert(checked_ssize_multiply(&s, 200, 4) == FALSE);
    assert(s == 800);
    assert(checked_ssize_multiply(&s, SSIZE_MAX, 2) == TRUE);
    assert(checked_ssize_multiply(&s, SSIZE_MIN, -1) == TRUE);
    assert(checked_ssize_add(&s, SSIZE_MAX, 1) == TRUE);
    assert(checked_ssize_subtract(&s, SSIZE_MIN, 1) == TRUE);

    /* out may alias the operands */
    v = 10;
    assert(checked_size_multiply(&v, v, 4) == FALSE);
    assert(v == 40);
}

static const testfunc_t*
checked_math_test(void)
{
    static const testfunc_t testfuncs[] = {
        checked_arithmetic_test,
        saturating_arithmetic_test,
        checked_arithmetic_wrapped_test,
        checked_int64_test,
        saturating_arithmetic_spot_test,
        checked_size_spot_test,
        NULL,
    };
    return testfuncs;
}
