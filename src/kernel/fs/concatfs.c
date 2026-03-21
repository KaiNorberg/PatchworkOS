#include <kernel/fs/concatfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/vnode.h>
#include <kernel/proc/process.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs.h>

typedef struct
{
    vnode_t vnode;
    dentry_t** targets;
    size_t targetCount;
    size_t targetCapacity;
} concatfs_vnode_t;

typedef struct
{
    concatfs_vnode_t* parent;
    dentry_t* target;
    size_t layer;
    dentry_t* current;
} concatfs_lookup_state_t;

typedef struct
{
    size_t layer;
    file_t* currentFile;
} concatfs_file_data_t;

typedef struct
{
    concatfs_file_data_t* fileData;
} concatfs_read_state_t;

static cache_t vnodeCache =
    CACHE_CREATE(vnodeCache, "concatfs_vnode", sizeof(concatfs_vnode_t), CACHE_LINE, NULL, NULL);

static cache_t lookupCache =
    CACHE_CREATE(lookupCache, "concatfs_lookup", sizeof(concatfs_lookup_state_t), CACHE_LINE, NULL, NULL);

static cache_t readCache =
    CACHE_CREATE(readCache, "concatfs_read", sizeof(concatfs_read_state_t), CACHE_LINE, NULL, NULL);

static cache_t fileDataCache =
    CACHE_CREATE(fileDataCache, "concatfs_file_data", sizeof(concatfs_file_data_t), CACHE_LINE, NULL, NULL);

static status_t concatfs_lookup(irp_t* irp);
static status_t concatfs_reclaim(irp_t* irp);
static status_t concatfs_read(irp_t* irp);
static status_t concatfs_open(irp_t* irp);
static status_t concatfs_close(irp_t* irp);

static vnode_class_t concatfsClass = {.name = "concatfs",
    .type = FILE_TYPE_DIRECTORY,
    .cache = &vnodeCache,
    .handlers = {
        VNODE_DIR_HANDLERS(),
        [IRP_MJ_LOOKUP] = concatfs_lookup,
        [IRP_MJ_RECLAIM] = concatfs_reclaim,
        [IRP_MJ_READ] = concatfs_read,
        [IRP_MJ_OPEN] = concatfs_open,
        [IRP_MJ_CLOSE] = concatfs_close,
    }};

static concatfs_vnode_t* concatfs_vnode_new(file_volume_t volume, file_number_t number)
{
    concatfs_vnode_t* vnode = CONTAINER_OF(vnode_new(volume, &concatfsClass, number), concatfs_vnode_t, vnode);
    if (vnode == NULL)
    {
        return NULL;
    }

    vnode->targets = NULL;
    vnode->targetCount = 0;
    vnode->targetCapacity = 0;

    return vnode;
}

static status_t concatfs_vnode_push_target(concatfs_vnode_t* vnode, dentry_t* target)
{
    if (vnode == NULL || target == NULL)
    {
        return ERR(FS, INVAL);
    }

    if (vnode->targetCount >= vnode->targetCapacity)
    {
        size_t newCapacity = vnode->targetCapacity == 0 ? 4 : vnode->targetCapacity * 2;
        dentry_t** newTargets = (dentry_t**)realloc(vnode->targets, newCapacity * sizeof(dentry_t*));
        if (newTargets == NULL)
        {
            return ERR(FS, NOMEM);
        }
        vnode->targets = newTargets;
        vnode->targetCapacity = newCapacity;
    }

    vnode->targets[vnode->targetCount++] = REF(target);
    return OK;
}

static void concatfs_lookup_free(concatfs_lookup_state_t* state)
{
    if (state->parent != NULL)
    {
        UNREF(state->parent);
        state->parent = NULL;
    }

    if (state->target != NULL)
    {
        UNREF(state->target);
        state->target = NULL;
    }

    if (state->current != NULL)
    {
        UNREF(state->current);
        state->current = NULL;
    }

    cache_free(state);
}

static status_t concatfs_lookup_next(irp_t* irp, concatfs_lookup_state_t* state);

static status_t concatfs_lookup_complete(irp_t* irp, void* ctx)
{
    concatfs_lookup_state_t* state = ctx;

    if (!IS_ERR(irp->status) && DENTRY_IS_POSITIVE(state->current))
    {
        dentry_make_positive(state->target, state->current->vnode);
        irp->status = OK;
        concatfs_lookup_free(state);
        return OK;
    }

    state->layer++;
    return concatfs_lookup_next(irp, state);
}

static status_t concatfs_lookup_next(irp_t* irp, concatfs_lookup_state_t* state)
{
    if (state->layer >= state->parent->targetCount)
    {
        irp->status = ERR(FS, NOENT);
        concatfs_lookup_free(state);
        return OK;
    }

    dentry_t* target = state->parent->targets[state->layer];

    dentry_t* check = dentry_get(target, state->target->name, strlen(state->target->name));
    if (check != NULL)
    {
        UNREF_DEFER(check);
        if (DENTRY_IS_POSITIVE(check))
        {
            dentry_make_positive(state->target, check->vnode);
            irp->status = OK;
            concatfs_lookup_free(state);
            return OK;
        }
    }

    state->current = dentry_new(target, state->target->name);
    if (state->current == NULL)
    {
        irp->status = ERR(FS, NOMEM);
        concatfs_lookup_free(state);
        return OK;
    }

    irp_prep_lookup(irp, state->current);
    irp_set_complete(irp, concatfs_lookup_complete, state);
    irp->status = OK;
    return vnode_call(target->vnode, irp);
}

static status_t concatfs_lookup(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    concatfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, concatfs_vnode_t, vnode);

    concatfs_lookup_state_t* state = cache_alloc(&lookupCache);
    if (state == NULL)
    {
        return ERR(FS, NOMEM);
    }

    state->parent = REF(vnode);
    state->target = REF(frame->lookup.dentry);
    state->layer = 0;
    state->current = NULL;

    return concatfs_lookup_next(irp, state);
}

static status_t concatfs_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;

    concatfs_file_data_t* data = cache_alloc(&fileDataCache);
    if (data == NULL)
    {
        return ERR(FS, NOMEM);
    }

    data->layer = 0;
    data->currentFile = NULL;
    file->data = data;
    return OK;
}

static status_t concatfs_close(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    concatfs_file_data_t* data = file->data;

    if (data != NULL)
    {
        if (data->currentFile != NULL)
        {
            UNREF(data->currentFile);
        }
        cache_free(data);
        file->data = NULL;
    }
    return OK;
}

static status_t concatfs_read_next(irp_t* irp, concatfs_read_state_t* state);

static status_t concatfs_read_complete(irp_t* irp, void* ctx)
{
    concatfs_read_state_t* state = ctx;
    concatfs_file_data_t* fileData = state->fileData;

    if (IS_ERR(irp->status))
    {
        cache_free(state);
        return OK;
    }

    if (irp->result > 0)
    {
        cache_free(state);
        return OK;
    }

    if (fileData->currentFile != NULL)
    {
        UNREF(fileData->currentFile);
        fileData->currentFile = NULL;
    }
    fileData->layer++;

    return concatfs_read_next(irp, state);
}

static status_t concatfs_read_open_complete(irp_t* irp, void* ctx)
{
    concatfs_read_state_t* state = ctx;
    concatfs_file_data_t* fileData = state->fileData;
    irp_frame_t* frame = irp_current(irp);

    if (IS_ERR(irp->status))
    {
        UNREF(fileData->currentFile);
        fileData->currentFile = NULL;
        fileData->layer++;
        return concatfs_read_next(irp, state);
    }

    irp_prep_read(irp, frame->read.buffer, IOCUR);
    irp_set_complete(irp, concatfs_read_complete, state);
    irp->status = OK;
    return file_call(fileData->currentFile, irp);
}

static status_t concatfs_read_next(irp_t* irp, concatfs_read_state_t* state)
{
    concatfs_file_data_t* fileData = state->fileData;
    irp_frame_t* frame = irp_current(irp);
    concatfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, concatfs_vnode_t, vnode);

    if (fileData->layer >= vnode->targetCount)
    {
        irp->status = INFO(VFS, EOF);
        irp->result = 0;
        cache_free(state);
        return OK;
    }

    if (fileData->currentFile == NULL)
    {
        dentry_t* targetDentry = vnode->targets[fileData->layer];

        fileData->currentFile = file_new(targetDentry, frame->file->path.binding, MODE_READ | MODE_DIRECTORY);
        if (fileData->currentFile == NULL)
        {
            irp->status = ERR(FS, NOMEM);
            cache_free(state);
            return OK;
        }

        if (targetDentry->vnode->cls->handlers[IRP_MJ_OPEN] != NULL)
        {
            irp_prep_open(irp, NULL);
            irp_set_complete(irp, concatfs_read_open_complete, state);
            irp->status = OK;
            return file_call(fileData->currentFile, irp);
        }
    }

    irp_prep_read(irp, frame->read.buffer, IOCUR);
    irp_set_complete(irp, concatfs_read_complete, state);
    irp->status = OK;
    return file_call(fileData->currentFile, irp);
}

static status_t concatfs_read(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    concatfs_file_data_t* fileData = frame->file->data;

    if (fileData == NULL)
    {
        return ERR(FS, INVAL);
    }

    concatfs_read_state_t* state = cache_alloc(&readCache);
    if (state == NULL)
    {
        return ERR(FS, NOMEM);
    }
    state->fileData = fileData;

    return concatfs_read_next(irp, state);
}

static status_t concatfs_reclaim(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    concatfs_vnode_t* vnode = CONTAINER_OF(frame->vnode, concatfs_vnode_t, vnode);

    for (size_t i = 0; i < vnode->targetCount; i++)
    {
        UNREF(vnode->targets[i]);
    }

    if (vnode->targets != NULL)
    {
        free(vnode->targets);
    }

    return OK;
}

static status_t concatfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    if (frame->file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    concatfs_vnode_t* vnode = concatfs_vnode_new(volume_new(), 0);
    if (vnode == NULL)
    {
        return ERR(FS, NOMEM);
    }

    dentry_t* root = dentry_new(NULL, NULL);
    if (root == NULL)
    {
        UNREF(vnode);
        return ERR(FS, NOMEM);
    }
    UNREF_DEFER(root);

    dentry_make_positive(root, &vnode->vnode);

    const char* key;
    char* value;
    OPTIONS_FOR_EACH(frame->open.payload, key, value)
    {
        if (strcmp(key, "targets") == 0)
        {
            const char* p = value;
            while (*p != '\0')
            {
                fd_t target;
                if (sscanf(p, "%llu", &target) != 1)
                {
                    return ERR(FS, INVAL);
                }

                file_t* file = file_table_get(&irp->process->files, target);
                if (file == NULL)
                {
                    return ERR(FS, BADFD);
                }
                UNREF_DEFER(file);

                status_t status = concatfs_vnode_push_target(vnode, file->path.dentry);
                if (IS_ERR(status))
                {
                    return status;
                }

                while (*p != '\0' && *p != ',')
                {
                    p++;
                }
                if (*p == ',')
                {
                    p++;
                }
                else if (*p != '\0')
                {
                    return ERR(FS, INVAL);
                }
            }
        }
        else
        {
            return ERR(FS, INVAL);
        }
    }

    return file_redirect(frame->file, root);
}

static vnode_class_t cloneClass = {
    .name = "concatfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = concatfs_clone_open,
        },
};

static filesystem_t concatfs = {
    .name = "concatfs",
    .clone = &cloneClass,
};

void concatfs_init(void)
{
    status_t status = filesystem_register(&concatfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register concatfs %Y", status);
    }
}
