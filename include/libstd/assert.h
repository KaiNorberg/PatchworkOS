#ifndef _ASSERT_H
#define _ASSERT_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include "_internal/config.h"

_PUBLIC void _assert_99(const char* const, const char* const, const char* const);
_PUBLIC void _assert_89(const char* const);

/* If NDEBUG is set, assert() is a null operation. */
#undef assert

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#if __STDC_VERSION__ >= 199901L
#define assert(expression) \
    ((expression) ? (void)0 \
                  : _assert_99("Assertion failed: " #expression " function ", __func__, " file " __FILE__ "."))
#else
#define assert(expression) ((expression) ? (void)0 : _assert_89("Assertion failed: " #expression " file " __FILE__ "."))
#endif
#endif

#if __STDC_VERSION__ >= 201112L
#define static_assert _Static_assert
#endif

#ifdef __cplusplus
}
#endif

#endif
