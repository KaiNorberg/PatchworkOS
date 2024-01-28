#include "ram_disk.h"

#include "string/string.h"
#include "heap/heap.h"
#include "tty/tty.h"

RamDirectory* rootDir;

void print_directory(RamDirectory* directory, uint64_t indentation)
{
    for (uint64_t j = 0; j < indentation * 4; j++)
    {
        tty_put(' ');
    }
    tty_print(directory->name);
    tty_put('\n');

    for (uint64_t i = 0; i < directory->directoryAmount; i++)
    {
        print_directory(&directory->directories[i], indentation + 1);
    }

    for (uint64_t i = 0; i < directory->fileAmount; i++)
    {
        for (uint64_t j = 0; j < (indentation + 1) * 4; j++)
        {
            tty_put(' ');
        }
        tty_print(directory->files[i].name);
        tty_put('\n');
    }
}

void ram_disk_init(RamDirectory* rootDirectory)
{    
    tty_start_message("Ram disk initializing");

    rootDir = rootDirectory;

    tty_end_message(TTY_MESSAGE_OK);
}

uint8_t ram_disk_compare_names(const char* nameStart, const char* nameEnd, const char* otherName)
{
    uint64_t otherNameLength = strlen(otherName);
    if (otherNameLength != (uint64_t)nameEnd - (uint64_t)nameStart)
    {
        return 0;
    }

    uint8_t memoryCompare = memcmp(nameStart, otherName, otherNameLength);
    
    if (memoryCompare == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

RamFile* ram_disk_get(const char* path)
{
    uint64_t index = 1;
    uint64_t prevIndex = 1;

    RamDirectory* currentDir = rootDir;

    if (strlen(path) < 3)
    {
        return 0;
    }

    if (path[0] == '/')
    {
        while (1)
        {
            index++;
        
            if (path[index] == '/')
            {
                for (uint64_t i = 0; i < currentDir->directoryAmount; i++)
                {
                    if (ram_disk_compare_names(path + prevIndex, path + index, currentDir->directories[i].name))
                    {
                        currentDir = &currentDir->directories[i];
                        break;
                    }
                }

                prevIndex = index + 1;
            }
            else if (path[index] == '\0')
            {                
                for (uint64_t i = 0; i < currentDir->fileAmount; i++)
                {
                    if (ram_disk_compare_names(path + prevIndex, path + index, currentDir->files[i].name))
                    {
                        return &currentDir->files[i];
                    }
                }

                return 0;
            }
        }
    }

    return 0;
}

FILE* ram_disk_open(const char* filename)
{
    RamFile* rawFile = ram_disk_get(filename);

    if (rawFile)
    {
        FILE* newFile = kmalloc(sizeof(FILE));

        newFile->seekOffset = 0;
        newFile->fileHandle = rawFile;

        return newFile;        
    }
    else
    {
        return 0;
    }
}

uint32_t ram_disk_seek(FILE *stream, int64_t offset, uint32_t origin)
{
    switch (origin)
    {
    case SEEK_SET:
    {
        stream->seekOffset = offset;
    }
    break;
    case SEEK_CUR:
    {
        stream->seekOffset += offset;
    }
    break;
    case SEEK_END:
    {
        stream->seekOffset = stream->fileHandle->size + offset;
    }
    break;
    }

    return 0;
}

uint64_t ram_disk_tell(FILE *stream)
{
    return stream->seekOffset;
}

uint32_t ram_disk_get_c(FILE* stream)
{
    uint8_t out = stream->fileHandle->data[stream->seekOffset];
    stream->seekOffset++;
    return out;
}

uint64_t ram_disk_read(void* buffer, uint64_t size, FILE* stream)
{
    for (uint64_t i = 0; i < size; i++)
    {
        ((uint8_t*)buffer)[i] = ram_disk_get_c(stream);
    }

    return size;
}

uint32_t ram_disk_close(FILE* stream)
{
    kfree(stream);

    return 0;
}