#include "rsdt.h"

#include "string/string.h"

void* rsdt_get(EFI_SYSTEM_TABLE* systemTable)
{
    Print(L"Retrieving RSDP... ");

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

    Print(L"Done!\n\r");

	return rsdp;
}