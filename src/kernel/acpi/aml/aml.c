#include "aml.h"

#include "aml_state.h"
#include "mem/heap.h"
#include "encoding/term.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>

static mutex_t mutex;

static aml_node_t* root = NULL;

uint64_t aml_init(void)
{
    mutex_init(&mutex);

    root = aml_add_node(NULL, "\\___", AML_NODE_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList. The DefBlockHeader is already read
    // as thats the `acpi_header_t`. So the entire code is a termlist.

    aml_state_t state;
    aml_state_init(&state, data, size);

    // When aml first starts its not in any scope, so we pass NULL as the scope.
    uint64_t result = aml_termlist_read(&state, NULL, size);

    aml_state_deinit(&state);
    return result;
}

aml_node_t* aml_add_node(aml_node_t* parent, const char* name, aml_node_type_t type)
{
    if (name == NULL || strnlen_s(name, AML_MAX_NAME + 1) != AML_MAX_NAME || type < AML_NODE_NONE ||
        type >= AML_NODE_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(&mutex);

    aml_node_t* node = heap_alloc(sizeof(aml_node_t), HEAP_NONE);
    if (node == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    list_entry_init(&node->entry);
    node->type = type;
    list_init(&node->children);
    memcpy(node->name, name, AML_MAX_NAME);

    if (parent != NULL)
    {
        node->parent = parent;
        list_push(&parent->children, &node->entry);
    }
    else
    {
        node->parent = NULL;
    }

    return node;
}

aml_node_t* aml_find_node(const char* path, aml_node_t* start)
{
    MUTEX_SCOPE(&mutex);

    // TODO: Implement aml_find_node
    errno = ENOSYS;
    return NULL;
}

aml_node_t* aml_root_get(void)
{
    MUTEX_SCOPE(&mutex);

    if (root == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return root;
}
