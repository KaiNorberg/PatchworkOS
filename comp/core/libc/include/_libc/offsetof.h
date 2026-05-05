#ifndef _INTERNAL_OFFSETOF_H
#define _INTERNAL_OFFSETOF_H 1

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#endif
