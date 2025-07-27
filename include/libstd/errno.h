#ifndef _ERRNO_H
#define _ERRNO_H 1

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/ERR.h"
#include "_internal/config.h"

/**
 * @brief Error values.
 * @ingroup libstd
 * @defgroup libstd_errno Errno
 *
 * The errno values in Patchwork are taken from POSIX.
 *
 */

/**
 * @brief Retrieves pointer to per thread errno value.
 *
 * @return int*
 */
int* _errno_get(void);
#define errno (*_errno_get())

/**
 * @brief Operation not permitted
 * @ingroup libstd_errno
 */
#define EPERM 1

/**
 * @brief No such file or directory
 * @ingroup libstd_errno
 */
#define ENOENT 2

/**
 * @brief No such process
 * @ingroup libstd_errno
 */
#define ESRCH 3

/**
 * @brief Interrupted system call
 * @ingroup libstd_errno
 */
#define EINTR 4

/**
 * @brief I/O error
 * @ingroup libstd_errno
 */
#define EIO 5

/**
 * @brief No such device or address
 * @ingroup libstd_errno
 */
#define ENXIO 6

/**
 * @brief Argument list too long
 * @ingroup libstd_errno
 */
#define E2BIG 7

/**
 * @brief Exec format error
 * @ingroup libstd_errno
 */
#define ENOEXEC 8

/**
 * @brief Bad file number
 * @ingroup libstd_errno
 */
#define EBADF 9

/**
 * @brief No child processes
 * @ingroup libstd_errno
 */
#define ECHILD 10

/**
 * @brief Try again
 * @ingroup libstd_errno
 */
#define EAGAIN 11

/**
 * @brief Out of memory
 * @ingroup libstd_errno
 */
#define ENOMEM 12

/**
 * @brief Permission denied
 * @ingroup libstd_errno
 */
#define EACCES 13

/**
 * @brief Bad address
 * @ingroup libstd_errno
 */
#define EFAULT 14

/**
 * @brief Block device required
 * @ingroup libstd_errno
 */
#define ENOTBLK 15

/**
 * @brief Device or resource busy
 * @ingroup libstd_errno
 */
#define EBUSY 16

/**
 * @brief File exists
 * @ingroup libstd_errno
 */
#define EEXIST 17

/**
 * @brief Cross-device link
 * @ingroup libstd_errno
 */
#define EXDEV 18

/**
 * @brief No such device
 * @ingroup libstd_errno
 */
#define ENODEV 19

/**
 * @brief Not a directory
 * @ingroup libstd_errno
 */
#define ENOTDIR 20

/**
 * @brief Is a directory
 * @ingroup libstd_errno
 */
#define EISDIR 21

/**
 * @brief Invalid argument
 * @ingroup libstd_errno
 */
#define EINVAL 22

/**
 * @brief File table overflow
 * @ingroup libstd_errno
 */
#define ENFILE 23

/**
 * @brief Too many open files
 * @ingroup libstd_errno
 */
#define EMFILE 24

/**
 * @brief Not a typewriter
 * @ingroup libstd_errno
 */
#define ENOTTY 25

/**
 * @brief Text file busy
 * @ingroup libstd_errno
 */
#define ETXTBSY 26

/**
 * @brief File too large
 * @ingroup libstd_errno
 */
#define EFBIG 27

/**
 * @brief No space left on device
 * @ingroup libstd_errno
 */
#define ENOSPC 28

/**
 * @brief Illegal seek
 * @ingroup libstd_errno
 */
#define ESPIPE 29

/**
 * @brief Read-only file system
 * @ingroup libstd_errno
 */
#define EROFS 30

/**
 * @brief Too many links
 * @ingroup libstd_errno
 */
#define EMLINK 31

/**
 * @brief Broken pipe
 * @ingroup libstd_errno
 */
#define EPIPE 32

/**
 * @brief Math argument out of domain of func
 * @ingroup libstd_errno
 */
#define EDOM 33

/**
 * @brief Math result not representable
 * @ingroup libstd_errno
 */
#define ERANGE 34

/**
 * @brief Resource deadlock would occur
 * @ingroup libstd_errno
 */
#define EDEADLK 35

/**
 * @brief File name too long
 * @ingroup libstd_errno
 */
#define ENAMETOOLONG 36

/**
 * @brief No record locks available
 * @ingroup libstd_errno
 */
#define ENOLCK 37

/**
 * @brief Function not implemented
 * @ingroup libstd_errno
 */
#define ENOSYS 38

/**
 * @brief Directory not empty
 * @ingroup libstd_errno
 */
#define ENOTEMPTY 39

/**
 * @brief Too many symbolic links encountered
 * @ingroup libstd_errno
 */
#define ELOOP 40

/**
 * @brief Operation would block
 * @ingroup libstd_errno
 */
#define EWOULDBLOCK EAGAIN

/**
 * @brief No message of desired type
 * @ingroup libstd_errno
 */
#define ENOMSG 42

/**
 * @brief Identifier removed
 * @ingroup libstd_errno
 */
#define EIDRM 43

/**
 * @brief Channel number out of range
 * @ingroup libstd_errno
 */
#define ECHRNG 44

/**
 * @brief Level 2 not synchronized
 * @ingroup libstd_errno
 */
#define EL2NSYNC 45

/**
 * @brief Level 3 halted
 * @ingroup libstd_errno
 */
#define EL3HLT 46

/**
 * @brief Level 3 reset
 * @ingroup libstd_errno
 */
#define EL3RST 47

/**
 * @brief Link number out of range
 * @ingroup libstd_errno
 */
#define ELNRNG 48

/**
 * @brief Protocol driver not attached
 * @ingroup libstd_errno
 */
#define EUNATCH 49

/**
 * @brief No CSI structure available
 * @ingroup libstd_errno
 */
#define ENOCSI 50

/**
 * @brief Level 2 halted
 * @ingroup libstd_errno
 */
#define EL2HLT 51

/**
 * @brief Invalid exchange
 * @ingroup libstd_errno
 */
#define EBADE 52

/**
 * @brief Invalid request descriptor
 * @ingroup libstd_errno
 */
#define EBADR 53

/**
 * @brief Exchange full
 * @ingroup libstd_errno
 */
#define EXFULL 54

/**
 * @brief No anode
 * @ingroup libstd_errno
 */
#define ENOANO 55

/**
 * @brief Invalid request code
 * @ingroup libstd_errno
 */
#define EBADRQC 56

/**
 * @brief Invalid slot
 * @ingroup libstd_errno
 */
#define EBADSLT 57

/**
 * @brief Bad font file format
 * @ingroup libstd_errno
 */
#define EBFONT 59

/**
 * @brief Device not a stream
 * @ingroup libstd_errno
 */
#define ENOSTR 60

/**
 * @brief No data available
 * @ingroup libstd_errno
 */
#define ENODATA 61

/**
 * @brief Timer expired
 * @ingroup libstd_errno
 */
#define ETIME 62

/**
 * @brief Out of streams resources
 * @ingroup libstd_errno
 */
#define ENOSR 63

/**
 * @brief Machine is not on the network
 * @ingroup libstd_errno
 */
#define ENONET 64

/**
 * @brief Package not installed
 * @ingroup libstd_errno
 */
#define ENOPKG 65

/**
 * @brief Object is remote
 * @ingroup libstd_errno
 */
#define EREMOTE 66

/**
 * @brief Link has been severed
 * @ingroup libstd_errno
 */
#define ENOLINK 67

/**
 * @brief Advertise error
 * @ingroup libstd_errno
 */
#define EADV 68

/**
 * @brief Srmount error
 * @ingroup libstd_errno
 */
#define ESRMNT 69

/**
 * @brief Communication error on send
 * @ingroup libstd_errno
 */
#define ECOMM 70

/**
 * @brief Protocol error
 * @ingroup libstd_errno
 */
#define EPROTO 71

/**
 * @brief Multihop attempted
 * @ingroup libstd_errno
 */
#define EMULTIHOP 72

/**
 * @brief RFS specific error
 * @ingroup libstd_errno
 */
#define EDOTDOT 73

/**
 * @brief Not a data message
 * @ingroup libstd_errno
 */
#define EBADMSG 74

/**
 * @brief Value too large for defined data type
 * @ingroup libstd_errno
 */
#define EOVERFLOW 75

/**
 * @brief Name not unique on network
 * @ingroup libstd_errno
 */
#define ENOTUNIQ 76

/**
 * @brief File descriptor in bad state
 * @ingroup libstd_errno
 */
#define EBADFD 77

/**
 * @brief Remote address changed
 * @ingroup libstd_errno
 */
#define EREMCHG 78

/**
 * @brief Can not access a needed shared library
 * @ingroup libstd_errno
 */
#define ELIBACC 79

/**
 * @brief Accessing a corrupted shared library
 * @ingroup libstd_errno
 */
#define ELIBBAD 80

/**
 * @brief .lib section in a.out corrupted
 * @ingroup libstd_errno
 */
#define ELIBSCN 81

/**
 * @brief Attempting to link in too many shared libraries
 * @ingroup libstd_errno
 */
#define ELIBMAX 82

/**
 * @brief Cannot exec a shared library directly
 * @ingroup libstd_errno
 */
#define ELIBEXEC 83

/**
 * @brief Illegal byte sequence
 * @ingroup libstd_errno
 */
#define EILSEQ 84

/**
 * @brief Interrupted system call should be restarted
 * @ingroup libstd_errno
 */
#define ERESTART 85

/**
 * @brief Streams pipe error
 * @ingroup libstd_errno
 */
#define ESTRPIPE 86

/**
 * @brief Too many users
 * @ingroup libstd_errno
 */
#define EUSERS 87

/**
 * @brief Socket operation on non-socket
 * @ingroup libstd_errno
 */
#define ENOTSOCK 88

/**
 * @brief Destination address required
 * @ingroup libstd_errno
 */
#define EDESTADDRREQ 89

/**
 * @brief Message too long
 * @ingroup libstd_errno
 */
#define EMSGSIZE 90

/**
 * @brief Protocol wrong type for socket
 * @ingroup libstd_errno
 */
#define EPROTOTYPE 91

/**
 * @brief Protocol not available
 * @ingroup libstd_errno
 */
#define ENOPROTOOPT 92

/**
 * @brief Protocol not supported
 * @ingroup libstd_errno
 */
#define EPROTONOSUPPORT 93

/**
 * @brief Socket type not supported
 * @ingroup libstd_errno
 */
#define ESOCKTNOSUPPORT 94

/**
 * @brief Operation not supported on transport endpoint
 * @ingroup libstd_errno
 */
#define EOPNOTSUPP 95

/**
 * @brief Protocol family not supported
 * @ingroup libstd_errno
 */
#define EPFNOSUPPORT 96

/**
 * @brief Address family not supported by protocol
 * @ingroup libstd_errno
 */
#define EAFNOSUPPORT 97

/**
 * @brief Address already in use
 * @ingroup libstd_errno
 */
#define EADDRINUSE 98

/**
 * @brief Cannot assign requested address
 * @ingroup libstd_errno
 */
#define EADDRNOTAVAIL 99

/**
 * @brief Network is down
 * @ingroup libstd_errno
 */
#define ENETDOWN 100

/**
 * @brief Network is unreachable
 * @ingroup libstd_errno
 */
#define ENETUNREACH 101

/**
 * @brief Network dropped connection because of reset
 * @ingroup libstd_errno
 */
#define ENETRESET 102

/**
 * @brief Software caused connection abort
 * @ingroup libstd_errno
 */
#define ECONNABORTED 103

/**
 * @brief Connection reset by peer
 * @ingroup libstd_errno
 */
#define ECONNRESET 104

/**
 * @brief No buffer space available
 * @ingroup libstd_errno
 */
#define ENOBUFS 105

/**
 * @brief Transport endpoint is already connected
 * @ingroup libstd_errno
 */
#define EISCONN 106

/**
 * @brief Transport endpoint is not connected
 * @ingroup libstd_errno
 */
#define ENOTCONN 107

/**
 * @brief Cannot send after transport endpoint shutdown
 * @ingroup libstd_errno
 */
#define ESHUTDOWN 108

/**
 * @brief Too many references: cannot splice
 * @ingroup libstd_errno
 */
#define ETOOMANYREFS 109

/**
 * @brief Connection timed out
 * @ingroup libstd_errno
 */
#define ETIMEDOUT 110

/**
 * @brief Connection refused
 * @ingroup libstd_errno
 */
#define ECONNREFUSED 111

/**
 * @brief Host is down
 * @ingroup libstd_errno
 */
#define EHOSTDOWN 112

/**
 * @brief No route to host
 * @ingroup libstd_errno
 */
#define EHOSTUNREACH 113

/**
 * @brief Operation already in progress
 * @ingroup libstd_errno
 */
#define EALREADY 114

/**
 * @brief Operation now in progress
 * @ingroup libstd_errno
 */
#define EINPROGRESS 115

/**
 * @brief Stale NFS file handle
 * @ingroup libstd_errno
 */
#define ESTALE 116

/**
 * @brief Structure needs cleaning
 * @ingroup libstd_errno
 */
#define EUCLEAN 117

/**
 * @brief Not a XENIX named type file
 * @ingroup libstd_errno
 */
#define ENOTNAM 118

/**
 * @brief No XENIX semaphores available
 * @ingroup libstd_errno
 */
#define ENAVAIL 119

/**
 * @brief Is a named type file
 * @ingroup libstd_errno
 */
#define EISNAM 120

/**
 * @brief Remote I/O error
 * @ingroup libstd_errno
 */
#define EREMOTEIO 121

/**
 * @brief Quota exceeded
 * @ingroup libstd_errno
 */
#define EDQUOT 122

/**
 * @brief No medium found
 * @ingroup libstd_errno
 */
#define ENOMEDIUM 123

/**
 * @brief Wrong medium type
 * @ingroup libstd_errno
 */
#define EMEDIUMTYPE 124

/**
 * @brief Operation Canceled
 * @ingroup libstd_errno
 */
#define ECANCELED 125

/**
 * @brief Required key not available
 * @ingroup libstd_errno
 */
#define ENOKEY 126

/**
 * @brief Key has expired
 * @ingroup libstd_errno
 */
#define EKEYEXPIRED 127

/**
 * @brief Key has been revoked
 * @ingroup libstd_errno
 */
#define EKEYREVOKED 128

/**
 * @brief Key was rejected by service
 * @ingroup libstd_errno
 */
#define EKEYREJECTED 129

/**
 * @brief Owner died
 * @ingroup libstd_errno
 */
#define EOWNERDEAD 130

/**
 * @brief State not recoverable
 * @ingroup libstd_errno
 */
#define ENOTRECOVERABLE 131

/**
 * @brief Operation not possible due to RF-kill
 * @ingroup libstd_errno
 */
#define ERFKILL 132

/**
 * @brief Maximum value for posix error codes (not inclusive)
 * @ingroup libstd_errno
 */
#define ERR_POSIX_MAX 133

/**
 * @brief Invalid or unknown control request
 * @ingroup libstd_errno
 */
#define EUNKNOWNCTL 133

/**
 * @brief Invalid path format
 * @ingroup libstd_errno
 */
#define EBADPATH 134

/**
 * @brief Invalid path flag
 * @ingroup libstd_errno
 */
#define EBADFLAG 135

/**
 * @brief Operation not supported
 * @ingroup libstd_errno
 */
#define ENOTSUP 136

/**
 * @brief Resource disconnected or freed
 * @ingroup libstd_errno
 */
#define EDISCONNECTED 137

/**
 * @brief Process spawn failed
 * @ingroup libstd_errno
 */
#define ESPAWNFAIL 138

/**
 * @brief No such label
 * @ingroup libstd_errno
 */
#define ENOLABEL 139

/**
 * @brief Maximum value for all error codes (not inclusive)
 * @ingroup libstd_errno
 */
#define ERR_MAX 140

#if _USE_ANNEX_K == 1
#include "_internal/errno_t.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif
