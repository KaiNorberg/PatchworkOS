#ifndef _INTTYPES_H
#define _INTTYPES_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include "_libstd/config.h"

typedef __INTMAX_TYPE__ intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

typedef struct
{
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

/* 7.8.1 Macros for format specifiers */

#define PRIdLEAST8 "d"
#define PRIiLEAST8 "i"
#define PRIoLEAST8 "o"
#define PRIuLEAST8 "u"
#define PRIxLEAST8 "x"
#define PRIXLEAST8 "X"

#define PRIdFAST8 "d"
#define PRIiFAST8 "i"
#define PRIoFAST8 "o"
#define PRIuFAST8 "u"
#define PRIxFAST8 "x"
#define PRIXFAST8 "X"

#define PRIdLEAST16 "d"
#define PRIiLEAST16 "i"
#define PRIoLEAST16 "o"
#define PRIuLEAST16 "u"
#define PRIxLEAST16 "x"
#define PRIXLEAST16 "X"

#define PRIdFAST16 "d"
#define PRIiFAST16 "i"
#define PRIoFAST16 "o"
#define PRIuFAST16 "u"
#define PRIxFAST16 "x"
#define PRIXFAST16 "X"

#define PRIdLEAST32 "d"
#define PRIiLEAST32 "i"
#define PRIoLEAST32 "o"
#define PRIuLEAST32 "u"
#define PRIxLEAST32 "x"
#define PRIXLEAST32 "X"

#define PRIdFAST32 "d"
#define PRIiFAST32 "i"
#define PRIoFAST32 "o"
#define PRIuFAST32 "u"
#define PRIxFAST32 "x"
#define PRIXFAST32 "X"

#define PRIdLEAST64 "d"
#define PRIiLEAST64 "i"
#define PRIoLEAST64 "o"
#define PRIuLEAST64 "u"
#define PRIxLEAST64 "x"
#define PRIXLEAST64 "X"

#define PRIdFAST64 "d"
#define PRIiFAST64 "i"
#define PRIoFAST64 "o"
#define PRIuFAST64 "u"
#define PRIxFAST64 "x"
#define PRIXFAST64 "X"

#define PRIdMAX "d"
#define PRIiMAX "i"
#define PRIoMAX "o"
#define PRIuMAX "u"
#define PRIxMAX "x"
#define PRIXMAX "X"

#define PRIdPTR "d"
#define PRIiPTR "i"
#define PRIoPTR "o"
#define PRIuPTR "u"
#define PRIxPTR "x"
#define PRIXPTR "X"

/*
#define SCNdLEAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST8_PREFIX, d ) )
#define SCNiLEAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST8_PREFIX, i ) )
#define SCNoLEAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST8_PREFIX, o ) )
#define SCNuLEAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST8_PREFIX, u ) )
#define SCNxLEAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST8_PREFIX, x ) )

#define SCNdFAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST8_PREFIX, d ) )
#define SCNiFAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST8_PREFIX, i ) )
#define SCNoFAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST8_PREFIX, o ) )
#define SCNuFAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST8_PREFIX, u ) )
#define SCNxFAST8  _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST8_PREFIX, x ) )

#define SCNdLEAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST16_PREFIX, d ) )
#define SCNiLEAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST16_PREFIX, i ) )
#define SCNoLEAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST16_PREFIX, o ) )
#define SCNuLEAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST16_PREFIX, u ) )
#define SCNxLEAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST16_PREFIX, x ) )

#define SCNdFAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST16_PREFIX, d ) )
#define SCNiFAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST16_PREFIX, i ) )
#define SCNoFAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST16_PREFIX, o ) )
#define SCNuFAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST16_PREFIX, u ) )
#define SCNxFAST16 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST16_PREFIX, x ) )

#define SCNdLEAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST32_PREFIX, d ) )
#define SCNiLEAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST32_PREFIX, i ) )
#define SCNoLEAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST32_PREFIX, o ) )
#define SCNuLEAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST32_PREFIX, u ) )
#define SCNxLEAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST32_PREFIX, x ) )

#define SCNdFAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST32_PREFIX, d ) )
#define SCNiFAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST32_PREFIX, i ) )
#define SCNoFAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST32_PREFIX, o ) )
#define SCNuFAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST32_PREFIX, u ) )
#define SCNxFAST32 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST32_PREFIX, x ) )

#define SCNdLEAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST64_PREFIX, d ) )
#define SCNiLEAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST64_PREFIX, i ) )
#define SCNoLEAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST64_PREFIX, o ) )
#define SCNuLEAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST64_PREFIX, u ) )
#define SCNxLEAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_LEAST64_PREFIX, x ) )

#define SCNdFAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST64_PREFIX, d ) )
#define SCNiFAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST64_PREFIX, i ) )
#define SCNoFAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST64_PREFIX, o ) )
#define SCNuFAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST64_PREFIX, u ) )
#define SCNxFAST64 _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INT_FAST64_PREFIX, x ) )

#define SCNdMAX _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTMAX_PREFIX, d ) )
#define SCNiMAX _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTMAX_PREFIX, i ) )
#define SCNoMAX _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTMAX_PREFIX, o ) )
#define SCNuMAX _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTMAX_PREFIX, u ) )
#define SCNxMAX _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTMAX_PREFIX, x ) )

#define SCNdPTR _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTPTR_PREFIX, d ) )
#define SCNiPTR _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTPTR_PREFIX, i ) )
#define SCNoPTR _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTPTR_PREFIX, o ) )
#define SCNuPTR _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTPTR_PREFIX, u ) )
#define SCNxPTR _PDCLIB_value2string( _PDCLIB_concat( _PDCLIB_INTPTR_PREFIX, x ) )
*/

/* 7.8.2 Functions for greatest-width integer types */

_PUBLIC intmax_t imaxabs(intmax_t j);

_PUBLIC imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

_PUBLIC intmax_t strtoimax(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
_PUBLIC uintmax_t strtoumax(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);

#ifdef __cplusplus
}
#endif

#endif
