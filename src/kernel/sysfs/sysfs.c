#include "sysfs.h"

#include <string.h>

#include "heap/heap.h"
#include "sched/sched.h"
#include "tty/tty.h"
#include "vfs/vfs.h"
#include "vfs/utils/utils.h"

static Filesystem sysfs;
/*static Node* root;

static Node* node_new(Node* parent, const char* name, NodeType type, NodeContext context)
{
    Node* node = kmalloc(sizeof(Node));
    list_head_init(&node->head);
    vfs_copy_name(node->name, name);
    node->type = type;
    node->context = context;
    list_init(&node->children);
    lock_init(&node->lock);

    if (parent != NULL)
    {
        LOCK_GUARD(parent->lock);
        list_push(&parent->children, node);
    }

    return node;
}

static Node* node_find_child(Node* parent, const char* name)
{
    LOCK_GUARD(parent->lock);

    Node* child;
    LIST_FOR_EACH(child, &parent->children)
    {
        if (vfs_compare_names(name, child->name))
        {
            return child; 
        }   
    }

    return NULL;
}

static Node* sysfs_traverse(const char* path)
{
    Node* node = root;
    const char* name = vfs_first_dir(path);
    while (name != NULL)
    {
        node = node_find_child(node, name);
        if (node == NULL)
        {
            return NULL;
        }

        name = vfs_next_name(name);
    }

    return node;
}

File* sysfs_open(Drive* volume, const char* path)
{
    Node* node = sysfs_traverse(path);
    if (node == NULL)
    {
        return NULLPTR(EPATH);
    }
    if (node->type != NODE_TYPE_FILE)
    {
        return NULLPTR(EPATH);
    }

    return file_new(volume, node);
}

uint64_t sysfs_create_node(const char* path, NodeContext context)
{
    Node* node = root;
    const char* name = vfs_first_dir(path);
    while (name != NULL)
    {
        Node* child = node_find_child(node, name);
        if (child == NULL)
        {
            child = node_new(node, name, NODE_TYPE_DIR, );
        }

        name = vfs_next_dir(name);
    }

    return node;
}*/

void sysfs_init()
{
    tty_start_message("Sysfs initializing");

    //root = node_new(NULL, "root", NODE_TYPE_DIR, NULL);

    /*memset(&sysfs, 0, sizeof(Filesystem));
    sysfs.name = "sysfs";

    if (vfs_mount('A', &sysfs, NULL) == ERR)
    {
        tty_print("Failed to mount sysfs");
        tty_end_message(TTY_MESSAGE_ER);
    }*/

    tty_end_message(TTY_MESSAGE_OK);
}