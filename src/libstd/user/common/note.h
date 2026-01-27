#pragma once

#include <signal.h>
#include <stdint.h>
#include <sys/proc.h>

#define _NOTE_MAX_HANDLERS 32

void _note_init(void);

bool _note_handler_add(atnotify_func_t func);

void _note_handler_remove(atnotify_func_t func);

int _signal_raise(int sig);

sighandler_t _signal_handler_add(int sig, sighandler_t func);

void _signal_handler_remove(int sig, sighandler_t func);