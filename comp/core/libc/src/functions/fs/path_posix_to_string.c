#include "common/path_mode.h"

status_t path_posix_to_string(mode_t mode, char* out, uint64_t length, uint64_t* outLength)
{
    return path_mode_to_string(path_posix_to_mode(mode), out, length, outLength);
}