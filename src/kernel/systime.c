#include "systime.h"
#include "apic.h"
#include "hpet.h"
#include "io.h"
#include "irq.h"
#include "smp.h"
#include "vectors.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/math.h>

static _Atomic nsec_t accumulator;
static time_t bootEpoch;

static uint8_t cmos_read(uint8_t reg)
{
    io_outb(CMOS_ADDRESS, reg);
    return io_inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static void systime_accumulate(void)
{
    atomic_fetch_add(&accumulator, hpet_read_counter());
    hpet_reset_counter();
}

static void systime_irq_handler(uint8_t irq)
{
    systime_accumulate();
    io_outb(CMOS_ADDRESS, 0x0C);
    io_inb(CMOS_DATA);
}

static void systime_rtc_init(void)
{
    irq_install(systime_irq_handler, IRQ_CMOS);
    io_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8B);
    io_outb(CMOS_DATA, temp | 0x40);
    io_outb(CMOS_ADDRESS, 0x8A);
    temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8A);
    io_outb(CMOS_DATA, (temp & 0xF0) | 15);
}

static void systime_read_cmos_time(void)
{
    uint8_t second = bcd_to_bin(cmos_read(0x00));
    uint8_t minute = bcd_to_bin(cmos_read(0x02));
    uint8_t hour = bcd_to_bin(cmos_read(0x04));
    uint8_t day = bcd_to_bin(cmos_read(0x07));
    uint8_t month = bcd_to_bin(cmos_read(0x08));
    uint16_t year = bcd_to_bin(cmos_read(0x09)) + 2000;

    struct tm tm = {
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900,
    };
    bootEpoch = mktime(&tm);
}

void systime_init(void)
{
    systime_accumulate();
    systime_read_cmos_time();
    systime_rtc_init();

    printf("systime: init epoch=%d", systime_time());
}

nsec_t systime_uptime(void)
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}

time_t systime_time(void)
{
    return bootEpoch + systime_uptime() / SEC;
}

static void systime_timer_init_ipi(trap_frame_t* trapFrame)
{
    nsec_t uptime = systime_uptime();
    nsec_t interval = (SEC / CONFIG_TIMER_HZ) / smp_cpu_amount();
    nsec_t offset = ROUND_UP(uptime, interval) - uptime;
    hpet_sleep(offset + interval * smp_self_unsafe()->id);

    apic_timer_init(VECTOR_TIMER, CONFIG_TIMER_HZ);
}

void systime_timer_init(void)
{
    smp_send_others(systime_timer_init_ipi);
    smp_send_self(systime_timer_init_ipi);

    printf("systime: timer_init hz=%d", CONFIG_TIMER_HZ);
}