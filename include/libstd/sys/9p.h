#ifndef _SYS_9P_H
#define _SYS_9P_H 1

#include <sys/defs.h>

/**
 * @brief 9P Protocol.
 * @ingroup libstd
 * @defgroup libstd_sys_9p 9P Protocol
 *
 * The `sys/9p.h` header provides definitions for the 9P protocol, a distributed file system protocol developed by Bell
 * Labs.
 *
 * ## Messages
 *
 * Message types prefixed with 'T' are client-to-server messages, while those prefixed with 'R' are server-to-client
 * messages and are usually in response to a 'T' message. The client is the entity initiating requests, and the server
 * is the entity responding to those requests.
 *
 * All messages follow the format `size[4] type[1] tag[2] ...` where `size` is the total size of the message in bytes
 * (including the size field itself), `type` is one of the message types defined in `ninep_msg_type_t`, and `tag` is a
 * identifier used to match requests and responses.
 *
 * @note The values specified within square brackets indicate the size of each field in bytes and all multi-byte fields
 * are encoded in little-endian format.
 *
 * @see http://rfc.nop.hu/plan9/rfc9p.pdf for the 9P protocol specification.
 *
 * @{
 */

/**
 * @brief 9P message types.
 */
typedef enum
{
    Tversion = 100,
    Rversion = 101,
    Tauth = 102,
    Rauth = 103,
    Tattach = 104,
    Rattach = 105,
    Terror = 106,
    Rerror = 107,
    Tflush = 108,
    Rflush = 109,
    Twalk = 110,
    Rwalk = 111,
    Topen = 112,
    Ropen = 113,
    Tcreate = 114,
    Rcreate = 115,
    Tread = 116,
    Rread = 117,
    Twrite = 118,
    Rwrite = 119,
    Tclunk = 120,
    Rclunk = 121,
    Tremove = 122,
    Rremove = 123,
    Tstat = 124,
    Rstat = 125,
    Twstat = 126,
    Rwstat = 127,
    Tmax,
} ninep_msg_type_t;

/** @} */

#endif