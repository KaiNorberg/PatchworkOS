#pragma once

#include <lib-status.h>

#if defined(__cplusplus)
extern "C" {
#endif
 
#include <stdint.h>

int64_t spawn(uint64_t fd);

void exit(uint64_t status);

void sleep(uint64_t milliseconds);

void sys_test(const char* string);
 
#if defined(__cplusplus)
} /* extern "C" */
#endif
