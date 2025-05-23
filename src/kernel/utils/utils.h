#pragma once

#include "defs.h"

#define READ_8(address) (*((volatile uint8_t*)(address)))
#define WRITE_8(address, value) (*((volatile uint8_t*)(address)) = (uint8_t)value)

#define READ_16(address) (*((volatile uint16_t*)(address)))
#define WRITE_16(address, value) (*((volatile uint16_t*)(address)) = (uint16_t)value)

#define READ_32(address) (*((volatile uint32_t*)(address)))
#define WRITE_32(address, value) (*((volatile uint32_t*)(address)) = (uint32_t)value)

#define READ_64(address) (*((volatile uint64_t*)(address)))
#define WRITE_64(address, value) (*((volatile uint64_t*)(address)) = (uint64_t)value)
