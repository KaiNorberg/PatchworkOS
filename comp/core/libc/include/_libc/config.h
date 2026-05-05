#ifndef _INTERNAL_CONFIG_H
#define _INTERNAL_CONFIG_H 1

#if (__STDC_WANT_LIB_EXT1__ + 0) != 0
#define _USE_ANNEX_K 1
#else
#define _USE_ANNEX_K 0
#endif

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

#if defined(__cplusplus) || !defined(__STDC_VERSION) || __STDC_VERSION__ < 199901L
#define _RESTRICT
#define _INLINE
#else
#define _RESTRICT restrict
#define _INLINE inline
#endif

#endif