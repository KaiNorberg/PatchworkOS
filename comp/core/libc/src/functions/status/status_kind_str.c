#include <libc/status.h>

const char* _kindStrings[] = {
    [STATUS_KIND_NONE] = "none",
    [STATUS_KIND_GENERIC] = "generic",
    [STATUS_KIND_MEM] = "memory",
    [STATUS_KIND_IO] = "i/o",
    [STATUS_KIND_ACCESS] = "access",
    [STATUS_KIND_RESOURCE] = "resource",
    [STATUS_KIND_PROC] = "process",
    [STATUS_KIND_DEV] = "device",
    [STATUS_KIND_FMT] = "format",
    [STATUS_KIND_SYS] = "system",
};

const char* status_kind_str(status_kind_t kind)
{
    if (kind < STATUS_KIND_NONE || kind >= STATUS_KIND_MAX)
    {
        return "unknown";
    }
    return _kindStrings[kind];
}