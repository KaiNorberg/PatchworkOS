#ifndef _ERRNO_H
#define _ERRNO_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/config.h"

int* _ErrnoFunc(void);
#define errno (*_ErrnoFunc())

#define EDOM 1         // Math argument out of domain
#define ERANGE 2       // Math result not representable
#define EILSEQ 3       // Illegal byte sequence
#define EIMPL 4        // Not implemented
#define EFAULT 5       // Bad address
#define EEXIST 6       // Already exists
#define ELETTER 7      // Invalid letter
#define EPATH 8        // Invalid path
#define EMFILE 9       // To many open files
#define EBADF 10       // Bad file descriptor
#define EACCES 11      // Permission denied
#define EEXEC 12       // Bad executable
#define ENOMEM 13      // Out of memory
#define EREQ 14        // Bad request
#define EFLAGS 15      // Bad flag/flags
#define EINVAL 16      // Invalid argument
#define EBUFFER 17     // Bad buffer
#define ENOTDIR 18     // Not a directory
#define EISDIR 19      // Is a directory
#define ENOOBJ 20      // No such object
#define EPIPE 21       // Broken pipe
#define EAGAIN 22      // Try again
#define ENOTSUP 23     // Operation not supported
#define EWOULDBLOCK 24 // Would block
#define EBUSY 25       // Busy
#define EINTR 26       // Interrupted system call
#define ENOSPC 27      // No space left on device
#define EXDEV 28       // Cross-device link

#define ERROR_MAX 29

#if _USE_ANNEX_K == 1
#include "_AUX/errno_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif
