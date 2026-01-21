#include <kernel/cpu/port.h>
#include <kernel/log/log.h>
#include <kernel/module/module.h>
#include <kernel/sched/clock.h>
#include <modules/acpi/devices.h>
#include <modules/acpi/tables.h>
#include <kernel/sync/lock.h>

#include <time.h>

/**
 * @brief Real Time Clock
 * @defgroup kernel_drivers_rtc RTC
 * @ingroup kernel_drivers
 *
 * The RTC driver provides functions to read the current time from the CMOS RTC.
 *
 * @see [OSDev CMOS](https://wiki.osdev.org/CMOS)
 *
 * @{
 */

static uint8_t centuryRegister = 0;

static port_t addressPort = 0;
static port_t dataPort = 0;
static lock_t lock = LOCK_CREATE();

static uint8_t rtc_read(uint8_t reg)
{
    out8(addressPort, reg | 0x80); // Force NMI disable
    return in8(dataPort);
}

static int rtc_update_in_progress(void)
{
    return (rtc_read(0x0A) & 0x80);
}

static uint8_t rtc_bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static time_t rtc_read_epoch(void)
{
    LOCK_SCOPE(&lock);

    while (rtc_update_in_progress())
    {
        ASM("pause");
    }

    uint8_t seconds = rtc_bcd_to_bin(rtc_read(0x00));
    uint8_t minutes = rtc_bcd_to_bin(rtc_read(0x02));
    uint8_t hours = rtc_bcd_to_bin(rtc_read(0x04));
    uint8_t day = rtc_bcd_to_bin(rtc_read(0x07));
    uint8_t month = rtc_bcd_to_bin(rtc_read(0x08));
    uint8_t year = rtc_bcd_to_bin(rtc_read(0x09));

    uint16_t fullYear;
    if (centuryRegister != 0)
    {
        uint8_t cent = rtc_bcd_to_bin(rtc_read(centuryRegister));
        fullYear = cent * 100 + year;
    }
    else
    {
        fullYear = (year >= 70) ? (1900 + year) : (2000 + year);
    }

    struct tm stime = (struct tm){
        .tm_sec = seconds,
        .tm_min = minutes,
        .tm_hour = hours,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = fullYear - 1900,
    };

    return mktime(&stime);
}

static clock_source_t source = {
    .name = "CMOS RTC",
    .precision = CLOCKS_PER_SEC,
    .read_ns = NULL,
    .read_epoch = rtc_read_epoch,
};

static uint64_t rtc_init(const char* deviceName)
{
    LOCK_SCOPE(&lock);

    acpi_device_cfg_t* acpiCfg = acpi_device_cfg_lookup(deviceName);
    if (acpiCfg == NULL)
    {
        LOG_ERR("rtc failed to get ACPI device config for '%s'\n", deviceName);
        return ERR;
    }

    if (acpi_device_cfg_get_port(acpiCfg, 0, &addressPort) == ERR ||
        acpi_device_cfg_get_port(acpiCfg, 1, &dataPort) == ERR)
    {
        LOG_ERR("rtc device '%s' has invalid port resources\n", deviceName);
        return ERR;
    }

    fadt_t* fadt = (fadt_t*)acpi_tables_lookup(FADT_SIGNATURE, sizeof(fadt_t), 0);
    if (fadt != NULL)
    {
        centuryRegister = fadt->century;
    }

    if (clock_source_register(&source) == ERR)
    {
        LOG_ERR("failed to register RTC\n");
        return ERR;
    }

    return 0;
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (rtc_init(event->deviceAttach.name) == ERR)
        {
            LOG_ERR("failed to initialize RTC\n");
            return ERR;
        }
        break;
    default:
        break;
    }
    return 0;
}

MODULE_INFO("RTC Driver", "Kai Norberg", "A driver for the CMOS Real Time Clock", OS_VERSION, "MIT", "PNP0B00");