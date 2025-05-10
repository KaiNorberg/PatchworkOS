#ifndef _STDLIB_H
#define _STDLIB_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/size_t.h"

_PUBLIC char* lltoa(long long number, char* str, int base);
#define ltoa(number, str, base) lltoa(number, str, base)
#define itoa(number, str, base) lltoa(number, str, base)

_PUBLIC char* ulltoa(unsigned long long number, char* str, int base);
#define ultoa(number, str, base) lltoa(number, str, base)
#define uitoa(number, str, base) lltoa(number, str, base)

_PUBLIC double atof(const char* nptr);
_PUBLIC double strtod(const char* _RESTRICT nptr, char** _RESTRICT endptr);
_PUBLIC float strtof(const char* _RESTRICT nptr, char** _RESTRICT endptr);
_PUBLIC long double strtold(const char* _RESTRICT nptr, char** _RESTRICT endptr);

_PUBLIC long int strtol(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
_PUBLIC long long int strtoll(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
_PUBLIC unsigned long int strtoul(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
_PUBLIC unsigned long long int strtoull(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);

_PUBLIC int atoi(const char* nptr);
_PUBLIC long int atol(const char* nptr);
_PUBLIC long long int atoll(const char* nptr);

#define RAND_MAX 32767

_PUBLIC int rand(void);
_PUBLIC void srand(unsigned int seed);

_PUBLIC void* malloc(size_t size);
_PUBLIC void* calloc(size_t nmemb, size_t size);
_PUBLIC void free(void* ptr);
_PUBLIC void* realloc(void* ptr, size_t size);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE -1

_PUBLIC _NORETURN void abort(void);

_PUBLIC int at_quick_exit(void (*func)(void));

_PUBLIC int atexit(void (*func)(void));

_PUBLIC _NORETURN void exit(int status);

_PUBLIC _NORETURN void quick_exit(int status);

_PUBLIC _NORETURN void _Exit(int status);

_PUBLIC char* getenv(const char* name);

_PUBLIC int system(const char* string);

_PUBLIC void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
    int (*compar)(const void*, const void*));

_PUBLIC void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

_PUBLIC int abs(int j);
_PUBLIC long int labs(long int j);
_PUBLIC long long int llabs(long long int j);

typedef struct
{
    int quot;
    int rem;
} div_t;

typedef struct
{
    long int quot;
    long int rem;
} ldiv_t;

typedef struct
{
    long long int quot;
    long long int rem;
} lldiv_t;

_PUBLIC div_t div(int numer, int denom);
_PUBLIC ldiv_t ldiv(long int numer, long int denom);
_PUBLIC lldiv_t lldiv(long long int numer, long long int denom);

#if _USE_ANNEX_K == 1

#include "_AUX/errno_t.h"
#include "_AUX/rsize_t.h"

typedef void (*constraint_handler_t)(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

_PUBLIC constraint_handler_t set_constraint_handler_s(constraint_handler_t handler);

_PUBLIC void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

_PUBLIC void ignore_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

_PUBLIC errno_t getenv_s(size_t* _RESTRICT len, char* _RESTRICT value, rsize_t maxsize, const char* _RESTRICT name);

_PUBLIC void* bsearch_s(const void* key, const void* base, rsize_t nmemb, rsize_t size,
    int (*compar)(const void* k, const void* y, void* context), void* context);

_PUBLIC errno_t qsort_s(void* base, rsize_t nmemb, rsize_t size,
    int (*compar)(const void* x, const void* y, void* context), void* context);

#endif

#if defined(__cplusplus)
}
#endif

#endif
