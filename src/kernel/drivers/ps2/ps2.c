#include "ps2.h"
#include "ps2_kbd.h"
#include "ps2_mouse.h"

#include "cpu/port.h"
#include "log/log.h"
#include "log/panic.h"
#include "sched/timer.h"
#include "acpi/fadt.h"
#include <errno.h>

static bool isDualChannel = false;

static ps2_device_info_t firstDevice = {0};
static ps2_device_info_t secondDevice = {0};

static const ps2_device_info_t knownDevices[] = {
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Ancient AT keyboard", .id = {0}, .idLength = 0},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "Standard PS/2 mouse", .id = {0x00}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "Mouse with scroll wheel", .id = {0x03}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_MOUSE, .name = "5-button mouse", .id = {0x04}, .idLength = 1},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "MF2 keyboard type1", .id = {0xAB, 0x83}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "MF2 keyboard type2", .id = {0xAB, 0xC1}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Short keyboard", .id = {0xAB, 0x84}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "NCD N-97 keyboard", .id = {0xAB, 0x85}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "122-key keyboards", .id = {0xAB, 0x86}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"G\" keyboards", .id = {0xAB, 0x90}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"P\" keyboards", .id = {0xAB, 0x91}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "Japanese \"A\" keyboards", .id = {0xAB, 0x92}, .idLength = 2},
    {.type = PS2_DEVICE_TYPE_KEYBOARD, .name = "NCD Sun layout keyboard", .id = {0xAC, 0xA1}, .idLength = 2},
};

#define PS2_KNOWN_DEVICE_AMOUNT (sizeof(knownDevices) / sizeof(knownDevices[0]))

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

static const char* ps2_device_type_to_string(ps2_device_type_t type)
{
    switch (type)
    {
    case PS2_DEVICE_TYPE_MOUSE:
        return "mouse";
    case PS2_DEVICE_TYPE_KEYBOARD:
        return "keyboard";
    default:
        return "invalid type";
    }
}

static uint64_t ps2_self_test(void)
{
    ps2_cmd(PS2_CMD_CFG_READ);
    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        return ERR;
    }

    if (ps2_cmd(PS2_CMD_SELF_TEST) == ERR)
    {
        return ERR;
    }
    ps2_self_test_response_t response;
    if (PS2_READ(&response) == ERR)
    {
        return ERR;
    }

    if (response != PS2_SELF_TEST_PASS)
    {
        panic(NULL, "ps2 self test %s", ps2_self_test_response_to_string(response));
    }

    if (ps2_cmd(PS2_CMD_CFG_WRITE) == ERR)
    {
        return ERR;
    }
    if (PS2_WRITE(cfg) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t ps2_check_if_dual_channel(void)
{
    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        return ERR;
    }
    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        return ERR;
    }

    if (cfg & PS2_CFG_SECOND_CLOCK_DISABLE)
    {
        ps2_cmd(PS2_CMD_SECOND_ENABLE);

        if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
        {
            return ERR;
        }
        if (PS2_READ(&cfg) == ERR)
        {
            return ERR;
        }

        if (!(cfg & PS2_CFG_SECOND_CLOCK_DISABLE)) // If the flag changed, then the second port exists.
        {
            isDualChannel = true;
            LOG_INFO("dual channel PS/2 controller detected\n");

            if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
            {
                return ERR;
            }
        }
        else
        {
            isDualChannel = false;
            LOG_INFO("single channel PS/2 controller detected\n");
        }
    }

    return 0;
}

static uint64_t ps2_device_test(ps2_device_t device)
{
    if (ps2_cmd(device == PS2_DEVICE_FIRST ? PS2_CMD_FIRST_TEST : PS2_CMD_SECOND_TEST) == ERR)
    {
        return ERR;
    }
    ps2_device_test_response_t response;
    if (PS2_READ(&response) == ERR)
    {
        return ERR;
    }

    if (response != PS2_DEVICE_TEST_PASS)
    {
        LOG_ERR("ps2 %s device test fail (%s)\n", ps2_device_to_string(device), ps2_device_test_response_to_string(response));
        return ERR;
    }

    return 0;
}

static uint64_t ps2_device_reset(ps2_device_t device)
{
    uint64_t result = ps2_device_cmd(device, PS2_DEV_RESET);
    if (result == ERR)
    {
        return ERR;
    }

    uint8_t response[2];
    if (PS2_READ(&response[0]) == ERR || PS2_READ(&response[1]) == ERR)
    {
        return ERR;
    }

    if (response[0] != PS2_DEVICE_RESET_PASS_1 && response[0] != PS2_DEVICE_RESET_PASS_2)
    {
        LOG_ERR("ps2 %s device reset fail, invalid first byte\n", ps2_device_to_string(device));
        return ERR;
    }

    if (response[1] != PS2_DEVICE_RESET_PASS_1 && response[1] != PS2_DEVICE_RESET_PASS_2)
    {
        LOG_ERR("ps2 %s device reset fail, invalid second byte\n", ps2_device_to_string(device));
        return ERR;
    }

    LOG_INFO("ps2 %s device reset success, 0x%x 0x%x\n", ps2_device_to_string(device), response[0], response[1]);

    return 0;
}

static uint64_t ps2_device_identify(ps2_device_t device, ps2_device_info_t* info)
{
    ps2_device_ack_t ack;
    if (ps2_device_cmd(device, PS2_DEV_DISABLE_SCANNING) == ERR || PS2_READ(&ack) == ERR || ack != PS2_DEVICE_ACK)
    {
        LOG_ERR("ps2 %s device identify failed to disable scanning\n", ps2_device_to_string(device));
        return ERR;
    }

    if (ps2_device_cmd(device, PS2_DEV_IDENTIFY) == ERR || PS2_READ(&ack) == ERR || ack != PS2_DEVICE_ACK)
    {
        LOG_ERR("ps2 %s device identify failed to identify device\n", ps2_device_to_string(device));
        return ERR;
    }

    uint8_t id[2] = {0};
    uint8_t idLength = 0;

    if (PS2_READ(&id[0]) == ERR)
    {
        LOG_ERR("ps2 %s device identify failed to read id[0]\n", ps2_device_to_string(device));
        return ERR;
    }

    idLength = 1;

    if (PS2_READ(&id[1]) == ERR)
    {
        if (errno != ETIMEDOUT)
        {
            LOG_ERR("ps2 %s device identify failed to read id[1]\n", ps2_device_to_string(device));
            return ERR;
        }
    }
    else
    {
        idLength = 2;
    }

    info->device = device;
    *info->id = *id;
    info->idLength = idLength;
    info->data = NULL;

    for (uint8_t i = 0; i < PS2_KNOWN_DEVICE_AMOUNT; i++)
    {
        if (idLength == knownDevices[i].idLength && memcmp(id, knownDevices[i].id, idLength) == 0)
        {
            info->type = knownDevices[i].type;
            info->name = knownDevices[i].name;
            LOG_INFO("ps2 %s device identified as '%s', type %s\n", ps2_device_to_string(device), info->name, ps2_device_type_to_string(info->type));
            return 0;
        }
    }

    ps2_device_type_t type = device == PS2_DEVICE_FIRST ? PS2_DEVICE_TYPE_KEYBOARD : PS2_DEVICE_TYPE_MOUSE;

    LOG_WARN("ps2 %s device identity unknown assuming %s\n", ps2_device_to_string(device), ps2_device_type_to_string(type));
    info->type = type;
    info->name = "unknown";
    return 0;
}

static uint64_t ps2_device_init(ps2_device_t device, ps2_device_info_t *info)
{
    if (ps2_device_reset(device) == ERR)
    {
        return ERR;
    }

    if (ps2_device_identify(device, info) == ERR)
    {
        return ERR;
    }

    if (info->type == PS2_DEVICE_TYPE_KEYBOARD)
    {
        if (ps2_kbd_init(info) == ERR)
        {
            return ERR;
        }
    }
    else if (info->type == PS2_DEVICE_TYPE_MOUSE)
    {
        if (ps2_mouse_init(info) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

static uint64_t ps2_devices_init(void)
{
    bool firstAvailable = false;
    bool secondAvailable = false;

    if (ps2_device_test(PS2_DEVICE_FIRST) == ERR)
    {
        return ERR;
    }
    firstAvailable = true;
    if (ps2_cmd(PS2_CMD_FIRST_ENABLE) == ERR)
    {
        return ERR;
    }

    if (isDualChannel)
    {
        if (ps2_device_test(PS2_DEVICE_SECOND) == ERR)
        {
            return ERR;
        }
        secondAvailable = true;
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            return ERR;
        }
    }

    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        return ERR;
    }
    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        return ERR;
    }

    cfg |= PS2_CFG_FIRST_IRQ;
    if (isDualChannel)
    {
        cfg |= PS2_CFG_SECOND_IRQ;
    }
    cfg &= ~PS2_CFG_FIRST_TRANSLATION;

    if (ps2_cmd(PS2_CMD_CFG_WRITE) == ERR)
    {
        return ERR;
    }
    if (PS2_WRITE(cfg) == ERR)
    {
        return ERR;
    }

    if (firstAvailable && ps2_device_init(PS2_DEVICE_FIRST, &firstDevice) == ERR)
    {
        return ERR;
    }

    if (secondAvailable && ps2_device_init(PS2_DEVICE_SECOND, &secondDevice) == ERR)
    {
        return ERR;
    }

    return 0;
}

void ps2_init(void)
{
    if (!(fadt_get()->bootArchFlags & FADT_BOOT_ARCH_PS2_EXISTS))
    {
        panic(NULL, "ps2 not supported, for now the OS needs ps2 support as no other HID devices are supported");
    }

    if (ps2_cmd(PS2_CMD_FIRST_DISABLE) == ERR)
    {
        panic(NULL, "ps2 first disable fail");
    }
    if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
    {
        panic(NULL, "ps2 second disable fail");
    }

    port_inb(PS2_PORT_DATA); // Discard

    if (ps2_self_test())
    {
        panic(NULL, "ps2 self test fail");
    }

    if (ps2_check_if_dual_channel() == ERR)
    {
        panic(NULL, "ps2 check if dual channel fail");
    }

    if (ps2_devices_init() == ERR)
    {
        panic(NULL, "ps2 devices init fail");
    }
}

uint64_t ps2_wait_until_set(ps2_status_bits_t status)
{
    uint64_t time = timer_uptime();

    while (time + CLOCKS_PER_SEC > timer_uptime())
    {
        if (port_inb(PS2_PORT_STATUS) & status)
        {
            return 0;
        }

        asm volatile("pause");
    }

    errno = ETIMEDOUT;
    return ERR;
}

uint64_t ps2_wait_until_clear(ps2_status_bits_t status)
{
    uint64_t time = timer_uptime();

    while (time + CLOCKS_PER_SEC > timer_uptime())
    {
        ps2_status_bits_t currentStatus = port_inb(PS2_PORT_STATUS);
        if (!(currentStatus & status))
        {
            return 0;
        }

        // If we are waiting for IN_FULL to clear, we should read any available data to avoid deadlocks.
        if ((status & PS2_STATUS_IN_FULL) && (currentStatus & PS2_STATUS_OUT_FULL))
        {
            port_inb(PS2_PORT_DATA);
        }

        asm volatile("pause");
    }

    errno = ETIMEDOUT;
    return ERR;
}

uint64_t ps2_cmd(ps2_cmd_t command)
{
    if (ps2_wait_until_clear(PS2_STATUS_IN_FULL) == ERR)
    {
        return ERR;
    }
    port_outb(PS2_PORT_CMD, command);
    return 0;
}

uint64_t ps2_device_cmd(ps2_device_t device, ps2_device_cmd_t command)
{
    if (device == PS2_DEVICE_FIRST)
    {
        return PS2_WRITE(command);
    }
    else if (device == PS2_DEVICE_SECOND)
    {
        if (ps2_cmd(PS2_CMD_SECOND_WRITE) == ERR)
        {
            return ERR;
        }
        return PS2_WRITE(command);
    }
    else
    {
        panic(NULL, "invalid device");
        return ERR;
    }
}
