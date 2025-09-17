#include "data_object.h"

#include "mem/heap.h"

void aml_string_deinit(aml_string_t* string)
{
    if (string->allocated && string->content != NULL)
    {
        heap_free(string->content);
        string->content = NULL;
        string->length = 0;
        string->allocated = false;
    }
}

void aml_buffer_deinit(aml_buffer_t* buffer)
{
    if (buffer->allocated && buffer->content != NULL)
    {
        heap_free(buffer->content);
        buffer->content = NULL;
        buffer->length = 0;
        buffer->capacity = 0;
        buffer->allocated = false;
    }
}

void aml_package_deinit(aml_package_t* package)
{
    if (package->elements != NULL)
    {
        for (uint64_t i = 0; i < package->numElements; i++)
        {
            aml_data_object_deinit(&package->elements[i]);
        }
        heap_free(package->elements);
        package->elements = NULL;
        package->numElements = 0;
    }
}
