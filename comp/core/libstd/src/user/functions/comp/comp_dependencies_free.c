#include <sys/comp.h>
#include <stdlib.h>

void comp_dependencies_free(comp_dependency_t* deps, size_t count)
{
    if (deps != NULL)
    {
        for (size_t i = 0; i < count; i++)
        {
            scon_deinit(&deps[i].scon);
            free(deps[i].manifest);
        }
        free(deps);
    }
}
