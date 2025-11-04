#include <kernel/acpi/aml/debug.h>

#include <kernel/acpi/aml/state.h>
#include <kernel/log/log.h>
#include <kernel/sched/timer.h>

static void aml_debug_dump_print_line(const uint8_t* start, uint64_t lineStart, uint64_t lineEnd)
{
    LOG_ERR("  %08x: ", lineStart);
    for (uint64_t j = 0; j < 16; j++)
    {
        if (lineStart + j <= lineEnd)
        {
            LOG_ERR("%02x ", start[lineStart + j]);
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
            uint8_t c = start[lineStart + j];
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

static void aml_debug_dump(const uint8_t* start, const uint8_t* end, const uint8_t* current)
{
    uint64_t index = current - start;
    uint64_t dataSize = end - start;

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
        aml_debug_dump_print_line(start, prevLineStart, prevLineEnd);
    }

    uint64_t errorLineEnd = errorLineStart + 15;
    if (errorLineEnd >= dataSize)
    {
        errorLineEnd = dataSize - 1;
    }
    aml_debug_dump_print_line(start, errorLineStart, errorLineEnd);

    uint64_t errorOffsetInLine = index - errorLineStart;
    LOG_ERR("            ");
    for (uint64_t i = 0; i < errorOffsetInLine; i++)
    {
        LOG_ERR("   ");
    }
    LOG_ERR("^^");
    uint64_t reminaingInLine = 15 - errorOffsetInLine;
    for (uint64_t i = 0; i < reminaingInLine; i++)
    {
        LOG_ERR("   ");
    }
    LOG_ERR("    ");
    for (uint64_t i = 0; i < errorOffsetInLine; i++)
    {
        LOG_ERR(" ");
    }
    LOG_ERR("^\n");

    if (nextLineStart < dataSize)
    {
        uint64_t nextLineEnd = nextLineStart + 15;
        if (nextLineEnd >= dataSize)
        {
            nextLineEnd = dataSize - 1;
        }
        aml_debug_dump_print_line(start, nextLineStart, nextLineEnd);
    }
}

void aml_debug_error(aml_term_list_ctx_t* ctx, const char* function, const char* format, ...)
{
    aml_state_t* state = ctx->state;

    if (state->errorDepth++ == 0)
    {
        LOG_ERR("AML ERROR in '%s()'", function);

        const uint8_t* start = NULL;
        const uint8_t* end = NULL;

        aml_method_obj_t* method = aml_method_find(ctx->current);
        if (method != NULL)
        {
            LOG_ERR(" at method '%s' and offset 0x%lx\n", AML_NAME_TO_STRING(method->name),
                ctx->current - method->start);
            DEREF(method);
            start = method->start;
            end = method->end;
        }
        else
        {
            LOG_ERR(" at offset 0x%lx\n", ctx->current - ctx->start);
            start = ctx->start;
            end = ctx->end;
        }

        LOG_ERR("message: ");

        va_list args;
        va_start(args, format);
        log_vprint(LOG_LEVEL_ERR, format, args);
        va_end(args);

        LOG_ERR("\n");

        aml_debug_dump(start, end, ctx->current);
        LOG_ERR("backtrace:\n");
    }
    else
    {
        if (state->errorDepth == 10)
        {
            LOG_ERR("  ...\n");
            return;
        }
        else if (state->errorDepth > 10)
        {
            return;
        }
        LOG_ERR("  %s() -> ", function);

        va_list args;
        va_start(args, format);
        log_vprint(LOG_LEVEL_ERR, format, args);
        va_end(args);

        LOG_ERR("\n");
    }
}
