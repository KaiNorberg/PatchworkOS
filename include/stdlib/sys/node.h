#include <string.h>
#ifndef _SYS_NODE_H
#define _SYS_NODE_H 1

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct node
{
    list_entry_t entry;
    uint64_t type;
    struct node* parent;
    list_t children;
    char name[MAX_NAME];
} node_t;

#define NODE_CONTAINER(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

static inline void node_init(node_t* node, const char* name, uint64_t type)
{
    list_entry_init(&node->entry);
    node->type = type;
    node->parent = NULL;
    list_init(&node->children);
    strcpy(node->name, name);
}

static inline void node_push(node_t* parent, node_t* child)
{
    child->parent = parent;
    list_push(&parent->children, &child->entry);
}

static inline uint64_t node_remove(node_t* node)
{
    if (!list_empty(&node->children))
    {
        return ERR;
    }

    if (node->parent != NULL)
    {
        list_remove(&node->entry);
        node->parent = NULL;
    }

    return 0;
}

static inline node_t* node_find(node_t* node, const char* name, char deliminator)
{
    node_t* child;
    LIST_FOR_EACH(child, &node->children, entry)
    {
        for (uint64_t i = 0; i < MAX_NAME; i++)
        {
            if (name[i] == '\0' || name[i] == deliminator)
            {
                if (child->name[i] == '\0' || child->name[i] == deliminator)
                {
                    return child;
                }
                else
                {
                    break;
                }
            }
            if (name[i] != child->name[i])
            {
                break;
            }
        }
    }

    return NULL;
}

static inline node_t* node_traverse(node_t* node, const char* path, char deliminator)
{
    while (path != NULL && path[0] != '\0')
    {
        if (path[0] == deliminator)
        {
            path++;
        }

        node_t* child = node_find(node, path, deliminator);
        if (child == NULL)
        {
            return NULL;
        }

        path = strchr(path, deliminator);
        node = child;
    }

    return node;
}

#if defined(__cplusplus)
}
#endif

#endif
