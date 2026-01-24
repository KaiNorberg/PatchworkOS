#include <kernel/cpu/port.h>
#include <kernel/drivers/com.h>

void com_init(com_port_t port)
{
    com_reg_write(port, COM_REG_INTERRUPT_ID, 0);
    com_reg_write(port, COM_REG_LINE_CONTROL, COM_LINE_DLAB);
    com_reg_write(port, COM_REG_BAUD_LOW, 0x03);
    com_reg_write(port, COM_REG_BAUD_HIGH, 0);
    com_reg_write(port, COM_REG_LINE_CONTROL, COM_LINE_SIZE_8);
    com_reg_write(port, COM_REG_MODEM_CONTROL, COM_MODEM_DTR | COM_MODEM_RTS | COM_MODEM_OUT2);
}

uint8_t com_read(com_port_t port)
{
    while ((com_reg_read(port, COM_REG_LINE_STATUS) & COM_LINE_READ_READY) == 0)
    {
        ASM("pause");
    }
    return com_reg_read(port, COM_REG_RECEIVE);
}

void com_write(com_port_t port, uint8_t value)
{
    while ((com_reg_read(port, COM_REG_LINE_STATUS) & COM_LINE_WRITE_READY) == 0)
    {
        ASM("pause");
    }
    com_reg_write(port, COM_REG_TRANSMIT, value);
}

uint8_t com_reg_read(com_port_t port, com_reg_t reg)
{
    return in8(port + reg);
}

void com_reg_write(com_port_t port, com_reg_t reg, uint8_t value)
{
    out8(port + reg, value);
}
