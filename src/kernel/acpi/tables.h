#pragma once

#include "acpi.h"

/**
 * @brief ACPI Tables
 * @defgroup kernel_acpi_tables ACPI Tables
 * @ingroup kernel_acpi
 *
 * This module defines the ACPI tables found in the ACPI specification, tables defined outside of the specification, for
 * example, MCFG is defined in their own files.
 *
 * @{
 */

/**
 * @brief The maximum number of ACPI tables that are supported.
 *
 * As far as I know there should never be even close to this many on any machine.
 */
#define ACPI_MAX_TABLES 64

/**
 * @brief Enum for the `fadt_t::bootArchFlags` field.
 */
typedef enum
{
    FADT_BOOT_ARCH_PS2_EXISTS = (1 << 1)
} fadt_boot_arch_flags_t;

/**
 * @brief FADT generic Address
 * @struct fadt_gas_t
 *
 */
typedef struct PACKED
{
    uint8_t addressSpace;
    uint8_t bitWidth;
    uint8_t bitOffset;
    uint8_t accessSize;
    uint64_t address;
} fadt_gas_t;

/**
 * @brief Fixed ACPI Description Table
 * @struct fadt_t
 *
 * @see Section 5.2.9 table 5.9 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint32_t firmwareControl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferredPowerManagementProfile;
    uint16_t sciInterrupt;
    uint32_t smiCommandPort;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4BiosReq;
    uint8_t pstateControl;
    uint32_t pm1aEventBlock;
    uint32_t pm1bEventBlock;
    uint32_t pm1aControlBlock;
    uint32_t pm1bControlBlock;
    uint32_t pm2ControlBlock;
    uint32_t pmTimerBlock;
    uint32_t gpe0Block;
    uint32_t gpe1Block;
    uint8_t pm1EventLength;
    uint8_t pm1ControlLength;
    uint8_t pm2ControlLength;
    uint8_t pmTimerLength;
    uint8_t gpe0Length;
    uint8_t gpe1Length;
    uint8_t gpe1Base;
    uint8_t cStateControl;
    uint16_t worstC2Latency;
    uint16_t worstC3Latency;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlarm;
    uint8_t monthAlarm;
    uint8_t century;
    uint16_t bootArchFlags;
    uint8_t reserved2;
    uint32_t flags;
    fadt_gas_t resetReg;
    uint8_t resetValue;
    uint8_t reserved3[3];
    uint64_t xFirmwareControl;
    uint64_t xDsdt;
    fadt_gas_t xPm1aEventBlock;
    fadt_gas_t xPm1bEventBlock;
    fadt_gas_t xPm1aControlBlock;
    fadt_gas_t xPm1bControlBlock;
    fadt_gas_t xPm2ControlBlock;
    fadt_gas_t xPmTimerBlock;
    fadt_gas_t xGpe0Block;
    fadt_gas_t xGpe1Block;
} fadt_t;

/**
 * @brief Type safe way to get the FADT table
 *
 * @return fadt_t* The FADT table pointer
 */
#define FADT_GET() ((fadt_t*)acpi_tables_lookup("FACP", 0))

/**
 * @brief Multiple APIC Description Table flags.
 *
 * @see Section 5.2.12 table 5.20 of the ACPI specification for more details.
 */
typedef uint32_t madt_flags_t;

#define MADT_FLAG_PCAT_COMPAT ((madt_flags_t)(1 << 0))

/**
 * @brief MADT Interrupt Controller Types.
 *
 * @see Section 5.2.12 table 5.21 of the ACPI specification for more details.
 */
typedef uint8_t madt_interrupt_controller_type_t;

#define MADT_INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC ((madt_interrupt_controller_type_t)0)
#define MADT_INTERRUPT_CONTROLLER_IO_APIC ((madt_interrupt_controller_type_t)1)

/**
 * @brief MADT Interrupt Controller Header
 * @struct madt_interrupt_controller_header_t
 */
typedef struct PACKED
{
    madt_interrupt_controller_type_t type;
    uint8_t length;
} madt_interrupt_controller_header_t;

/**
 * @brief MADT Processor Local APIC flags
 *
 * @see Section 5.2.12.2 table 5.23 of the ACPI specification for more details.
 */
typedef uint32_t madt_processor_local_apic_flags_t;

#define MADT_PROCESSOR_LOCAL_APIC_ENABLED (1 << 0)
#define MADT_PROCESSOR_LOCAL_APIC_ONLINE_CAPABLE (1 << 1)

/**
 * @brief MADT Interrupt Controller: Processor Local APIC
 * @struct madt_processor_local_apic_t
 *
 * @see Section 5.2.12.2 table 5.22 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    madt_interrupt_controller_header_t header;
    uint8_t acpiProcessorUid;
    uint8_t apicId;
    uint32_t flags;
} madt_processor_local_apic_t;

/**
 * @brief MADT Interrupt Controller: IO APIC
 * @struct madt_ioapic_t
 *
 * @see Section 5.2.12.3 table 5.24 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    madt_interrupt_controller_header_t header;
    uint8_t ioApicId;
    uint8_t reserved;
    uint32_t ioApicAddress;
    uint32_t globalSystemInterruptBase;
} madt_ioapic_t;

/**
 * @brief Multiple APIC Description Table
 * @struct madt_t
 *
 * @see Section 5.2.12 table 5.19 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint32_t localInterruptControllerAddress;
    madt_flags_t flags;
    madt_interrupt_controller_header_t interruptControllers[];
} madt_t;

/**
 * @brief Iterate over all MADT interrupt controllers
 *
 * @param madt The MADT table to iterate over.
 * @param ic A pointer to a madt_interrupt_controller_header_t* that will be set to the current interrupt controller.
 */
#define MADT_FOR_EACH(madt, ic) \
    for (ic = (typeof(ic))madt->interruptControllers; (uint8_t*)ic < (uint8_t*)madt + madt->header.length && \
        (uint8_t*)ic + sizeof(madt_interrupt_controller_header_t) <= (uint8_t*)madt + madt->header.length && \
        (uint8_t*)ic + ic->header.length <= (uint8_t*)madt + madt->header.length; \
        ic = (typeof(ic))((uint8_t*)ic + ic->header.length))

/**
 * @brief Type safe way to get the MADT table
 *
 * @return madt_t* The MADT table pointer
 */
#define MADT_GET() ((madt_t*)acpi_tables_lookup("APIC", 0))

/**
 * @brief Differentiated System Description Table
 * @struct dsdt_t
 *
 * @see Section 5.2.11.1 table 5.17 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint8_t definitionBlock[];
} dsdt_t;

/**
 * @brief Type safe way to get the DSDT table
 *
 * @return dsdt_t* The DSDT table pointer
 */
#define DSDT_GET() ((dsdt_t*)acpi_tables_lookup("DSDT", 0))

/**
 * @brief Secondary System Description Table
 * @struct ssdt_t
 *
 * @see Section 5.2.11.2 table 5.18 of the ACPI specification for more details.
 */
typedef struct PACKED
{
    sdt_header_t header;
    uint8_t definitionBlock[];
} ssdt_t;

/**
 * @brief Type safe way to get the n'th SSDT table
 *
 * @param n The index of the SSDT table to get (0 indexed).
 * @return ssdt_t* The SSDT table pointer
 */
#define SSDT_GET(n) ((ssdt_t*)acpi_tables_lookup("SSDT", n))

/**
 * @brief ACPI System Description Table handler
 * @struct acpi_sdt_handler_t
 *
 * This structure is used to register handlers for specific ACPI tables.
 */
typedef struct
{
    const char* signature;                 ///< The signature of the table to handle.
    uint64_t (*init)(sdt_header_t* table); ///< The handler function to call when the table is first loaded.
} acpi_sdt_handler_t;

/**
 * @brief Macro to register an ACPI SDT handler
 *
 * This macro registers a handler for a specific ACPI table signature using the linker section `.acpi_sdt_handlers`.
 *
 * Might be integrated into some bigger device driver registration system in the future.
 *
 * @param sig The signature of the table to handle.
 * @param handler_name The name of the handler function to generate.
 */
#define ACPI_SDT_HANDLER_REGISTER(sig, initHandler) \
    static const acpi_sdt_handler_t _acpiSdtHandler##handler_name \
        __attribute__((used, section(".acpi_sdt_handlers"))) = { \
            .signature = sig, \
            .init = initHandler, \
    };

/**
 * @brief Load all ACPI tables and call their handlers.
 *
 * @param xsdt The XSDT to load the tables from.
 * @return On success, the number of tables loaded. On failure, ERR.
 */
uint64_t acpi_tables_init(xsdt_t* xsdt);

/**
 * @brief Lookup the n'th table matching the signature.
 *
 * @param signature The signature of the table to look up.
 * @param n The index of the table to look up (0 indexed).
 * @return The table if found, NULL otherwise.
 */
sdt_header_t* acpi_tables_lookup(const char* signature, uint64_t n);

/** @} */
