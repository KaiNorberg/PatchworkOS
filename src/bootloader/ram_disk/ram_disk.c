#include "ram_disk.h"

#include "file_system/file_system.h"
#include "string/string.h"

RamDiskFile ram_disk_load_file(EFI_FILE* volume, CHAR16* path)
{
	EFI_FILE* fileHandle = file_system_open_raw(volume, path);

	UINT64 size = file_system_get_size(fileHandle);
	uint8_t* data = file_system_read(fileHandle, size);

	RamDiskFile output;
	output.name = char16_to_char(path);
	output.data = data;
	output.size = size;

  	file_system_close(fileHandle);

	return output;
}

RamDiskDirectory ram_disk_load_directory(EFI_FILE* volume, const char* name)
{		
	RamDiskDirectory out;
	out.name = name;
	out.files = 0;
	out.fileAmount = 0;
	out.directories = 0;
	out.directoryAmount = 0;

	while (1) 
	{
		EFI_FILE_INFO* fileInfo;
		UINTN fileInfoSize = 0;

		EFI_STATUS status = file_system_read_to_buffer(volume, &fileInfoSize, NULL);
        if (status != EFI_BUFFER_TOO_SMALL) 
		{
            break;
		}

		fileInfo = AllocatePool(fileInfoSize);

		status = file_system_read_to_buffer(volume, &fileInfoSize, fileInfo);
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

				RamDiskDirectory newDirectory = ram_disk_load_directory(subVolume, char16_to_char(fileInfo->FileName));
				
				RamDiskDirectory* newDirectoryArray = AllocatePool(sizeof(RamDiskDirectory) * (out.directoryAmount + 1));
				if (out.directoryAmount != 0)
				{
					CopyMem(newDirectoryArray, out.directories, sizeof(RamDiskDirectory) * out.directoryAmount);
					FreePool(out.directories);
				}
				out.directories = newDirectoryArray;
				out.directories[out.directoryAmount] = newDirectory;
				out.directoryAmount++;

				file_system_close(subVolume);
			}
		}
		else
		{
			RamDiskFile newFile = ram_disk_load_file(volume, fileInfo->FileName);
			
			RamDiskFile* newFileArray = AllocatePool(sizeof(RamDiskFile) * (out.fileAmount + 1));
			if (out.fileAmount != 0)
			{
				CopyMem(newFileArray, out.files, sizeof(RamDiskFile) * out.fileAmount);
				FreePool(out.files);
			}
			out.files = newFileArray;
			out.files[out.fileAmount] = newFile;
			out.fileAmount++;
			
		}

		FreePool(fileInfo);
	}

	return out;
}
