#ifndef _CTYPE_H
#define _CTYPE_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/ascii.h"
#include "_internal/config.h"

#define isalnum(c) ((int)(_asciiTable[(int)(c)].flags & (_ASCII_ALPHA | _ASCII_DIGIT)))

#define isalpha(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_ALPHA))

#define isblank(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_BLANK))

#define iscntrl(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_CNTRL))

#define isdigit(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_DIGIT))

#define isgraph(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_GRAPH))

#define islower(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_LOWER))

#define isprint(c) ((int)!(_asciiTable[(int)(c)].flags & _ASCII_CNTRL))

#define ispunct(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_PUNCT))

#define isspace(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_SPACE))

#define isupper(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_UPPER))

#define isxdigit(c) ((int)(_asciiTable[(int)(c)].flags & _ASCII_XDIGIT))

#define tolower(c) ((int)(_asciiTable[(int)(c)].lower))

#define toupper(c) ((int)(_asciiTable[(int)(c)].upper))

#if defined(__cplusplus)
}
#endif

#endif
