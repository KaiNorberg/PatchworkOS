#include "rsdp.h"

#include "string/string.h"

void* rsdp_get(EFI_SYSTEM_TABLE* systemTable)
{
    EFI_CONFIGURATION_TABLE* configTable = systemTable->ConfigurationTable;
    void* rsdp = 0;
    EFI_GUID acpi2TableGuid = ACPI_20_TABLE_GUID;

    for (UINTN i = 0; i < systemTable->NumberOfTableEntries; i++)
	{
	    if (CompareGuid(&configTable[i].VendorGuid, &acpi2TableGuid) && strcmp("RSD PTR ", configTable->VendorTable))
		{
		    rsdp = configTable->VendorTable;
		}
	    configTable++;
	}

    if (rsdp == 0)
	{
	    Print(L"ERROR: Failed to locate rsdp!");
	    while (1)
		{
		    asm volatile("hlt");
		}
	}

    return rsdp;
}