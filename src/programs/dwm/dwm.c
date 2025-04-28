#include "dwm.h"

#include "client.h"
#include "surface.h"
#include "screen.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fb.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/proc.h>

static fd_t handle;
static char id[MAX_NAME];

// static fd_t kbd;
// static fd_t mouse;

static list_t clients;
static uint64_t clientAmount;

static surface_t* wall; // The root of the window tree
static surface_t* cursor;
static surface_t* panels;

static bool redrawNeeded;

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new");
    read(handle, id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "bind dwm");
    writef(ctl, "listen");
    close(ctl);

    // kbd = open("sys:/kbd/ps2");
    // mouse = open("sys:/mouse/ps2");

    list_init(&clients);
    clientAmount = 0;

    wall = NULL;

    redrawNeeded = false;
}

void dwm_deinit(void)
{
    close(handle);
}

uint64_t dwm_attach_to_wall(surface_t* surface)
{
    switch (surface->type)
    {
    case SURFACE_WALL:
    {
        if (wall != NULL)
        {
            return ERR;
        }

        wall = surface;
    }
    break;
    case SURFACE_WINDOW:
    {
        if (wall == NULL)
        {
            return ERR;
        }

        list_push(&wall->children, &surface->surfaceEntry);
    }
    break;
    default:
    {
        return ERR;
    }
    }

    return 0;
}

void dwm_set_redraw_needed(void)
{
    redrawNeeded = true;
}

// canvasRect is the area on the screen that we are allowed to draw within, offset is the point on the screen that the surfaces position is relative to.
static void dwm_draw_surface(surface_t* surface, const rect_t* canvasRect, const point_t* offset)
{
    // The position of the surface on the screen.
    rect_t surfaceRect = RECT_INIT_DIM(surface->pos.x + offset->x, surface->pos.y + offset->y, surface->gfx.width, surface->gfx.height);
    RECT_FIT(&surfaceRect, canvasRect);

    point_t srcPoint = {
        .x = MAX(surfaceRect.left - (surface->pos.x + offset->x), 0),
        .y = MAX(surfaceRect.top - (surface->pos.x + offset->y), 0),
    };

    screen_transfer(surface, &surfaceRect, &srcPoint);

    point_t newOffset = {
        .x = surface->pos.x + offset->x,
        .y = surface->pos.y + offset->y,
    };
    surface_t* child;
    LIST_FOR_EACH(child, &surface->children, surfaceEntry)
    {
        dwm_draw_surface(child, &surfaceRect, &newOffset);
    }
}

static void dwm_redraw(void)
{
    redrawNeeded = false;
    if (wall == NULL)
    {
        return;
    }

    rect_t screenRect;
    screen_rect(&screenRect);
    point_t offset = {0};
    dwm_draw_surface(wall, &screenRect, &offset);

    screen_swap();
}

static void dwm_poll(fd_t data)
{
    pollfd_t* fds = malloc(sizeof(pollfd_t) * (1 + clientAmount));
    fds[0].fd = data;
    fds[0].requested = POLL_READ;
    fds[0].occurred = 0;

    uint64_t i = 1;
    client_t* client;
    LIST_FOR_EACH(client, &clients, entry)
    {
        pollfd_t* fd = &fds[i++];
        fd->fd = client->fd;
        fd->requested = POLL_READ;
        fd->occurred = 0;
    }
    poll(fds, 1 + clientAmount, SEC / 60);

    if (fds[0].occurred & POLL_READ)
    {
        fd_t fd = openf("sys:/net/local/%s/accept", id);
        client_t* newClient = client_new(fd);
        if (newClient != NULL)
        {
            list_push(&clients, &newClient->entry);
            clientAmount++;
        }
        else
        {
            close(fd);
        }
    }

    i = 1;
    client_t* temp;
    LIST_FOR_EACH_SAFE(client, temp, &clients, entry)
    {
        pollfd_t* fd = &fds[i++];
        if (fd->occurred & POLL_READ)
        {
            if (client_recieve_cmds(client) == ERR)
            {
                list_remove(&client->entry);
                client_free(client);
                clientAmount--;
            }
        }
    }

    free(fds);

    if (redrawNeeded)
    {
        dwm_redraw();
    }
}

void dwm_loop(void)
{
    fd_t data = openf("sys:/net/local/%s/data", id);

    while (1)
    {
        dwm_poll(data);
    }

    close(data);
}
