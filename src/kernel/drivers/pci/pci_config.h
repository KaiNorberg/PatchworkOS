#pragma once

#include "acpi/acpi.h"

#include <stdint.h>

/**
 * @brief PCI configuration space
 * @defgroup kernel_drivers_pci_config PCI Configuration Space
 * @ingroup kernel_drivers_pci
 *
 * @{
 */

/**
 * @brief PCI-e Configuration Space Base Address Allocation Structure
 * @struct pci_config_bar_t
 */
typedef struct PACKED
{
    uint64_t base;
    uint16_t segmentGroup;
    uint8_t startBus;
    uint8_t endBus;
    uint32_t reserved;
} pci_config_bar_t;

/**
 * @brief PCI Express Memory-mapped Configuration
 * @struct mcfg_t
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint64_t reserved;
    pci_config_bar_t entries[];
} mcfg_t;

/**
 * @brief Initialize PCI configuration space access
 *
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t pci_config_init(void);

/**
 * @brief Read a byte from PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @return Byte read from the configuration space
 */
uint8_t pci_config_read8(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset);

/**
 * @brief Read a word from PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @return Word read from the configuration space
 */
uint16_t pci_config_read16(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset);

/**
 * @brief Read a dword from PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @return DWord read from the configuration space
 */
uint32_t pci_config_read32(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset);

/**
 * @brief Write a byte to PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @param value Byte value to write
 */
void pci_config_write8(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset,
    uint8_t value);

/**
 * @brief Write a word to PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @param value Word value to write
 */
void pci_config_write16(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset,
    uint16_t value);

/**
 * @brief Write a dword to PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @param value DWord value to write
 */
void pci_config_write32(uint16_t segmentGroup, uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset,
    uint32_t value);

/** @} */
