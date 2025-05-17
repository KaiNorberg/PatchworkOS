#ifndef _AUX_CONTAINER_OF_H
#define _AUX_CONTAINER_OF_H 1

#define CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#define CONTAINER_OF_SAFE(ptr, type, member) \
    ({ \
        void* p = ptr; \
        ((p != NULL) ? CONTAINER_OF(p, type, member) : NULL); \
    })

#endif