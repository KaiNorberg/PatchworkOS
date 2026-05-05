#ifndef _SYS_SCON_PRIV_H
#define _SYS_SCON_PRIV_H 1

#include <libc/scon.h>

#define SCON_MAX_DEPTH 16
#define SCON_MAX_ERROR_LEN 512

typedef enum scon_item_flags
{
    SCON_ITEM_NONE = 0,
    SCON_ITEM_DYNAMIC = (1 << 0),
} scon_item_flags_t;

typedef struct scon_item
{
    scon_item_t* next;
    scon_type_t type;
    scon_item_flags_t flags;
    size_t sourceIndex;
    union {
        struct
        {
            const char* str;
            size_t length;
        } atom;
        dstr_t dstr;
        struct
        {
            scon_item_t* first;
            scon_item_t* last;
        } list;
    };
} scon_item_t;

#define SCON_ARENA_BLOCK_SIZE 128

typedef struct scon_block
{
    list_entry_t link;
    size_t used;
    scon_item_t items[SCON_ARENA_BLOCK_SIZE];
} scon_block_t;

typedef struct scon
{
    const char* input;
    scon_item_t* root;
    list_t blocks;
    scon_item_t* freeList;
    char error[SCON_MAX_ERROR_LEN];
} scon_t;

#endif
