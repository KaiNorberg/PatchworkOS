#pragma once

void terminal_init();

void terminal_put(const char chr);

void terminal_print(const char* string);

__attribute__((noreturn)) void terminal_loop();
