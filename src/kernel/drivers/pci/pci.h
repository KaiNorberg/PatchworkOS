#pragma once

#include <stdint.h>

/**
 * @brief Peripheral Component Interconnect
 * @defgroup kernel_drivers_pci PCI
 * @ingroup kernel_drivers
 *
 * Id like to use the PCI Firmware Specification as a reference for this, but unfortunately, its not freely available.
 * So we use the OSDev Wiki instead.
 *
 * @see [OSDev PCI](https://wiki.osdev.org/PCI)
 * @see [OSDev PCI Express](https://wiki.osdev.org/PCI_Express)
 *
 * @{
 */

/**
 * @brief Initialize the PCI subsystem
 */
void pci_init(void);

/** @} */
