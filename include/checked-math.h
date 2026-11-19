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

#ifndef CHECKED_MATH_H
#define CHECKED_MATH_H 1

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <X11/Xmd.h>
#include <X11/Xdefs.h>
#include "misc.h"

/* Checked arithmetic for the protocol data types (CARD8 ... CARD64,
 * INT8 ... INT64) and the standard C integer types.
 *
 * All of these return TRUE if overflow occurred, FALSE otherwise.
 * *out* is set to the calculated value (the wrapped value in case of
 * overflow).
 */

/* Use the compiler's checked arithmetic builtins where available,
 * they know the type widths and produce optimal code.
 */
#if defined(__has_builtin)
# if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && __has_builtin(__builtin_mul_overflow)
#  define _X_HAS_OVERFLOW_BUILTINS 1
# endif
#elif defined(__GNUC__) && __GNUC__ >= 5
# define _X_HAS_OVERFLOW_BUILTINS 1
#endif

#ifdef _X_HAS_OVERFLOW_BUILTINS

#define _X_DEFINE_SIGNED_CHECKED_OPS(prefix, type, min, max)             \
static inline Bool                                                         \
prefix##_add(type *out, type a, type b)                                    \
{                                                                          \
    return __builtin_add_overflow(a, b, out);                              \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_subtract(type *out, type a, type b)                               \
{                                                                          \
    return __builtin_sub_overflow(a, b, out);                              \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_multiply(type *out, type a, type b)                               \
{                                                                          \
    return __builtin_mul_overflow(a, b, out);                              \
}

#define _X_DEFINE_UNSIGNED_CHECKED_OPS(prefix, type, max)                  \
static inline Bool                                                         \
prefix##_add(type *out, type a, type b)                                    \
{                                                                          \
    return __builtin_add_overflow(a, b, out);                              \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_subtract(type *out, type a, type b)                               \
{                                                                          \
    return __builtin_sub_overflow(a, b, out);                              \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_multiply(type *out, type a, type b)                               \
{                                                                          \
    return __builtin_mul_overflow(a, b, out);                              \
}

#else /* ! _X_HAS_OVERFLOW_BUILTINS */

/* Portable fallbacks. The arithmetic is done as unsigned 64 bit math
 * (so wrapping is well-defined), and the result is then checked
 * against the range of the type.
 *
 * For the signed types, the 64 bit result is converted back to int64_t
 * (two's complement wrap), range-checked against the type, and
 * additionally checked for the sign-change conditions of 64 bit
 * overflow, which is the only check that applies to INT64 itself.
 */
#define _X_DEFINE_SIGNED_CHECKED_OPS(prefix, type, min, max)               \
static inline Bool                                                         \
prefix##_add(type *out, type a, type b)                                    \
{                                                                          \
    int64_t result = (int64_t) ((uint64_t) a + (uint64_t) b);              \
    Bool overflow = result < (min) || result > (max) ||                    \
                    ((a < 0) == (b < 0) && (a < 0) != (result < 0));        \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_subtract(type *out, type a, type b)                               \
{                                                                          \
    int64_t result = (int64_t) ((uint64_t) a - (uint64_t) b);              \
    Bool overflow = result < (min) || result > (max) ||                    \
                    ((a < 0) != (b < 0) && (a < 0) != (result < 0));        \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_multiply(type *out, type a, type b)                               \
{                                                                          \
    int64_t result = (int64_t) ((uint64_t) a * (uint64_t) b);              \
    Bool overflow = result < (min) || result > (max) ||                    \
                    _x_int64_multiply_overflows(a, b);                     \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}

/* For the unsigned types the result stays in uint64_t, where both
 * wraparound of the 64 bit computation and exceeding the (possibly
 * narrower) type are detectable directly.
 */
#define _X_DEFINE_UNSIGNED_CHECKED_OPS(prefix, type, max)                  \
static inline Bool                                                         \
prefix##_add(type *out, type a, type b)                                    \
{                                                                          \
    uint64_t result = (uint64_t) a + (uint64_t) b;                         \
    Bool overflow = result < (uint64_t) a || result > (max);               \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_subtract(type *out, type a, type b)                               \
{                                                                          \
    uint64_t result = (uint64_t) a - (uint64_t) b;                         \
    Bool overflow = result > (uint64_t) a || result > (max);               \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}                                                                          \
                                                                           \
static inline Bool                                                         \
prefix##_multiply(type *out, type a, type b)                               \
{                                                                          \
    uint64_t result = (uint64_t) a * (uint64_t) b;                         \
    Bool overflow = result > (max) ||                                      \
                    _x_uint64_multiply_overflows(a, b);                    \
    *out = (type) result;                                                  \
    return overflow;                                                       \
}

/* Helpers for the fallback multiply implementations above: check
 * whether the product of two 64 bit values overflowed the 64 bit
 * range. Done with division instead of examining the wrapped result,
 * so that no undefined behaviour (signed overflow, INT64_MIN / -1) is
 * invoked on the way.
 */
static inline Bool
_x_int64_multiply_overflows(int64_t a, int64_t b)
{
    if (a == 0 || b == 0)
        return FALSE;
    if (a == -1)
        return b == INT64_MIN;
    if (b == -1)
        return a == INT64_MIN;
    if (a > 0)
        return b > INT64_MAX / a || b < INT64_MIN / a;
    return b > INT64_MIN / a || b < INT64_MAX / a;
}

static inline Bool
_x_uint64_multiply_overflows(uint64_t a, uint64_t b)
{
    return a != 0 && a * b / a != b;
}

#endif /* _X_HAS_OVERFLOW_BUILTINS */
#undef _X_HAS_OVERFLOW_BUILTINS

_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_CARD8, CARD8, UINT8_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_CARD16, CARD16, UINT16_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_CARD32, CARD32, UINT32_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_CARD64, CARD64, UINT64_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_INT8, INT8, INT8_MIN, INT8_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_INT16, INT16, INT16_MIN, INT16_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_INT32, INT32, INT32_MIN, INT32_MAX)
/* INT64 only exists ifdef LONG64 but where it does exist it's int64_t width, so let's use that */
_X_DEFINE_SIGNED_CHECKED_OPS(checked_INT64, int64_t, INT64_MIN, INT64_MAX)

_X_DEFINE_SIGNED_CHECKED_OPS(checked_char, char, CHAR_MIN, CHAR_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_schar, signed char, SCHAR_MIN, SCHAR_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uchar, unsigned char, UCHAR_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_short, short, SHRT_MIN, SHRT_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_ushort, unsigned short, USHRT_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_int, int, INT_MIN, INT_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uint, unsigned int, UINT_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_long, long, LONG_MIN, LONG_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_ulong, unsigned long, ULONG_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_longlong, long long, LLONG_MIN, LLONG_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_ulonglong, unsigned long long, ULLONG_MAX)

_X_DEFINE_SIGNED_CHECKED_OPS(checked_int8, int8_t, INT8_MIN, INT8_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uint8, uint8_t, UINT8_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_int16, int16_t, INT16_MIN, INT16_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uint16, uint16_t, UINT16_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_int32, int32_t, INT32_MIN, INT32_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uint32, uint32_t, UINT32_MAX)
_X_DEFINE_SIGNED_CHECKED_OPS(checked_int64, int64_t, INT64_MIN, INT64_MAX)
_X_DEFINE_UNSIGNED_CHECKED_OPS(checked_uint64, uint64_t, UINT64_MAX)

/* Saturating arithmetic for the same types: the result is clamped to the
 * smallest/largest value the type can hold.
 */
#define _X_DEFINE_SIGNED_SATURATING_OPS(prefix, cprefix, type, min, max)     \
static inline type                                                          \
prefix##_add(type a, type b)                                               \
{                                                                           \
    type result;                                                            \
    if (cprefix##_add(&result, a, b))                                       \
        return (a < 0) ? (min) : (max);                                     \
    return result;                                                          \
}                                                                           \
                                                                            \
static inline type                                                          \
prefix##_subtract(type a, type b)                                          \
{                                                                           \
    type result;                                                            \
    if (cprefix##_subtract(&result, a, b))                                  \
        return (b > 0) ? (min) : (max);                                     \
    return result;                                                          \
}                                                                           \
                                                                            \
static inline type                                                          \
prefix##_multiply(type a, type b)                                          \
{                                                                           \
    type result;                                                            \
    if (cprefix##_multiply(&result, a, b))                                  \
        return ((a < 0) != (b < 0)) ? (min) : (max);                        \
    return result;                                                          \
}

#define _X_DEFINE_UNSIGNED_SATURATING_OPS(prefix, cprefix, type, max)        \
static inline type                                                          \
prefix##_add(type a, type b)                                               \
{                                                                           \
    type result;                                                            \
    if (cprefix##_add(&result, a, b))                                       \
        return (max);                                                       \
    return result;                                                          \
}                                                                           \
                                                                            \
static inline type                                                          \
prefix##_subtract(type a, type b)                                          \
{                                                                           \
    type result;                                                            \
    if (cprefix##_subtract(&result, a, b))                                  \
        return 0;                                                           \
    return result;                                                          \
}                                                                           \
                                                                            \
static inline type                                                          \
prefix##_multiply(type a, type b)                                          \
{                                                                           \
    type result;                                                            \
    if (cprefix##_multiply(&result, a, b))                                  \
        return (max);                                                       \
    return result;                                                          \
}

_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_CARD8, checked_CARD8, CARD8, UINT8_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_CARD16, checked_CARD16, CARD16, UINT16_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_CARD32, checked_CARD32, CARD32, UINT32_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_CARD64, checked_CARD64, CARD64, UINT64_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_INT8, checked_INT8, INT8, INT8_MIN, INT8_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_INT16, checked_INT16, INT16, INT16_MIN, INT16_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_INT32, checked_INT32, INT32, INT32_MIN, INT32_MAX)
/* INT64 only exists ifdef LONG64 but where it does exist it's int64_t width, so let's use that */
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_INT64, checked_INT64, int64_t, INT64_MIN, INT64_MAX)

_X_DEFINE_SIGNED_SATURATING_OPS(saturating_char, checked_char, char, CHAR_MIN, CHAR_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_schar, checked_schar, signed char, SCHAR_MIN, SCHAR_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uchar, checked_uchar, unsigned char, UCHAR_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_short, checked_short, short, SHRT_MIN, SHRT_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_ushort, checked_ushort, unsigned short, USHRT_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_int, checked_int, int, INT_MIN, INT_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uint, checked_uint, unsigned int, UINT_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_long, checked_long, long, LONG_MIN, LONG_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_ulong, checked_ulong, unsigned long, ULONG_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_longlong, checked_longlong, long long, LLONG_MIN, LLONG_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_ulonglong, checked_ulonglong, unsigned long long, ULLONG_MAX)

_X_DEFINE_SIGNED_SATURATING_OPS(saturating_int8, checked_int8, int8_t, INT8_MIN, INT8_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uint8, checked_uint8, uint8_t, UINT8_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_int16, checked_int16, int16_t, INT16_MIN, INT16_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uint16, checked_uint16, uint16_t, UINT16_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_int32, checked_int32, int32_t, INT32_MIN, INT32_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uint32, checked_uint32, uint32_t, UINT32_MAX)
_X_DEFINE_SIGNED_SATURATING_OPS(saturating_int64, checked_int64, int64_t, INT64_MIN, INT64_MAX)
_X_DEFINE_UNSIGNED_SATURATING_OPS(saturating_uint64, checked_uint64, uint64_t, UINT64_MAX)

#endif /* CHECKED_MATH_H */
