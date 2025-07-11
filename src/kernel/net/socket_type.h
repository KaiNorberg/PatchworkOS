#pragma once

typedef enum
{
    SOCKET_SEQPACKET = 1 << 0,
    SOCKET_TYPE_AMOUNT = 1,
} socket_type_t;

const char* socket_type_to_string(socket_type_t type);
