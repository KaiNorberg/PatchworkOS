#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <stdint.h>
#include <sys/atomint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/NULL.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"
#include "_AUX/pid_t.h"
#include "_AUX/tid_t.h"
#include "_AUX/clock_t.h"

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

typedef enum
{
    FUTEX_WAIT,
    FUTEX_WAKE,
    FUTEX_TRYLOCK
} futex_op_t;

#define FUTEX_ALL UINT64_MAX

#define FUTEX_UNLOCKED 0
#define FUTEX_LOCKED 1
#define FUTEX_CONTESTED 2

// argv[0] = executable
pid_t spawn(const char** argv, const spawn_fd_t* fds);

pid_t process_id(void);

tid_t thread_id(void);

void* mmap(fd_t fd, void* address, uint64_t length, prot_t prot);

uint64_t munmap(void* address, uint64_t length);

uint64_t mprotect(void* address, uint64_t length, prot_t prot);

// if op == FUTEX_WAIT then wait until *addr != val.
// if op == FUTEX_WAKE then val = amount of threads to wake.
uint64_t futex(atomic_uint64* addr, uint64_t val, futex_op_t op, clock_t timeout);

clock_t uptime(void);

#if defined(__cplusplus)
}
#endif

#endif
