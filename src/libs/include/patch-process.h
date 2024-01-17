#ifndef _PATCH_PROCESS_H
#define _PATCH_PROCESS_H 1

#if defined(__cplusplus)
extern "C" {
#endif
 
#include <stdint.h>

uint64_t fork();

void sys_test(const char* string);
 
#if defined(__cplusplus)
} /* extern "C" */
#endif
 
#endif