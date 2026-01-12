#define __STDC_WANT_LIB_EXT1__ 1
#include "client.h"

#include "compositor.h"
#include "dwm.h"
#include "screen.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static surface_t* client_surface_find(client_t* client, surface_id_t id)
{
    surface_t* surface;
    LIST_FOR_EACH(surface, &client->surfaces, clientEntry)
    {
        if (surface->id == id)
        {
            return surface;
        }

        // Surfaces are sorted
        if (surface->id > id)
        {
            return NULL;
        }
    }
    return NULL;
}

client_t* client_new(fd_t fd)
{
    client_t* client = malloc(sizeof(client_t));
    if (client == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    list_entry_init(&client->entry);
    client->fd = fd;
    list_init(&client->surfaces);
    // Subscibe to events 0-63 by default
    client->bitmask[0] = UINT64_MAX;
    client->bitmask[1] = 0;
    client->bitmask[2] = 0;
    client->bitmask[3] = 0;

    return client;
}

void client_free(client_t* client)
{
    surface_t* surface;
    surface_t* temp;
    LIST_FOR_EACH_SAFE(surface, temp, &client->surfaces, clientEntry)
    {
        rect_t screenRect = SURFACE_SCREEN_RECT(surface);
        compositor_invalidate(&screenRect);

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
        errno = EINVAL;
        return ERR;
    }
    cmd_screen_info_t* cmd = (cmd_screen_info_t*)header;

    event_screen_info_t event;
    if (cmd->index != 0)
    {
        event.width = 0;
        event.height = 0;
        errno = EINVAL;
        return ERR;
    }
    else
    {
        event.width = screen_width();
        event.height = screen_height();
    }
    client_send_event(client, SURFACE_ID_NONE, EVENT_SCREEN_INFO, &event, sizeof(event_screen_info_t));
    return 0;
}

static uint64_t client_action_surface_new(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_new_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_surface_new_t* cmd = (cmd_surface_new_t*)header;

    if (cmd->type < 0 || cmd->type >= SURFACE_TYPE_AMOUNT)
    {
        errno = EINVAL;
        return ERR;
    }

    int32_t width = RECT_WIDTH(&cmd->rect);
    int32_t height = RECT_HEIGHT(&cmd->rect);
    if (width <= 0 || height <= 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (strnlen_s(cmd->name, MAX_NAME) >= MAX_NAME)
    {
        errno = ENAMETOOLONG;
        return ERR;
    }

    point_t point = {.x = cmd->rect.left, .y = cmd->rect.top};
    surface_t* surface = surface_new(client, cmd->name, &point, width, height, cmd->type);
    if (surface == NULL)
    {
        return ERR;
    }

    event_surface_new_t event;
    if (share(event.shmemKey, sizeof(event.shmemKey), surface->shmem, CLOCKS_NEVER) == ERR)
    {
        surface_free(surface);
        return ERR;
    }

    if (dwm_attach(surface) == ERR)
    {
        surface_free(surface);
        return ERR;
    }

    list_push_back(&client->surfaces, &surface->clientEntry);

    client_send_event(client, surface->id, EVENT_SURFACE_NEW, &event, sizeof(event));
    return 0;
}

static uint64_t client_action_surface_free(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_free_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_surface_free_t* cmd = (cmd_surface_free_t*)header;

    surface_t* surface = client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    rect_t screenRect = SURFACE_SCREEN_RECT(surface);
    compositor_invalidate(&screenRect);

    list_remove(&surface->clientEntry);
    dwm_detach(surface);
    surface_free(surface);
    return 0;
}

static uint64_t client_action_surface_move(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_move_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_surface_move_t* cmd = (cmd_surface_move_t*)header;

    surface_t* surface = client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    uint64_t width = RECT_WIDTH(&cmd->rect);
    uint64_t height = RECT_HEIGHT(&cmd->rect);

    rect_t oldScreenRect = SURFACE_SCREEN_RECT(surface);
    if (surface->width != width || surface->height != height)
    {
        // @todo Implement resizing surfaces
        errno = ENOSYS;
        return ERR;
    }
    surface->pos = (point_t){.x = cmd->rect.left, .y = cmd->rect.top};
    rect_t newScreenRect = SURFACE_SCREEN_RECT(surface);

    compositor_invalidate(&oldScreenRect);
    compositor_invalidate(&newScreenRect);

    dwm_report_produce(surface, surface->client, REPORT_RECT);
    return 0;
}

static uint64_t client_action_surface_timer_set(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_timer_set_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_surface_timer_set_t* cmd = (cmd_surface_timer_set_t*)header;

    surface_t* surface = client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    surface->timer.flags = cmd->flags;
    surface->timer.timeout = cmd->timeout;
    surface->timer.deadline = CLOCKS_DEADLINE(cmd->timeout, uptime());
    return 0;
}

static uint64_t client_action_surface_invalidate(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_invalidate_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_surface_invalidate_t* cmd = (cmd_surface_invalidate_t*)header;

    if (RECT_HAS_NEGATIVE_DIMS(&cmd->invalidRect))
    {
        errno = EINVAL;
        return ERR;
    }

    surface_t* surface = client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        errno = ENOENT; // No such surface
        return ERR;
    }

    rect_t surfaceRect = SURFACE_CONTENT_RECT(surface);
    rect_t invalidRect = cmd->invalidRect;
    RECT_FIT(&invalidRect, &surfaceRect);

    rect_t screenInvalidRect = RECT_INIT_DIM(surface->pos.x + invalidRect.left, surface->pos.y + invalidRect.top,
        RECT_WIDTH(&invalidRect), RECT_HEIGHT(&invalidRect));
    compositor_invalidate(&screenInvalidRect);
    return 0;
}

static uint64_t client_action_surface_focus_set(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_focus_set_t))
    {
        return ERR;
    }
    cmd_surface_focus_set_t* cmd = (cmd_surface_focus_set_t*)header;

    surface_t* surface = cmd->isGlobal ? dwm_surface_find(cmd->target) : client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        return 0; // In the future some error system needs to be created to notify clients of errors like these, but
                  // since this needs to be able to fail (race conditions dont worry) we just have to ignore the error
                  // for now.
    }

    dwm_focus_set(surface);
    rect_t screenRect = SURFACE_SCREEN_RECT(surface);
    compositor_invalidate(&screenRect);
    return 0;
}

static uint64_t client_action_surface_visible_set(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_visible_set_t))
    {
        return ERR;
    }
    cmd_surface_visible_set_t* cmd = (cmd_surface_visible_set_t*)header;

    surface_t* surface = cmd->isGlobal ? dwm_surface_find(cmd->target) : client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        return 0; // See client_action_surface_focus_set().
    }

    bool isSurfaceVisible = surface->flags & SURFACE_VISIBLE;
    if (isSurfaceVisible != cmd->isVisible)
    {
        surface->flags ^= SURFACE_VISIBLE;
        dwm_focus_set(surface);
        rect_t screenRect = SURFACE_SCREEN_RECT(surface);
        compositor_invalidate(&screenRect);
        dwm_report_produce(surface, surface->client, REPORT_IS_VISIBLE);
    }
    return 0;
}

static uint64_t client_action_surface_report(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_surface_report_t))
    {
        return ERR;
    }
    cmd_surface_report_t* cmd = (cmd_surface_report_t*)header;

    surface_t* surface = cmd->isGlobal ? dwm_surface_find(cmd->target) : client_surface_find(client, cmd->target);
    if (surface == NULL)
    {
        return 0; // See client_action_surface_focus_set().
    }

    dwm_report_produce(surface, client, REPORT_NONE);
    return 0;
}

static uint64_t client_action_subscribe(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_subscribe_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_subscribe_t* cmd = (cmd_subscribe_t*)header;

    if (cmd->event >= DWM_MAX_EVENT)
    {
        errno = EINVAL;
        return ERR;
    }

    client->bitmask[cmd->event / 64] |= (1ULL << (cmd->event % 64));
    return 0;
}

static uint64_t client_action_unsubscribe(client_t* client, const cmd_header_t* header)
{
    if (header->size != sizeof(cmd_unsubscribe_t))
    {
        errno = EINVAL;
        return ERR;
    }
    cmd_unsubscribe_t* cmd = (cmd_unsubscribe_t*)header;

    if (cmd->event >= DWM_MAX_EVENT)
    {
        errno = EINVAL;
        return ERR;
    }

    client->bitmask[cmd->event / 64] &= ~(1ULL << (cmd->event % 64));
    return 0;
}

static uint64_t (*actions[])(client_t*, const cmd_header_t*) = {
    [CMD_SCREEN_INFO] = client_action_screen_info,
    [CMD_SURFACE_NEW] = client_action_surface_new,
    [CMD_SURFACE_FREE] = client_action_surface_free,
    [CMD_SURFACE_MOVE] = client_action_surface_move,
    [CMD_SURFACE_TIMER_SET] = client_action_surface_timer_set,
    [CMD_SURFACE_INVALIDATE] = client_action_surface_invalidate,
    [CMD_SURFACE_FOCUS_SET] = client_action_surface_focus_set,
    [CMD_SURFACE_VISIBLE_SET] = client_action_surface_visible_set,
    [CMD_SURFACE_REPORT] = client_action_surface_report,
    [CMD_SUBSCRIBE] = client_action_subscribe,
    [CMD_UNSUBSCRIBE] = client_action_unsubscribe,
};

static uint64_t client_process_cmds(client_t* client, cmd_buffer_t* cmds)
{
    if (cmds->size > CMD_BUFFER_MAX_DATA)
    {
        printf("dwm client: invalid command buffer size, got %lu\n", cmds->size);
        errno = EPROTO;
        return ERR;
    }

    uint64_t amount = 0;
    cmd_header_t* cmd;
    CMD_BUFFER_FOR_EACH(cmds, cmd)
    {
        amount++;
        if (amount > cmds->amount || ((uint64_t)cmd + cmd->size - (uint64_t)cmds) > cmds->size ||
            cmd->magic != CMD_MAGIC || cmd->type >= CMD_TYPE_AMOUNT)
        {
            printf("dwm client: corrupt command detected amount=%lu size=%lu magic=%x type=%u\n", amount, cmd->size,
                cmd->magic, cmd->type);
            errno = EPROTO;
            return ERR;
        }
    }

    if (amount != cmds->amount)
    {
        printf("dwm client: invalid command amount, expected %lu, got %lu\n", cmds->amount, amount);
        errno = EPROTO;
        return ERR;
    }

    CMD_BUFFER_FOR_EACH(cmds, cmd)
    {
        if (actions[cmd->type](client, cmd) == ERR)
        {
            printf("dwm client: command type %u caused error\n", cmd->type);
            return ERR;
        }
    }

    return 0;
}

uint64_t client_receive_cmds(client_t* client)
{
    errno = EOK;
    uint64_t freeSpace = CLIENT_RECV_BUFFER_SIZE - client->recvLen;
    if (freeSpace == 0)
    {
        printf("dwm client: receive buffer full\n");
        errno = EMSGSIZE;
        return ERR;
    }

    size_t readSize = read(client->fd, client->recvBuffer + client->recvLen, freeSpace);
    if (readSize == ERR)
    {
        if (errno == EWOULDBLOCK)
        {
            return 0;
        }
        perror("dwm client: read error");
        return ERR;
    }

    if (readSize == 0)
    {
        printf("dwm client: end of file\n");
        errno = EPIPE;
        return ERR;
    }

    client->recvLen += readSize;

    while (client->recvLen > 0)
    {
        if (client->recvLen < sizeof(uint64_t))
        {
            break;
        }

        cmd_buffer_t* cmds = (cmd_buffer_t*)client->recvBuffer;
        if (client->recvLen < cmds->size)
        {
            break;
        }

        if (client_process_cmds(client, cmds) == ERR)
        {
            return ERR;
        }

        client->recvLen -= cmds->size;
        if (client->recvLen > 0)
        {
            memmove(client->recvBuffer, client->recvBuffer + cmds->size, client->recvLen);
        }
    }

    return 0;
}

static uint64_t client_send_all(fd_t fd, const void* data, size_t size)
{
    const char* p = (const char*)data;
    uint64_t sent = 0;
    while (sent < size)
    {
        uint64_t n = write(fd, p + sent, size - sent);
        if (n == ERR)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("dwm client: write error");
            return ERR;
        }

        if (n == 0)
        {
            errno = EPIPE;
            perror("dwm client: write error (0 bytes written)");
            return ERR;
        }

        sent += n;
    }

    return 0;
}

uint64_t client_send_event(client_t* client, surface_id_t target, event_type_t type, void* data, uint64_t size)
{
    if (client->bitmask[type / 64] & (1ULL << (type % 64)))
    {
        event_t event = {.type = type, .target = target};
        memcpy(&event.raw, data, size);

        if (client_send_all(client->fd, &event, sizeof(event_t)) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}
