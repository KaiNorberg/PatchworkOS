#include "ps2.h"
#include "ps2_kbd.h"
#include "ps2_mouse.h"

#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>
#include <modules/acpi/aml/object.h>
#include <modules/acpi/devices.h>
#include <modules/acpi/resources.h>
#include <modules/acpi/tables.h>

#include <errno.h>
#include <string.h>
#include <time.h>

static port_t dataPort = 0;
static port_t statusPort = 0;
static port_t commandPort = 0;

static uint64_t currentConfig = 0;

static bool isDualChannel = false;
static bool controllerInitialized = false;

static ps2_known_device_t knownKeyboards[] = {
    {"PNP0300", "IBM PC/XT keyboard controller (83-key)"},
    {"PNP0301", "IBM PC/AT keyboard controller (86-key)"},
    {"PNP0302", "IBM PC/XT keyboard controller (84-key)"},
    {"PNP0303", "IBM Enhanced (101/102-key, PS/2 mouse support)"},
    {"PNP0304", "Olivetti Keyboard (83-key)"},
    {"PNP0305", "Olivetti Keyboard (102-key)"},
    {"PNP0306", "Olivetti Keyboard (86-key)"},
    {"PNP0307", "Microsoft Windows(R) Keyboard"},
    {"PNP0308", "General Input Device Emulation Interface (GIDEI) legacy"},
    {"PNP0309", "Olivetti Keyboard (A101/102 key)"},
    {"PNP030A", "AT&T 302 keyboard"},
    {"PNP030B", "Reserved by Microsoft"},
    {"PNP0320", "Japanese 101-key keyboard"},
    {"PNP0321", "Japanese AX keyboard"},
    {"PNP0322", "Japanese 106-key keyboard A01"},
    {"PNP0323", "Japanese 106-key keyboard 002/003"},
    {"PNP0324", "Japanese 106-key keyboard 001"},
    {"PNP0325", "Japanese Toshiba Desktop keyboard"},
    {"PNP0326", "Japanese Toshiba Laptop keyboard"},
    {"PNP0327", "Japanese Toshiba Notebook keyboard"},
    {"PNP0340", "Korean 84-key keyboard"},
    {"PNP0341", "Korean 86-key keyboard"},
    {"PNP0342", "Korean Enhanced keyboard"},
    {"PNP0343", "Korean Enhanced keyboard 101(b/c)"},
    {"PNP0344", "Korean Enhanced keyboard 103"},
};

static ps2_known_device_t knownMice[] = {
    {"PNP0F00", "Microsoft Bus Mouse"},
    {"PNP0F01", "Microsoft Serial Mouse"},
    {"PNP0F02", "Microsoft InPort Mouse"},
    {"PNP0F03", "Microsoft PS/2-style Mouse"},
    {"PNP0F04", "Mouse Systems Mouse"},
    {"PNP0F05", "Mouse Systems 3-Button Mouse (COM2)"},
    {"PNP0F06", "Genius Mouse (COM1)"},
    {"PNP0F07", "Genius Mouse (COM2)"},
    {"PNP0F08", "Logitech Serial Mouse"},
    {"PNP0F09", "Microsoft BallPoint Serial Mouse"},
    {"PNP0F0A", "Microsoft Plug and Play Mouse"},
    {"PNP0F0B", "Microsoft Plug and Play BallPoint Mouse"},
    {"PNP0F0C", "Microsoft-compatible Serial Mouse"},
    {"PNP0F0D", "Microsoft-compatible InPort-compatible Mouse"},
    {"PNP0F0E", "Microsoft-compatible PS/2-style Mouse"},
    {"PNP0F0F", "Microsoft-compatible Serial BallPoint-compatible Mouse"},
    {"PNP0F10", "Texas Instruments QuickPort Mouse"},
    {"PNP0F11", "Microsoft-compatible Bus Mouse"},
    {"PNP0F12", "Logitech PS/2-style Mouse"},
    {"PNP0F13", "PS/2 Port for PS/2-style Mice"},
    {"PNP0F14", "Microsoft Kids Mouse"},
    {"PNP0F15", "Logitech bus mouse"},
    {"PNP0F16", "Logitech SWIFT device"},
    {"PNP0F17", "Logitech-compatible serial mouse"},
    {"PNP0F18", "Logitech-compatible bus mouse"},
    {"PNP0F19", "Logitech-compatible PS/2-style Mouse"},
    {"PNP0F1A", "Logitech-compatible SWIFT Device"},
    {"PNP0F1B", "HP Omnibook Mouse"},
    {"PNP0F1C", "Compaq LTE Trackball PS/2-style Mouse"},
    {"PNP0F1D", "Compaq LTE Trackball Serial Mouse"},
    {"PNP0F1E", "Microsoft Kids Trackball Mouse"},
    {"PNP0F1F", "Reserved by Microsoft Input Device Group"},
    {"PNP0F20", "Reserved by Microsoft Input Device Group"},
    {"PNP0F21", "Reserved by Microsoft Input Device Group"},
    {"PNP0F22", "Reserved by Microsoft Input Device Group"},
    {"PNP0F23", "Reserved by Microsoft Input Device Group"},
    {"PNP0FFC", "Reserved (temporarily) by Microsoft Kernel team"},
    {"PNP0FFF", "Reserved by Microsoft Systems (SDA Standard Compliant SD Host Controller Vendor)"},
};

static ps2_device_info_t devices[PS2_DEV_COUNT] = {
    [0] = {.device = PS2_DEV_FIRST},
    [1] = {.device = PS2_DEV_SECOND},
};
static lock_t attachLock = LOCK_CREATE();

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

static uint64_t ps2_controller_init(void)
{
    if (controllerInitialized)
    {
        return 0;
    }
    controllerInitialized = true;

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

    currentConfig = ps2_cmd_and_read(PS2_CMD_CFG_READ);
    if (currentConfig == ERR)
    {
        LOG_ERR("ps2 failed to read initial config\n");
        return ERR;
    }

    LOG_DEBUG("ps2 initial config byte: 0x%02x\n", currentConfig);
    currentConfig &=
        ~(PS2_CFG_FIRST_IRQ | PS2_CFG_FIRST_CLOCK_DISABLE | PS2_CFG_FIRST_TRANSLATION | PS2_CFG_SECOND_IRQ);
    LOG_DEBUG("ps2 setting config byte to: 0x%02x\n", currentConfig);

    if (ps2_cmd_and_write(PS2_CMD_CFG_WRITE, currentConfig) == ERR)
    {
        LOG_ERR("ps2 failed to write initial config\n");
        return ERR;
    }

    return 0;
}

static void ps2_controller_deinit(void)
{
    if (!controllerInitialized)
    {
        return;
    }
    controllerInitialized = false;

    if (devices[PS2_DEV_FIRST].initialized)
    {
        ps2_kbd_deinit(&devices[PS2_DEV_FIRST]);
    }

    if (devices[PS2_DEV_SECOND].initialized)
    {
        ps2_mouse_deinit(&devices[PS2_DEV_SECOND]);
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

static uint64_t ps2_self_test(void)
{
    uint64_t cfg = ps2_cmd_and_read(PS2_CMD_CFG_READ);
    if (cfg == ERR)
    {
        LOG_ERR("ps2 failed to read config byte.\n");
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
        LOG_ERR("ps2 failed to write config byte.\n");
        return ERR;
    }

    return 0;
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

static uint64_t ps2_devices_test(void)
{
    uint64_t response = ps2_cmd_and_read(PS2_CMD_FIRST_TEST);
    if (response == ERR || response != PS2_DEV_TEST_PASS)
    {
        LOG_ERR("first port test failed with response %s.\n", ps2_device_test_response_to_string(response));
        return ERR;
    }

    if (!isDualChannel)
    {
        return 0;
    }

    response = ps2_cmd_and_read(PS2_CMD_SECOND_TEST);
    if (response == ERR || response != PS2_DEV_TEST_PASS)
    {
        LOG_ERR("second port test failed with response %s.\n", ps2_device_test_response_to_string(response));
        return ERR;
    }

    return 0;
}

void ps2_drain(void)
{
    clock_wait(PS2_SMALL_DELAY);
    while ((in8(statusPort)) & PS2_STATUS_OUT_FULL)
    {
        in8(dataPort);
        clock_wait(PS2_SMALL_DELAY);
    }
}

uint64_t ps2_wait_until_set(ps2_status_bits_t status)
{
    uint64_t startTime = clock_uptime();
    while ((in8(statusPort) & status) == 0)
    {
        if ((clock_uptime() - startTime) > PS2_WAIT_TIMEOUT)
        {
            errno = ETIMEDOUT;
            return ERR;
        }
        ASM("pause");
    }
    return 0;
}

uint64_t ps2_wait_until_clear(ps2_status_bits_t status)
{
    uint64_t startTime = clock_uptime();
    while ((in8(statusPort) & status) != 0)
    {
        if ((clock_uptime() - startTime) > PS2_WAIT_TIMEOUT)
        {
            errno = ETIMEDOUT;
            return ERR;
        }
        ASM("pause");
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
    return in8(dataPort);
}

uint64_t ps2_read_no_wait(void)
{
    if (!(in8(statusPort) & PS2_STATUS_OUT_FULL))
    {
        errno = EAGAIN;
        return ERR;
    }

    return in8(dataPort);
}

uint64_t ps2_write(uint8_t data)
{
    if (ps2_wait_until_clear(PS2_STATUS_IN_FULL) == ERR)
    {
        errno = ETIMEDOUT;
        return ERR;
    }
    out8(dataPort, data);
    return 0;
}

uint64_t ps2_cmd(ps2_cmd_t command)
{
    if (ps2_wait_until_clear(PS2_STATUS_IN_FULL) == ERR)
    {
        errno = ETIMEDOUT;
        return ERR;
    }
    out8(commandPort, command);
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

static uint64_t ps2_device_init(ps2_device_t device)
{
    ps2_device_info_t* info = &devices[device];

    if (ps2_device_cmd(device, PS2_DEV_CMD_RESET) == ERR)
    {
        LOG_ERR("%s port reset failed\n", ps2_device_to_string(device));
        return ERR;
    }
    clock_wait(PS2_LARGE_DELAY);

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

    // The device might send its id bytes here, but we don't care about them.
    ps2_drain();

    if (ps2_device_cmd(device, PS2_DEV_CMD_DISABLE_SCANNING) == ERR)
    {
        LOG_ERR("ps2 %s device disable scanning failed\n", ps2_device_to_string(device));
        return ERR;
    }

    if (device == PS2_DEV_FIRST)
    {
        LOG_INFO("found PS/2 keyboard '%s' on IRQ %u\n", info->name, info->irq);
        if (ps2_kbd_init(info) == ERR)
        {
            LOG_ERR("ps2 %s device keyboard initialization failed\n", ps2_device_to_string(device));
            return ERR;
        }
    }
    else
    {
        LOG_INFO("found PS/2 mouse '%s' on IRQ %u\n", info->name, info->irq);
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

    info->initialized = true;
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

static uint64_t ps2_attach_device(const char* type, const char* name)
{
    LOCK_SCOPE(&attachLock);

    acpi_device_cfg_t* acpiCfg = acpi_device_cfg_lookup(name);
    if (acpiCfg == NULL)
    {
        LOG_ERR("ps2 failed to get ACPI device config for '%s'\n", name);
        return ERR;
    }

    // We dont know if the controllers resources will be specified on the first device or the second device,
    // so we might need to delay initialization of the first device until the second device is attached.
    if (acpiCfg->ioCount != 0)
    {
        if (controllerInitialized)
        {
            LOG_ERR("ps2 device '%s' cannot initialize controller (already initialized)\n", name);
            return ERR;
        }
        controllerInitialized = true;

        if (acpi_device_cfg_get_port(acpiCfg, 0, &dataPort) == ERR ||
            acpi_device_cfg_get_port(acpiCfg, 1, &statusPort) == ERR)
        {
            LOG_ERR("ps2 device '%s' has invalid status port resource\n", name);
            return ERR;
        }
        commandPort = statusPort; // Command port is the same as status port

        if (ps2_controller_init() == ERR)
        {
            LOG_ERR("ps2 controller initialization failed\n");
            return ERR;
        }
        if (ps2_self_test() == ERR)
        {
            LOG_ERR("ps2 controller self test failed\n");
            return ERR;
        }
        if (ps2_check_if_dual_channel() == ERR)
        {
            LOG_ERR("ps2 controller dual channel check failed\n");
            return ERR;
        }
        if (ps2_devices_test() == ERR)
        {
            LOG_ERR("ps2 controller devices test failed\n");
            return ERR;
        }
    }

    if (acpiCfg->irqCount != 1)
    {
        LOG_ERR("ps2 device '%s' has invalid IRQ resource count %d\n", name, acpiCfg->irqCount);
        return ERR;
    }

    ps2_device_t targetDevice;
    if (module_device_types_contains(PS2_KEYBOARD_PNP_IDS, type))
    {
        targetDevice = PS2_DEV_FIRST;
    }
    else if (module_device_types_contains(PS2_MOUSE_PNP_IDS, type))
    {
        targetDevice = PS2_DEV_SECOND;
    }
    else
    {
        LOG_ERR("ps2 device '%s' has unknown type '%s'\n", name, type);
        return ERR;
    }

    if (devices[targetDevice].attached)
    {
        LOG_ERR("ps2 device '%s' cannot be attached to %s port (port already attached)\n", name,
            ps2_device_to_string(targetDevice));
        return ERR;
    }

    if (devices[targetDevice].initialized)
    {
        LOG_ERR("ps2 device '%s' cannot be attached to %s port (port already in use by %s)\n", name,
            ps2_device_to_string(targetDevice), devices[targetDevice].name);
        return ERR;
    }

    devices[targetDevice].pnpId = type;

    devices[targetDevice].name = NULL;
    if (targetDevice == PS2_DEV_FIRST)
    {
        for (size_t i = 0; i < ARRAY_SIZE(knownKeyboards); i++)
        {
            if (strcmp(type, knownKeyboards[i].pnpId) == 0)
            {
                devices[targetDevice].name = knownKeyboards[i].name;
                break;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < ARRAY_SIZE(knownMice); i++)
        {
            if (strcmp(type, knownMice[i].pnpId) == 0)
            {
                devices[targetDevice].name = knownMice[i].name;
                break;
            }
        }
    }

    if (devices[targetDevice].name == NULL)
    {
        devices[targetDevice].name = "Unknown PS/2 Device";
        LOG_WARN("ps2 device '%s' has unknown PNP ID '%s'\n", name, type);
    }

    devices[targetDevice].irq = acpiCfg->irqs[0].virt;
    devices[targetDevice].attached = true;

    if (!controllerInitialized)
    {
        LOG_INFO("delaying ps2 device '%s' initialization (controller not initialized)\n", name);
        return 0;
    }

    uint64_t attachedAmount = devices[PS2_DEV_FIRST].attached + devices[PS2_DEV_SECOND].attached;
    if (isDualChannel && attachedAmount < PS2_DEV_COUNT)
    {
        LOG_INFO("delaying ps2 device '%s' initialization (waiting for other device)\n", name);
        return 0;
    }

    for (ps2_device_t dev = 0; dev < PS2_DEV_COUNT; dev++)
    {
        if (devices[dev].attached && !devices[dev].initialized)
        {
            if (ps2_device_init(dev) == ERR)
            {
                LOG_ERR("ps2 failed to initialize device '%s' on %s port\n", name, ps2_device_to_string(dev));
                return ERR;
            }
            devices[dev].initialized = true;
        }
    }

    currentConfig &= ~(PS2_CFG_FIRST_CLOCK_DISABLE | PS2_CFG_SECOND_CLOCK_DISABLE);

    if (devices[PS2_DEV_FIRST].initialized)
    {
        if (ps2_kbd_irq_register(&devices[PS2_DEV_FIRST]) == ERR)
        {
            LOG_ERR("ps2 failed to register IRQ for keyboard device\n");
            return ERR;
        }

        currentConfig |= PS2_CFG_FIRST_IRQ;
    }
    else
    {
        currentConfig |= PS2_CFG_FIRST_CLOCK_DISABLE;
    }

    if (devices[PS2_DEV_SECOND].initialized)
    {
        if (ps2_mouse_irq_register(&devices[PS2_DEV_SECOND]) == ERR)
        {
            LOG_ERR("ps2 failed to register IRQ for mouse device\n");
            return ERR;
        }
        currentConfig |= PS2_CFG_SECOND_IRQ;
    }
    else
    {
        currentConfig |= PS2_CFG_SECOND_CLOCK_DISABLE;
    }

    if (ps2_cmd_and_write(PS2_CMD_CFG_WRITE, currentConfig) == ERR)
    {
        LOG_ERR("ps2 failed to write final config byte\n");
        return ERR;
    }

    if (devices[PS2_DEV_FIRST].initialized)
    {
        if (ps2_cmd(PS2_CMD_FIRST_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to enable first port\n");
            return ERR;
        }
    }

    if (devices[PS2_DEV_SECOND].initialized)
    {
        if (ps2_cmd(PS2_CMD_SECOND_ENABLE) == ERR)
        {
            LOG_ERR("ps2 failed to enable second port\n");
            return ERR;
        }
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
            ps2_controller_deinit();
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