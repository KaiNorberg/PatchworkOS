#pragma once

#include <stdint.h>
#include <sys/kbd.h>

#define ANSI_MAX_LENGTH 32

typedef struct
{
    char buffer[ANSI_MAX_LENGTH];
    uint8_t length;
} ansi_t;

void ansi_init(ansi_t* ansi);

typedef enum
{
    ANSI_STILL_PARSING,
    ANSI_PRINTABLE,
    ANSI_BACKSPACE,
    ANSI_NEWLINE,
    ANSI_TAB,
    ANSI_ARROW_UP,
    ANSI_ARROW_DOWN,
    ANSI_ARROW_LEFT,
    ANSI_ARROW_RIGHT,
    ANSI_PAGE_UP,
    ANSI_PAGE_DOWN,
    ANSI_HOME,
    ANSI_END,
    ANSI_DELETE,
} ansi_result_type_t;

typedef struct
{
    ansi_result_type_t type;
    char printable;
} ansi_result_t;

uint64_t ansi_parse(ansi_t* ansi, char input, ansi_result_t* result);
