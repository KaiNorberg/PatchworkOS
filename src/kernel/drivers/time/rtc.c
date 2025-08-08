#include "rtc.h"
#include "cpu/irq.h"
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

static void rtc_irq(irq_t irq)
{
    port_outb(CMOS_ADDRESS, 0x0C);
    port_inb(CMOS_DATA);
}

void rtc_init(void)
{
    irq_install(IRQ_CMOS, rtc_irq);
    port_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = port_inb(CMOS_DATA);
    port_outb(CMOS_ADDRESS, 0x8B);
    port_outb(CMOS_DATA, temp | 0x40);
    port_outb(CMOS_ADDRESS, 0x8A);
    temp = port_inb(CMOS_DATA);
    port_outb(CMOS_ADDRESS, 0x8A);
    port_outb(CMOS_DATA, (temp & 0xF0) | 15);
}

void rtc_read(struct tm* time)
{
    uint8_t second = bcd_to_bin(cmos_read(0x00));
    uint8_t minute = bcd_to_bin(cmos_read(0x02));
    uint8_t hour = bcd_to_bin(cmos_read(0x04));
    uint8_t day = bcd_to_bin(cmos_read(0x07));
    uint8_t month = bcd_to_bin(cmos_read(0x08));
    uint16_t year = bcd_to_bin(cmos_read(0x09)) + 2000;

    if (time != NULL)
    {
        *time = (struct tm){
            .tm_sec = second,
            .tm_min = minute,
            .tm_hour = hour,
            .tm_mday = day,
            .tm_mon = month - 1,
            .tm_year = year - 1900,
        };
    }
}
