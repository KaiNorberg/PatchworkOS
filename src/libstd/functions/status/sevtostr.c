#include <sys/status.h>

const char* _severityStrings[] = {
    [ST_SEV_OK] = "ok",
    [ST_SEV_ERR] = "error",
};

const char* sevtostr(st_sev_t sev)
{
    if (sev < ST_SEV_OK || sev > ST_SEV_ERR)
    {
        return "unknown";
    }
    return _severityStrings[sev];
}