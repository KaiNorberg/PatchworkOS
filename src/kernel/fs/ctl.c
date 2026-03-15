#include <kernel/fs/ctl.h>

#include <kernel/io/irp.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/pmm.h>
#include <kernel/sched/thread.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define CTL_MAX_DEPTH 32

static cache_t cache = CACHE_CREATE(cache, "ctl_state", sizeof(ctl_state_t), 64, NULL, NULL);

static status_t ctl_do_next(irp_t* irp, ctl_state_t* state);

static void ctl_state_free(ctl_state_t* state)
{
    if (state == NULL)
    {
        return;
    }

    UNREF(state->file);
    cache_free(state);
}

static status_t ctl_completion(irp_t* irp, void* ctx)
{
    ctl_state_t* state = (ctl_state_t*)ctx;

    if (state->next != NULL)
    {
        if (IS_INFO(irp->status) || state->runAlways)
        {
            status_t status = ctl_do_next(irp, state);
            if (IS_ERR(status))
            {
                irp->status = status;
                ctl_state_free(state);
                return OK;
            }
            return INFO(IO, COMPLETE);
        }

        char* next = NULL;
        for (char* p = state->next; *p != '\0'; p++)
        {
            if (*p == ';' || *p == '\n')
            {
                next = p + 1;
                break;
            }
        }

        if (next != NULL)
        {
            state->next = next;
            while (isspace(*state->next))
            {
                state->next++;
            }

            if (*state->next != '\0')
            {
                status_t status = ctl_do_next(irp, state);
                if (IS_ERR(status))
                {
                    irp->status = status;
                    ctl_state_free(state);
                    return OK;
                }
                return INFO(IO, COMPLETE);
            }
        }
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
    char* sep = NULL;
    state->runAlways = true;

    for (char* p = string; *p != '\0'; p++)
    {
        if (*p == ';' || *p == '\n')
        {
            sep = p;
            state->runAlways = true;
            break;
        }

        if (p[0] == '&' && p[1] == '&')
        {
            sep = p;
            state->runAlways = false;
            break;
        }
    }

    if (sep != NULL)
    {
        *sep = '\0';
        state->next = sep + (state->runAlways ? 1 : 2);
        while (isspace(*state->next))
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

    char* args = string;
    while (*args != '\0' && !isspace(*args))
    {
        args++;
    }

    if (*args != '\0')
    {
        *args = '\0';
        args++;
        while (isspace(*args))
        {
            args++;
        }

        size_t len = strlen(args);
        while (len > 0 && isspace(args[len - 1]))
        {
            args[--len] = '\0';
        }
    }
    else
    {
        args = NULL;
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

    if (args == NULL)
    {
        args = "";
    }

    irp_frame_t* next = irp_next(irp);
    assert(next != NULL);

    irp_set_complete(irp, ctl_completion, state);
    irp_prep_control(irp, cmd, args);
    return file_call(state->file, irp);
}

status_t ctl_dispatch(irp_t* irp, file_t* file)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->major != IRP_MJ_WRITE)
    {
        return ERR(VFS, MJ_NOSYS);
    }

    ctl_state_t* state = cache_alloc(&cache);
    if (state == NULL)
    {
        return ERR(VFS, NOMEM);
    }

    size_t bytesWritten;
    status_t status =
        sglist_copy_out(frame->write.buffer, SIZE_MAX, 0, &bytesWritten, state->buffer, CTL_BUFFER_SIZE - 1);
    if (IS_ERR(status))
    {
        cache_free(state);
        return status;
    }

    state->buffer[bytesWritten] = '\0';
    state->next = state->buffer;
    state->file = REF(file);
    state->depth = 0;

    status = ctl_do_next(irp, state);
    if (IS_ERR(status))
    {
        ctl_state_free(state);
        return status;
    }
    return INFO(IO, PENDING);
}

status_t ctl_generic_write(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->major != IRP_MJ_WRITE)
    {
        return ERR(VFS, MJ_NOSYS);
    }

    return ctl_dispatch(irp, frame->file);
}
