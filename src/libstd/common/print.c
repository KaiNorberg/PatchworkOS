#include "print.h"

#include "common/digits.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _KERNEL_
#include "user/common/file.h"
#endif

#define _DBL_SIGN(bytes) (((unsigned)bytes[7] & 0x80) >> 7)
#define _DBL_DEC(bytes) ((_DBL_EXP(bytes) > 0) ? 1 : 0)
#define _DBL_EXP(bytes) ((((unsigned)bytes[7] & 0x7f) << 4) | (((unsigned)bytes[6] & 0xf0) >> 4))
#define _DBL_BIAS 1023
#define _DBL_MANT_START(bytes) (bytes + 6)

#if __LDBL_MANT_DIG__ == 64

/* Intel "Extended Precision" format, using 80 bits (64bit mantissa) */
#define _LDBL_SIGN(bytes) (((unsigned)bytes[9] & 0x80) >> 7)
#define _LDBL_DEC(bytes) (((unsigned)bytes[7] & 0x80) >> 7)
#define _LDBL_EXP(bytes) ((((unsigned)bytes[9] & 0x7f) << 8) | (unsigned)bytes[8])
#define _LDBL_BIAS 16383
#define _LDBL_MANT_START(bytes) (bytes + 7)

#elif __LDBL_MANT_DIG__ == 113

/* IEEE "Quadruple Precision" format, using 128 bits (113bit mantissa) */
#define _LDBL_SIGN(bytes) (((unsigned)bytes[15] & 0x80) >> 7)
#define _LDBL_DEC(bytes) ((_LDBL_EXP(bytes) > 0) ? 1 : 0)
#define _LDBL_EXP(bytes) ((((unsigned)bytes[15] & 0x7f) << 8) | (unsigned)bytes[14])
#define _LDBL_BIAS 16383
#define _LDBL_MANT_START(bytes) (bytes + 13)

#else
/* IEEE "Double Precision" format, using 64 bits (53bit mantissa,
   same as DBL above) */
#define _LDBL_SIGN(bytes) (((unsigned)bytes[7] & 0x80) >> 7)
#define _LDBL_DEC(bytes) ((_LDBL_EXP(bytes) > 0) ? 1 : 0)
#define _LDBL_EXP(bytes) ((((unsigned)bytes[7] & 0x7f) << 4) | (((unsigned)bytes[6] & 0xf0) >> 4))
#define _LDBL_BIAS 1023
#define _LDBL_MANT_START(bytes) (bytes + 6)
#endif

#ifndef _KERNEL_
#define _PRINT_PUT(ctx, x) \
    ({ \
        int character = x; \
        if ((ctx)->totalChars < (ctx)->maxChars) \
        { \
            if ((ctx)->stream != NULL) \
            { \
                putc(character, (ctx)->stream); \
            } \
            else \
            { \
                (ctx)->buffer[(ctx)->totalChars] = character; \
            } \
        } \
        ++((ctx)->totalChars); \
    })
#else
#define _PRINT_PUT(ctx, x) \
    ({ \
        int character = x; \
        if ((ctx)->totalChars < (ctx)->maxChars) \
        { \
            if ((ctx)->buffer != NULL) \
            { \
                (ctx)->buffer[(ctx)->totalChars] = character; \
            } \
        } \
        ++((ctx)->totalChars); \
    })
#endif

#ifndef _KERNEL_

static void _print_hexa(int sign, int exp, int dec, unsigned char const* mant, size_t mant_dig, _format_ctx_t* ctx)
{
    size_t excess_bits;
    char value = '\0';

    unsigned char mantissa[32] = {0};
    size_t m = 0;

    char exponent[32];
    size_t e = 0;

    size_t i;

    char const* digit_chars = (ctx->flags & FORMAT_LOWER) ? _digits : _xdigits;

    int index_offset = 0;

    //_static_assert(__FLT_RADIX__ == 2, "Assuming 2-based FP");
    //_static_assert(__CHAR_BIT__ == 8, "Assuming 8-bit bytes");

    /* Mantissa */
    /* -------- */

    /* Handle the most significant byte (which might need masking) */
    excess_bits = (mant_dig - 1) % 8;

    if (excess_bits > 0)
    {
        value = *mant & ((1 << excess_bits) - 1);

        if (excess_bits >= 4)
        {
            mantissa[1] = value & 0x0f;
            value >>= 4;
            excess_bits -= 4;
            ++m;
        }

        index_offset = 1;
    }

    mantissa[0] = (dec << excess_bits) | (value & ((1 << excess_bits) - 1));

    /* Now handle the remaining bytes. */
    /* This is doing a little trick: m is the highest valid index
    (or a count of *fractional* digits, if you like), not a count
    of elements (0..1, not 1..2), so it can double as an index
    into the mant[] array (when divided by 2).
    */
    while (m < ((mant_dig + 3) / 4 - 1))
    {
        value = *(mant - ((m / 2) + index_offset));
        mantissa[++m] = (value & 0xf0) >> 4;
        mantissa[++m] = (value & 0x0f);
    }

    /* Roll back trailing zeroes */
    while (m > 0 && mantissa[m] == 0)
    {
        --m;
    }

    /* Exponent */
    /* -------- */

    exp -= excess_bits;

    if ((m == 0 && dec == 0) || exp == 0)
    {
        /* All zero */
        exponent[0] = '+';
        exponent[1] = '0';
        e = 2;
    }
    else
    {
        if (dec == 0)
        {
            /* Subnormal */
            ++exp;
        }

        if (exp >= 0)
        {
            exponent[0] = '+';
        }
        else
        {
            exponent[0] = '-';
            exp *= -1;
        }

        for (e = 1; exp > 0; ++e)
        {
            div_t d = div(exp, 10);
            exponent[e] = digit_chars[d.rem];
            exp = d.quot;
        }
    }

    exponent[e] = '\0';

    /* Rounding */
    /* -------- */

    if ((ctx->precision >= 0) && (m > (size_t)ctx->precision))
    {
        i = ctx->precision;

        if ((mantissa[i + 1] > 8) || ((mantissa[i + 1] == 8) && ((m >= i + 2) || (mantissa[i] % 2))))
        {
            while ((++mantissa[i]) > 0xf)
            {
                mantissa[i--] = 0;
            }
        }

        m = i;
    }

    /* Padding */
    /* ------- */

    ctx->currentChars = m + 4 + (sign != '\0') + ((m > 0) || (ctx->precision > 0) || (ctx->flags & FORMAT_ALT)) + e;

    if (!(ctx->flags & (FORMAT_ZERO | FORMAT_MINUS)))
    {
        for (i = ctx->currentChars; i < ctx->width; ++i)
        {
            _PRINT_PUT(ctx, ' ');
        }
    }

    if (sign != '\0')
    {
        _PRINT_PUT(ctx, sign);
    }

    /* Output */
    /* ------ */

    _PRINT_PUT(ctx, '0');
    _PRINT_PUT(ctx, (ctx->flags & FORMAT_LOWER) ? 'x' : 'X');

    _PRINT_PUT(ctx, digit_chars[mantissa[0]]);

    if (((m > 0) && (ctx->precision != 0)) || (ctx->precision > 0) || (ctx->flags & FORMAT_ALT))
    {
        _PRINT_PUT(ctx, '.');
    }

    if ((ctx->flags & FORMAT_ZERO) && !(ctx->flags & FORMAT_MINUS))
    {
        for (i = ctx->currentChars; i < ctx->width; ++i)
        {
            _PRINT_PUT(ctx, '0');
        }
    }

    for (i = 1; i <= m; ++i)
    {
        _PRINT_PUT(ctx, digit_chars[mantissa[i]]);
    }

    while ((int)i <= ctx->precision)
    {
        _PRINT_PUT(ctx, '0');
        ++i;
    }

    _PRINT_PUT(ctx, (ctx->flags & FORMAT_LOWER) ? 'p' : 'P');
    _PRINT_PUT(ctx, exponent[0]);

    for (i = e - 1; i > 0; --i)
    {
        _PRINT_PUT(ctx, exponent[i]);
    }
}

/* dec:      1 - normalized, 0 - subnormal
exp:      INT_MAX - infinity, INT_MIN - Not a Number
mant:     MSB of the mantissa
mant_dig: base FLT_RADIX digits in the mantissa, including the decimal
*/
static void _print_fp(int sign, int exp, int dec, unsigned char const* mant, size_t mant_dig, _format_ctx_t* ctx)
{
    /* Turning sign bit into sign character. */
    if (sign)
    {
        sign = '-';
    }
    else if (ctx->flags & FORMAT_PLUS)
    {
        sign = '+';
    }
    else if (ctx->flags & FORMAT_SPACE)
    {
        sign = ' ';
    }
    else
    {
        sign = '\0';
    }

    if (exp == INT_MIN || exp == INT_MAX)
    {
        /* "nan" / "inf" */
        char const* s =
            (ctx->flags & FORMAT_LOWER) ? ((exp == INT_MIN) ? "nan" : "inf") : ((exp == INT_MIN) ? "NAN" : "INF");

        ctx->currentChars = (sign == '\0') ? 3 : 4;

        if (!(ctx->flags & FORMAT_MINUS))
        {
            while (ctx->currentChars < ctx->width)
            {
                _PRINT_PUT(ctx, ' ');
                ++ctx->currentChars;
            }
        }

        if (sign != '\0')
        {
            _PRINT_PUT(ctx, sign);
        }

        while (*s)
        {
            _PRINT_PUT(ctx, *s++);
        }

        return;
    }

    switch (ctx->flags & (FORMAT_DECIMAL | FORMAT_EXPONENT | FORMAT_GENERIC | FORMAT_HEXA))
    {
    case FORMAT_HEXA:
        _print_hexa(sign, exp, dec, mant, mant_dig, ctx);
        break;
    case FORMAT_DECIMAL:
    case FORMAT_EXPONENT:
    case FORMAT_GENERIC:
    default:
        break;
    }
}

static void _print_double(double value, _format_ctx_t* ctx)
{
    unsigned char bytes[sizeof(double)];
    int exp;
    memcpy(bytes, &value, sizeof(double));
    exp = _DBL_EXP(bytes) - _DBL_BIAS;

    if (exp == __DBL_MAX_EXP__)
    {
        /*                           NAN       INF */
        exp = (value != value) ? INT_MIN : INT_MAX;
    }

    _print_fp(_DBL_SIGN(bytes), exp, _DBL_DEC(bytes), _DBL_MANT_START(bytes), __DBL_MANT_DIG__, ctx);
}

static void _print_ldouble(long double value, _format_ctx_t* ctx)
{
    unsigned char bytes[sizeof(long double)];
    int exp;
    memcpy(bytes, &value, sizeof(long double));
    exp = _LDBL_EXP(bytes) - _LDBL_BIAS;

    if (exp == __LDBL_MAX_EXP__)
    {
        /*                           NAN       INF */
        exp = (value != value) ? INT_MIN : INT_MAX;
    }

    _print_fp(_LDBL_SIGN(bytes), exp, _LDBL_DEC(bytes), _LDBL_MANT_START(bytes), __LDBL_MANT_DIG__, ctx);
}

#endif

static void _int_format(intmax_t value, _format_ctx_t* ctx)
{
    /* At worst, we need two prefix characters (hex prefix). */
    char preface[3] = "\0";
    size_t preidx = 0;

    if (ctx->precision < 0)
    {
        ctx->precision = 1;
    }

    if ((ctx->flags & FORMAT_ALT) && (ctx->base == 16 || ctx->base == 8) && (value != 0))
    {
        /* Octal / hexadecimal prefix for "%#" conversions */
        preface[preidx++] = '0';

        if (ctx->base == 16)
        {
            preface[preidx++] = (ctx->flags & FORMAT_LOWER) ? 'x' : 'X';
        }
    }

    if (value < 0)
    {
        /* Negative sign for negative values - at all times. */
        preface[preidx++] = '-';
    }
    else if (!(ctx->flags & FORMAT_UNSIGNED))
    {
        /* plus sign / extra space are only for signed conversions */
        if (ctx->flags & FORMAT_PLUS)
        {
            preface[preidx++] = '+';
        }
        else
        {
            if (ctx->flags & FORMAT_SPACE)
            {
                preface[preidx++] = ' ';
            }
        }
    }

    {
        /* At this point, ctx->currentChars has the number of digits queued up.
            Determine if we have a precision requirement to pad those.
        */
        size_t prec_pads =
            ((size_t)ctx->precision > ctx->currentChars) ? ((size_t)ctx->precision - ctx->currentChars) : 0;

        if (!(ctx->flags & (FORMAT_MINUS | FORMAT_ZERO)))
        {
            /* Space padding is only done if no zero padding or left alignment
                is requested. Calculate the number of characters that WILL be
                printed, including any prefixes determined above.
            */
            /* The number of characters to be printed, plus prefixes if any. */
            /* This line contained probably the most stupid, time-wasting bug
                I've ever perpetrated. Greetings to Samface, DevL, and all
                sceners at Breakpoint 2006.
            */
            size_t characters =
                preidx + ((ctx->currentChars > (size_t)ctx->precision) ? ctx->currentChars : (size_t)ctx->precision);

            if (ctx->width > characters)
            {
                size_t i;

                for (i = 0; i < ctx->width - characters; ++i)
                {
                    _PRINT_PUT(ctx, ' ');
                    ++(ctx->currentChars);
                }
            }
        }

        /* Now we did the padding, do the prefixes (if any). */
        preidx = 0;

        while (preface[preidx] != '\0')
        {
            _PRINT_PUT(ctx, preface[preidx++]);
            ++(ctx->currentChars);
        }

        /* Do the precision padding if necessary. */
        while (prec_pads-- > 0)
        {
            _PRINT_PUT(ctx, '0');
            ++(ctx->currentChars);
        }

        if ((!(ctx->flags & FORMAT_MINUS)) && (ctx->flags & FORMAT_ZERO))
        {
            /* If field is not left aligned, and zero padding is requested, do
                so.
            */
            while (ctx->currentChars < ctx->width)
            {
                _PRINT_PUT(ctx, '0');
                ++(ctx->currentChars);
            }
        }
    }
}

static void _print_integer(imaxdiv_t div, _format_ctx_t* ctx)
{
    if (ctx->currentChars == 0 && div.quot == 0 && div.rem == 0 && ctx->precision == 0)
    {
        _int_format(0, ctx);
    }
    else
    {
        ++(ctx->currentChars);

        if (div.quot != 0)
        {
            _print_integer(imaxdiv(div.quot, ctx->base), ctx);
        }
        else
        {
            _int_format(div.rem, ctx);
        }

        if (div.rem < 0)
        {
            div.rem *= -1;
        }

        if (ctx->flags & FORMAT_LOWER)
        {
            _PRINT_PUT(ctx, _digits[div.rem]);
        }
        else
        {
            _PRINT_PUT(ctx, _xdigits[div.rem]);
        }
    }
}

static void _print_string(const char* s, _format_ctx_t* ctx)
{
    if (ctx->flags & FORMAT_CHAR)
    {
        ctx->precision = 1;
    }
    else
    {
        if (ctx->precision < 0)
        {
            ctx->precision = strlen(s);
        }
        else
        {
            int i;

            for (i = 0; i < ctx->precision; ++i)
            {
                if (s[i] == 0)
                {
                    ctx->precision = i;
                    break;
                }
            }
        }
    }

    if (!(ctx->flags & FORMAT_MINUS) && (ctx->width > (size_t)ctx->precision))
    {
        while (ctx->currentChars < (ctx->width - ctx->precision))
        {
            _PRINT_PUT(ctx, ' ');
            ++(ctx->currentChars);
        }
    }

    while (ctx->precision > 0)
    {
        _PRINT_PUT(ctx, *(s++));
        --(ctx->precision);
        ++(ctx->currentChars);
    }

    if (ctx->flags & FORMAT_MINUS)
    {
        while (ctx->width > ctx->currentChars)
        {
            _PRINT_PUT(ctx, ' ');
            ++(ctx->currentChars);
        }
    }
}

const char* _print(const char* spec, _format_ctx_t* ctx)
{
    const char* orig_spec = spec;

    if (*(++spec) == '%')
    {
        /* %% -> print single '%' */
        _PRINT_PUT(ctx, *spec);
        return ++spec;
    }

    /* Initializing ctx structure */
    ctx->flags = 0;
    ctx->base = 0;
    ctx->currentChars = 0;
    ctx->width = 0;
    ctx->precision = EOF;

    /* First come 0..n flags */
    do
    {
        switch (*spec)
        {
        case '-':
            /* left-aligned output */
            ctx->flags |= FORMAT_MINUS;
            ++spec;
            break;

        case '+':
            /* positive numbers prefixed with '+' */
            ctx->flags |= FORMAT_PLUS;
            ++spec;
            break;

        case '#':
            /* alternative format (leading 0x for hex, 0 for octal) */
            ctx->flags |= FORMAT_ALT;
            ++spec;
            break;

        case ' ':
            /* positive numbers prefixed with ' ' */
            ctx->flags |= FORMAT_SPACE;
            ++spec;
            break;

        case '0':
            /* right-aligned padding done with '0' instead of ' ' */
            ctx->flags |= FORMAT_ZERO;
            ++spec;
            break;

        default:
            /* not a flag, exit flag parsing */
            ctx->flags |= FORMAT_DONE;
            break;
        }
    } while (!(ctx->flags & FORMAT_DONE));

    /* Optional field width */
    if (*spec == '*')
    {
        /* Retrieve width value from argument stack */
        int width = va_arg(ctx->arg, int);

        if (width < 0)
        {
            ctx->flags |= FORMAT_MINUS;
            ctx->width = abs(width);
        }
        else
        {
            ctx->width = width;
        }

        ++spec;
    }
    else
    {
        /* If a width is given, strtol() will return its value. If not given,
           strtol() will return zero. In both cases, endptr will point to the
           rest of the conversion specifier - just what we need.
        */
        ctx->width = (int)strtol(spec, (char**)&spec, 10);
    }

    /* Optional precision */
    if (*spec == '.')
    {
        ++spec;

        if (*spec == '*')
        {
            /* Retrieve precision value from argument stack. A negative value
               is as if no precision is given - as precision is initalized to
               EOF (negative), there is no need for testing for negative here.
            */
            ctx->precision = va_arg(ctx->arg, int);
            ++spec;
        }
        else
        {
            char* endptr;
            ctx->precision = (int)strtol(spec, &endptr, 10);

            if (spec == endptr)
            {
                /* Decimal point but no number - equals zero */
                ctx->precision = 0;
            }

            spec = endptr;
        }

        /* Having a precision cancels out any zero flag. */
        ctx->flags &= ~FORMAT_ZERO;
    }

    /* Optional length modifier
       We step one character ahead in any case, and step back only if we find
       there has been no length modifier (or step ahead another character if it
       has been "hh" or "ll").
    */
    switch (*(spec++))
    {
    case 'h':
        if (*spec == 'h')
        {
            /* hh -> char */
            ctx->flags |= FORMAT_CHAR;
            ++spec;
        }
        else
        {
            /* h -> short */
            ctx->flags |= FORMAT_SHORT;
        }

        break;

    case 'l':
        if (*spec == 'l')
        {
            /* ll -> long long */
            ctx->flags |= FORMAT_LLONG;
            ++spec;
        }
        else
        {
            /* k -> long */
            ctx->flags |= FORMAT_LONG;
        }

        break;

    case 'j':
        /* j -> intmax_t, which might or might not be long long */
        ctx->flags |= FORMAT_INTMAX;
        break;

    case 'z':
        /* z -> size_t, which might or might not be unsigned int */
        ctx->flags |= FORMAT_SIZE;
        break;

    case 't':
        /* t -> ptrdiff_t, which might or might not be long */
        ctx->flags |= FORMAT_PTRDIFF;
        break;

    case 'L':
        /* L -> long double */
        ctx->flags |= FORMAT_LDOUBLE;
        break;

    default:
        --spec;
        break;
    }

    /* Conversion specifier */
    switch (*spec)
    {
    case 'd':
        /* FALLTHROUGH */

    case 'i':
        ctx->base = 10;
        break;

    case 'o':
        ctx->base = 8;
        ctx->flags |= FORMAT_UNSIGNED;
        break;

    case 'u':
        ctx->base = 10;
        ctx->flags |= FORMAT_UNSIGNED;
        break;

    case 'x':
        ctx->base = 16;
        ctx->flags |= (FORMAT_LOWER | FORMAT_UNSIGNED);
        break;

    case 'X':
        ctx->base = 16;
        ctx->flags |= FORMAT_UNSIGNED;
        break;

    case 'f':
        ctx->base = 2;
        ctx->flags |= (FORMAT_DECIMAL | FORMAT_DOUBLE | FORMAT_LOWER);
        break;

    case 'F':
        ctx->base = 2;
        ctx->flags |= (FORMAT_DECIMAL | FORMAT_DOUBLE);
        break;

    case 'e':
        ctx->base = 2;
        ctx->flags |= (FORMAT_EXPONENT | FORMAT_DOUBLE | FORMAT_LOWER);
        break;

    case 'E':
        ctx->base = 2;
        ctx->flags |= (FORMAT_EXPONENT | FORMAT_DOUBLE);
        break;

    case 'g':
        ctx->base = 2;
        ctx->flags |= (FORMAT_GENERIC | FORMAT_DOUBLE | FORMAT_LOWER);
        break;

    case 'G':
        ctx->base = 2;
        ctx->flags |= (FORMAT_GENERIC | FORMAT_DOUBLE);
        break;

    case 'a':
        ctx->base = 2;
        ctx->flags |= (FORMAT_HEXA | FORMAT_DOUBLE | FORMAT_LOWER);
        break;

    case 'A':
        ctx->base = 2;
        ctx->flags |= (FORMAT_HEXA | FORMAT_DOUBLE);
        break;

    case 'c':
        /* TODO: wide chars. */
        {
            char c[1];
            c[0] = (char)va_arg(ctx->arg, int);
            ctx->flags |= FORMAT_CHAR;
            _print_string(c, ctx);
            return ++spec;
        }

    case 's':
        /* TODO: wide chars. */
        _print_string(va_arg(ctx->arg, char*), ctx);
        return ++spec;

    case 'p':
        ctx->base = 16;
        ctx->flags |= (FORMAT_LOWER | FORMAT_UNSIGNED | FORMAT_ALT | FORMAT_POINTER);
        break;

    case 'n':
    {
        int* val = va_arg(ctx->arg, int*);
        *val = ctx->totalChars;
        return ++spec;
    }

    default:
        /* No conversion specifier. Bad conversion. */
        return orig_spec;
    }

    /* Do the actual output based on our findings */
    if (ctx->base != 0)
    {
        /* TODO: Check for invalid flag combinations. */
        if (ctx->flags & FORMAT_DOUBLE)
        {
            /* Floating Point conversions */
#ifndef _KERNEL_
            if (ctx->flags & FORMAT_LDOUBLE)
            {
                long double value = va_arg(ctx->arg, long double);
                _print_ldouble(value, ctx);
            }
            else
            {
                double value = va_arg(ctx->arg, double);
                _print_double(value, ctx);
            }
#endif
        }
        else
        {
            if (ctx->flags & FORMAT_UNSIGNED)
            {
                /* Integer conversions (unsigned) */
                uintmax_t value;
                imaxdiv_t div;

                switch (ctx->flags &
                    (FORMAT_CHAR | FORMAT_SHORT | FORMAT_LONG | FORMAT_LLONG | FORMAT_SIZE | FORMAT_POINTER |
                        FORMAT_INTMAX))
                {
                case FORMAT_CHAR:
                    value = (uintmax_t)(unsigned char)va_arg(ctx->arg, int);
                    break;

                case FORMAT_SHORT:
                    value = (uintmax_t)(unsigned short)va_arg(ctx->arg, int);
                    break;

                case 0:
                    value = (uintmax_t)va_arg(ctx->arg, unsigned int);
                    break;

                case FORMAT_LONG:
                    value = (uintmax_t)va_arg(ctx->arg, unsigned long);
                    break;

                case FORMAT_LLONG:
                    value = (uintmax_t)va_arg(ctx->arg, unsigned long long);
                    break;

                case FORMAT_SIZE:
                    value = (uintmax_t)va_arg(ctx->arg, size_t);
                    break;

                case FORMAT_POINTER:
                    value = (uintmax_t)(uintptr_t)va_arg(ctx->arg, void*);
                    break;

                case FORMAT_INTMAX:
                    value = va_arg(ctx->arg, uintmax_t);
                    break;

                default:
                    // puts("UNSUPPORTED PRINTF FLAG COMBINATION");
                    return NULL;
                }

                div.quot = value / ctx->base;
                div.rem = value % ctx->base;
                _print_integer(div, ctx);
            }
            else
            {
                /* Integer conversions (signed) */
                intmax_t value;

                switch (ctx->flags & (FORMAT_CHAR | FORMAT_SHORT | FORMAT_LONG | FORMAT_LLONG | FORMAT_INTMAX))
                {
                case FORMAT_CHAR:
                    value = (intmax_t)(char)va_arg(ctx->arg, int);
                    break;

                case FORMAT_SHORT:
                    value = (intmax_t)(short)va_arg(ctx->arg, int);
                    break;

                case 0:
                    value = (intmax_t)va_arg(ctx->arg, int);
                    break;

                case FORMAT_LONG:
                    value = (intmax_t)va_arg(ctx->arg, long);
                    break;

                case FORMAT_LLONG:
                    value = (intmax_t)va_arg(ctx->arg, long long);
                    break;

                case FORMAT_PTRDIFF:
                    value = (intmax_t)va_arg(ctx->arg, ptrdiff_t);
                    break;

                case FORMAT_INTMAX:
                    value = va_arg(ctx->arg, intmax_t);
                    break;

                default:
                    // puts("UNSUPPORTED PRINTF FLAG COMBINATION");
                    return NULL;
                }

                _print_integer(imaxdiv(value, ctx->base), ctx);
            }
        }

        if (ctx->flags & FORMAT_MINUS)
        {
            /* Left-aligned filling */
            while (ctx->currentChars < ctx->width)
            {
                _PRINT_PUT(ctx, ' ');
                ++(ctx->currentChars);
            }
        }

        if (ctx->totalChars >= ctx->maxChars && ctx->maxChars > 0)
        {
            ctx->buffer[ctx->maxChars - 1] = '\0';
        }
    }

    return ++spec;
}
