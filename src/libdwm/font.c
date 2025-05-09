#include "internal.h"

#include <stdlib.h>
#include <string.h>

font_t* font_default(display_t* disp)
{
    return &disp->defaultFont;
}

font_t* font_new(display_t* disp, const char* name, uint64_t desiredHeight)
{
    if (strlen(name) >= MAX_NAME)
    {
        return NULL;
    }

    font_t* font = malloc(sizeof(font_t));
    if (font == NULL)
    {
        return NULL;
    }
    list_entry_init(&font->entry);
    font->disp = disp;

    cmd_font_new_t* cmd = display_cmds_push(disp, CMD_FONT_NEW, sizeof(cmd_font_new_t));
    strcpy(cmd->name, name);
    cmd->desiredHeight = desiredHeight;
    display_cmds_flush(font->disp);

    event_t event;
    if (display_wait_for_event(disp, &event, EVENT_FONT_NEW) == ERR)
    {
        free(font);
        return NULL;
    }

    font->id = event.fontNew.id;
    font->width = event.fontNew.width;
    font->height = event.fontNew.height;
    return font;
}

void font_free(font_t* font)
{
    if (font == &font->disp->defaultFont)
    {
        return;
    }

    cmd_font_free_t* cmd = display_cmds_push(font->disp, CMD_FONT_FREE, sizeof(cmd_font_free_t));
    cmd->id = font->id;
    display_cmds_flush(font->disp);

    list_remove(&font->entry);
    free(font);
}

uint64_t font_height(font_t* font)
{
    return font->height;
}

uint64_t font_width(font_t* font)
{
    return font->width;
}
