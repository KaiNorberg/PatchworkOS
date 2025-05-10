#ifndef _CTYPE_H
#define _CTYPE_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/config.h"

_PUBLIC int isalnum(int c);

_PUBLIC int isalpha(int c);

_PUBLIC int isblank(int c);

_PUBLIC int iscntrl(int c);

_PUBLIC int isdigit(int c);

_PUBLIC int isgraph(int c);

_PUBLIC int islower(int c);

_PUBLIC int isprint(int c);

_PUBLIC int ispunct(int c);

_PUBLIC int isspace(int c);

_PUBLIC int isupper(int c);

_PUBLIC int isxdigit(int c);

_PUBLIC int tolower(int c);

_PUBLIC int toupper(int c);

#if defined(__cplusplus)
}
#endif

#endif
