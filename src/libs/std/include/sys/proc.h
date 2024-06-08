#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include "../_AUX/config.h"
#include "../_AUX/ERR.h"
#include "../_AUX/fd_t.h"

#define PAGE_SIZE 0x1000
#define SIZE_IN_PAGES(size) (((size) + PAGE_SIZE - 1) / PAGE_SIZE)
#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

#define PROT_NONE 0
#define PROT_READ (1 << 0)
#define PROT_WRITE (1 << 1)

typedef uint64_t pid_t;
typedef uint64_t tid_t;
typedef uint64_t prot_t;
typedef uint64_t nsec_t;

//Nanoseconds per second.
#define SEC ((nsec_t)1000000000)

nsec_t uptime(void);

uint64_t sleep(nsec_t nanoseconds);

pid_t spawn(const char* path);

pid_t getpid(void);

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot);

uint64_t munmap(void* address, uint64_t length);

uint64_t mprotect(void* address, uint64_t length, prot_t prot);

#if defined(__cplusplus)
}
#endif

#endif