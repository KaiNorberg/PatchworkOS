#ifndef _AUX_CONFIG_H
#define _AUX_CONFIG_H 1

#ifndef _IS_STATIC
#define _IS_STATIC 1
#endif

///////////////////////////////////////////////

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

#ifdef _IS_STATIC
#define _PUBLIC
#define _LOCAL
#else
#if defined _WIN32 || defined __CYGWIN__
#ifdef _BUILD
#ifdef __GNUC__
#define _PUBLIC __attribute__((dllexport))
#else
#define _PUBLIC __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define _PUBLIC __attribute__((dllimport))
#else
#define _PUBLIC __declspec(dllimport)
#endif
#endif
#define _LOCAL
#else
#if __GNUC__ >= 4
#define _PUBLIC __attribute__((visibility("default")))
#define _LOCAL __attribute__((visibility("hidden")))
#else
#define _PUBLIC
#define _LOCAL
#endif
#endif

#endif

#endif
