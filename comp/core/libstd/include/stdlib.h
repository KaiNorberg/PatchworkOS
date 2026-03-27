#ifndef _STDLIB_H
#define _STDLIB_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/NULL.h"
#include "_libstd/config.h"
#include "_libstd/size_t.h"

char* lltoa(long long number, char* str, int base);
#define ltoa(number, str, base) lltoa(number, str, base)
#define itoa(number, str, base) lltoa(number, str, base)

char* ulltoa(unsigned long long number, char* str, int base);
#define ultoa(number, str, base) lltoa(number, str, base)
#define uitoa(number, str, base) lltoa(number, str, base)

double atof(const char* nptr);
double strtod(const char* _RESTRICT nptr, char** _RESTRICT endptr);
float strtof(const char* _RESTRICT nptr, char** _RESTRICT endptr);
long double strtold(const char* _RESTRICT nptr, char** _RESTRICT endptr);

long long int strtoll(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
#define strtol(nptr, endptr, base) ((long int)strtoll(nptr, endptr, base))

unsigned long int strtoul(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);
unsigned long long int strtoull(const char* _RESTRICT nptr, char** _RESTRICT endptr, int base);

long long int atoll(const char* nptr);
#define atol(nptr) ((long)atoll(nptr))
#define atoi(nptr) ((int)atoll(nptr))

#define RAND_MAX 32767

int rand(void);
void srand(unsigned int seed);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE -1

_NORETURN void abort(void);

int at_quick_exit(void (*func)(void));

int atexit(void (*func)(void));

_NORETURN void exit(int status);

_NORETURN void quick_exit(int status);

_NORETURN void _Exit(int status);

char* getenv(const char* name);

int system(const char* string);

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);

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

div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);
lldiv_t lldiv(long long int numer, long long int denom);

int system(const char* command);

#if _USE_ANNEX_K == 1

#include "_libstd/errno_t.h"
#include "_libstd/rsize_t.h"

typedef void (*constraint_handler_t)(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

constraint_handler_t set_constraint_handler_s(constraint_handler_t handler);

void abort_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

void ignore_handler_s(const char* _RESTRICT msg, void* _RESTRICT ptr, errno_t err);

errno_t getenv_s(size_t* _RESTRICT len, char* _RESTRICT value, rsize_t maxsize, const char* _RESTRICT name);

void* bsearch_s(const void* key, const void* base, rsize_t nmemb, rsize_t size,
    int (*compar)(const void* k, const void* y, void* context), void* context);

errno_t qsort_s(void* base, rsize_t nmemb, rsize_t size, int (*compar)(const void* x, const void* y, void* context),
    void* context);

#endif

#if defined(__cplusplus)
}
#endif

#endif
