#include "file_system.h"

#include "string/string.h"
#include "heap/heap.h"
#include "tty/tty.h"

RawDirectory* rootDir;

void print_directory(RawDirectory* directory, uint64_t indentation)
{    
    tty_start_message("File system initializing");

    for (int j = 0; j < indentation * 4; j++)
    {
        tty_put(' ');
    }
    tty_print(directory->Name);
    tty_put('\n');

    for (int i = 0; i < directory->DirectoryAmount; i++)
    {
        print_directory(&directory->Directories[i], indentation + 1);
    }

    for (int i = 0; i < directory->FileAmount; i++)
    {
        for (int j = 0; j < (indentation + 1) * 4; j++)
        {
            tty_put(' ');
        }
        tty_print(directory->Files[i].Name);
        tty_put('\n');
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void file_system_init(RawDirectory* rootDirectory)
{
    rootDir = rootDirectory;
}

uint8_t file_system_compare_names(const char* nameStart, const char* nameEnd, const char* otherName)
{
    uint64_t otherNameLength = strlen(otherName);
    if (otherNameLength != nameEnd - nameStart)
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

RawFile* file_system_get(const char* path)
{
    uint64_t index = 1;
    uint64_t prevIndex = 1;

    RawDirectory* currentDir = rootDir;

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
                for (int i = 0; i < currentDir->DirectoryAmount; i++)
                {
                    if (file_system_compare_names(path + prevIndex, path + index, currentDir->Directories[i].Name))
                    {
                        currentDir = &currentDir->Directories[i];
                        break;
                    }
                }

                prevIndex = index + 1;
            }
            else if (path[index] == '\0')
            {                
                for (int i = 0; i < currentDir->FileAmount; i++)
                {
                    if (file_system_compare_names(path + prevIndex, path + index, currentDir->Files[i].Name))
                    {
                        return &currentDir->Files[i];
                    }
                }

                return 0;
            }
        }
    }

    return 0;
}

FILE* file_system_open(const char* filename, const char* mode)
{
    RawFile* rawFile = file_system_get(filename);

    if (rawFile)
    {
        FILE* newFile = kmalloc(sizeof(FILE));

        newFile->SeekOffset = 0;
        newFile->FileHandle = rawFile;

        return newFile;        
    }
    else
    {
        return 0;
    }
}

uint32_t file_system_seek(FILE *stream, int64_t offset, uint32_t origin)
{
    switch (origin)
    {
    case SEEK_SET:
    {
        stream->SeekOffset = offset;
    }
    break;
    case SEEK_CUR:
    {
        stream->SeekOffset += offset;
    }
    break;
    case SEEK_END:
    {
        stream->SeekOffset = stream->FileHandle->Size + offset;
    }
    break;
    }

    return 0;
}

uint64_t file_system_tell(FILE *stream)
{
    return stream->SeekOffset;
}

uint32_t file_system_get_c(FILE* stream)
{
    uint8_t out = stream->FileHandle->Data[stream->SeekOffset];
    stream->SeekOffset++;
    return out;
}

uint64_t file_system_read(void* buffer, uint64_t size, FILE* stream)
{
    for (uint64_t i = 0; i < size; i++)
    {
        ((uint8_t*)buffer)[i] = file_system_get_c(stream);
    }

    return size;
}

uint32_t file_system_close(FILE* stream)
{
    kfree(stream);

    return 0;
}