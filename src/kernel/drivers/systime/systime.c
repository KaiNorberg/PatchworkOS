#include "systime.h"
#include "cpu/apic.h"
#include "cpu/irq.h"
#include "cpu/port.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "hpet.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/math.h>

static _Atomic(clock_t) accumulator;
static time_t bootEpoch;

static uint8_t cmos_read(uint8_t reg)
{
    port_out(CMOS_ADDRESS, reg);
    return port_in(CMOS_DATA);
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
    port_out(CMOS_ADDRESS, 0x0C);
    port_in(CMOS_DATA);
}

static void systime_rtc_init(void)
{
    irq_install(systime_irq_handler, IRQ_CMOS);
    port_out(CMOS_ADDRESS, 0x8B);
    uint8_t temp = port_in(CMOS_DATA);
    port_out(CMOS_ADDRESS, 0x8B);
    port_out(CMOS_DATA, temp | 0x40);
    port_out(CMOS_ADDRESS, 0x8A);
    temp = port_in(CMOS_DATA);
    port_out(CMOS_ADDRESS, 0x8A);
    port_out(CMOS_DATA, (temp & 0xF0) | 15);
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

    printf("systime: init epoch=%d\n", systime_unix_epoch());
}

clock_t systime_uptime(void)
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}

time_t systime_unix_epoch(void)
{
    return bootEpoch + systime_uptime() / CLOCKS_PER_SEC;
}

static void systime_timer_init_ipi(trap_frame_t* trapFrame)
{
    clock_t uptime = systime_uptime();
    clock_t interval = (CLOCKS_PER_SEC / CONFIG_TIMER_HZ) / smp_cpu_amount();
    clock_t offset = ROUND_UP(uptime, interval) - uptime;
    hpet_sleep(offset + interval * smp_self_unsafe()->id);

    apic_timer_init(VECTOR_TIMER, CONFIG_TIMER_HZ);
}

void systime_timer_init(void)
{
    smp_send_others(systime_timer_init_ipi);
    systime_timer_init_ipi(NULL);

    printf("systime: timer_init hz=%d\n", CONFIG_TIMER_HZ);
}
