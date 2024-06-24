#ifndef _AUX_CONFIG_H
#define _AUX_CONFIG_H 1

#if defined(__cplusplus) || !defined(__STDC_VERSION) || __STDC_VERSION__ < 199901L
#define _RESTRICT
#else
#define _RESTRICT restrict
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
#define _NORETURN [[noreturn]]
#else
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define _NORETURN _Noreturn
#else
#define _NORETURN
#endif
#endif

#define ALIGNED(alignment) __attribute__((aligned(alignment)))

#endif
