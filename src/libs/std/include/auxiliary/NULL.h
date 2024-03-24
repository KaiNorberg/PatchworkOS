#ifndef _AUX_NULL_H
#define _AUX_NULL_H 1

#ifdef __cplusplus
#if __cplusplus >= 201103L
#define NULL nullptr
#else
#define NULL 0
#endif
#else
#define NULL ((void*)0)
#endif

#endif