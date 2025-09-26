#include "aml.h"

#include "aml_patch_up.h"
#include "aml_predefined.h"
#include "aml_state.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "log/log.h"

#include "log/log.h"

#include <errno.h>
#include <sys/math.h>

static mutex_t globalMutex;

static aml_node_t* root = NULL;

uint64_t aml_init(void)
{
    mutex_init(&globalMutex);

    root = aml_node_new(NULL, AML_ROOT_NAME, AML_NODE_ROOT | AML_NODE_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    if (aml_node_init_device(root) == ERR)
    {
        aml_node_free(root);
        root = NULL;
        return ERR;
    }

    if (aml_predefined_init() == ERR)
    {
        aml_node_free(root);
        root = NULL;
        return ERR;
    }

    aml_patch_up_init();

    return 0;
}

uint64_t aml_parse(const uint8_t* start, const uint8_t* end)
{
    if (start == NULL || end == NULL || start >= end)
    {
        errno = EINVAL;
        return ERR;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList.
    // The DefBlockHeader is already read as thats the `sdt_header_t`.
    // So the entire code is a termlist.

    aml_state_t state;
    if (aml_state_init(&state, start, end) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_term_list_read(&state, aml_root_get(), end);

    aml_state_deinit(&state);
    return result;
}

mutex_t* aml_global_mutex_get(void)
{
    return &globalMutex;
}

aml_node_t* aml_root_get(void)
{
    if (root == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return root;
}

void aml_print_tree(aml_node_t* node, uint32_t depth, bool isLast)
{
    for (uint32_t i = 0; i < depth; i++)
    {
        if (i == depth - 1)
        {
            LOG_INFO("%s", isLast ? "└── " : "├── ");
        }
        else
        {
            LOG_INFO("│   ");
        }
    }

    LOG_INFO("%.*s [%s", AML_NAME_LENGTH, node->segment, aml_node_to_string(node));
    switch (node->type)
    {
    default:
        break;
    }
    LOG_INFO("]\n");

    aml_node_t* child;
    LIST_FOR_EACH(child, &node->children, entry)
    {
        aml_print_tree(child, depth + 1, list_last(&node->children) == &child->entry);
    }
}
