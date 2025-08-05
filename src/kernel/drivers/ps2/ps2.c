#include "ps2.h"

#include "cpu/port.h"
#include "log/log.h"
#include "log/panic.h"
#include "sched/timer.h"

static bool isDualChannel = false;

static ps2_device_info_t firstDevice = {0};
static ps2_device_info_t secondDevice = {0};

static const ps2_device_info_t knownDevices[] = {
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Ancient AT keyboard", .id = {0}, .idLength = 0},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "Standard PS/2 mouse", .id = {0x00}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "Mouse with scroll wheel", .id = {0x03}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "5-button mouse", .id = {0x04}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "MF2 keyboard 0x83", .id = {0xAB, 0x83}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "MF2 keyboard 0xC1", .id = {0xAB, 0xC1}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Short keyboard", .id = {0xAB, 0x84}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "NCD N-97 keyboard", .id = {0xAB, 0x85}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "122-key keyboards", .id = {0xAB, 0x86}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"G\" keyboards", .id = {0xAB, 0x90}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"P\" keyboards", .id = {0xAB, 0x91}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"A\" keyboards", .id = {0xAB, 0x92}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "NCD Sun layout keyboard", .id = {0xAC, 0xA1}, .idLength = 2},
};

static const char* ps2_device_test_response_to_string(ps2_device_test_response_t response)
{
    switch (response)
    {
    case PS2_DEVICE_TEST_PASS:
        return "pass";
    case PS2_DEVICE_TEST_CLOCK_STUCK_LOW:
        return "clock stuck low";
    case PS2_DEVICE_TEST_CLOCK_STUCK_HIGH:
        return "clock stuck high";
    case PS2_DEVICE_TEST_DATA_STUCK_LOW:
        return "data stuck low";
    case PS2_DEVICE_TEST_DATA_STUCK_HIGH:
        return "data stuck high";
    default:
        return "invalid response";
    }
}

static const char* ps2_self_test_response_to_string(ps2_self_test_response_t response)
{
    switch (response)
    {
    case PS2_SELF_TEST_PASS:
        return "pass";
    case PS2_SELF_TEST_FAIL:
        return "fail";
    default:
        return "invalid response";
    }
}

static const char* ps2_device_to_string(ps2_device_t device)
{
    switch (device)
    {
    case PS2_DEVICE_FIRST:
        return "first";
    case PS2_DEVICE_SECOND:
        return "second";
    default:
        return "invalid device";
    }
    }

static void ps2_self_test(void)
{
    ps2_cmd(PS2_CMD_CFG_READ);
    ps2_config_bits_t cfg = ps2_read();

    ps2_cmd(PS2_CMD_SELF_TEST);
    ps2_self_test_response_t selfTest = ps2_read();
    if (selfTest != PS2_SELF_TEST_PASS)
    {
        panic(NULL, "ps2 self test %s", ps2_self_test_response_to_string(selfTest));
    }

    ps2_cmd(PS2_CMD_CFG_WRITE);
    ps2_write(cfg);
}

static void ps2_check_if_dual_channel(void)
{
    ps2_cmd(PS2_CMD_CFG_READ);
    ps2_config_bits_t cfg = ps2_read();

    if (cfg & PS2_CFG_SECOND_CLOCK_DISABLE)
    {
        ps2_cmd(PS2_CMD_SECOND_ENABLE);

        ps2_cmd(PS2_CMD_CFG_READ);
        cfg = ps2_read();

        if (!(cfg & PS2_CFG_SECOND_CLOCK_DISABLE)) // If the flag changed, then the second port exists.
        {
            isDualChannel = true;
            LOG_INFO("dual channel PS/2 controller detected\n");

            ps2_cmd(PS2_CMD_SECOND_DISABLE);
        }
        else
        {
            isDualChannel = false;
            LOG_INFO("single channel PS/2 controller detected\n");
        }
    }
}

static uint64_t ps2_device_test(ps2_device_t device)
{
    ps2_cmd(device == PS2_DEVICE_FIRST ? PS2_CMD_FIRST_TEST : PS2_CMD_SECOND_TEST);
    ps2_device_test_response_t deviceTest = ps2_read();
    if (deviceTest != PS2_DEVICE_TEST_PASS)
    {
        LOG_ERR("ps2 %s device test fail (%s)", ps2_device_to_string(device), ps2_device_test_response_to_string(deviceTest));
        return ERR;
    }

    return 0;
}

static void ps2_devices_init(void)
{

}

void ps2_init(void)
{
    ps2_cmd(PS2_CMD_FIRST_DISABLE);
    ps2_cmd(PS2_CMD_SECOND_DISABLE);

    port_inb(PS2_PORT_DATA); // Discard

    ps2_self_test();
    ps2_check_if_dual_channel();
    ps2_devices_init();
}

void ps2_wait(void)
{
    uint64_t time = timer_uptime();

    while (time + CLOCKS_PER_SEC > timer_uptime())
    {
        uint8_t status = port_inb(PS2_PORT_STATUS);
        if (status & PS2_STATUS_OUT_FULL)
        {
            ps2_read(); // Discard
        }
        if (!(status & (PS2_STATUS_IN_FULL | PS2_STATUS_OUT_FULL)))
        {
            return;
        }
    }

    panic(NULL, "PS2 Timeout");
}

uint8_t ps2_read(void)
{
    uint64_t time = timer_uptime();

    while (time + CLOCKS_PER_SEC > timer_uptime())
    {
        uint8_t status = port_inb(PS2_PORT_STATUS);
        if (status & PS2_STATUS_OUT_FULL)
        {
            port_wait();
            return port_inb(PS2_PORT_DATA);
        }
    }

    panic(NULL, "PS2 Timeout");
}

void ps2_write(uint8_t data)
{
    ps2_wait();
    port_outb(PS2_PORT_DATA, data);
}

void ps2_cmd(ps2_cmd_t command)
{
    ps2_wait();
    port_outb(PS2_PORT_CMD, command);
}

uint8_t ps2_cmd_with_response(ps2_cmd_t command)
{
    ps2_cmd(command);
    return ps2_read();
}
