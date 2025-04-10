#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/nsec_t.h"
#include "_AUX/pid_t.h"
#include "_AUX/tid_t.h"

#define PAGE_SIZE 0x1000
#define SIZE_IN_PAGES(size) (((size) + PAGE_SIZE - 1) / PAGE_SIZE)
#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

typedef enum prot
{
    PROT_NONE = 0,
    PROT_READ = (1 << 0),
    PROT_WRITE = (1 << 1)
} prot_t;

typedef struct
{
    fd_t child;
    fd_t parent;
} spawn_fd_t;

#define SPAWN_FD_END \
    (spawn_fd_t) \
    { \
        .child = FD_NONE, .parent = FD_NONE \
    }

// Nanoseconds per second.
#define SEC ((nsec_t)1000000000)

#define NEVER ((nsec_t)UINT64_MAX)

nsec_t uptime(void);

uint64_t sleep(nsec_t nanoseconds);

// argv[0] = executable
pid_t spawn(const char** argv, const spawn_fd_t* fds);

pid_t getpid(void);

tid_t gettid(void);

fd_t procfd(pid_t pid, const char* file);

tid_t split(void* entry, uint64_t argc, ...);

_NORETURN void thread_exit(void);

void yield(void);

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot);

uint64_t munmap(void* address, uint64_t length);

uint64_t mprotect(void* address, uint64_t length, prot_t prot);

#if defined(__cplusplus)
}
#endif

#endif
