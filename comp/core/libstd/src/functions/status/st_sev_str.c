#include <libstd/status.h>

const char* _severityStrings[] = {
    [ST_SEV_INFO] = "info",
    [ST_SEV_ERR] = "error",
};

const char* st_sev_str(st_sev_t sev)
{
    if (sev < ST_SEV_INFO || sev > ST_SEV_ERR)
    {
        return "unknown";
    }
    return _severityStrings[sev];
}