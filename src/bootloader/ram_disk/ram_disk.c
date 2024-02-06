#include "ram_disk.h"

#include "file_system/file_system.h"
#include "string/string.h"
#include "memory/memory.h"

#include "../common.h"

RamFile ram_disk_load_file(EFI_FILE* volume, CHAR16* path)
{
	EFI_FILE* fileHandle = file_system_open_raw(volume, path);

	UINT64 size = file_system_get_size(fileHandle);
	uint8_t* data = memory_allocate_pool(size, EFI_MEMORY_TYPE_RAM_DISK);
	file_system_read(fileHandle, size, data);

	RamFile output;
	output.name = char16_to_char(path);
	output.data = data;
	output.size = size;

  	file_system_close(fileHandle);

	return output;
}

void ram_disk_load_directory(RamDirectory* out, EFI_FILE* volume, const char* name)
{			
	out->name = name;
	out->files = 0;
	out->fileAmount = 0;
	out->directories = 0;
	out->directoryAmount = 0;

	while (1) 
	{
		EFI_FILE_INFO* fileInfo;
		UINTN fileInfoSize = 0;

		EFI_STATUS status = uefi_call_wrapper(volume->Read, 3, volume, &fileInfoSize, NULL);
        if (status != EFI_BUFFER_TOO_SMALL) 
		{
            break;
		}

		fileInfo = AllocatePool(fileInfoSize);

		status = file_system_read(volume, fileInfoSize, fileInfo);
		if (EFI_ERROR(status)) 
		{
			Print(L"Error reading file info\n");
			FreePool(fileInfo);
			break;
		}

		if (fileInfo->Attribute & EFI_FILE_DIRECTORY) 
		{
			if (StrCmp(fileInfo->FileName, L".") != 0 && StrCmp(fileInfo->FileName, L"..") != 0) 
			{
				EFI_FILE_PROTOCOL* subVolume = file_system_open_raw(volume, fileInfo->FileName);

				RamDirectory newDirectory;
				ram_disk_load_directory(&newDirectory, subVolume, char16_to_char(fileInfo->FileName));
				
				RamDirectory* newDirectoryArray = memory_allocate_pool(sizeof(RamDirectory) * (out->directoryAmount + 1), EFI_MEMORY_TYPE_RAM_DISK);
				if (out->directoryAmount != 0)
				{
					CopyMem(newDirectoryArray, out->directories, sizeof(RamDirectory) * out->directoryAmount);
					FreePool(out->directories);
				}
				out->directories = newDirectoryArray;
				out->directories[out->directoryAmount] = newDirectory;
				out->directoryAmount++;

				file_system_close(subVolume);
			}
		}
		else
		{
			RamFile newFile = ram_disk_load_file(volume, fileInfo->FileName);
			
			RamFile* newFileArray = memory_allocate_pool(sizeof(RamFile) * (out->fileAmount + 1), EFI_MEMORY_TYPE_RAM_DISK);
			if (out->fileAmount != 0)
			{
				CopyMem(newFileArray, out->files, sizeof(RamFile) * out->fileAmount);
				FreePool(out->files);
			}
			out->files = newFileArray;
			out->files[out->fileAmount] = newFile;
			out->fileAmount++;
		}

		FreePool(fileInfo);
	}
}
