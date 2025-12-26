#ifndef _INTERNAL_PID_T_H
#define _INTERNAL_PID_T_H 1

/**
 * @brief Process Identifier.
 * @ingroup libstd
 *
 * The `pid_t` type is used to store process identifiers, valid id's will map to folders found in `/proc`.
 *
 */
typedef __UINT64_TYPE__ pid_t;

#endif
