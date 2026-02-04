#include <kernel/fs/ctl.h>

#include <kernel/io/irp.h>
#include <kernel/mem/pmm.h>
#include <kernel/sched/thread.h>

#include <stdlib.h>
#include <string.h>
#include <sys/argsplit.h>

#define CTL_MAX_DEPTH 64

static status_t ctl_do_next(irp_t* irp, ctl_state_t* state);

static void ctl_state_free(ctl_state_t* state)
{
    if (state == NULL)
    {
        return;
    }

    UNREF(state->vnode);
    free(state);
}

static status_t ctl_completion(irp_t* irp, void* ctx)
{
    ctl_state_t* state = (ctl_state_t*)ctx;

    if (IS_OK(irp->status) && state->next != NULL)
    {
        status_t status = ctl_do_next(irp, state);
        if (IS_ERR(status))
        {
            irp->status = status;
            ctl_state_free(state);
            return OK;
        }
        return INFO(IO, PENDING);
    }

    ctl_state_free(state);
    return OK;
}

static status_t ctl_do_next(irp_t* irp, ctl_state_t* state)
{
    if (++state->depth > CTL_MAX_DEPTH)
    {
        return ERR(IO, OVERFLOW);
    }

    char* string = state->next;
    char* sep = strstr(string, "&&");

    if (sep != NULL)
    {
        *sep = '\0';
        state->next = sep + 2;
        while (*state->next == ' ')
        {
            state->next++;
        }
        if (*state->next == '\0')
        {
            state->next = NULL;
        }
    }
    else
    {
        state->next = NULL;
    }

    char* args = strchr(string, ' ');
    if (args != NULL)
    {
        *args = '\0';
        args++;
        while (*args == ' ')
        {
            args++;
        }
    }

    iocmd_t cmd = 0;
    for (size_t i = 0; i < sizeof(iocmd_t); i++)
    {
        if (string[i] == '\0' || isspace(string[i]))
        {
            break;
        }
        cmd |= (iocmd_t)(unsigned char)string[i] << (i * 8);
    }

    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    irp_set_complete(irp, ctl_completion, state);
    irp_prep_control(irp, cmd, args);
    return vnode_call(state->vnode, irp);
}

status_t ctl_dispatch(irp_t* irp, vnode_t* vnode)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->major != IRP_MJ_WRITE)
    {
        return ERR(VFS, MJ_NOSYS);
    }

    ctl_state_t* state = malloc(sizeof(ctl_state_t));
    if (state == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    size_t bytesWritten;
    status_t status =
        mdl_copy_to_buffer(frame->write.buffer, frame->write.count, 0, &bytesWritten, state->buffer, CTL_BUFFER_SIZE - 1);
    if (IS_ERR(status))
    {
        free(state);
        return status;
    }

    state->buffer[bytesWritten] = '\0';
    state->next = state->buffer;
    state->vnode = REF(vnode);
    state->depth = 0;

    ctl_do_next(irp, state);
    return INFO(IO, PENDING);
}

status_t ctl_generic_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->major != IRP_MJ_WRITE)
    {
        return ERR(VFS, MJ_NOSYS);
    }

    return ctl_dispatch(irp, frame->vnode);
}
