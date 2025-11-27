#include <kernel/cpu/io.h>
#include <kernel/log/log.h>
#include <kernel/module/module.h>
#include <kernel/sched/sys_time.h>

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

static port_t addressPort;
static port_t dataPort;

static uint8_t rtc_read(uint8_t reg)
{
    io_out8(addressPort, reg);
    return io_in8(dataPort);
}

static uint8_t rtc_bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static time_t rtc_read_epoch(void)
{
    struct tm time;
    uint8_t second = rtc_bcd_to_bin(rtc_read(0x00));
    uint8_t minute = rtc_bcd_to_bin(rtc_read(0x02));
    uint8_t hour = rtc_bcd_to_bin(rtc_read(0x04));
    uint8_t day = rtc_bcd_to_bin(rtc_read(0x07));
    uint8_t month = rtc_bcd_to_bin(rtc_read(0x08));
    uint16_t year = rtc_bcd_to_bin(rtc_read(0x09)) + 2000;

    time = (struct tm){
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900,
    };
    return mktime(&time);
}

static sys_time_source_t source = {
    .name = "CMOS RTC",
    .precision = CLOCKS_PER_SEC, 
    .read_ns = NULL,
    .read_epoch = rtc_read_epoch,
};

static uint64_t rtc_init(const char* deviceName)
{
    acpi_device_cfg_t* acpiCfg = acpi_device_cfg_get(deviceName);
    if (acpiCfg == NULL)
    {
        LOG_ERR("rtc failed to get ACPI device config for '%s'\n", deviceName);
        return ERR;
    }

    if (acpi_device_cfg_get_port(acpiCfg, 0, &addressPort) == ERR || acpi_device_cfg_get_port(acpiCfg, 1, &dataPort) == ERR)
    {
        LOG_ERR("rtc device '%s' has invalid port resources\n", deviceName);
        return ERR;
    }

    if (sys_time_register_source(&source) == ERR)
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