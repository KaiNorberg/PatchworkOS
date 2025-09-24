#include "aml.h"

#include "aml_state.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "log/log.h"
#include "runtime/lock_rule.h"
#include "runtime/opregion.h"

#include "log/log.h"

#include <errno.h>
#include <sys/math.h>

static mutex_t globalMutex;

static aml_node_t* root = NULL;

static inline uint64_t aml_create_predefined_node(const char* name, aml_data_type_t type)
{
    aml_node_t* node = aml_node_new(root, name, AML_NODE_PREDEFINED);
    if (node == NULL)
    {
        return ERR;
    }

    switch (type)
    {
    case AML_DATA_DEVICE:
        if (aml_node_init_device(node) == ERR)
        {
            aml_node_free(node);
            return ERR;
        }
        break;
    default:
        LOG_ERR("unimplemented predefined node type '%s' for node '%s'\n", aml_data_type_to_string(type), name);
        aml_node_free(node);
        errno = ENOSYS;
        return ERR;
    }

    return 0;
}

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

    // Normal predefined root nodes, see section 5.3.1 of the ACPI specification.
    if (aml_create_predefined_node("_GPE", AML_DATA_DEVICE) == ERR ||
        aml_create_predefined_node("_PR", AML_DATA_DEVICE) == ERR ||
        aml_create_predefined_node("_SB", AML_DATA_DEVICE) == ERR ||
        aml_create_predefined_node("_SI", AML_DATA_DEVICE) == ERR ||
        aml_create_predefined_node("_TZ", AML_DATA_DEVICE) == ERR)
    {
        aml_node_free(root);
        root = NULL;
        return ERR;
    }

    // OS specific predefined nodes, see section 5.7 of the ACPI specification.
    /*if (aml_create_predefined_node("_GL", AML_DATA_MUTEX) == ERR ||
        aml_create_predefined_node("_OS", AML_DATA_STRING) == ERR ||
        aml_create_predefined_node("_OSI", AML_DATA_METHOD) == ERR ||
        aml_create_predefined_node("_REV", AML_DATA_INTEGER) == ERR)
    {
        aml_node_free(root);
        root = NULL;
        return ERR;
    }*/

    return 0;
}

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList.
    // The DefBlockHeader is already read as thats the `sdt_header_t`.
    // So the entire code is a termlist.

    aml_state_t state;
    if (aml_state_init(&state, data, size) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_term_list_read(&state, aml_root_get(), size);

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
