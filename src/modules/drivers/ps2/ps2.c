#include "ps2.h"
#include "ps2_kbd.h"
#include "ps2_mouse.h"

#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/resources.h>
#include <kernel/acpi/tables.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <errno.h>
#include <string.h>

static port_t dataPort = 0;
static port_t statusPort = 0;
static port_t commandPort = 0;
static bool isDualChannel = false;
static bool initialized = false;

static ps2_device_info_t devices[PS2_DEV_COUNT] = {0};
static lock_t attachLock = LOCK_CREATE;

// TODO: Add more known devices
static const ps2_device_info_t knownDevices[] = {
    {.type = PS2_DEV_TYPE_MOUSE_STANDARD, .name = "Standard PS/2 mouse", .firstIdByte = 0x00},
    {.type = PS2_DEV_TYPE_MOUSE_SCROLL, .name = "Mouse with scroll wheel", .firstIdByte = 0x03},
    {.type = PS2_DEV_TYPE_MOUSE_5BUTTON, .name = "5-button mouse", .firstIdByte = 0x04},
    {.type = PS2_DEV_TYPE_KEYBOARD, .name = "Standard PS/2 keyboard", .firstIdByte = 0xAB},
    {.type = PS2_DEV_TYPE_KEYBOARD, .name = "NCD Sun keyboard", .firstIdByte = 0xAC},
    {.type = PS2_DEV_TYPE_KEYBOARD, .name = "Trust keyboard", .firstIdByte = 0x5D},
    {.type = PS2_DEV_TYPE_KEYBOARD, .name = "NMB SGI keyboard", .firstIdByte = 0x47},
};
#define PS2_KNOWN_DEVICE_AMOUNT (sizeof(knownDevices) / sizeof(knownDevices[0]))

#define PS2_ANCIENT_KEYBOARD_ID 0xFF

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

static const char* ps2_device_test_response_to_string(ps2_device_test_response_t response)
{
    switch (response)
    {
    case PS2_DEV_TEST_PASS:
        return "pass";
    case PS2_DEV_TEST_CLOCK_STUCK_LOW:
        return "clock stuck low";
    case PS2_DEV_TEST_CLOCK_STUCK_HIGH:
        return "clock stuck high";
    case PS2_DEV_TEST_DATA_STUCK_LOW:
        return "data stuck low";
    case PS2_DEV_TEST_DATA_STUCK_HIGH:
        return "data stuck high";
    default:
        return "invalid response";
    }
}

static const char* ps2_device_to_string(ps2_device_t device)
{
    switch (device)
    {
    case PS2_DEV_FIRST:
        return "first";
    case PS2_DEV_SECOND:
        return "second";
    default:
        return "invalid device";
    }
}

static const char* ps2_device_type_to_string(ps2_device_type_t type)
{
    switch (type)
    {
    case PS2_DEV_TYPE_NONE:
        return "unknown";
    case PS2_DEV_TYPE_KEYBOARD:
        return "keyboard";
    case PS2_DEV_TYPE_MOUSE_STANDARD:
        return "mouse standard";
    case PS2_DEV_TYPE_MOUSE_SCROLL:
        return "mouse scroll";
    case PS2_DEV_TYPE_MOUSE_5BUTTON:
        return "mouse 5button";
    default:
        return "invalid device type";
    }
}

void ps2_drain(void)
{
    sys_time_wait(PS2_SMALL_DELAY);
    while ((io_in8(statusPort)) & PS2_STATUS_OUT_FULL)
    {
        io_in8(dataPort);
        sys_time_wait(PS2_SMALL_DELAY);
    }
}

uint64_t ps2_wait_until_set(ps2_status_bits_t status)
{
    uint64_t startTime = sys_time_uptime();
    while ((io_in8(statusPort) & status) == 0)
    {
        if ((sys_time_uptime() - startTime) > PS2_WAIT_TIMEOUT)
        {
            errno = ETIMEDOUT;
            return ERR;
        }
        asm volatile("pause");
    }
    return 0;
}

uint64_t ps2_wait_until_clear(ps2_status_bits_t status)
{
    uint64_t startTime = sys_time_uptime();
    while ((io_in8(statusPort) & status) != 0)
    {
        if ((sys_time_uptime() - startTime) > PS2_WAIT_TIMEOUT)
        {
            errno = ETIMEDOUT;
            return ERR;
        }
        asm volatile("pause");
    }
    return 0;
}

uint64_t ps2_read(void)
{
    if (ps2_wait_until_set(PS2_STATUS_OUT_FULL) == ERR)
    {
        errno = ETIMEDOUT;
        return ERR;
    }
    return io_in8(dataPort);
}

uint64_t ps2_write(uint8_t data)
{
    if (ps2_wait_until_clear(PS2_STATUS_IN_FULL) == ERR)
    {
        errno = ETIMEDOUT;
        return ERR;
    }
    io_out8(dataPort, data);
    return 0;
}

uint64_t ps2_cmd(ps2_cmd_t command)
{
    if (ps2_wait_until_clear(PS2_STATUS_IN_FULL) == ERR)
    {
        errno = ETIMEDOUT;
        return ERR;
    }
    io_out8(commandPort, command);
    return 0;
}

uint64_t ps2_cmd_and_read(ps2_cmd_t command)
{
    if (ps2_cmd(command) == ERR)
    {
        return ERR;
    }
    return ps2_read();
}

uint64_t ps2_cmd_and_write(ps2_cmd_t command, uint8_t data)
{
    if (ps2_cmd(command) == ERR)
    {
        return ERR;
    }
    return ps2_write(data);
}

static uint64_t ps2_check_if_dual_channel(void)
{
    uint64_t cfg = ps2_cmd_and_read(PS2_CMD_CFG_READ);
    if (cfg == ERR)
    {
        LOG_ERR("ps2 failed to read config for dual channel check\n");
        return ERR;
    }

    if (cfg & PS2_CFG_SECOND_CLOCK_DISABLE)
    {
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to send second port enable command\n");
            return ERR;
        }

        cfg = ps2_cmd_and_read(PS2_CMD_CFG_READ);
        if (cfg == ERR)
        {
            LOG_ERR("ps2 failed to read config after second port enable\n");
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

static uint64_t ps2_device_ports_test(void)
{
    uint64_t response = ps2_cmd_and_read(PS2_CMD_FIRST_TEST);
    if (response == ERR || response != PS2_DEV_TEST_PASS)
    {
        devices[0].active = false;
        LOG_WARN("first port test failed with response '%s'\n", ps2_device_test_response_to_string(response));
    }
    else
    {
        devices[0].active = true;
    }

    if (!isDualChannel)
    {
        return 0;
    }

    response = ps2_cmd_and_read(PS2_CMD_SECOND_TEST);
    if (response == ERR || response != PS2_DEV_TEST_PASS)
    {
        devices[1].active = false;
        LOG_WARN("second port test failed with response '%s'\n", ps2_device_test_response_to_string(response));
    }
    else
    {
        devices[1].active = true;
    }

    return 0;
}

static uint64_t ps2_device_init(ps2_device_t device, irq_virt_t irq)
{
    ps2_device_info_t* info = &devices[device];
    info->device = device;

    if (ps2_device_cmd(device, PS2_DEV_CMD_RESET) == ERR)
    {
        LOG_ERR("%s port reset failed\n", ps2_device_to_string(device));
        return ERR;
    }
    sys_time_wait(PS2_LARGE_DELAY);

    uint64_t response = ps2_read();
    if (response == ERR)
    {
        LOG_ERR("ps2 %s device reset failed to read response\n", ps2_device_to_string(device));
        return ERR;
    }

    if (response != PS2_DEV_RESPONSE_BAT_OK)
    {
        LOG_ERR("ps2 %s device reset failed, invalid response 0x%02x\n", ps2_device_to_string(device), response);
        return ERR;
    }

    // The device might send its id bytes here, but we use the identify command to get the id for consistency so we just
    // drain the data instead.
    ps2_drain();

    if (ps2_device_cmd(device, PS2_DEV_CMD_DISABLE_SCANNING) == ERR)
    {
        LOG_ERR("ps2 %s device disable scanning failed\n", ps2_device_to_string(device));
        return ERR;
    }

    errno = EOK;
    response = ps2_device_cmd_and_read(device, PS2_DEV_CMD_IDENTIFY);
    if (response == ERR)
    {
        if (errno != ETIMEDOUT)
        {
            LOG_ERR("ps2 %s device identify failed\n", ps2_device_to_string(device));
            return ERR;
        }

        info->firstIdByte = PS2_ANCIENT_KEYBOARD_ID;
        info->type = PS2_DEV_TYPE_KEYBOARD;
        info->name = "Ancient AT keyboard";
        LOG_INFO("ps2 %s device identify timed out, probably 'Ancient AT keyboard'\n", ps2_device_to_string(device));
    }
    else
    {
        info->firstIdByte = response;
    }

    // We ignore the rest of the id, as only the first byte is needed to know the device type.
    ps2_drain();

    info->type = PS2_DEV_TYPE_NONE;
    info->name = "Unknown";
    info->irq = irq;
    for (size_t i = 0; i < PS2_KNOWN_DEVICE_AMOUNT; i++)
    {
        if (info->firstIdByte == knownDevices[i].firstIdByte)
        {
            info->type = knownDevices[i].type;
            info->name = knownDevices[i].name;
            break;
        }
    }

    if ((device == PS2_DEV_FIRST && info->type != PS2_DEV_TYPE_KEYBOARD) ||
        (device != PS2_DEV_FIRST && info->type == PS2_DEV_TYPE_KEYBOARD))
    {
        LOG_WARN("ps2 %s device type mismatch expected %s but detected %s\n", ps2_device_to_string(device),
            device == PS2_DEV_FIRST ? "keyboard" : "mouse", ps2_device_type_to_string(info->type));
    }

    LOG_INFO("ps2 %s device identified as '%s' with first ID byte 0x%02x\n", ps2_device_to_string(device), info->name,
        info->firstIdByte);

    if (info->type == PS2_DEV_TYPE_NONE)
    {
        LOG_ERR("ps2 %s device type unknown\n", ps2_device_to_string(device));
        return ERR;
    }

    if (info->type == PS2_DEV_TYPE_KEYBOARD)
    {
        if (ps2_kbd_init(info) == ERR)
        {
            LOG_ERR("ps2 %s device keyboard initialization failed\n", ps2_device_to_string(device));
            return ERR;
        }
    }
    else
    {
        if (ps2_mouse_init(info) == ERR)
        {
            LOG_ERR("ps2 %s device mouse initialization failed\n", ps2_device_to_string(device));
            return ERR;
        }
    }

    if (ps2_device_cmd(device, PS2_DEV_CMD_ENABLE_SCANNING) == ERR)
    {
        LOG_ERR("ps2 %s device enable scanning failed\n", ps2_device_to_string(device));
        return ERR;
    }

    return 0;
}

uint64_t ps2_device_cmd(ps2_device_t device, ps2_device_cmd_t command)
{
    for (uint8_t i = 0; i < PS2_COMMAND_RETRIES; i++)
    {
        if (device == PS2_DEV_SECOND)
        {
            if (ps2_cmd(PS2_CMD_SECOND_WRITE) == ERR)
            {
                return ERR;
            }
        }
        if (ps2_write(command) == ERR)
        {
            return ERR;
        }

        uint64_t response = ps2_read();
        if (response == ERR)
        {
            continue;
        }

        if (response == PS2_DEV_RESPONSE_ACK)
        {
            return 0;
        }

        if (response == PS2_DEV_RESPONSE_RESEND)
        {
            continue;
        }

        LOG_WARN("%s device cmd 0x%02x got unexpected response 0x%02x\n", ps2_device_to_string(device), command,
            response);
        return ERR;
    }

    LOG_ERR("%s device cmd 0x%02x failed after %d retries\n", ps2_device_to_string(device), command,
        PS2_COMMAND_RETRIES);
    return ERR;
}

uint64_t ps2_device_cmd_and_read(ps2_device_t device, ps2_device_cmd_t command)
{
    if (ps2_device_cmd(device, command) == ERR)
    {
        return ERR;
    }
    return ps2_read();
}

uint64_t ps2_device_sub_cmd(ps2_device_t device, ps2_device_cmd_t command, uint8_t subCommand)
{
    if (ps2_device_cmd(device, command) == ERR)
    {
        return ERR;
    }
    return ps2_device_cmd(device, subCommand);
}

static uint64_t ps2_controller_init(void)
{
    if (initialized)
    {
        return 0;
    }
    initialized = true;

    if (ps2_cmd(PS2_CMD_FIRST_DISABLE) == ERR)
    {
        LOG_ERR("ps2 first device disable failed during controller init\n");
        return ERR;
    }
    if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
    {
        LOG_ERR("ps2 second device disable failed during controller init\n");
        return ERR;
    }
    ps2_drain();

    uint64_t cfg = ps2_cmd_and_read(PS2_CMD_CFG_READ);
    if (cfg == ERR)
    {
        LOG_ERR("ps2 failed to read initial config\n");
        return ERR;
    }

    LOG_DEBUG("ps2 initial config byte: 0x%02x\n", cfg);
    cfg &= ~(PS2_CFG_FIRST_IRQ | PS2_CFG_FIRST_CLOCK_DISABLE | PS2_CFG_FIRST_TRANSLATION | PS2_CFG_SECOND_IRQ);
    LOG_DEBUG("ps2 setting config byte to: 0x%02x\n", cfg);

    if (ps2_cmd_and_write(PS2_CMD_CFG_WRITE, cfg) == ERR)
    {
        LOG_ERR("ps2 failed to write initial config\n");
        return ERR;
    }

    uint64_t response = ps2_cmd_and_read(PS2_CMD_SELF_TEST);
    if (response == ERR)
    {
        LOG_ERR("ps2 failed to send self test command\n");
        return ERR;
    }

    if (response != PS2_SELF_TEST_PASS)
    {
        LOG_ERR("ps2 self test failed %s", ps2_self_test_response_to_string(response));
        return ERR;
    }

    if (ps2_cmd_and_write(PS2_CMD_CFG_WRITE, cfg) == ERR)
    {
        LOG_ERR("ps2 failed rewrite config byte\n");
        return ERR;
    }

    return 0;
}

static void ps2_controller_deinit(void)
{
    if (!initialized)
    {
        return;
    }
    initialized = false;

    for (uint64_t i = 0; i < PS2_DEV_COUNT; i++)
    {
        if (devices[i].active && devices[i].private != NULL)
        {
            if (devices[i].type == PS2_DEV_TYPE_NONE)
            {
                continue;
            }

            if (devices[i].type == PS2_DEV_TYPE_KEYBOARD)
            {
                ps2_kbd_deinit(&devices[i]);
            }
            else
            {
                ps2_mouse_deinit(&devices[i]);
            }
        }
    }

    if (ps2_cmd(PS2_CMD_FIRST_DISABLE) == ERR)
    {
        LOG_WARN("ps2 first device disable failed during deinit\n");
    }

    if (ps2_cmd(PS2_CMD_SECOND_DISABLE) == ERR)
    {
        LOG_WARN("ps2 second device disable failed during deinit\n");
    }
}

static uint64_t ps2_attach_device(const char* type, const char* name)
{
    LOCK_SCOPE(&attachLock);

    acpi_device_cfg_t* acpiCfg = acpi_device_cfg_get(name);
    if (acpiCfg == NULL)
    {
        LOG_ERR("ps2 failed to get ACPI device config for '%s'\n", name);
        return ERR;
    }

    // The first device attached will initialize the controller
    if (!initialized)
    {
        if (acpiCfg->ioCount != 2)
        {
            LOG_ERR("ps2 device '%s' has invalid IO resource count %d\n", name, acpiCfg->ioCount);
            return ERR;
        }

        if (acpiCfg->ios[0].length < 1 || acpiCfg->ios[1].length < 1)
        {
            LOG_ERR("ps2 device '%s' has invalid IO resource lengths\n", name);
            return ERR;
        }

        dataPort = acpiCfg->ios[0].base;
        statusPort = acpiCfg->ios[1].base;
        commandPort = statusPort; // Command port is the same as status port

        if (ps2_controller_init() == ERR)
        {
            LOG_ERR("ps2 controller initialization failed\n");
            return ERR;
        }

        if (ps2_check_if_dual_channel() == ERR)
        {
            LOG_ERR("ps2 dual channel detection failed\n");
            return ERR;
        }

        if (ps2_device_ports_test() == ERR)
        {
            LOG_ERR("ps2 devices test failed\n");
            return ERR;
        }
    }

    if (acpiCfg->irqCount != 1)
    {
        LOG_ERR("ps2 device '%s' has invalid IRQ resource count %d\n", name, acpiCfg->irqCount);
        return ERR;
    }
    irq_virt_t irq = acpiCfg->irqs[0].virt;

    ps2_device_t targetDevice = PS2_DEV_FIRST;
    if (module_device_types_contains(PS2_MOUSE_PNP_IDS, type))
    {
        targetDevice = PS2_DEV_SECOND;
    }

    if (!devices[targetDevice].active)
    {
        LOG_ERR("ps2 device '%s' cannot be attached to %s port (port is not active)\n", name,
            ps2_device_to_string(targetDevice));
        return ERR;
    }

    if (devices[targetDevice].type != PS2_DEV_TYPE_NONE)
    {
        LOG_ERR("ps2 device '%s' cannot be attached to %s port (port already in use by %s)\n", name,
            ps2_device_to_string(targetDevice), devices[targetDevice].name);
        return ERR;
    }

    if (targetDevice == PS2_DEV_FIRST)
    {
        if (ps2_cmd(PS2_CMD_FIRST_ENABLE) == ERR)
        {
            LOG_ERR("ps2 first device enable failed\n");
            return ERR;
        }
    }
    else
    {
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            LOG_ERR("ps2 second device enable failed\n");
            return ERR;
        }
    }

    if (ps2_device_init(targetDevice, irq) == ERR)
    {
        LOG_ERR("ps2 failed to initialize device '%s' on %s port\n", name, ps2_device_to_string(targetDevice));
        return ERR;
    }

    uint64_t cfg = ps2_cmd_and_read(PS2_CMD_CFG_READ);
    if (cfg == ERR)
    {
        LOG_ERR("ps2 failed to read config for final setup\n");
        return ERR;
    }

    if (targetDevice == PS2_DEV_FIRST)
    {
        cfg |= PS2_CFG_FIRST_IRQ;
    }
    else
    {
        cfg |= PS2_CFG_SECOND_IRQ;
    }

    if (ps2_cmd_and_write(PS2_CMD_CFG_WRITE, cfg) == ERR)
    {
        LOG_ERR("ps2 failed to write new config\n");
        return ERR;
    }

    return 0;
}

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (ps2_attach_device(event->deviceAttach.type, event->deviceAttach.name) == ERR)
        {
            return ERR;
        }
        break;
    case MODULE_EVENT_UNLOAD:
        ps2_controller_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("PS2 Driver", "Kai Norberg", "A PS/2 keyboard and mouse driver", OS_VERSION, "MIT",
    PS2_KEYBOARD_PNP_IDS ";" PS2_MOUSE_PNP_IDS);