#pragma once

#include "defs.h"

typedef enum com_port
{
    COM1 = 0x3F8,
    COM2 = 0x2F8,
    COM3 = 0x3E8,
    COM4 = 0x2E8,
    COM5 = 0x5F8,
    COM6 = 0x4F8,
    COM7 = 0x5E8,
    COM8 = 0x48E
} com_port_t;

typedef enum com_reg
{
    COM_REG_RECEIVE = 0,
    COM_REG_TRANSMIT = 0,
    COM_REG_INTERRUPT_ENABLE = 1,
    COM_REG_BAUD_LOW = 0,  // DLAB must be set to 1
    COM_REG_BAUD_HIGH = 1, // DLAB must be set to 1
    COM_REG_INTERRUPT_ID = 2,
    COM_REG_FIFO_CONTROL = 2,
    COM_REG_LINE_CONTROL = 3,
    COM_REG_MODEM_CONTROL = 4,
    COM_REG_LINE_STATUS = 5,
    COM_REG_MODEM_STATUS = 6,
    COM_REG_SCRATCH = 7
} com_reg_t;

typedef enum com_line_control
{
    COM_LINE_SIZE_5 = 0,
    COM_LINE_SIZE_6 = 1,
    COM_LINE_SIZE_7 = 2,
    COM_LINE_SIZE_8 = 3,
    COM_LINE_DLAB = 1 << 7
} com_line_control_t;

typedef enum com_modem_control
{
    COM_MODEM_DTR = 1 << 0,
    COM_MODEM_RTS = 1 << 1,
    COM_MODEM_OUT1 = 1 << 2,
    COM_MODEM_OUT2 = 1 << 3,
    COM_MODEM_LOOP = 1 << 4
} com_modem_control_t;

typedef enum com_line_status
{
    COM_LINE_READ_READY = 1 << 0,
    COM_LINE_WRITE_READY = 1 << 5
} com_line_status_t;

void com_init(com_port_t port);

uint8_t com_read(com_port_t port);

void com_write(com_port_t port, uint8_t value);

uint8_t com_reg_read(com_port_t port, com_reg_t reg);

void com_reg_write(com_port_t port, com_reg_t reg, uint8_t value);
