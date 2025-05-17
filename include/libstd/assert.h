#ifndef _ASSERT_H
#define _ASSERT_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include "_AUX/config.h"

_PUBLIC void _Assert99(const char* const, const char* const, const char* const);
_PUBLIC void _Assert89(const char* const);

/* If NDEBUG is set, assert() is a null operation. */
#undef assert

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#if __STDC_VERSION__ >= 199901L
#define assert(expression) \
    ((expression) ? (void)0 \
                  : _Assert99("Assertion failed: " #expression ", function ", __func__, ", file " __FILE__ ".\n"))
#else
#define assert(expression) \
    ((expression) ? (void)0 : _Assert89("Assertion failed: " #expression ", file " __FILE__ ".\n"))
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif