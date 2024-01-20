#pragma once

#if defined(__cplusplus)
extern "C" {
#endif
 
#include <stdint.h>

uint64_t fork();

uint64_t sleep(uint64_t milliseconds);

uint64_t sys_test(const char* string);
 
#if defined(__cplusplus)
} /* extern "C" */
#endif
