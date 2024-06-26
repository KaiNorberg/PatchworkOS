#include "debug.h"

#include "font.h"
#include "pmm.h"
#include "regs.h"
#include "smp.h"
#include "time.h"

#include <stdlib.h>
#include <sys/gfx.h>

static psf_t font;
static surface_t surface;
static point_t pos;

static uint64_t debug_column_to_screen(int64_t x)
{
    return (((surface.width / (PSF_WIDTH * DEBUG_SCALE)) - DEBUG_COLUMN_AMOUNT * DEBUG_COLUMN_WIDTH) / 2 +
               x * DEBUG_COLUMN_WIDTH) *
        PSF_WIDTH * DEBUG_SCALE;
}

static uint64_t debug_row_to_screen(int64_t y)
{
    return (((surface.height / (PSF_HEIGHT * DEBUG_SCALE)) - DEBUG_ROW_AMOUNT) / 2 + y) * PSF_HEIGHT * DEBUG_SCALE;
}

static void debug_start(const char* message)
{
    rect_t rect;
    RECT_INIT_DIM(&rect, 0, 0, surface.width, surface.height);
    gfx_rect(&surface, &rect, DEBUG_BACKGROUND);

    char buffer[MAX_PATH];
    strcpy(buffer, " Kernel Panic - ");
    strcat(buffer, message);
    strcat(buffer, " ");

    point_t msgPos = (point_t){
        .x = surface.width / 2 - (strlen(buffer) * PSF_WIDTH * DEBUG_SCALE) / 2,
        .y = debug_row_to_screen(-3),
    };

    font.foreground = DEBUG_BACKGROUND;
    font.background = DEBUG_WHITE;
    gfx_psf_string(&surface, &font, &msgPos, buffer);
    font.foreground = DEBUG_WHITE;
    font.background = DEBUG_BACKGROUND;

    const char* restartMessage = "Please restart your machine";
    point_t pos = (point_t){
        .x = surface.width / 2 - (strlen(restartMessage) * PSF_WIDTH * DEBUG_SCALE) / 2,
        .y = debug_row_to_screen(DEBUG_ROW_AMOUNT + 2),
    };
    gfx_psf_string(&surface, &font, &pos, restartMessage);
}

static void debug_print(const char* string)
{
    point_t scaledPos = (point_t){
        .x = debug_column_to_screen(pos.x),
        .y = debug_row_to_screen(pos.y),
    };
    gfx_psf_string(&surface, &font, &scaledPos, string);
}

static void debug_value(const char* string, uint64_t value)
{
    char buffer[MAX_PATH];

    strcpy(buffer, string);
    strcat(buffer, " = 0x");
    ulltoa(value, buffer + strlen(buffer), 16);

    debug_print(buffer);
    pos.y += 1;
}

static void debug_move(const char* name, uint8_t x)
{
    if (name != NULL)
    {
        char buffer[MAX_PATH];

        strcpy(buffer, "[");
        strcat(buffer, name);
        strcat(buffer, "]");

        pos.x = x;
        pos.y = 0;
        debug_print(buffer);
    }

    pos.x = x;
    pos.y = 1;
}

void debug_init(gop_buffer_t* gopBuffer)
{
    font.foreground = DEBUG_WHITE;
    font.background = DEBUG_BACKGROUND;
    font.scale = DEBUG_SCALE;
    font.glyphs = font_get() + sizeof(psf_header_t);

    surface.buffer = gopBuffer->base;
    surface.height = gopBuffer->height;
    surface.width = gopBuffer->width;
    surface.stride = gopBuffer->stride;
}

void debug_panic(const char* message)
{
    asm volatile("cli");
    if (smp_initialized())
    {
        smp_send_ipi_to_others(IPI_HALT);
    }

    debug_start(message);

    debug_move("Memory", 1);
    debug_value("free pages", pmm_free_amount());
    debug_value("reserved pages", pmm_reserved_amount());

    debug_move("Other", 2);
    debug_value("uptime", time_uptime());
    debug_value("cpu id", smp_self()->id);

    while (1)
    {
        asm volatile("hlt");
    }
}

void debug_exception(trap_frame_t const* trapFrame, const char* message)
{
    asm volatile("cli");
    if (smp_initialized())
    {
        smp_send_ipi_to_others(IPI_HALT);
    }

    debug_start(message);

    debug_move("Memory", 0);
    debug_value("Locked Pages", pmm_reserved_amount());
    debug_value("Unlocked Pages", pmm_free_amount());

    debug_move("Trap Frame", 1);
    debug_value("Vector", trapFrame->vector);
    debug_value("Error Code", trapFrame->errorCode);
    debug_value("RIP", trapFrame->rip);
    debug_value("RSP", trapFrame->rsp);
    debug_value("RFLAGS", trapFrame->rflags);
    debug_value("CS", trapFrame->cs);
    debug_value("SS", trapFrame->ss);

    debug_move("Registers", 2);
    debug_value("R9", trapFrame->r9);
    debug_value("R8", trapFrame->r8);
    debug_value("RBP", trapFrame->rbp);
    debug_value("RDI", trapFrame->rdi);
    debug_value("RSI", trapFrame->rsi);
    debug_value("RDX", trapFrame->rdx);
    debug_value("RCX", trapFrame->rcx);
    debug_value("RBX", trapFrame->rbx);
    debug_value("RAX", trapFrame->rax);
    debug_value("CR2", cr2_read());
    debug_value("CR3", cr3_read());
    debug_value("CR4", cr4_read());
    debug_value("R15", trapFrame->r15);
    debug_value("R14", trapFrame->r14);
    debug_value("R13", trapFrame->r13);
    debug_value("R12", trapFrame->r12);
    debug_value("R11", trapFrame->r11);
    debug_value("R10", trapFrame->r10);

    debug_move("Other", 3);
    debug_value("Current Time", time_uptime());
    debug_value("Cpu Id", smp_self()->id);

    while (1)
    {
        asm volatile("hlt");
    }
}
