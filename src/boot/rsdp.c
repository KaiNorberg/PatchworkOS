#include "rsdp.h"

#include <string.h>

void* rsdp_get(EFI_SYSTEM_TABLE* systemTable)
{
    Print(L"Searching for RSDP... ");
    EFI_CONFIGURATION_TABLE* configTable = systemTable->ConfigurationTable;
    EFI_GUID acpi2TableGuid = ACPI_20_TABLE_GUID;

    void* rsdp = NULL;
    for (uint64_t i = 0; i < systemTable->NumberOfTableEntries; i++)
    {
        if (CompareGuid(&configTable[i].VendorGuid, &acpi2TableGuid) &&
            memcmp("RSD PTR ", configTable->VendorTable, 8) == 0)
        {
            rsdp = configTable->VendorTable;
        }
        configTable++;
    }

    if (rsdp == NULL)
    {
        Print(L"failed to locate rsdp!\n");
    }
    else
    {
        Print(L"found at %p!\n", rsdp);
    }
    return rsdp;
}
