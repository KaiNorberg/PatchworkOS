#include "rtc.h"
#include "cpu/port.h"

static uint8_t cmos_read(uint8_t reg)
{
    port_outb(CMOS_ADDRESS, reg);
    return port_inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

void rtc_read(struct tm* time)
{
    if (time == NULL)
    {
        return;
    }

    uint8_t second = bcd_to_bin(cmos_read(0x00));
    uint8_t minute = bcd_to_bin(cmos_read(0x02));
    uint8_t hour = bcd_to_bin(cmos_read(0x04));
    uint8_t day = bcd_to_bin(cmos_read(0x07));
    uint8_t month = bcd_to_bin(cmos_read(0x08));
    uint16_t year = bcd_to_bin(cmos_read(0x09)) + 2000;

    *time = (struct tm){
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900,
    };
}
