#include <libc/status.h>

const char* _severityStrings[] = {
    [ST_SEV_INFO] = "info",
    [ST_SEV_ERR] = "error",
};

const char* status_sev_str(status_sev_t sev)
{
    if (sev < ST_SEV_INFO || sev > ST_SEV_ERR)
    {
        return "unknown";
    }
    return _severityStrings[sev];
}