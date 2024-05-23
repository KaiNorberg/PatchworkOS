#ifndef _CTYPE_H
#define _CTYPE_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#include "_AUX/config.h"

#define isalnum(ch) (isdigit((ch)) || isalpha((ch)))
#define isalpha(ch) (((ch) >= 'A' && (ch) <= 'Z') || ((ch) >= 'a' && (ch) <= 'z'))
#define isdigit(ch) (((ch) >= '0' && (ch) <= '9'))

int tolower(int ch);

#if defined(__cplusplus)
}
#endif

#endif