#include "PeCoffLib.h"





UINT32 GetResource(EFI_LOADED_IMAGE *Image,UINT16 ResName,UINT16 ResId,UINT8 *ResData)
{
	PIMAGE_DOS_HEADER dosHeader= (PIMAGE_DOS_HEADER)Image->ImageBase;
	PIMAGE_NT_HEADERS pe = (PIMAGE_NT_HEADERS)((UINT8 *)dosHeader + dosHeader->e_lfanew);
	PIMAGE_RESOURCE_DIRECTORY img_res_dir,img_res_dir_base;
	PIMAGE_RESOURCE_DIRECTORY_ENTRY img_res_dir_entry;
	PIMAGE_RESOURCE_DATA_ENTRY img_res_data;
	int nb_resources;

	img_res_dir_base = (PIMAGE_RESOURCE_DIRECTORY) (pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress + pe->OptionalHeader.ImageBase);

	/* Check if there are some resources in RESOURCE_DIRECTORY */
	nb_resources = img_res_dir_base->NumberOfIdEntries + img_res_dir_base->NumberOfNamedEntries;
	if(!nb_resources)
		return 0;

	/* Look for desired resource type in each entry */
	img_res_dir_entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((UINT8 *)img_res_dir_base + sizeof(IMAGE_RESOURCE_DIRECTORY));
	do {
		if(img_res_dir_entry->Name == ResName)		// FIX: if & 0x80000000 => Name => ptr to IMAGE_RESOURCE_DIR_STRING_U 
			break;
		img_res_dir_entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((UINT8 *)img_res_dir_entry + sizeof(IMAGE_RESOURCE_DIRECTORY));
	}while(--nb_resources != 0);

	/* Check for resource entry in this subtype entry */
	if(img_res_dir_entry->OffsetToData & 0x80000000) 
	{
		img_res_dir = (PIMAGE_RESOURCE_DIRECTORY) ((UINT8 *)img_res_dir_base + (img_res_dir_entry->OffsetToData & 0x7FFFFFFF));
		nb_resources = img_res_dir->NumberOfIdEntries + img_res_dir->NumberOfNamedEntries;
		if(!nb_resources)
			return 0;

		img_res_dir_entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((UINT8 *)img_res_dir + sizeof(IMAGE_RESOURCE_DIRECTORY));
		do {
			if(img_res_dir_entry->Name == ResId)		// FIX: if & 0x80000000 => Name => ptr to IMAGE_RESOURCE_DIR_STRING_U 
				break;
			img_res_dir_entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((UINT8 *)img_res_dir_entry + sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
		}while(--nb_resources != 0);

		if(img_res_dir_entry->OffsetToData & 0x80000000) 
		{
			img_res_dir = (PIMAGE_RESOURCE_DIRECTORY) ((UINT8 *)img_res_dir_base + (img_res_dir_entry->OffsetToData & 0x7FFFFFFF));
			nb_resources = img_res_dir->NumberOfIdEntries + img_res_dir->NumberOfNamedEntries;
			if(!nb_resources)
				return 0;
			img_res_dir_entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((UINT8 *)img_res_dir + sizeof(IMAGE_RESOURCE_DIRECTORY));

		}
	}

	/* Get ptr to raw data */
	img_res_data = (PIMAGE_RESOURCE_DATA_ENTRY) ((UINT8 *)img_res_dir_base + img_res_dir_entry->OffsetToData);
	if(ResData)
		CopyMem(ResData,(UINT8 *)pe->OptionalHeader.ImageBase + img_res_data->OffsetToData,img_res_data->Size);

	return img_res_data->Size;
}
