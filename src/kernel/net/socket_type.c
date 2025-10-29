#include <kernel/net/socket_type.h>

const char* socket_type_to_string(socket_type_t type)
{
    switch (type)
    {
    case SOCKET_SEQPACKET:
        return "seqpacket";
    default:
        return "unknown";
    }
}
