#include "file_system.h"

#include "libc/include/string.h"

#include "kernel/tty/tty.h"

RawDirectory* rootDir;

void print_directory(RawDirectory* directory, uint64_t indentation)
{
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

FileContent* file_system_get(const char* path)
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