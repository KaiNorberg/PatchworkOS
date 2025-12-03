#ifndef _STDINT_H
#define _STDINT_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

/* 7.18.1.1 Exact-width integer types */
typedef __INT8_TYPE__ int8_t;
typedef __UINT8_TYPE__ uint8_t;
typedef __INT16_TYPE__ int16_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __INT32_TYPE__ int32_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT64_TYPE__ int64_t;
typedef __UINT64_TYPE__ uint64_t;

/* 7.18.1.2 Minimum-width integer types */
typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;

typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;

/* 7.18.1.3 Fastest minimum-width integer types */
typedef __INT_FAST8_TYPE__ int_fast8_t;
typedef __INT_FAST16_TYPE__ int_fast16_t;
typedef __INT_FAST32_TYPE__ int_fast32_t;
typedef __INT_FAST64_TYPE__ int_fast64_t;

typedef __UINT_FAST8_TYPE__ uint_fast8_t;
typedef __UINT_FAST16_TYPE__ uint_fast16_t;
typedef __UINT_FAST32_TYPE__ uint_fast32_t;
typedef __UINT_FAST64_TYPE__ uint_fast64_t;

/* 7.18.1.4 Integer types capable of holding object pointers */
typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;

/* 7.18.1.5 Greatest-width integer types */
typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

/* 7.18.2 Limits of specified-width integer types */

#if defined(__cplusplus) && __cplusplus < 201103L
#ifndef __STDC_LIMIT_MACROS
#define _NO_LIMIT_MACROS
#endif
#endif

#ifndef _NO_LIMIT_MACROS

/* 7.18.2.1 Limits of exact-width integer types */
#define INT8_MIN (-__INT8_MAX__ - 1)
#define INT8_MAX __INT8_MAX__
#define UINT8_MAX __UINT8_MAX__

#define INT16_MIN (-__INT16_MAX__ - 1)
#define INT16_MAX __INT16_MAX__
#define UINT16_MAX __UINT16_MAX__

#define INT32_MIN (-__INT32_MAX__ - 1)
#define INT32_MAX __INT32_MAX__
#define UINT32_MAX __UINT32_MAX__

#define INT64_MIN (-__INT64_MAX__ - 1)
#define INT64_MAX __INT64_MAX__
#define UINT64_MAX __UINT64_MAX__

/* 7.18.2.2 Limits of minimum-width integer types */
#define INT_LEAST8_MIN (-__INT_LEAST8_MAX__ - 1)
#define INT_LEAST8_MAX __INT_LEAST8_MAX__
#define UINT_LEAST8_MAX __UINT_LEAST8_MAX__

#define INT_LEAST16_MIN (-__INT_LEAST16_MAX__ - 1)
#define INT_LEAST16_MAX __INT_LEAST16_MAX__
#define UINT_LEAST16_MAX __UINT_LEAST16_MAX__

#define INT_LEAST32_MIN (-__INT_LEAST32_MAX__ - 1)
#define INT_LEAST32_MAX __INT_LEAST32_MAX__
#define UINT_LEAST32_MAX __UINT_LEAST32_MAX__

#define INT_LEAST64_MIN (-__INT_LEAST64_MAX__ - 1)
#define INT_LEAST64_MAX __INT_LEAST64_MAX__
#define UINT_LEAST64_MAX __UINT_LEAST64_MAX__

/* 7.18.2.3 Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN (-__INT_FAST8_MAX__ - 1)
#define INT_FAST8_MAX __INT_FAST8_MAX__
#define UINT_FAST8_MAX __UINT_FAST8_MAX__

#define INT_FAST16_MIN (-__INT_FAST16_MAX__ - 1)
#define INT_FAST16_MAX __INT_FAST16_MAX__
#define UINT_FAST16_MAX __UINT_FAST16_MAX__

#define INT_FAST32_MIN (-__INT_FAST32_MAX__ - 1)
#define INT_FAST32_MAX __INT_FAST32_MAX__
#define UINT_FAST32_MAX __UINT_FAST32_MAX__

#define INT_FAST64_MIN (-__INT_FAST64_MAX__ - 1)
#define INT_FAST64_MAX __INT_FAST64_MAX__
#define UINT_FAST64_MAX __UINT_FAST64_MAX__

/* 7.18.2.4 Limits of integer types capable of holding object pointers */
#define INTPTR_MIN (-__INTPTR_MAX__ - 1)
#define INTPTR_MAX __INTPTR_MAX__
#define UINTPTR_MAX __UINTPTR_MAX__

/* 7.18.2.5 Limits of greatest-width integer types */
#define INTMAX_MIN (-__INTMAX_MAX__ - 1)
#define INTMAX_MAX __INTMAX_MAX__
#define UINTMAX_MAX __UINTMAX_MAX__

/* 7.18.3 Limits of other integer types */
#define PTRDIFF_MIN (-__PTRDIFF_MAX__ - 1)
#define PTRDIFF_MAX __PTRDIFF_MAX__

#define SIG_ATOMIC_MIN __SIG_ATOMIC_MIN__
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__

#define SIZE_MAX __SIZE_MAX__

#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__

#define WINT_MIN __WINT_MIN__
#define WINT_MAX __WINT_MAX__

#endif

/* 7.18.4 Macros for integer constants */

#if defined(__cplusplus) && __cplusplus < 201103L
#ifndef __STDC_CONSTANT_MACROS
#define _NO_CONSTANT_MACROS
#endif
#endif

#ifndef _NO_CONSTANT_MACROS

#define INT8_C(value) __INT8_C(value)
#define INT16_C(value) __INT16_C(value)
#define INT32_C(value) __INT32_C(value)
#define INT64_C(value) __INT64_C(value)

#define UINT8_C(value) __UINT8_C(value)
#define UINT16_C(value) __UINT16_C(value)
#define UINT32_C(value) __UINT32_C(value)
#define UINT64_C(value) __UINT64_C(value)

#define INTMAX_C(value) __INTMAX_C(value)

#define UINTMAX_C(value) __UINTMAX_C(value)

#endif

// Extension for 128-bit integers

#define INT128_MIN (-INT128_MAX - 1)
#define INT128_MAX ((int128_t)((uint128_t)~0 >> 1))
#define UINT128_MAX ((uint128_t)~0)

typedef __int128_t int128_t;
typedef __uint128_t uint128_t;

#if _USE_ANNEX_K == 1
#define RSIZE_MAX (__SIZE_MAX__ >> 1)
#endif

#if defined(__cplusplus)
}
#endif

#endif
