#pragma once

#if defined(__cplusplus)
extern "C" {
#endif
 
#include <stdint.h>

uint64_t exit(uint64_t status);

uint64_t spawn(const char* path);

uint64_t sleep(uint64_t milliseconds);

uint64_t sys_test(const char* string);
 
#if defined(__cplusplus)
} /* extern "C" */
#endif
