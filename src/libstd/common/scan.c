#include "scan.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "common/digits.h"
#include "platform/platform.h"

#if _PLATFORM_HAS_IO == 1
#include "platform/user/common/file.h"
#endif

static int _ScanGet(_FormatCtx_t* ctx)
{
    int rc = EOF;

#if _PLATFORM_HAS_IO == 1
    if (ctx->stream != NULL)
    {
        if (_FILE_CHECK_AVAIL(ctx->stream) != ERR)
        {
            rc = _FILE_GETC(ctx->stream);
        }
    }
    else
    {
        rc = (*ctx->buffer == '\0') ? EOF : (unsigned char)*((ctx->buffer)++);
    }
#else
    if (ctx->buffer != NULL)
    {
        rc = (*ctx->buffer == '\0') ? EOF : (unsigned char)*((ctx->buffer)++);
    }
#endif

    if (rc != EOF)
    {
        ++(ctx->totalChars);
        ++(ctx->currentChars);
    }

    return rc;
}

/* Helper function to put a read character back into the string or stream,
   whatever is used for input.
*/
static void _ScanUnget(int c, _FormatCtx_t* ctx)
{
#if _PLATFORM_HAS_IO == 1
    if (ctx->stream != NULL)
    {
        _FileUngetcUnlocked(ctx->stream, c); /* TODO: Error? */
    }
    else
    {
        --(ctx->buffer);
    }
#else
    if (ctx->buffer != NULL)
    {
        --(ctx->buffer);
    }
#endif

    --(ctx->totalChars);
    --(ctx->currentChars);
}

/* Helper function to check if a character is part of a given scanset */
static int _ScanInScanset(const char* scanlist, const char* end_scanlist, int rc)
{
    /* SOLAR */
    int previous = -1;

    while (scanlist != end_scanlist)
    {
        if ((*scanlist == '-') && (previous != -1))
        {
            /* possible scangroup ("a-z") */
            if (++scanlist == end_scanlist)
            {
                /* '-' at end of scanlist does not describe a scangroup */
                return rc == '-';
            }

            while (++previous <= (unsigned char)*scanlist)
            {
                if (previous == rc)
                {
                    return 1;
                }
            }

            previous = -1;
        }
        else
        {
            /* not a scangroup, check verbatim */
            if (rc == (unsigned char)*scanlist)
            {
                return 1;
            }

            previous = (unsigned char)(*scanlist++);
        }
    }

    return 0;
}

const char* _Scan(const char* spec, _FormatCtx_t* ctx)
{
    /* generic input character */
    int rc;
    const char* prev_spec;
    const char* orig_spec = spec;
    int value_parsed;

    if (*(++spec) == '%')
    {
        /* %% -> match single '%' */
        rc = _ScanGet(ctx);

        switch (rc)
        {
        case EOF:

            /* input error */
            if (ctx->maxChars == 0)
            {
                ctx->maxChars = -1;
            }

            return NULL;

        case '%':
            return ++spec;

        default:
            _ScanUnget(rc, ctx);
            break;
        }
    }

    /* Initializing ctx structure */
    ctx->flags = 0;
    ctx->base = -1;
    ctx->currentChars = 0;
    ctx->width = 0;
    ctx->precision = 0;

    /* '*' suppresses assigning parsed value to variable */
    if (*spec == '*')
    {
        ctx->flags |= FORMAT_SUPPRESSED;
        ++spec;
    }

    /* If a width is given, strtol() will return its value. If not given,
       strtol() will return zero. In both cases, endptr will point to the
       rest of the conversion specifier - just what we need.
    */
    prev_spec = spec;
    ctx->width = (int)strtol(spec, (char**)&spec, 10);

    if (spec == prev_spec)
    {
        ctx->width = SIZE_MAX;
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
            /* l -> long */
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

    /* whether valid input had been parsed */
    value_parsed = 0;

    switch (*spec)
    {
    case 'd':
        ctx->base = 10;
        break;

    case 'i':
        ctx->base = 0;
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
        ctx->flags |= FORMAT_UNSIGNED;
        break;

    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
        break;

    case 'c':
    {
        char* c = NULL;

        if (!(ctx->flags & FORMAT_SUPPRESSED))
        {
            c = va_arg(ctx->arg, char*);
        }

        /* for %c, default width is one */
        if (ctx->width == SIZE_MAX)
        {
            ctx->width = 1;
        }

        /* reading until width reached or input exhausted */
        while ((ctx->currentChars < ctx->width) && ((rc = _ScanGet(ctx)) != EOF))
        {
            if (c != NULL)
            {
                *(c++) = rc;
            }

            value_parsed = 1;
        }

        /* width or input exhausted */
        if (value_parsed)
        {
            if (c != NULL)
            {
                ++ctx->maxChars;
            }

            return ++spec;
        }
        else
        {
            /* input error, no character read */
            if (ctx->maxChars == 0)
            {
                ctx->maxChars = -1;
            }

            return NULL;
        }
    }

    case 's':
    {
        char* c = NULL;

        if (!(ctx->flags & FORMAT_SUPPRESSED))
        {
            c = va_arg(ctx->arg, char*);
        }

        while ((ctx->currentChars < ctx->width) && ((rc = _ScanGet(ctx)) != EOF))
        {
            if (isspace((unsigned char)rc))
            {
                _ScanUnget(rc, ctx);

                if (value_parsed)
                {
                    /* matching sequence terminated by whitespace */
                    if (c != NULL)
                    {
                        *c = '\0';
                        ++ctx->maxChars;
                    }

                    return ++spec;
                }
                else
                {
                    /* matching error */
                    return NULL;
                }
            }
            else
            {
                /* match */
                if (c != NULL)
                {
                    *(c++) = rc;
                }

                value_parsed = 1;
            }
        }

        /* width or input exhausted */
        if (value_parsed)
        {
            if (c != NULL)
            {
                *c = '\0';
                ++ctx->maxChars;
            }

            return ++spec;
        }
        else
        {
            /* input error, no character read */
            if (ctx->maxChars == 0)
            {
                ctx->maxChars = -1;
            }

            return NULL;
        }
    }

    case '[':
    {
        const char* endspec = spec;
        int negative_scanlist = 0;
        char* c = NULL;

        if (!(ctx->flags & FORMAT_SUPPRESSED))
        {
            c = va_arg(ctx->arg, char*);
        }

        if (*(++endspec) == '^')
        {
            negative_scanlist = 1;
            ++endspec;
        }

        spec = endspec;

        do
        {
            /* TODO: This can run beyond a malformed format string */
            ++endspec;
        } while (*endspec != ']');

        /* read according to scanlist, equiv. to %buffer above */
        while ((ctx->currentChars < ctx->width) && ((rc = _ScanGet(ctx)) != EOF))
        {
            if (negative_scanlist)
            {
                if (_ScanInScanset(spec, endspec, rc))
                {
                    _ScanUnget(rc, ctx);
                    break;
                }
            }
            else
            {
                if (!_ScanInScanset(spec, endspec, rc))
                {
                    _ScanUnget(rc, ctx);
                    break;
                }
            }

            if (c != NULL)
            {
                *(c++) = rc;
            }

            value_parsed = 1;
        }

        /* width or input exhausted */
        if (value_parsed)
        {
            if (c != NULL)
            {
                *c = '\0';
                ++ctx->maxChars;
            }

            return ++endspec;
        }
        else
        {
            if (ctx->maxChars == 0)
            {
                ctx->maxChars = -1;
            }

            return NULL;
        }
    }

    case 'p':
        ctx->base = 16;
        ctx->flags |= FORMAT_POINTER;
        break;

    case 'n':
    {
        if (!(ctx->flags & FORMAT_SUPPRESSED))
        {
            int* val = va_arg(ctx->arg, int*);
            *val = ctx->totalChars;
        }

        return ++spec;
    }

    default:
        /* No conversion specifier. Bad conversion. */
        return orig_spec;
    }

    if (ctx->base != -1)
    {
        /* integer conversion */
        uintmax_t value = 0; /* absolute value read */
        int prefix_parsed = 0;
        int sign = 0;

        while ((ctx->currentChars < ctx->width) && ((rc = _ScanGet(ctx)) != EOF))
        {
            if (isspace((unsigned char)rc))
            {
                if (sign)
                {
                    /* matching sequence terminated by whitespace */
                    _ScanUnget(rc, ctx);
                    break;
                }
                else
                {
                    /* leading whitespace not counted against width */
                    ctx->currentChars--;
                }
            }
            else
            {
                if (!sign)
                {
                    /* no sign parsed yet */
                    switch (rc)
                    {
                    case '-':
                        sign = -1;
                        break;

                    case '+':
                        sign = 1;
                        break;

                    default:
                        /* not a sign; put back character */
                        sign = 1;
                        _ScanUnget(rc, ctx);
                        break;
                    }
                }
                else
                {
                    if (!prefix_parsed)
                    {
                        /* no prefix (0x... for hex, 0... for octal) parsed yet */
                        prefix_parsed = 1;

                        if (rc != '0')
                        {
                            /* not a prefix; if base not yet set, set to decimal */
                            if (ctx->base == 0)
                            {
                                ctx->base = 10;
                            }

                            _ScanUnget(rc, ctx);
                        }
                        else
                        {
                            /* starts with zero, so it might be a prefix. */
                            /* check what follows next (might be 0x...) */
                            if ((ctx->currentChars < ctx->width) && ((rc = _ScanGet(ctx)) != EOF))
                            {
                                if (tolower((unsigned char)rc) == 'x')
                                {
                                    /* 0x... would be prefix for hex base... */
                                    if ((ctx->base == 0) || (ctx->base == 16))
                                    {
                                        ctx->base = 16;
                                    }
                                    else
                                    {
                                        /* ...unless already set to other value */
                                        _ScanUnget(rc, ctx);
                                        value_parsed = 1;
                                    }
                                }
                                else
                                {
                                    /* 0... but not 0x.... would be octal prefix */
                                    _ScanUnget(rc, ctx);

                                    if (ctx->base == 0)
                                    {
                                        ctx->base = 8;
                                    }

                                    /* in any case we have read a zero */
                                    value_parsed = 1;
                                }
                            }
                            else
                            {
                                /* failed to read beyond the initial zero */
                                value_parsed = 1;
                                break;
                            }
                        }
                    }
                    else
                    {
                        char* digitptr = (char*)memchr(_Digits, tolower((unsigned char)rc), ctx->base);

                        if (digitptr == NULL)
                        {
                            /* end of input item */
                            _ScanUnget(rc, ctx);
                            break;
                        }

                        value *= ctx->base;
                        value += digitptr - _Digits;
                        value_parsed = 1;
                    }
                }
            }
        }

        /* width or input exhausted, or non-matching character */
        if (!value_parsed)
        {
            /* out of input before anything could be parsed - input error */
            /* FIXME: if first character does not match, value_parsed is not set - but it is NOT an input error */
            if ((ctx->maxChars == 0) && (rc == EOF))
            {
                ctx->maxChars = -1;
            }

            return NULL;
        }

        /* convert value to target type and assign to parameter */
        if (!(ctx->flags & FORMAT_SUPPRESSED))
        {
            switch (ctx->flags &
                (FORMAT_CHAR | FORMAT_SHORT | FORMAT_LONG | FORMAT_LLONG | FORMAT_INTMAX | FORMAT_SIZE |
                    FORMAT_PTRDIFF | FORMAT_POINTER | FORMAT_UNSIGNED))
            {
            case FORMAT_CHAR:
                *(va_arg(ctx->arg, char*)) = (char)(value * sign);
                break;

            case FORMAT_CHAR | FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, unsigned char*)) = (unsigned char)(value * sign);
                break;

            case FORMAT_SHORT:
                *(va_arg(ctx->arg, short*)) = (short)(value * sign);
                break;

            case FORMAT_SHORT | FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, unsigned short*)) = (unsigned short)(value * sign);
                break;

            case 0:
                *(va_arg(ctx->arg, int*)) = (int)(value * sign);
                break;

            case FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, unsigned int*)) = (unsigned int)(value * sign);
                break;

            case FORMAT_LONG:
                *(va_arg(ctx->arg, long*)) = (long)(value * sign);
                break;

            case FORMAT_LONG | FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, unsigned long*)) = (unsigned long)(value * sign);
                break;

            case FORMAT_LLONG:
                *(va_arg(ctx->arg, long long*)) = (long long)(value * sign);
                break;

            case FORMAT_LLONG | FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, unsigned long long*)) = (unsigned long long)(value * sign);
                break;

            case FORMAT_INTMAX:
                *(va_arg(ctx->arg, intmax_t*)) = (intmax_t)(value * sign);
                break;

            case FORMAT_INTMAX | FORMAT_UNSIGNED:
                *(va_arg(ctx->arg, uintmax_t*)) = (uintmax_t)(value * sign);
                break;

            case FORMAT_SIZE:
                /* FORMAT_SIZE always implies unsigned */
                *(va_arg(ctx->arg, size_t*)) = (size_t)(value * sign);
                break;

            case FORMAT_PTRDIFF:
                /* FORMAT_PTRDIFF always implies signed */
                *(va_arg(ctx->arg, ptrdiff_t*)) = (ptrdiff_t)(value * sign);
                break;

            case FORMAT_POINTER:
                /* FORMAT_POINTER always implies unsigned */
                *(uintptr_t*)(va_arg(ctx->arg, void*)) = (uintptr_t)(value * sign);
                break;

            default:
                return NULL; /* behaviour unspecified */
            }

            ++(ctx->maxChars);
        }

        return ++spec;
    }

    /* TODO: Floats. */
    return NULL;
}