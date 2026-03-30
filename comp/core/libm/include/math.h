#ifndef _MATH_H
#define _MATH_H 1

#include <libc/defs.h>

/// @todo Implement libm rounding modes.

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libc/config.h"

#ifdef _static_assert
_static_assert(sizeof(double) == sizeof(long double), "double and long double must be the same size");
#endif

typedef union {
    double val;
    struct
    {
        unsigned long long mant : 52;
        unsigned long long exp : 11;
        unsigned long long sign : 1;
    };
} _double_t;

#ifdef _static_assert
_static_assert(sizeof(_double_t) == sizeof(double), "_double_t must be the same size as double");
#endif

#define _DOUBLE_BITS_MANTISSA 52
#define _DOUBLE_MAX_INTEGER 9007199254740992.0
#define _DOUBLE_PRECISION 53
#define _DOUBLE_BIAS 0x3FF
#define _DOUBLE_MAX_EXP 0x7FF

typedef union {
    float val;
    struct
    {
        unsigned int mant : 23;
        unsigned int exp : 8;
        unsigned int sign : 1;
    };
} _float_t;

#ifdef _static_assert
_static_assert(sizeof(_float_t) == sizeof(float), "_float_t must be the same size as float");
#endif

#define _FLOAT_BITS_MANTISSA 23
#define _FLOAT_MAX_INTEGER 16777216.0F
#define _FLOAT_PRECISION 24
#define _FLOAT_BIAS 0x7F
#define _FLOAT_MAX_EXP 0xFF

#define M_E ((double)2.7182818284590452354)
#define M_LOG2E ((double)1.4426950408889634074)
#define M_LOG10E ((double)0.43429448190325182765)
#define M_LN2 ((double)0.69314718055994530942)
#define M_LN10 ((double)2.30258509299404568402)
#define M_PI ((double)3.14159265358979323846)
#define M_PI_2 ((double)1.57079632679489661923)
#define M_PI_4 ((double)0.78539816339744830962)
#define M_1_PI ((double)0.31830988618379067154)
#define M_2_PI ((double)0.63661977236758134308)
#define M_2_SQRTPI ((double)1.12837916709551257390)
#define M_SQRT2 ((double)1.41421356237309504880)
#define M_SQRT1_2 ((double)0.70710678118654752440)

#define HUGE_VALF ((float)(1e+300 * 1e+300))
#define HUGE_VAL ((double)(1e+300 * 1e+300))
#define HUGE_VALL ((long double)(1e+300 * 1e+300))

#define INFINITY ((float)(1.0F / 0.0F))
#define NAN ((float)(0.0F / 0.0F))

// #define FP_FAST_FMAF
// #define FP_FAST_FMA
// #define FP_FAST_FMAL

#define FP_ILOGB0 (-__INT_MAX__ - 1)
#define FP_ILOGBNAN __INT_MAX__

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2

#define math_errhandling (MATH_ERRNO)

#define FP_INFINITE 1
#define FP_NAN 2
#define FP_NORMAL 3
#define FP_SUBNORMAL 4
#define FP_ZERO 5

#define isinf(x) \
    (sizeof(x) == sizeof(float)           ? (__extension__({ \
        _float_t _f = {.val = (float)(x)}; \
        _f.exp == _FLOAT_MAX_EXP&& _f.mant == 0; \
    })) \
            : sizeof(x) == sizeof(double) ? (__extension__({ \
                  _double_t _d = {.val = (double)(x)}; \
                  _d.exp == _DOUBLE_MAX_EXP&& _d.mant == 0; \
              })) \
                                          : 0)

#define isnan(x) \
    (sizeof(x) == sizeof(float)           ? (__extension__({ \
        _float_t _f = {.val = (float)(x)}; \
        _f.exp == _FLOAT_MAX_EXP&& _f.mant != 0; \
    })) \
            : sizeof(x) == sizeof(double) ? (__extension__({ \
                  _double_t _d = {.val = (double)(x)}; \
                  _d.exp == _DOUBLE_MAX_EXP&& _d.mant != 0; \
              })) \
                                          : 0)

#define isnormal(x) \
    (sizeof(x) == sizeof(float)           ? (__extension__({ \
        _float_t _f = {.val = (float)(x)}; \
        _f.exp != 0x00 && _f.exp != 0xFF; \
    })) \
            : sizeof(x) == sizeof(double) ? (__extension__({ \
                  _double_t _d = {.val = (double)(x)}; \
                  _d.exp != 0x000 && _d.exp != _DOUBLE_MAX_EXP; \
              })) \
                                          : 0)

#define signbit(x) \
    (sizeof(x) == sizeof(float)           ? (__extension__({ \
        _float_t _f = {.val = (float)(x)}; \
        _f.sign; \
    })) \
            : sizeof(x) == sizeof(double) ? (__extension__({ \
                  _double_t _d = {.val = (double)(x)}; \
                  _d.sign; \
              })) \
                                          : 0)

#define isfinite(x) (!isinf(x) && !isnan(x))

#define fpclassify(x) \
    (isnan(x) ? FP_NAN : isinf(x) ? FP_INFINITE : isnormal(x) ? FP_NORMAL : (x == 0.0) ? FP_ZERO : FP_SUBNORMAL)

#define isgreater(x, y) ((x) > (y))
#define isgreaterequal(x, y) ((x) >= (y))
#define isless(x, y) ((x) < (y))
#define islessequal(x, y) ((x) <= (y))
#define islessgreater(x, y) ((x) < (y) || (x) > (y))
#define isunordered(x, y) (isnan(x) || isnan(y))

double modf(double value, double* iptr);
float modff(float value, float* iptr);
#define modfl modf

static inline double fabs(double x)
{
    _double_t d = {.val = x};
    d.sign = 0;
    return d.val;
}
static inline float fabsf(float x)
{
    _float_t f = {.val = x};
    f.sign = 0;
    return f.val;
}
#define fabsl fabs

static inline double sqrt(double x)
{
    double res;
    ASM("fsqrt" : "=t"(res) : "0"(x));
    return res;
}
static inline float sqrtf(float x)
{
    float res;
    ASM("fsqrt" : "=t"(res) : "0"(x));
    return res;
}
#define sqrtl sqrt

static inline double copysign(double x, double y)
{
    _double_t dx = {.val = x};
    _double_t dy = {.val = y};
    dx.sign = dy.sign;
    return dx.val;
}
static inline float copysignf(float x, float y)
{
    _float_t dx = {.val = x};
    _float_t dy = {.val = y};
    dx.sign = dy.sign;
    return dx.val;
}
#define copysignl copysign

static inline double fmin(double x, double y)
{
    return x < y ? x : y;
}
static inline float fminf(float x, float y)
{
    return x < y ? x : y;
}
#define fminl fmin

static inline double fmax(double x, double y)
{
    return x > y ? x : y;
}
static inline float fmaxf(float x, float y)
{
    return x > y ? x : y;
}
#define fmaxl fmax

static inline double nan(const char* tagp)
{
    (void)tagp;
    return NAN;
}
static inline float nanf(const char* tagp)
{
    (void)tagp;
    return NAN;
}
#define nanl nan

static inline double ceil(double x)
{
    double iptr;
    double frac = modf(x, &iptr);
    if (frac > 0.0)
    {
        return iptr + 1.0;
    }
    return iptr;
}
static inline float ceilf(float x)
{
    float iptr;
    float frac = modff(x, &iptr);
    if (frac > 0.0F)
    {
        return iptr + 1.0F;
    }
    return iptr;
}
#define ceill ceil

static inline double floor(double x)
{
    double iptr;
    double frac = modf(x, &iptr);
    if (frac < 0.0)
    {
        return iptr - 1.0;
    }
    return iptr;
}
static inline float floorf(float x)
{
    float iptr;
    float frac = modff(x, &iptr);
    if (frac < 0.0F)
    {
        return iptr - 1.0F;
    }
    return iptr;
}
#define floorl floor

static inline double fma(double x, double y, double z)
{
    return (x * y) + z;
}
static inline float fmaf(float x, float y, float z)
{
    return (x * y) + z;
}
#define fmal fma

// double round(double x)
// float roundf(float x);
// long double roundl(long double x);

static inline double round(double x)
{
    double iptr;
    double frac = modf(x, &iptr);
    if (frac >= 0.5)
    {
        return iptr + 1.0;
    }
    if (frac <= -0.5)
    {
        return iptr - 1.0;
    }
    return iptr;
}
static inline float roundf(float x)
{
    float iptr;
    float frac = modff(x, &iptr);
    if (frac >= 0.5F)
    {
        return iptr + 1.0F;
    }
    if (frac <= -0.5F)
    {
        return iptr - 1.0F;
    }
    return iptr;
}
#define roundl round

static inline long int lround(double x)
{
    return (long int)round(x);
}
static inline long int lroundf(float x)
{
    return (long int)roundf(x);
}
#define lroundl lround

static inline double rint(double x)
{
    return round(x);
}
static inline float rintf(float x)
{
    return roundf(x);
}
#define rintl rint

static inline long int lrint(double x)
{
    return (long int)round(x);
}
static inline long int lrintf(float x)
{
    return (long int)roundf(x);
}
#define lrintl lrint

static inline long long int llrint(double x)
{
    return (long long int)round(x);
}
static inline long long int llrintf(float x)
{
    return (long long int)roundf(x);
}
#define llrintl llrint

static inline long long int llround(double x)
{
    return (long long int)round(x);
}
static inline long long int llroundf(float x)
{
    return (long long int)roundf(x);
}
#define llroundl llround

static inline double nearbyint(double x)
{
    return round(x);
}
static inline float nearbyintf(float x)
{
    return roundf(x);
}
#define nearbyintl nearbyint

static inline double trunc(double x)
{
    double iptr;
    modf(x, &iptr);
    return iptr;
}
static inline float truncf(float x)
{
    float iptr;
    modff(x, &iptr);
    return iptr;
}
#define truncl trunc

double ldexp(double x, int exp);
float ldexpf(float x, int exp);
#define ldexpl ldexp

static inline double scalbn(double x, int exp)
{
    return ldexp(x, exp);
}
static inline float scalbnf(float x, int exp)
{
    return ldexpf(x, exp);
}
#define scalbnl scalbn

static inline double atan(double x)
{
    double res;
    ASM("fld1\n\tfpatan" : "=t"(res) : "0"(x));
    return res;
}
static inline float atanf(float x)
{
    float res;
    ASM("fld1\n\tfpatan" : "=t"(res) : "0"(x));
    return res;
}
#define atanl atan

static inline double atan2(double y, double x)
{
    double res;
    ASM("fldl %1\n\tfldl %2\n\tfpatan" : "=t"(res) : "m"(y), "m"(x));
    return res;
}
static inline float atan2f(float y, float x)
{
    float res;
    ASM("flds %1\n\tflds %2\n\tfpatan" : "=t"(res) : "m"(y), "m"(x));
    return res;
}
#define atan2l atan2

static inline double cos(double x)
{
    double res;
    ASM("fcos" : "=t"(res) : "0"(x));
    return res;
}
static inline float cosf(float x)
{
    float res;
    ASM("fcos" : "=t"(res) : "0"(x));
    return res;
}
#define cosl cos

static inline double sin(double x)
{
    double res;
    ASM("fsin" : "=t"(res) : "0"(x));
    return res;
}
static inline float sinf(float x)
{
    float res;
    ASM("fsin" : "=t"(res) : "0"(x));
    return res;
}
#define sinl sin

static inline double tan(double x)
{
    double res;
    ASM("fptan\n\tfstp %%st(0)" : "=t"(res) : "0"(x));
    return res;
}
static inline float tanf(float x)
{
    float res;
    ASM("fptan\n\tfstp %%st(0)" : "=t"(res) : "0"(x));
    return res;
}
#define tanl tan

double frexp(double value, int* exp);
float frexpf(float value, int* exp);
#define frexpl frexp

int ilogb(double x);
int ilogbf(float x);
#define ilogbl ilogb

static inline double log(double x)
{
    double res;
    ASM("fldln2\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
static inline float logf(float x)
{
    float res;
    ASM("fldln2\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
#define logl log

static inline double log10(double x)
{
    double res;
    ASM("fldlg2\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
static inline float log10f(float x)
{
    float res;
    ASM("fldlg2\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
#define log10l log10

double log1p(double x);
float log1pf(float x);
#define log1pl log1p

static inline double log2(double x)
{
    double res;
    ASM("fld1\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
static inline float log2f(float x)
{
    float res;
    ASM("fld1\n\tfxch\n\tfyl2x" : "=t"(res) : "0"(x));
    return res;
}
#define log2l log2

double logb(double x);
float logbf(float x);
#define logbl logb

double scalbln(double x, long int n);
float scalblnf(float x, long int n);
#define scalblnl scalbln

double cbrt(double x);
float cbrtf(float x);
#define cbrtl cbrt

double hypot(double x, double y);
float hypotf(float x, float y);
#define hypotl hypot

double pow(double x, double y);
float powf(float x, float y);
#define powl pow

double erf(double x);
float erff(float x);
#define erfl erf

double erfc(double x);
float erfcf(float x);
#define erfcl erfc

double lgamma(double x);
float lgammaf(float x);
#define lgammal lgamma

double tgamma(double x);
float tgammaf(float x);
#define tgammal tgamma

static inline double fmod(double x, double y)
{
    double res;
    ASM("1:\n\tfprem\n\tfnstsw %%ax\n\tsahf\n\tjp 1b" : "=t"(res) : "0"(x), "u"(y) : "ax", "cc");
    return res;
}
static inline float fmodf(float x, float y)
{
    float res;
    ASM("1:\n\tfprem\n\tfnstsw %%ax\n\tsahf\n\tjp 1b" : "=t"(res) : "0"(x), "u"(y) : "ax", "cc");
    return res;
}
#define fmodl fmod

static inline double remainder(double x, double y)
{
    double res;
    ASM("1:\n\tfprem1\n\tfnstsw %%ax\n\tsahf\n\tjp 1b" : "=t"(res) : "0"(x), "u"(y) : "ax", "cc");
    return res;
}
static inline float remainderf(float x, float y)
{
    float res;
    ASM("1:\n\tfprem1\n\tfnstsw %%ax\n\tsahf\n\tjp 1b" : "=t"(res) : "0"(x), "u"(y) : "ax", "cc");
    return res;
}
#define remainderl remainder

double remquo(double x, double y, int* quo);
float remquof(float x, float y, int* quo);
#define remquol remquo

double nextafter(double x, double y);
float nextafterf(float x, float y);
#define nextafterl nextafter

double nexttoward(double x, long double y);
float nexttowardf(float x, long double y);
#define nexttowardl nexttoward

double fdim(double x, double y);
float fdimf(float x, float y);
#define fdiml fdim

static inline double exp2(double x)
{
    double res;
    ASM("fld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st(0)\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(
        res) : "0"(x));
    return res;
}
static inline float exp2f(float x)
{
    float res;
    ASM("fld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st(0)\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(
        res) : "0"(x));
    return res;
}
#define exp2l exp2

static inline double exp(double x)
{
    double res;
    ASM("fldl2e\n\tfmulp\n\tfld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), "
        "%%st(0)\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(res) : "0"(x));
    return res;
}
static inline float expf(float x)
{
    float res;
    ASM("fldl2e\n\tfmulp\n\tfld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), "
        "%%st(0)\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(res) : "0"(x));
    return res;
}
#define expl exp

static inline double expm1(double x)
{
    return exp(x) - 1.0;
}
static inline float expm1f(float x)
{
    return expf(x) - 1.0F;
}
#define expm1l expm1

static inline double cosh(double x)
{
    return (exp(x) + exp(-x)) / 2.0;
}
static inline float coshf(float x)
{
    return (expf(x) + expf(-x)) / 2.0F;
}
#define coshl cosh

static inline double sinh(double x)
{
    return (exp(x) - exp(-x)) / 2.0;
}
static inline float sinhf(float x)
{
    return (expf(x) - expf(-x)) / 2.0F;
}
#define sinhl sinh

static inline double tanh(double x)
{
    double e2x = exp(2.0 * x);
    return (e2x - 1.0) / (e2x + 1.0);
}
static inline float tanhf(float x)
{
    float e2x = expf(2.0F * x);
    return (e2x - 1.0F) / (e2x + 1.0F);
}
#define tanhl tanh

static inline double acos(double x)
{
    return atan2(sqrt(1.0 - x * x), x);
}
static inline float acosf(float x)
{
    return atan2f(sqrtf(1.0F - x * x), x);
}
#define acosl acos

static inline double asin(double x)
{
    return atan2(x, sqrt(1.0 - x * x));
}
static inline float asinf(float x)
{
    return atan2f(x, sqrtf(1.0F - x * x));
}
#define asinl asin

static inline double acosh(double x)
{
    return log(x + sqrt(x * x - 1.0));
}
static inline float acoshf(float x)
{
    return logf(x + sqrtf(x * x - 1.0F));
}
#define acoshl acosh

static inline double asinh(double x)
{
    return log(x + sqrt(x * x + 1.0));
}
static inline float asinhf(float x)
{
    return logf(x + sqrtf(x * x + 1.0F));
}
#define asinhl asinh

static inline double atanh(double x)
{
    return 0.5 * log((1.0 + x) / (1.0 - x));
}
static inline float atanhf(float x)
{
    return 0.5F * logf((1.0F + x) / (1.0F - x));
}
#define atanhl atanh

#if defined(__cplusplus)
}
#endif

#endif