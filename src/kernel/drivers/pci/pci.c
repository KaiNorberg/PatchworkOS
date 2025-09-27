#include "pci.h"

#include "pci_config.h"

#include "log/panic.h"

void pci_init(void)
{
    if (pci_config_init() == ERR)
    {
        panic(NULL, "Failed to initialize PCI configuration space access");
    }
}
