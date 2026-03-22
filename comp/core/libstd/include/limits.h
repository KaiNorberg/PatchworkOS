#ifndef _LIMITS_H
#define _LIMITS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include "_libstd/config.h"

/* TODO: Defined to 1 as multibyte characters are not supported yet. */
#define MB_LEN_MAX 1

#define LLONG_MIN (-__LONG_LONG_MAX__ - 1)
#define LLONG_MAX __LONG_LONG_MAX__
#define ULLONG_MAX __LONG_LONG_MAX__ * 2ULL + 1

#define CHAR_BIT __CHAR_BIT__
#define CHAR_MAX __SCHAR_MAX__
#define CHAR_MIN (-__SCHAR_MAX__ - 1)
#define SCHAR_MAX __SCHAR_MAX__
#define SCHAR_MIN (-__SCHAR_MAX__ - 1)
#define UCHAR_MAX (__SCHAR_MAX__ * 2 + 1)
#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-__SHRT_MAX__ - 1)
#define INT_MAX __INT_MAX__
#define INT_MIN (-__INT_MAX__ - 1)
#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-__LONG_MAX__ - 1)
#define USHRT_MAX (__SHRT_MAX__ * 2U + 1)
#define UINT_MAX (__INT_MAX__ * 2U + 1)
#define ULONG_MAX (__LONG_MAX__ * 2UL + 1)

#ifdef __cplusplus
}
#endif

#endif
