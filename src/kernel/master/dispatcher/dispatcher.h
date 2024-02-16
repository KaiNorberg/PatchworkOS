#pragma once

#include "master/interrupts/interrupts.h"

#include <stdint.h>

typedef void(*Callback)();

void dispatcher_init();

Callback dispatcher_fetch();

void dispatcher_dispatch(uint8_t irq);

void dispatcher_send(uint8_t irq);

void dispatcher_push(Callback callback, uint8_t irq);