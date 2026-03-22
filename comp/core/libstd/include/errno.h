#ifndef _ERRNO_H
#define _ERRNO_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/config.h"

int* _errno_get(void);

/**
 * @brief Error Numbers.
 * @defgroup libstd_errno Error Numbers
 * @ingroup libstd
 *
 * The errno values in Patchwork are taken from POSIX.
 *
 * @{
 */

#ifndef _KERNEL_
#ifndef _KERNEL_MODULE_
/**
 * @brief Error number variable.
 */
#define errno (*_errno_get())
#endif
#endif

/**
 * @brief No error
 */
#define EOK 0

/**
 * @brief Operation not permitted
 */
#define EPERM 1

/**
 * @brief No such file or directory
 */
#define ENOENT 2

/**
 * @brief No such process
 */
#define ESRCH 3

/**
 * @brief Interrupted system call
 */
#define EINTR 4

/**
 * @brief I/O error
 */
#define EIO 5

/**
 * @brief No such device or address
 */
#define ENXIO 6

/**
 * @brief Argument list too long
 */
#define E2BIG 7

/**
 * @brief Exec format error
 */
#define ENOEXEC 8

/**
 * @brief Bad file number
 */
#define EBADF 9

/**
 * @brief No child processes
 */
#define ECHILD 10

/**
 * @brief Try again
 */
#define EAGAIN 11

/**
 * @brief Out of memory
 */
#define ENOMEM 12

/**
 * @brief Permission denied
 */
#define EACCES 13

/**
 * @brief Bad address
 */
#define EFAULT 14

/**
 * @brief Block device required
 */
#define ENOTBLK 15

/**
 * @brief Device or resource busy
 */
#define EBUSY 16

/**
 * @brief File exists
 */
#define EEXIST 17

/**
 * @brief Cross-device link
 */
#define EXDEV 18

/**
 * @brief No such device
 */
#define ENODEV 19

/**
 * @brief Not a directory
 */
#define ENOTDIR 20

/**
 * @brief Is a directory
 */
#define EISDIR 21

/**
 * @brief Invalid argument
 */
#define EINVAL 22

/**
 * @brief File table overflow
 */
#define ENFILE 23

/**
 * @brief Too many open files
 */
#define EMFILE 24

/**
 * @brief Not a typewriter
 */
#define ENOTTY 25

/**
 * @brief Text file busy
 */
#define ETXTBSY 26

/**
 * @brief File too large
 */
#define EFBIG 27

/**
 * @brief No space left on device
 */
#define ENOSPC 28

/**
 * @brief Illegal seek
 */
#define ESPIPE 29

/**
 * @brief Read-only file system
 */
#define EROFS 30

/**
 * @brief Too many links
 */
#define EMLINK 31

/**
 * @brief Broken pipe
 */
#define EPIPE 32

/**
 * @brief Math argument out of domain of func
 */
#define EDOM 33

/**
 * @brief Math result not representable
 */
#define ERANGE 34

/**
 * @brief Resource deadlock would occur
 */
#define EDEADLK 35

/**
 * @brief File name too long
 */
#define ENAMETOOLONG 36

/**
 * @brief No record locks available
 */
#define ENOLCK 37

/**
 * @brief Function not implemented
 */
#define ENOSYS 38

/**
 * @brief Directory not empty
 */
#define ENOTEMPTY 39

/**
 * @brief Too many symbolic links encountered
 */
#define ELOOP 40

/**
 * @brief Operation would block
 */
#define EWOULDBLOCK EAGAIN

/**
 * @brief No message of desired type
 */
#define ENOMSG 42

/**
 * @brief Identifier removed
 */
#define EIDRM 43

/**
 * @brief Channel number out of range
 */
#define ECHRNG 44

/**
 * @brief Level 2 not synchronized
 */
#define EL2NSYNC 45

/**
 * @brief Level 3 halted
 */
#define EL3HLT 46

/**
 * @brief Level 3 reset
 */
#define EL3RST 47

/**
 * @brief Link number out of range
 */
#define ELNRNG 48

/**
 * @brief Protocol driver not attached
 */
#define EUNATCH 49

/**
 * @brief No CSI structure available
 */
#define ENOCSI 50

/**
 * @brief Level 2 halted
 */
#define EL2HLT 51

/**
 * @brief Invalid exchange
 */
#define EBADE 52

/**
 * @brief Invalid request descriptor
 */
#define EBADR 53

/**
 * @brief Exchange full
 */
#define EXFULL 54

/**
 * @brief No anode
 */
#define ENOANO 55

/**
 * @brief Invalid request code
 */
#define EBADRQC 56

/**
 * @brief Invalid slot
 */
#define EBADSLT 57

/**
 * @brief Bad font file format
 */
#define EBFONT 59

/**
 * @brief Device not a stream
 */
#define ENOSTR 60

/**
 * @brief No data available
 */
#define ENODATA 61

/**
 * @brief Timer expired
 */
#define ETIME 62

/**
 * @brief Out of streams resources
 */
#define ENOSR 63

/**
 * @brief Machine is not on the network
 */
#define ENONET 64

/**
 * @brief Package not installed
 */
#define ENOPKG 65

/**
 * @brief Object is remote
 */
#define EREMOTE 66

/**
 * @brief Link has been severed
 */
#define ENOLINK 67

/**
 * @brief Advertise error
 */
#define EADV 68

/**
 * @brief Srmount error
 */
#define ESRMNT 69

/**
 * @brief Communication error on send
 */
#define ECOMM 70

/**
 * @brief Protocol error
 */
#define EPROTO 71

/**
 * @brief Multihop attempted
 */
#define EMULTIHOP 72

/**
 * @brief RFS specific error
 */
#define EDOTDOT 73

/**
 * @brief Not a data message
 */
#define EBADMSG 74

/**
 * @brief Value too large for defined data type
 */
#define EOVERFLOW 75

/**
 * @brief Name not unique on network
 */
#define ENOTUNIQ 76

/**
 * @brief File descriptor in bad state
 */
#define EBADFD 77

/**
 * @brief Remote address changed
 */
#define EREMCHG 78

/**
 * @brief Can not access a needed shared library
 */
#define ELIBACC 79

/**
 * @brief Accessing a corrupted shared library
 */
#define ELIBBAD 80

/**
 * @brief .lib section in a.out corrupted
 */
#define ELIBSCN 81

/**
 * @brief Attempting to link in too many shared libraries
 */
#define ELIBMAX 82

/**
 * @brief Cannot exec a shared library directly
 */
#define ELIBEXEC 83

/**
 * @brief Illegal byte sequence
 */
#define EILSEQ 84

/**
 * @brief Interrupted system call should be restarted
 */
#define ERESTART 85

/**
 * @brief Streams pipe error
 */
#define ESTRPIPE 86

/**
 * @brief Too many users
 */
#define EUSERS 87

/**
 * @brief Socket operation on non-socket
 */
#define ENOTSOCK 88

/**
 * @brief Destination address required
 */
#define EDESTADDRREQ 89

/**
 * @brief Message too long
 */
#define EMSGSIZE 90

/**
 * @brief Protocol wrong type for socket
 */
#define EPROTOTYPE 91

/**
 * @brief Protocol not available
 */
#define ENOPROTOOPT 92

/**
 * @brief Protocol not supported
 */
#define EPROTONOSUPPORT 93

/**
 * @brief Socket type not supported
 */
#define ESOCKTNOSUPPORT 94

/**
 * @brief Operation not supported on transport endpoint
 */
#define EOPNOTSUPP 95

/**
 * @brief Protocol family not supported
 */
#define EPFNOSUPPORT 96

/**
 * @brief Address family not supported by protocol
 */
#define EAFNOSUPPORT 97

/**
 * @brief Address already in use
 */
#define EADDRINUSE 98

/**
 * @brief Cannot assign requested address
 */
#define EADDRNOTAVAIL 99

/**
 * @brief Network is down
 */
#define ENETDOWN 100

/**
 * @brief Network is unreachable
 */
#define ENETUNREACH 101

/**
 * @brief Network dropped connection because of reset
 */
#define ENETRESET 102

/**
 * @brief Software caused connection abort
 */
#define ECONNABORTED 103

/**
 * @brief Connection reset by peer
 */
#define ECONNRESET 104

/**
 * @brief No buffer space available
 */
#define ENOBUFS 105

/**
 * @brief Transport endpoint is already connected
 */
#define EISCONN 106

/**
 * @brief Transport endpoint is not connected
 */
#define ENOTCONN 107

/**
 * @brief Cannot send after transport endpoint shutdown
 */
#define ESHUTDOWN 108

/**
 * @brief Too many references: cannot splice
 */
#define ETOOMANYREFS 109

/**
 * @brief Connection timed out
 */
#define ETIMEDOUT 110

/**
 * @brief Connection refused
 */
#define ECONNREFUSED 111

/**
 * @brief Host is down
 */
#define EHOSTDOWN 112

/**
 * @brief No route to host
 */
#define EHOSTUNREACH 113

/**
 * @brief Operation already in progress
 */
#define EALREADY 114

/**
 * @brief Operation now in progress
 */
#define EINPROGRESS 115

/**
 * @brief Stale NFS file handle
 */
#define ESTALE 116

/**
 * @brief Structure needs cleaning
 */
#define EUCLEAN 117

/**
 * @brief Not a XENIX named type file
 */
#define ENOTNAM 118

/**
 * @brief No XENIX semaphores available
 */
#define ENAVAIL 119

/**
 * @brief Is a named type file
 */
#define EISNAM 120

/**
 * @brief Remote I/O error
 */
#define EREMOTEIO 121

/**
 * @brief Quota exceeded
 */
#define EDQUOT 122

/**
 * @brief No medium found
 */
#define ENOMEDIUM 123

/**
 * @brief Wrong medium type
 */
#define EMEDIUMTYPE 124

/**
 * @brief Operation Canceled
 */
#define ECANCELED 125

/**
 * @brief Required key not available
 */
#define ENOKEY 126

/**
 * @brief Key has expired
 */
#define EKEYEXPIRED 127

/**
 * @brief Key has been revoked
 */
#define EKEYREVOKED 128

/**
 * @brief Key was rejected by service
 */
#define EKEYREJECTED 129

/**
 * @brief Owner died
 */
#define EOWNERDEAD 130

/**
 * @brief State not recoverable
 */
#define ENOTRECOVERABLE 131

/**
 * @brief Operation not possible due to RF-kill
 */
#define ERFKILL 132

/**
 * @brief Maximum value for all error codes (not inclusive)
 */
#define EMAX 133

/** @} */

#if _USE_ANNEX_K == 1
#include "_libstd/errno_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif
