#ifndef _DLFCN_H
#define _DLFCN_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL 0x00000

int dlclose(void* handle);
char* dlerror(void);
void* dlopen(const char* filename, int flag);
void* dlsym(void* handle, const char* symbol);

#ifdef __cplusplus
}
#endif

#endif