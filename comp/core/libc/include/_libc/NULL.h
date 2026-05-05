#ifndef _INTERNAL_NULL_H
#define _INTERNAL_NULL_H 1

#ifndef NULL

#ifdef __cplusplus
#if __cplusplus >= 201103L
#define _NULL nullptr
#else
#define _NULL 0
#endif
#else
#define _NULL ((void*)0)
#endif

#define NULL _NULL

#endif

#endif
