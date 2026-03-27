#pragma once

#include <libstd/note.h>
#include <signal.h>
#include <stdint.h>

#define _NOTE_MAX_HANDLERS 32

void _note_init(void);

bool _note_handler_add(note_func_t func);

void _note_handler_remove(note_func_t func);

int _signal_raise(int sig);

sighandler_t _signal_handler_add(int sig, sighandler_t func);

void _signal_handler_remove(int sig, sighandler_t func);