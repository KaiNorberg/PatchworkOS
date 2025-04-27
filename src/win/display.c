#include "internal.h"

static void display_cmds_flush(display_t* disp)
{
    write(disp->data, &disp->cmds, CMD_BUFFER_SIZE(disp->cmds.amount));
}

static void display_cmds_push(display_t* disp, const cmd_t* cmd)
{
    if (disp->cmds.amount == CMD_BUFFER_MAX_CMD)
    {
        display_cmds_flush(disp);
    }

    disp->cmds.buffer[disp->cmds.amount] = *cmd;
    disp->cmds.amount++;
}

static void display_recieve_event(display_t* disp)
{

}

display_t* display_open(void)
{
    display_t* disp = malloc(sizeof(display_t));
    if (disp == NULL)
    {
        return NULL;
    }

    disp->handle = open("sys:/net/local/new");
    if (disp->handle == ERR)
    {
        free(disp);
        return NULL;
    }
    read(disp->handle, disp->id, MAX_NAME);

    fd_t ctl = openf("sys:/net/local/%s/ctl", disp->id);
    if (ctl == ERR)
    {
        close(disp->handle);
        free(disp);
        return NULL;
    }
    if (writef(ctl, "connect dwm") == ERR)
    {
        close(ctl);
        close(disp->handle);
        free(disp);
        return NULL;
    }
    close(ctl);

    disp->data = openf("sys:/net/local/%s/data", disp->id);
    if (disp->data == ERR)
    {
        close(disp->handle);
        free(disp);
    }

    disp->events.readIndex = 0;
    disp->events.writeIndex = 0;
    disp->cmds.amount = 0;
    return disp;
}

void display_close(display_t* disp)
{
    close(disp->handle);
    free(disp);
}

void display_screen_rect(display_t* disp, rect_t* rect)
{
    cmd_t cmd = {.type = CMD_SCREEN_INFO};
    display_cmds_push(disp, &cmd);
    display_cmds_flush(disp);


}
