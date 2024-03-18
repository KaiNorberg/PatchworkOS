#ifndef _INTERNAL_RESTRICT_H
#define _INTERNAL_RESTRICT_H 1

//Check if keywords are supported
#if defined(__cplusplus) || !defined(__STDC_VERSION) || __STDC_VERSION__ < 199901L
#define __RESTRICT
#else
#define __RESTRICT restrict
#endif
 
#endif