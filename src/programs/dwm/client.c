#include "client.h"

#include "compositor.h"
#include "dwm.h"
#include "psf.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static psf_t* client_find_font(client_t* client, font_id_t id)
{
    if (id == FONT_ID_DEFAULT)
    {
        return dwm_default_font();
    }

    psf_t* font;
    LIST_FOR_EACH(font, &client->fonts, entry)
    {
        if (font->id == id)
        {
            return font;
        }
    }
    return NULL;
}

static surface_t* client_find_surface(client_t* client, surface_id_t id)
{
    surface_t* surface;
    LIST_FOR_EACH(surface, &client->surfaces, clientEntry)
    {
        if (surface->id == id)
        {
            return surface;
        }
    }
    return NULL;
}

client_t* client_new(fd_t fd)
{
    client_t* client = malloc(sizeof(client_t));
    if (client == NULL)
    {
        return NULL;
    }
    list_entry_init(&client->entry);
    client->fd = fd;
    list_init(&client->surfaces);
    list_init(&client->fonts);

    return client;
}

void client_free(client_t* client)
{
    surface_t* wall = NULL;

    surface_t* surface;
    surface_t* temp;
    LIST_FOR_EACH_SAFE(surface, temp, &client->surfaces, clientEntry)
    {
        list_remove(&surface->clientEntry);
        dwm_detach(surface);
        surface_free(surface);
    }

    close(client->fd);
    free(client);
}

static uint64_t client_action_screen_info(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_screen_info_t))
    {
        return ERR;
    }
    cmd_screen_info_t* cmd = (cmd_screen_info_t*)header;

    if (cmd->index != 0)
    {
        return ERR;
    }

    event_screen_info_t event;
    if (cmd->index != 0)
    {
        event.width = 0;
        event.height = 0;
    }
    else
    {
        event.width = screen_width();
        event.height = screen_height();
    }
    client_send_event(client, EVENT_SCREEN_INFO, SURFACE_ID_NONE, &event, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t client_action_surface_new(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_new_t))
    {
        return ERR;
    }
    cmd_surface_new_t* cmd = (cmd_surface_new_t*)header;

    if (cmd->type < 0 || cmd->type >= SURFACE_TYPE_AMOUNT)
    {
        return ERR;
    }
    if (RECT_WIDTH(&cmd->rect) <= 0 || RECT_HEIGHT(&cmd->rect) <= 0)
    {
        return ERR;
    }
    if (client_find_surface(client, cmd->id) != NULL)
    {
        return ERR;
    }

    const rect_t* rect = &cmd->rect;
    point_t point = {.x = rect->left, .y = rect->top};
    surface_t* surface = surface_new(client, cmd->id, &point, RECT_WIDTH(rect), RECT_HEIGHT(rect), cmd->type);
    if (surface == NULL)
    {
        return ERR;
    }

    if (dwm_attach(surface) == ERR)
    {
        surface_free(surface);
    }

    list_push(&client->surfaces, &surface->clientEntry);

    dwm_focus_set(surface);
    return 0;
}

static uint64_t client_action_surface_free(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_free_t))
    {
        return ERR;
    }
    cmd_surface_free_t* cmd = (cmd_surface_free_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    list_remove(&surface->clientEntry);
    dwm_detach(surface);
    surface_free(surface);
    return 0;
}

static uint64_t client_action_draw_rect(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_rect_t))
    {
        return ERR;
    }
    cmd_draw_rect_t* cmd = (cmd_draw_rect_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_rect(&surface->gfx, &rect, cmd->pixel);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_draw_edge(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_edge_t))
    {
        return ERR;
    }
    cmd_draw_edge_t* cmd = (cmd_draw_edge_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_edge(&surface->gfx, &rect, cmd->width, cmd->foreground, cmd->background);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_draw_gradient(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_draw_gradient_t))
    {
        return ERR;
    }
    cmd_draw_gradient_t* cmd = (cmd_draw_gradient_t*)header;

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t rect = cmd->rect;
    RECT_FIT(&rect, &surfaceRect);
    gfx_gradient(&surface->gfx, &rect, cmd->start, cmd->end, cmd->type, cmd->addNoise);

    surface->invalid = true;
    compositor_set_redraw_needed();
    return 0;
}

static uint64_t client_action_font_new(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_font_new_t))
    {
        return ERR;
    }
    cmd_font_new_t* cmd = (cmd_font_new_t*)header;
    if (memchr(cmd->name, '\0', MAX_NAME) == NULL)
    {
        return ERR;
    }

    char path[MAX_PATH];
    sprintf(path, FONT_DIR "/%s.psf", cmd->name);

    psf_t* font = psf_new(path, cmd->desiredHeight);
    if (font == NULL)
    {
        return ERR;
    }

    event_font_new_t event;
    event.id = font->id;
    event.width = font->width * font->scale;
    event.height = font->height * font->scale;
    if (client_send_event(client, EVENT_FONT_NEW, SURFACE_ID_NONE, &event, sizeof(event_font_new_t)) == ERR)
    {
        psf_free(font);
        return ERR;
    }

    list_push(&client->fonts, &font->entry);
    return 0;
}

static uint64_t client_action_font_free(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_font_free_t))
    {
        return ERR;
    }
    cmd_font_free_t* cmd = (cmd_font_free_t*)header;

    if (cmd->id == FONT_ID_DEFAULT)
    {
        return ERR;
    }

    psf_t* font = client_find_font(client, cmd->id);
    if (font == NULL)
    {
        return ERR;
    }

    list_remove(&font->entry);
    psf_free(font);
    return 0;
}

static uint64_t client_action_font_info(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_font_info_t))
    {
        return ERR;
    }
    cmd_font_info_t* cmd = (cmd_font_info_t*)header;

    psf_t* font = client_find_font(client, cmd->id);
    if (font == NULL)
    {
        return ERR;
    }

    event_font_info_t event;
    event.id = cmd->id;
    event.width = font->width * font->scale;
    event.height = font->height * font->scale;
    if (client_send_event(client, EVENT_FONT_INFO, SURFACE_ID_NONE, &event, sizeof(event_font_info_t)) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t client_action_draw_string(client_t* client, const cmd_header_t* header)
{
    if (header->size <= sizeof(cmd_draw_string_t))
    {
        return ERR;
    }
    cmd_draw_string_t* cmd = (cmd_draw_string_t*)header;
    if (header->size != sizeof(cmd_draw_string_t) + cmd->length)
    {
        return ERR;
    }

    surface_t* surface = client_find_surface(client, cmd->target);
    if (surface == NULL)
    {
        return ERR;
    }

    psf_t* font = client_find_font(client, cmd->fontId);
    if (font == NULL)
    {
        return ERR;
    }

    rect_t textArea = RECT_INIT_DIM(cmd->point.x, cmd->point.y, font->width * (cmd->length + 1), font->height);
    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    if (!RECT_CONTAINS(&surfaceRect, &textArea))
    {
        return ERR;
    }

    point_t point = cmd->point;
    for (uint64_t i = 0; i < cmd->length; i++)
    {
        gfx_psf(&surface->gfx, font, &point, cmd->string[i], cmd->foreground, cmd->background);
        point.x += font->width * font->scale;
    }

    return 0;
}

static uint64_t (*actions[])(client_t*, const cmd_header_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_SURFACE_FREE] = client_action_surface_free,
    [CMD_DRAW_RECT] = client_action_draw_rect,
    [CMD_DRAW_EDGE] = client_action_draw_edge,
    [CMD_DRAW_GRADIENT] = client_action_draw_gradient,
    [CMD_FONT_NEW] = client_action_font_new,
    [CMD_FONT_FREE] = client_action_font_free,
    [CMD_FONT_INFO] = client_action_font_info,
    [CMD_DRAW_STRING] = client_action_draw_string,
};

uint64_t client_recieve_cmds(client_t* client)
{
    uint64_t readSize = read(client->fd, &client->cmds, sizeof(cmd_buffer_t) + 1); // Add plus one to check if packet is to big
    if (readSize > sizeof(cmd_buffer_t) || readSize == 0)                          // Program wrote to much or end of file
    {
        return ERR;
    }

    if (readSize != client->cmds.size || client->cmds.size > CMD_BUFFER_MAX_DATA)
    {
        return ERR;
    }

    uint64_t amount = 0;
    cmd_header_t* cmd;
    CMD_BUFFER_FOR_EACH(&client->cmds, cmd)
    {
        amount++;
        if (amount > client->cmds.amount || ((uint64_t)cmd + cmd->size - (uint64_t)&client->cmds) > readSize ||
            cmd->magic != CMD_MAGIC || cmd->type >= CMD_TYPE_AMOUNT)
        {
            return ERR;
        }
    }
    if (amount != client->cmds.amount)
    {
        return ERR;
    }

    CMD_BUFFER_FOR_EACH(&client->cmds, cmd)
    {
        if (actions[cmd->type](client, cmd) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

uint64_t client_send_event(client_t* client, event_type_t type, surface_id_t target, void* data, uint64_t size)
{
    printf("client_send_event start");
    event_t event = {.type = type, .target = target};
    memcpy(&event.raw, data, size);
    if (write(client->fd, &event, sizeof(event_t)) == ERR)
    {
        printf("client_send_event err2");
        return ERR;
    }
    printf("client_send_event end");
    return 0;
}
