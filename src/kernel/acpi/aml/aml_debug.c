#include "aml_debug.h"

#include "aml_state.h"

#include "log/log.h"

static void aml_debug_dump_print_line(aml_state_t* state, uint64_t lineStart, uint64_t lineEnd)
{
    LOG_ERR("  %08x: ", lineStart);
    for (uint64_t j = 0; j < 16; j++)
    {
        if (lineStart + j <= lineEnd)
        {
            LOG_ERR("%02x ", ((uint8_t*)state->start)[lineStart + j]);
        }
        else
        {
            LOG_ERR("   ");
        }
    }
    LOG_ERR(" | ");
    for (uint64_t j = 0; j < 16; j++)
    {
        if (lineStart + j <= lineEnd)
        {
            uint8_t c = ((uint8_t*)state->start)[lineStart + j];
            if (c >= 32 && c <= 126)
            {
                LOG_ERR("%c", c);
            }
            else
            {
                LOG_ERR(".");
            }
        }
    }
    LOG_ERR("\n");
}

static void aml_debug_dump(aml_state_t* state)
{
    uint64_t index = state->current - state->start;
    uint64_t dataSize = state->end - state->start;

    uint64_t errorLineStart = (index / 16) * 16;
    uint64_t prevLineStart = errorLineStart >= 16 ? errorLineStart - 16 : 0;
    uint64_t nextLineStart = errorLineStart + 16;

    if (errorLineStart > 0)
    {
        uint64_t prevLineEnd = errorLineStart - 1;
        if (prevLineEnd >= dataSize)
        {
            prevLineEnd = dataSize - 1;
        }
        aml_debug_dump_print_line(state, prevLineStart, prevLineEnd);
    }

    uint64_t errorLineEnd = errorLineStart + 15;
    if (errorLineEnd >= dataSize)
    {
        errorLineEnd = dataSize - 1;
    }
    aml_debug_dump_print_line(state, errorLineStart, errorLineEnd);

    uint64_t errorOffsetInLine = index - errorLineStart;
    LOG_ERR("            ");
    for (uint64_t i = 0; i < errorOffsetInLine; i++)
    {
        LOG_ERR("   ");
    }
    LOG_ERR("^^ error here\n");

    if (nextLineStart < dataSize)
    {
        uint64_t nextLineEnd = nextLineStart + 15;
        if (nextLineEnd >= dataSize)
        {
            nextLineEnd = dataSize - 1;
        }
        aml_debug_dump_print_line(state, nextLineStart, nextLineEnd);
    }
}

void aml_debug_error_print(aml_state_t* state, const char* function, const char* format, ...)
{
    if (state->debug.lastErrPos != state->current)
    {
        LOG_ERR("AML ERROR in '%s()' at pos 0x%lx (", function, state->current - state->start);

        va_list args;
        va_start(args, format);
        log_vprint(LOG_LEVEL_ERR, FILE_BASENAME, format, args);
        va_end(args);

        LOG_ERR(")\n");

        aml_debug_dump(state);
        LOG_ERR("Backtrace:\n");
    }
    else
    {
        LOG_ERR("  %s() -> ", function);

        va_list args;
        va_start(args, format);
        log_vprint(LOG_LEVEL_ERR, FILE_BASENAME, format, args);
        va_end(args);

        LOG_ERR("\n");
    }
    state->debug.lastErrPos = state->current;
}
