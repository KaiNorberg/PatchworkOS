#include "dwm.h"

#include "window.h"
#include "client.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/fb.h>
#include <sys/list.h>
#include <sys/proc.h>

static fd_t handle;
static char id[MAX_NAME];

//static fd_t kbd;
//static fd_t mouse;

static list_t clients;
static uint64_t clientAmount;

static window_t* wallpaper;

void dwm_init(void)
{
    fd_t handle = open("sys:/net/local/new");
    read(handle, id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", id);
    writef(ctl, "bind dwm");
    writef(ctl, "listen");
    close(ctl);

    //kbd = open("sys:/kbd/ps2");
    //mouse = open("sys:/mouse/ps2");

    list_init(&clients);
    clientAmount = 0;
}

void dwm_deinit(void)
{
    close(handle);
}

static void dwm_poll(fd_t data)
{
    pollfd_t* fds = malloc(sizeof(pollfd_t) * (1 + clientAmount));
    fds[0].fd = data;
    fds[0].requested = POLL_READ;

    uint64_t i = 1;
    client_t* client;
    LIST_FOR_EACH(client, &clients, entry)
    {
        pollfd_t* fd = &fds[i++];
        fd->fd = client->fd;
        fd->requested = POLL_READ;
    }
    poll(fds, 1 + clientAmount, NEVER);

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
                printf("client free");
                list_remove(&client->entry);
                client_free(client);
                clientAmount--;
            }
        }
    }

    free(fds);
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
