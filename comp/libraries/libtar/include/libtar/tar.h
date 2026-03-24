#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/status.h>

/**
 * @brief Tar parser library.
 * @defgroup tar libtar
 *
 * The included library provides functions for parsing tar archives, specifically UStar format.
 *
 * ## Format
 *
 * A tar file consists of a series of tar objects, each stored sequentially, starting with a header and followed by the
 * objects data aligned to a 512-byte boundary.
 *
 * @see [Wikipedia](https://en.wikipedia.org/wiki/Tar_(computing))
 *
 * @{
 */

#define TAR_BLOCK_SIZE 512 ///< Tar block size in bytes.

/**
 * @brief Tar file type enumeration.
 *
 * Represents the type of a tar file entry stored in `tar_header_t::typeflag`.
 */
typedef enum
{
    TAR_TYPE_REGULAR = '0',
    TAR_TYPE_LINK = '1',
    TAR_TYPE_SYMLINK = '2',
    TAR_TYPE_CHAR = '3',
    TAR_TYPE_BLOCK = '4',
    TAR_TYPE_DIR = '5',
    TAR_TYPE_FIFO = '6',
    TAR_TYPE_CONTIGUOUS = '7',
} tar_type_t;

/**
 * @brief Tar header structure.
 * @struct tar_header_t
 *
 * Stored at the beginning of each tar object.
 */
typedef struct
{
    uint8_t name[100];
    uint8_t mode[8];
    uint8_t uid[8];
    uint8_t gid[8];
    uint8_t size[12];
    uint8_t mtime[12];
    uint8_t chksum[8];
    uint8_t typeflag;
    uint8_t linkname[100];
    uint8_t magic[6];
    uint8_t version[2];
    uint8_t uname[32];
    uint8_t gname[32];
    uint8_t devmajor[8];
    uint8_t devminor[8];
    uint8_t prefix[155];
    uint8_t _padding[12];
} tar_header_t;

#ifdef static_assert
static_assert(sizeof(tar_header_t) == 512, "tar_header_t is not 512 bytes");
#endif

/**
 * @brief Tar structure.
 * @struct tar_t
 *
 * Represents a loaded tar file with its data and size.
 */
typedef struct
{
    void* data;
    size_t size;
} tar_t;

/*
 * @brief Tar entry structure.
 * @struct tar_entry_t
 *
 * Represents a parsed entry from a tar file.
 */
typedef struct
{
    char name[256];
    char linkname[256];
    tar_type_t type;
    size_t size;
    uint64_t mtime;
    const uint8_t* data;
} tar_entry_t;

/**
 * @brief Tar iterator structure.
 * @struct tar_iter_t
 */
typedef struct
{
    const tar_t* tar;
    size_t offset;
} tar_iter_t;

/**
 * @brief Initialize a tar structure.
 *
 * @param tar The tar structure to initialize.
 * @param data The data buffer of the tar file.
 * @param size The size of the data buffer.
 */
static inline void tar_init(tar_t* tar, void* data, size_t size)
{
    tar->data = data;
    tar->size = size;
}

/**
 * @brief Initialize a tar iterator.
 *
 * @param iter The iterator to initialize.
 * @param tar The tar file to iterate over.
 */
static inline void tar_iter_init(tar_iter_t* iter, const tar_t* tar)
{
    iter->tar = tar;
    iter->offset = 0;
}

static inline size_t _tar_block_align(size_t size)
{
    return (size + (TAR_BLOCK_SIZE - 1)) & ~(TAR_BLOCK_SIZE - 1);
}

static inline uint64_t _tar_octal(const uint8_t* str, size_t len)
{
    /// "... base-256 coding that is indicated by setting the high-order bit of the leftmost byte of a numeric field." -
    /// Wikipedia
    if (str[0] & (1 << 7))
    {
        uint64_t result = str[0] & ~(1 << 7);
        for (size_t i = 1; i < len; i++)
        {
            result = (result << 8) | str[i];
        }
        return result;
    }

    uint64_t result = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (result == 0 && (str[i] == ' ' || str[i] == '\0'))
        {
            continue;
        }
        if (str[i] < '0' || str[i] > '7')
        {
            break;
        }
        result = (result << 3) | (str[i] - '0');
    }
    return result;
}

static inline bool _tar_checksum_valid(const tar_header_t* header)
{
    uint64_t sum = 0;
    const uint8_t* raw = (const uint8_t*)header;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++)
    {
        if (i >= offsetof(tar_header_t, chksum) && i < offsetof(tar_header_t, chksum) + sizeof(header->chksum))
        {
            sum += ' ';
            continue;
        }
        sum += raw[i];
    }

    uint64_t stored = _tar_octal((const uint8_t*)header->chksum, sizeof(header->chksum));
    return sum == stored;
}

static inline bool _tar_is_zeros(const uint8_t* block)
{
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++)
    {
        if (block[i] != 0)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Advances the iterator to the next entry in the tar archive.
 *
 * @param iter The iterator to advance.
 * @param entry The entry to fill with the next entry's data.
 * @return An appropriate status value.
 */
static inline status_t tar_next(tar_iter_t* iter, tar_entry_t* entry)
{
    const tar_t* tar = iter->tar;

    if (iter->offset + sizeof(tar_header_t) > tar->size)
    {
        return ERR(USER, INVAL);
    }

    const tar_header_t* header = (const tar_header_t*)(tar->data + iter->offset);
    if (_tar_is_zeros((const uint8_t*)header))
    {
        return INFO(USER, EOF);
    }

    if (!_tar_checksum_valid(header))
    {
        return ERR(USER, ILSEQ);
    }
    iter->offset += sizeof(tar_header_t);

    size_t dataSize = _tar_octal((const uint8_t*)header->size, sizeof(header->size));
    size_t dataSizeAligned = _tar_block_align(dataSize);

    if (iter->offset + dataSizeAligned > tar->size)
    {
        return ERR(USER, INVAL);
    }

    iter->offset += dataSizeAligned;

    if (header->prefix[0] != 0)
    {
        snprintf(entry->name, sizeof(entry->name), "%.*s/%.*s", (int)sizeof(header->prefix),
            (const char*)header->prefix, (int)sizeof(header->name), (const char*)header->name);
    }
    else
    {
        snprintf(entry->name, sizeof(entry->name), "%.*s", (int)sizeof(header->name), (const char*)header->name);
    }

    if (header->typeflag == TAR_TYPE_SYMLINK)
    {
        snprintf(entry->linkname, sizeof(entry->linkname), "%.*s", (int)sizeof(header->linkname),
            (const char*)header->linkname);
    }

    entry->type = header->typeflag;
    entry->size = dataSize;
    entry->mtime = _tar_octal((const uint8_t*)header->mtime, sizeof(header->mtime));
    entry->data = tar->data + iter->offset - dataSizeAligned;
    return OK;
}

/** @} */
