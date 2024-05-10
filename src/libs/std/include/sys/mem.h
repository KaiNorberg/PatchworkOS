#ifndef _SYS_MEM_H
#define _SYS_MEM_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/fd_t.h"

#define PROT_NONE 0
#define PROT_READ (1 << 0)
#define PROT_WRITE (1 << 1)

_PUBLIC void* mmap(fd_t fd, void* address, uint64_t length, uint8_t prot);

_PUBLIC uint64_t munmap(void* address, uint64_t length);

_PUBLIC uint64_t mprotect(void* address, uint64_t length, uint8_t prot);

#if defined(__cplusplus)
}
#endif

#endif