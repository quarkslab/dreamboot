#pragma once

#include "common.h"
#include <pe.h>

/*
 * Get resource from .rsrc section
 * RT_BITMAP tested only for now
 */
UINT32 GetResource(EFI_LOADED_IMAGE *Image,UINT16 ResName,UINT16 ResId,UINT8 *ResData);

#define    RT_CURSOR           1
#define    RT_BITMAP           2
#define    RT_ICON             3
#define    RT_MENU             4
#define    RT_DIALOG           5
#define    RT_STRING           6
#define    RT_FONTDIR          7
#define    RT_FONT             8
#define    RT_ACCELERATORS     9
#define    RT_RCDATA           10
#define    RT_MESSAGETABLE     11
#define    RT_GROUP_CURSOR     12
#define    RT_GROUP_ICON       14
#define    RT_VERSION          16

typedef struct _IMAGE_RESOURCE_DIRECTORY 
{
	UINT32   Characteristics;
	UINT32   TimeDateStamp;
	UINT16    MajorVersion;
	UINT16    MinorVersion;
	UINT16    NumberOfNamedEntries;
	UINT16    NumberOfIdEntries;
//  IMAGE_RESOURCE_DIRECTORY_ENTRY DirectoryEntries[];
}IMAGE_RESOURCE_DIRECTORY, *PIMAGE_RESOURCE_DIRECTORY;

typedef struct _IMAGE_RESOURCE_DIRECTORY_ENTRY 
{
	UINT32 Name;
	UINT32 OffsetToData;
}IMAGE_RESOURCE_DIRECTORY_ENTRY, *PIMAGE_RESOURCE_DIRECTORY_ENTRY;


typedef struct _IMAGE_RESOURCE_DATA_ENTRY 
{
	UINT32   OffsetToData;
	UINT32   Size;
	UINT32   CodePage;
	UINT32   Reserved;
}IMAGE_RESOURCE_DATA_ENTRY, *PIMAGE_RESOURCE_DATA_ENTRY;

