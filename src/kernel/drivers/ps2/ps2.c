#include "ps2.h"
#include "ps2_kbd.h"
#include "ps2_mouse.h"

#include "cpu/port.h"
#include "log/log.h"
#include "log/panic.h"
#include "sched/timer.h"
#include "acpi/fadt.h"
#include <errno.h>
#include <string.h>

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

static uint64_t ps2_set_initial_config(void)
{
    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        LOG_ERR("ps2 failed to read initial config\n");
        return ERR;
    }

    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        LOG_ERR("ps2 failed to read initial config data\n");
        return ERR;
    }

    LOG_DEBUG("ps2 initial config byte: 0x%02x\n", cfg);
    cfg &= ~(PS2_CFG_FIRST_IRQ | PS2_CFG_FIRST_CLOCK_DISABLE | PS2_CFG_FIRST_TRANSLATION | PS2_CFG_SECOND_IRQ);
    LOG_DEBUG("ps2 setting config byte to: 0x%02x\n", cfg);

    if (ps2_cmd(PS2_CMD_CFG_WRITE) == ERR)
    {
        LOG_ERR("ps2 failed to write initial config\n");
        return ERR;
    }

    if (PS2_WRITE(cfg) == ERR)
    {
        LOG_ERR("ps2 failed to write initial config data\n");
        return ERR;
    }

    return 0;
}

static uint64_t ps2_self_test(void)
{
    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        LOG_ERR("ps2 failed to read config before self test\n");
        return ERR;
    }

    ps2_config_bits_t saved_cfg;
    if (PS2_READ(&saved_cfg) == ERR)
    {
        LOG_ERR("ps2 failed to read config data before self test\n");
        return ERR;
    }

    if (ps2_cmd(PS2_CMD_SELF_TEST) == ERR)
    {
        LOG_ERR("ps2 failed to send self test command\n");
        return ERR;
    }

    ps2_self_test_response_t response;
    if (PS2_READ(&response) == ERR)
    {
        LOG_ERR("ps2 failed to read self test response\n");
        return ERR;
    }

    if (response != PS2_SELF_TEST_PASS)
    {
        panic(NULL, "ps2 self test %s", ps2_self_test_response_to_string(response));
    }

    if (ps2_cmd(PS2_CMD_CFG_WRITE) == ERR)
    {
        LOG_ERR("ps2 failed to restore config after self test\n");
        return ERR;
    }

    if (PS2_WRITE(saved_cfg) == ERR)
    {
        LOG_ERR("ps2 failed to restore config data after self test\n");
        return ERR;
    }

    return 0;
}

static uint64_t ps2_check_if_dual_channel(void)
{
    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        LOG_ERR("ps2 failed to read config for dual channel check\n");
        return ERR;
    }

    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        LOG_ERR("ps2 failed to read config data for dual channel check\n");
        return ERR;
    }

    if (cfg & PS2_CFG_SECOND_CLOCK_DISABLE)
    {
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to send second port enable command\n");
            return ERR;
        }

        if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
        {
            LOG_ERR("ps2 failed to read config after second port enable\n");
            return ERR;
        }

        if (PS2_READ(&cfg) == ERR)
        {
            LOG_ERR("ps2 failed to read config data after second port enable\n");
            return ERR;
        }

        if (!(cfg & PS2_CFG_SECOND_CLOCK_DISABLE))
        {
            isDualChannel = true;
            LOG_INFO("dual channel PS/2 controller detected\n");

            if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
            {
                LOG_ERR("ps2 failed to disable second port after detection\n");
                return ERR;
            }
        }
        else
        {
            isDualChannel = false;
            LOG_INFO("single channel PS/2 controller detected\n");
        }
    }
    else
    {
        isDualChannel = false;
        LOG_INFO("single channel PS/2 controller detected (second port clock not disabled)\n");
    }

    return 0;
}

static uint64_t ps2_device_test(ps2_device_t device)
{
    ps2_cmd_t testCmd = (device == PS2_DEVICE_FIRST) ? PS2_CMD_FIRST_TEST : PS2_CMD_SECOND_TEST;
    if (ps2_cmd(testCmd) == ERR)
    {
        LOG_ERR("ps2 failed to send %s device test command\n", ps2_device_to_string(device));
        return ERR;
    }

    ps2_device_test_response_t response;
    if (PS2_READ(&response) == ERR)
    {
        LOG_ERR("ps2 failed to read %s device test response\n", ps2_device_to_string(device));
        return ERR;
    }

    if (response != PS2_DEVICE_TEST_PASS)
    {
        LOG_ERR("ps2 %s device test failed (%s)\n",
                ps2_device_to_string(device), ps2_device_test_response_to_string(response));
        return ERR;
    }

    return 0;
}

static uint64_t ps2_device_reset(ps2_device_t device)
{
    if (ps2_device_cmd(device, PS2_DEV_RESET) == ERR)
    {
        LOG_ERR("ps2 %s device reset command failed\n", ps2_device_to_string(device));
        return ERR;
    }

    uint8_t response[2];
    if (PS2_READ(&response[0]) == ERR)
    {
        LOG_ERR("ps2 %s device reset failed to read first response\n", ps2_device_to_string(device));
        return ERR;
    }

    if (PS2_READ(&response[1]) == ERR)
    {
        LOG_ERR("ps2 %s device reset failed to read second response\n", ps2_device_to_string(device));
        return ERR;
    }

    bool gotFa = (response[0] == PS2_DEVICE_RESET_PASS_1) || (response[1] == PS2_DEVICE_RESET_PASS_1);
    bool gotAa = (response[0] == PS2_DEVICE_RESET_PASS_2) || (response[1] == PS2_DEVICE_RESET_PASS_2);

    if (!gotFa || !gotAa)
    {
        LOG_ERR("ps2 %s device reset failed, invalid response sequence 0x%02x 0x%02x\n",
                ps2_device_to_string(device), response[0], response[1]);
        return ERR;
    }

    return 0;
}

static uint64_t ps2_device_disable_scanning(ps2_device_t device)
{
    if (ps2_device_cmd(device, PS2_DEV_DISABLE_SCANNING) == ERR)
    {
        LOG_ERR("ps2 %s device failed to send disable scanning command\n", ps2_device_to_string(device));
        return ERR;
    }

    uint8_t ack;
    if (PS2_READ(&ack) == ERR)
    {
        LOG_ERR("ps2 %s device failed to read disable scanning response\n", ps2_device_to_string(device));
        return ERR;
    }

    // TODO: Mice seem to return 0 as a valid response, i havent looked to much into this, but im aware that the data reporting command should probably be used for mice instead. For now this works, but might cause race conditions?
    if (ack != PS2_DEVICE_ACK && ack != 0)
    {
        LOG_ERR("ps2 %s disable scanning failed, unexpected response 0x%02x\n",
                ps2_device_to_string(device), ack);
        return ERR;
    }

    return 0;
}

static uint64_t ps2_device_enable_scanning(ps2_device_t device)
{
    if (ps2_device_cmd(device, PS2_DEV_ENABLE_SCANNING) == ERR)
    {
        LOG_ERR("ps2 %s device failed to send enable scanning command\n", ps2_device_to_string(device));
        return ERR;
    }

    uint8_t ack;
    if (PS2_READ(&ack) == ERR)
    {
        LOG_ERR("ps2 %s device failed to read enable scanning response\n", ps2_device_to_string(device));
        return ERR;
    }

    if (ack != PS2_DEVICE_ACK)
    {
        LOG_ERR("ps2 %s device enable scanning failed, expected 0xFA got 0x%02x\n",
                ps2_device_to_string(device), ack);
        return ERR;
    }

    return 0;
}

static uint64_t ps2_device_identify(ps2_device_t device, ps2_device_info_t* info)
{
    if (ps2_device_reset(device) == ERR)
    {
        LOG_ERR("ps2 %s device identify failed at reset\n", ps2_device_to_string(device));
        return ERR;
    }

    if (ps2_device_disable_scanning(device) == ERR)
    {
        LOG_ERR("ps2 %s device identify failed to disable scanning\n", ps2_device_to_string(device));
        return ERR;
    }

    if (ps2_device_cmd(device, PS2_DEV_IDENTIFY) == ERR)
    {
        LOG_ERR("ps2 %s device failed to send identify command\n", ps2_device_to_string(device));
        return ERR;
    }

    uint8_t ack;
    if (PS2_READ(&ack) == ERR)
    {
        LOG_ERR("ps2 %s device failed to read identify ACK\n", ps2_device_to_string(device));
        return ERR;
    }

    if (ack != PS2_DEVICE_ACK)
    {
        LOG_ERR("ps2 %s device identify failed, expected ACK (0xFA) got 0x%02x\n",
                ps2_device_to_string(device), ack);
        return ERR;
    }

    uint8_t id[2] = {0};
    uint8_t idLength = 0;

    errno = 0;
    if (PS2_READ(&id[0]) == 0)
    {
        idLength = 1;
        LOG_DEBUG("ps2 %s device ID byte 0: 0x%02x\n", ps2_device_to_string(device), id[0]);

        errno = 0;
        if (PS2_READ(&id[1]) == 0)
        {
            idLength = 2;
            LOG_DEBUG("ps2 %s device ID byte 1: 0x%02x\n", ps2_device_to_string(device), id[1]);
        }
        else if (errno != ETIMEDOUT)
        {
            LOG_ERR("ps2 %s device failed to read second ID byte (non-timeout error)\n",
                    ps2_device_to_string(device));
            return ERR;
        }
    }
    else if (errno == ETIMEDOUT)
    {
        idLength = 0;
        LOG_DEBUG("ps2 %s device sent no ID bytes (ancient device)\n", ps2_device_to_string(device));
    }
    else
    {
        LOG_ERR("ps2 %s device failed to read first ID byte (non-timeout error)\n",
                ps2_device_to_string(device));
        return ERR;
    }

    if (ps2_device_enable_scanning(device) == ERR)
    {
        LOG_ERR("ps2 %s device identify failed to enable scanning\n", ps2_device_to_string(device));
        return ERR;
    }

    info->device = device;
    memcpy(info->id, id, idLength);
    info->idLength = idLength;
    info->data = NULL;

    for (uint8_t i = 0; i < PS2_KNOWN_DEVICE_AMOUNT; i++)
    {
        if (idLength == knownDevices[i].idLength &&
            memcmp(id, knownDevices[i].id, idLength) == 0)
        {
            info->type = knownDevices[i].type;
            info->name = knownDevices[i].name;
            LOG_INFO("ps2 %s device identified as '%s', type %s\n",
                     ps2_device_to_string(device), info->name, ps2_device_type_to_string(info->type));
            return 0;
        }
    }

    ps2_device_type_t assumedType = (device == PS2_DEVICE_FIRST) ? PS2_DEVICE_TYPE_KEYBOARD : PS2_DEVICE_TYPE_MOUSE;

    LOG_WARN("ps2 %s device unknown ID (length=%d), assuming %s by convention\n",
             ps2_device_to_string(device), idLength, ps2_device_type_to_string(assumedType));

    info->type = assumedType;
    info->name = "unknown";

    if (idLength > 0)
    {
        if (idLength == 1)
        {
            LOG_WARN("ps2 unknown device ID: 0x%02x\n", id[0]);
        }
        else
        {
            LOG_WARN("ps2 unknown device ID: 0x%02x 0x%02x\n", id[0], id[1]);
        }
    }

    return 0;
}

static uint64_t ps2_device_init(ps2_device_info_t *info)
{
    if (info->type == PS2_DEVICE_TYPE_KEYBOARD)
    {
        if (ps2_kbd_init(info) == ERR)
        {
            LOG_ERR("ps2 keyboard initialization failed\n");
            return ERR;
        }
    }
    else if (info->type == PS2_DEVICE_TYPE_MOUSE)
    {
        if (ps2_mouse_init(info) == ERR)
        {
            LOG_ERR("ps2 mouse initialization failed\n");
            return ERR;
        }
    }
    else
    {
        LOG_WARN("ps2 unknown device type, skipping device specific initialization\n");
    }

    return 0;
}

static uint64_t ps2_devices_init(void)
{
    bool firstAvailable = false;
    bool secondAvailable = false;

    if (ps2_device_test(PS2_DEVICE_FIRST) == 0)
    {
        firstAvailable = true;
        LOG_INFO("ps2 first device available\n");
    }
    else
    {
        LOG_WARN("ps2 first device test failed, device unavailable\n");
    }

    if (isDualChannel)
    {
        if (ps2_device_test(PS2_DEVICE_SECOND) == 0)
        {
            secondAvailable = true;
            LOG_INFO("ps2 second device available\n");
        }
        else
        {
            LOG_WARN("ps2 second device test failed, device unavailable\n");
        }
    }

    if (!firstAvailable && !secondAvailable)
    {
        LOG_ERR("ps2 no working devices found\n");
        return ERR;
    }
    if (firstAvailable)
    {
        if (ps2_cmd(PS2_CMD_FIRST_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to enable first device\n");
            return ERR;
        }
    }

    if (secondAvailable)
    {
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to enable second device\n");
            return ERR;
        }
    }

    if (ps2_cmd(PS2_CMD_CFG_READ) == ERR)
    {
        LOG_ERR("ps2 failed to read config for interrupt setup\n");
        return ERR;
    }

    ps2_config_bits_t cfg;
    if (PS2_READ(&cfg) == ERR)
    {
        LOG_ERR("ps2 failed to read config data for interrupt setup\n");
        return ERR;
    }

    if (firstAvailable)
    {
        cfg |= PS2_CFG_FIRST_IRQ;
    }
    if (secondAvailable)
    {
        cfg |= PS2_CFG_SECOND_IRQ;
    }
    cfg &= ~PS2_CFG_FIRST_TRANSLATION;
    LOG_DEBUG("ps2 setting final config byte: 0x%02x\n", cfg);

    if (ps2_cmd(PS2_CMD_CFG_WRITE) == ERR)
    {
        LOG_ERR("ps2 failed to write final config\n");
        return ERR;
    }

    if (PS2_WRITE(cfg) == ERR)
    {
        LOG_ERR("ps2 failed to write final config data\n");
        return ERR;
    }

    if (firstAvailable)
    {
        if (ps2_device_identify(PS2_DEVICE_FIRST, &firstDevice) == ERR)
        {
            LOG_ERR("ps2 first device identification failed\n");
            firstAvailable = false;
        }
        else
        {
            if (ps2_device_init(&firstDevice) == ERR)
            {
                LOG_ERR("ps2 first device initialization failed\n");
                firstAvailable = false;
            }
        }
    }

    if (secondAvailable)
    {
        if (ps2_device_identify(PS2_DEVICE_SECOND, &secondDevice) == ERR)
        {
            LOG_ERR("ps2 second device identification failed\n");
            secondAvailable = false;
        }
        else
        {
            if (ps2_device_init(&secondDevice) == ERR)
            {
                LOG_ERR("ps2 second device initialization failed\n");
                secondAvailable = false;
            }
        }
    }

    if (!firstAvailable && !secondAvailable)
    {
        LOG_WARN("ps2 no devices successfully initialized\n");
        return ERR;
    }

    return 0;
}

void ps2_init(void)
{
    if (!(fadt_get()->bootArchFlags & FADT_BOOT_ARCH_PS2_EXISTS))
    {
        panic(NULL, "ps2 not supported by hardware (ACPI FADT indicates no PS/2 controller)");
    }
    LOG_INFO("ps2 controller detected via FADT\n");

    if (ps2_cmd(PS2_CMD_FIRST_DISABLE) == ERR)
    {
        panic(NULL, "ps2 first device disable failed");
    }
    if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
    {
        panic(NULL, "ps2 second device disable failed");
    }

    port_inb(PS2_PORT_DATA); // Discard

    if (ps2_set_initial_config() == ERR)
    {
        panic(NULL, "ps2 initial configuration failed");
    }

    if (ps2_self_test() == ERR)
    {
        panic(NULL, "ps2 controller self test failed");
    }

    if (ps2_check_if_dual_channel() == ERR)
    {
        panic(NULL, "ps2 dual channel detection failed");
    }

    if (ps2_devices_init() == ERR)
    {
        panic(NULL, "ps2 device initialization failed - no working devices");
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

        if (currentStatus & PS2_STATUS_OUT_FULL)
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
        panic(NULL, "invalid ps2 device specified");
        return ERR;
    }
}
