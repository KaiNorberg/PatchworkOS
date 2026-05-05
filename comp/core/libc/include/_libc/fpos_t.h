#ifndef _INTERNAL_FPOS_T_H
#define _INTERNAL_FPOS_T_H 1

typedef struct fpos
{
    long long unsigned int offset;
    int status;
} fpos_t;

#endif
