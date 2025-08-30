#include "namespace.h"

#include "tables.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"

static acpi_node_t* acpiRoot = NULL;

static acpi_node_t* acpi_node_new(acpi_node_t* parent, const char* name, acpi_node_type_t type)
{
    if (strlen(name) != ACPI_NODE_NAME_LEN)
    {
        LOG_ERR("Invalid ACPI node name length");
        return NULL;
    }

    acpi_node_t* node = heap_alloc(sizeof(acpi_node_t), HEAP_NONE);
    if (node == NULL)
    {
        return NULL;
    }
    list_entry_init(&node->entry);
    list_init(&node->children);
    memcpy(node->name, name, ACPI_NODE_NAME_LEN);
    node->type = type;

    if (parent != NULL)
    {
        assert(acpiRoot != NULL);
        list_push(&parent->children, &node->entry);
    }
    else
    {
        assert(acpiRoot == NULL);
    }

    return node;
}

void acpi_namespace_init(void)
{
    acpiRoot = acpi_node_new(NULL, "acpi", ACPI_NODE_TYPE_ROOT);
    if (acpiRoot == NULL)
    {
        panic(NULL, "Failed to create ACPI root node");
    }

    acpi_node_t* sbNode = acpi_node_new(acpiRoot, "_SB_", ACPI_NODE_TYPE_DEVICE);
    if (sbNode == NULL)
    {
        panic(NULL, "Failed to create ACPI System Bus node");
    }

    acpi_node_t* siNode = acpi_node_new(acpiRoot, "_SI_", ACPI_NODE_TYPE_DEVICE);
    if (siNode == NULL)
    {
        panic(NULL, "Failed to create ACPI System Indicators node");
    }

    acpi_node_t* gpeNode = acpi_node_new(acpiRoot, "_GPE", ACPI_NODE_TYPE_DEVICE);
    if (gpeNode == NULL)
    {
        panic(NULL, "Failed to create ACPI General Purpose Events node");
    }

    // TODO: Implement dsdt retreval from the FADT in tables.c, then just add everything else.

    /*dsdt_t* dsdt = DSDT_GET();
    if (dsdt == NULL)
    {
        panic(NULL, "Failed to retrieve DSDT");
    }

    LOG_INFO("DSDT found containing %ull bytes of AML code", dsdt->header.length - sizeof(dsdt_t));*/
}
