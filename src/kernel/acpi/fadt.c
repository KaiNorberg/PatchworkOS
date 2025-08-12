#include "fadt.h"

#include "log/log.h"
#include "log/panic.h"

#include <assert.h>

static fadt_t* fadt;

static bool fadt_is_valid(fadt_t* fadt)
{
    if (fadt->header.length < sizeof(fadt_t))
    {
        LOG_ERR("table too small\n");
        return false;
    }

    return true;
}

void fadt_init(void)
{
    fadt = (fadt_t*)acpi_lookup("FACP");
    if (fadt == NULL)
    {
        panic(NULL, "Unable to find fadt, hardware is not compatible");
    }

    if (!fadt_is_valid(fadt))
    {
        panic(NULL, "fadt is not valid");
    }

    LOG_INFO("fadt found with preferred power profile %u and sci interrupt %u\n", fadt->preferredPowerManagementProfile,
        fadt->sciInterrupt);
}

fadt_t* fadt_get(void)
{
    return fadt;
}
