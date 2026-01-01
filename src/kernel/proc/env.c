#include <kernel/proc/env.h>
#include <kernel/sync/mutex.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

void env_init(env_t* env)
{
    env->vars = NULL;
    env->count = 0;
    mutex_init(&env->mutex);
}

void env_deinit(env_t* env)
{
    mutex_deinit(&env->mutex);
    for (uint64_t i = 0; i < env->count; i++)
    {
        free(env->vars[i].key);
        free(env->vars[i].value);
    }
    free(env->vars);
}

uint64_t env_copy(env_t* dest, env_t* src)
{
    if (dest == NULL || src == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&src->mutex);

    for (uint64_t i = 0; i < src->count; i++)
    {
        if (env_set(dest, src->vars[i].key, src->vars[i].value) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

const char* env_get(env_t* env, const char* key)
{
    if (env == NULL || key == NULL)
    {
        return NULL;
    }

    MUTEX_SCOPE(&env->mutex);

    for (uint64_t i = 0; i < env->count; i++)
    {
        if (strcmp(env->vars[i].key, key) == 0)
        {
            return env->vars[i].value;
        }
    }

    return NULL;
}

uint64_t env_set(env_t* env, const char* key, const char* value)
{
    if (env == NULL || key == NULL || value == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&env->mutex);

    for (uint64_t i = 0; i < env->count; i++)
    {
        if (strcmp(env->vars[i].key, key) != 0)
        {
            continue;
        }

        char* newValue = strdup(value);
        if (newValue == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }
        free(env->vars[i].value);
        env->vars[i].value = newValue;
        return 0;
    }

    char* newKey = strdup(key);
    if (newKey == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    char* newValue = strdup(value);
    if (newValue == NULL)
    {
        free(newKey);
        errno = ENOMEM;
        return ERR;
    }

    env_var_t* newVars = realloc(env->vars, sizeof(env_var_t) * (env->count + 1));
    if (newVars == NULL)
    {
        free(newKey);
        free(newValue);
        errno = ENOMEM;
        return ERR;
    }
    env->vars = newVars;

    env->vars[env->count].key = newKey;
    env->vars[env->count].value = newValue;
    env->count++;

    return 0;
}

uint64_t env_unset(env_t* env, const char* key)
{
    if (env == NULL || key == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&env->mutex);

    for (uint64_t i = 0; i < env->count; i++)
    {
        if (strcmp(env->vars[i].key, key) != 0)
        {
            continue;
        }

        free(env->vars[i].key);
        free(env->vars[i].value);

        for (uint64_t j = i; j < env->count - 1; j++)
        {
            env->vars[j] = env->vars[j + 1];
        }

        env->count--;
        if (env->count == 0)
        {
            free(env->vars);
            env->vars = NULL;
        }
        else
        {
            env_var_t* newVars = realloc(env->vars, sizeof(env_var_t) * env->count);
            if (newVars != NULL)
            {
                env->vars = newVars;
            }
        }

        return 0;
    }

    return 0;
}