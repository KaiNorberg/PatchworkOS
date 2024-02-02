#pragma once

#include <stdint.h>

typedef void(*Callback)();

void dispatcher_init();

Callback dispatcher_fetch();

void dispatcher_push(Callback callback, uint8_t irq);

void dispatcher_dispatch(uint8_t irq);

void dispatcher_wait(Callback callback, uint8_t irq);