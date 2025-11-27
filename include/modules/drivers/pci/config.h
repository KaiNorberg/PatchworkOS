#pragma once

#include <modules/acpi/acpi.h>

#include <stdint.h>

/**
 * @brief PCI configuration space
 * @defgroup modules_drivers_pci_config PCI Configuration Space
 * @ingroup modules_drivers_pci
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
 * @brief PCI Segment Group Type
 */
typedef uint16_t pci_segment_group_t;

/**
 * @brief PCI Bus Type
 */
typedef uint8_t pci_bus_t;

/**
 * @brief PCI Slot Type
 */
typedef uint8_t pci_slot_t;

/**
 * @brief PCI Function Type
 */
typedef uint8_t pci_function_t;

/**
 * @brief PCI-e Configuration Space Base Address Allocation Structure
 * @struct pci_config_bar_t
 */
typedef struct PACKED
{
    uint64_t base;
    pci_segment_group_t segmentGroup;
    pci_bus_t startBus;
    pci_bus_t endBus;
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
 * @brief Read a byte from PCI configuration space
 *
 * @param segmentGroup Segment group number
 * @param bus Bus number
 * @param slot Slot number
 * @param function Function number
 * @param offset Offset within the configuration space
 * @return Byte read from the configuration space
 */
uint8_t pci_config_read8(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset);

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
uint16_t pci_config_read16(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset);

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
uint32_t pci_config_read32(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset);

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
void pci_config_write8(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint8_t value);

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
void pci_config_write16(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint16_t value);

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
void pci_config_write32(pci_segment_group_t segmentGroup, pci_bus_t bus, pci_slot_t slot, pci_function_t function,
    uint16_t offset, uint32_t value);

/** @} */
