#include "systime.h"
#include "cpu/apic.h"
#include "cpu/irq.h"
#include "cpu/port.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "hpet.h"
#include "log/log.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/math.h>

static _Atomic(clock_t) accumulator;
static time_t bootEpoch;

static uint8_t cmos_read(uint8_t reg)
{
    port_outb(CMOS_ADDRESS, reg);
    return port_inb(CMOS_DATA);
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
    port_outb(CMOS_ADDRESS, 0x0C);
    port_inb(CMOS_DATA);
}

static void systime_rtc_init(void)
{
    irq_install(systime_irq_handler, IRQ_CMOS);
    port_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = port_inb(CMOS_DATA);
    port_outb(CMOS_ADDRESS, 0x8B);
    port_outb(CMOS_DATA, temp | 0x40);
    port_outb(CMOS_ADDRESS, 0x8A);
    temp = port_inb(CMOS_DATA);
    port_outb(CMOS_ADDRESS, 0x8A);
    port_outb(CMOS_DATA, (temp & 0xF0) | 15);
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

    LOG_INFO("systime: init epoch=%d\n", systime_unix_epoch());
}

clock_t systime_uptime(void)
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}

time_t systime_unix_epoch(void)
{
    return bootEpoch + systime_uptime() / CLOCKS_PER_SEC;
}

void systime_timer_init(void)
{
    cpu_t* self = smp_self_unsafe();
    self->systime.apicTicksPerNs = apic_timer_ticks_per_ns();
    self->systime.nextDeadline = CLOCKS_NEVER;
    LOG_INFO("systime: timer init\n");
}

void systime_timer_trap(trap_frame_t* trapFrame, cpu_t* self)
{
    self->systime.nextDeadline = CLOCKS_NEVER;
}

void systime_timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout)
{
    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    clock_t deadline = uptime + timeout;
    if (deadline < self->systime.nextDeadline)
    {
        uint64_t ticks = (timeout * self->systime.apicTicksPerNs) >> APIC_TIMER_TICKS_FIXED_POINT_OFFSET;
        if (ticks > UINT32_MAX)
        {
            ticks = UINT32_MAX;
        }
        else if (ticks == 0)
        {
            ticks = 1;
        }

        self->systime.nextDeadline = deadline;
        apic_timer_one_shot(VECTOR_TIMER, (uint32_t)ticks);
    }
}