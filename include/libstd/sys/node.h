#ifndef _SYS_NODE_H
#define _SYS_NODE_H 1

#include <stdint.h>
#include <string.h>
#include <sys/list.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/MAX_NAME.h"

typedef struct node
{
    list_entry_t entry;
    uint64_t type;
    struct node* parent;
    list_t children;
    uint64_t childAmount;
    char name[MAX_NAME];
} node_t;

#define NODE_CONTAINER(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))
#define NODE_CONTAINER_SAFE(ptr, type, member) \
    ({ \
        node_t* n = ptr; \
        ((n != NULL) ? NODE_CONTAINER(n, type, member) : NULL); \
    })

static inline void node_init(node_t* node, const char* name, uint64_t type)
{
    list_entry_init(&node->entry);
    node->type = type;
    node->parent = NULL;
    list_init(&node->children);
    node->childAmount = 0;
    strcpy(node->name, name);
}

static inline void node_push(node_t* parent, node_t* child)
{
    child->parent = parent;
    list_push(&parent->children, &child->entry);
    parent->childAmount++;
}

static inline uint64_t node_remove(node_t* node)
{
    if (!list_empty(&node->children))
    {
        return ERR;
    }

    if (node->parent != NULL)
    {
        node->parent->childAmount--;
        list_remove(&node->entry);
        node->parent = NULL;
    }

    return 0;
}

static inline node_t* node_find(node_t* node, const char* name)
{
    node_t* child;
    LIST_FOR_EACH(child, &node->children, entry)
    {
        if (strcmp(child->name, name) == 0)
        {
            return child;
        }
    }

    return NULL;
}

#if defined(__cplusplus)
}
#endif

#endif
