#include <libc/status.h>

const char* _kindStrings[] = {
    [ST_KIND_NONE] = "none",
    [ST_KIND_GENERIC] = "generic",
    [ST_KIND_MEM] = "memory",
    [ST_KIND_IO] = "i/o",
    [ST_KIND_ACCESS] = "access",
    [ST_KIND_RESOURCE] = "resource",
    [ST_KIND_PROC] = "process",
    [ST_KIND_DEV] = "device",
    [ST_KIND_FMT] = "format",
    [ST_KIND_SYS] = "system",
};

const char* st_kind_str(st_kind_t kind)
{
    if (kind < ST_KIND_NONE || kind >= ST_KIND_MAX)
    {
        return "unknown";
    }
    return _kindStrings[kind];
}