#include "VgaFun.h"



EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

static EFI_UGA_PIXEL *vga_screen = (EFI_UGA_PIXEL *)NULL;
static UINT32 vga_hSize,vga_vSize;
static EFI_SURFACE vga_screen_surface;

static EFI_CONSOLE_CONTROL_PROTOCOL *console_control_protocol = (EFI_CONSOLE_CONTROL_PROTOCOL *)NULL;
static UINT32 vga_background_width = 0;
static UINT32 vga_background_height = 0;
static EFI_UGA_PIXEL *vga_background_pix = (EFI_UGA_PIXEL *)NULL;
static EFI_SURFACE vga_background_surface;

static UINT32 vga_charset_width = 0;
static UINT32 vga_charset_height = 0;
static EFI_UGA_PIXEL *vga_charset_pix = (EFI_UGA_PIXEL *)NULL;

static EFI_UGA_PIXEL vga_clearscreen_color = {110,110,110,0};
static EFI_UGA_DRAW_PROTOCOL *uga_draw_protocol;

static s_star stars[N_STARS];

static EFI_EVENT timerEvent;

static UINT32 rng_next = 1;

UINT32 rand()
{
	return ((rng_next = rng_next * 1103515245 + 12345) % (0x7fff + 1));
}

void srand(UINT32 seed) 
{
	rng_next = seed;
}


/*
 * Initialize a star randomly
 */
void InitStar(UINT32 i)
{
	static UINT32 ZMAX = 500;
	UINT32 xmax,ymax;

	/* Star color */
	stars[i].color.Blue = 255;
	stars[i].color.Green = 255;
	stars[i].color.Red = 255;

	/* */
    xmax = ((510*(50+ZMAX)) / (2*50));	
    ymax = ((100*(50+ZMAX)) / (2*50));  
	
	stars[i].z = 500;
    stars[i].x = rand()%(2*xmax+1) - xmax;	  
    stars[i].y = rand()%(2*ymax+1) - ymax;    

    stars[i].posx = 400 + (int) ((stars[i].x * 50) / (stars[i].z));
    stars[i].posy = 445 + (int) ((stars[i].y * 50) / (stars[i].z));
}


/*
 * Initialize starfield
 */
void InitStarfield()
{
	EFI_TIME current_time;
	UINT32 i;

	RT->GetTime(&current_time,(void *)NULL);
	srand(current_time.Nanosecond);

	for(i=0;i<N_STARS;i++)
	{
		InitStar(i);
	}
}


/*
 * Drawx starfirl on surface
 */
void DrawStarfield(EFI_SURFACE *surface)
{
	UINT32 i;

	for(i=0;i<N_STARS;i++)
	{
		CopyMem(surface->pix + (stars[i].posy * surface->width) + (stars[i].posx),&stars[i].color,sizeof(EFI_UGA_PIXEL));
	}
}


/*
 * Update starfierld logic 
 */
void UpdateStarfield()
{
	UINT32 i;

	for(i=0;i<N_STARS;i++)
	{
		stars[i].z -= 2.2f;

        if(stars[i].z > 0)
		{
             stars[i].posx = 400 + (int) ((stars[i].x * 50) / (stars[i].z));
             stars[i].posy = 445 + (int) ((stars[i].y * 50) / (stars[i].z));
		}

        if( (stars[i].posx < 131) || (stars[i].posx > 690) || (stars[i].posy < 351) || (stars[i].posy > 528) || (stars[i].z <= 1))
		{
			InitStar(i);
		}
	}
}


/*
 * Wait a keystroke and return
 */
void WaitForKey() 
{
	UINTN index;
	EFI_INPUT_KEY key;

	BS->WaitForEvent(1,&ST->ConIn->WaitForKey,&index);
	ST->ConIn->ReadKeyStroke(ST->ConIn, &key);

}


/*
 * Draw a transparent letter to backbuffer
 */
void DrawLetter(EFI_SURFACE *surface,CHAR16 ch,UINT32 posx,UINT32 posy)
{
	UINT32 pos = 32,i,j,index;

	if(ch==' ')
		pos = 0;
	else if((ch >= 'a')&&(ch <= 'z'))
		pos = ch - 'a' + 33;
	else if((ch >= 'A')&&(ch <= 'Z'))
		pos = ch - 'A' + 33;
	else if((ch >= '0')&&(ch <= '9'))
		pos = ch - '0' + 15;
	else if(ch=='-')
		pos = 6;
	else if(ch=='/')
		pos = 14;
	else if(ch=='@')
		pos = 32;
	else if(ch=='%')
		pos = 10;
	else if(ch=='$')
		pos = 4;
	else if(ch=='.')
		pos = 13;
	else if(ch=='+')
		pos = 11;
	else if(ch==':')
		pos = 25;
	else if(ch=='?')
		pos = 33;
	else if(ch=='!')
		pos = 1;

	for(i=0;i<16;i++)
	{
		for(j=0;j<16;j++)
		{
			index = pos*16+(i*vga_charset_width)+j;
			if(!(vga_charset_pix[index].Red==255 && vga_charset_pix[index].Blue==255 && vga_charset_pix[index].Green==0))
				surface->pix[posx+j+((posy+i)*surface->width)] = vga_charset_pix[index];
		}
	}
}


/*
 * Draw a string to backbuffer
 */
void DrawString(EFI_SURFACE *surface,CHAR16 *str,UINT32 posx,UINT32 posy)
{
	UINT32 i;
	UINTN len = StrLen(str);
	
	for(i=0;i<len;i++)
	{
		DrawLetter(surface,str[i],posx+i*17,posy);
	}
}


/*
 * Draw a string to backbuffer, old-skool wave effect
 */
void DrawWaveString(EFI_SURFACE *surface,CHAR16 *str,UINT32 posx,UINT32 posy)
{
	UINT32 i;
	UINTN len = StrLen(str);
	static float dt = 0.0f;
	UINT32 x_pos;

	for(i=0;i<len;i++)
	{
		x_pos = posx+i*17;
		DrawLetter(surface,str[i],x_pos,posy+(UINT32) (20.0f * sinf(((float)i / 5.0f)+dt)));
	}

	dt += 0.17f;
}


/*
 * Convert RT_BITMAP raw resource to EFI BltBuffer format
 */
void ConvertBitmapResToBltBuffer(VOID *BmpResImage,UINT32 BmpResImageSize,UINT32 *ImageWidth,UINT32 *imageHeight,EFI_UGA_PIXEL **pix)
{
	PBITMAPINFOHEADER bmInfo = (PBITMAPINFOHEADER)BmpResImage;
	EFI_UGA_PIXEL *current_pix;
	RGBTRIPLE *image_pixels,*current_line_pix;
	UINT32 h,w;

	*ImageWidth = bmInfo->biWidth;
	*imageHeight = bmInfo->biHeight;

	BS->AllocatePool(EfiLoaderData,bmInfo->biWidth*bmInfo->biHeight*sizeof(EFI_UGA_PIXEL),(void **)pix);
	current_pix = *pix;
	SetMem(current_pix,bmInfo->biWidth*bmInfo->biHeight*sizeof(EFI_UGA_PIXEL),0);
	image_pixels = (RGBTRIPLE *)((UINT8 *)bmInfo + sizeof(BITMAPINFOHEADER));

	for(h=0;h<bmInfo->biHeight;h++)
	{
		current_line_pix = &image_pixels[(bmInfo->biHeight - h - 1) * bmInfo->biWidth];
		for(w=0;w<bmInfo->biWidth;w++, current_pix++, current_line_pix++) 
		{
			current_pix->Blue = current_line_pix->rgbtBlue;
			current_pix->Green = current_line_pix->rgbtGreen;
			current_pix->Red = current_line_pix->rgbtRed;
		}
	}

}


/*
 * Initialize EFI UGA mode
 * Use default resolution
 */
EFI_STATUS InitVgaMode(EFI_LOADED_IMAGE *ImageInfo)
{
	UINT8 *vga_background_data = (UINT8 *)NULL;
	UINT8 *vga_charset_data = (UINT8 *)NULL;
	UINT32 vga_background_bmpsize = 0;
	UINT32 vga_charset_bmpsize = 0;
	UINT32 cDepth,rRate;
	EFI_STATUS err;

	/* Get UGA mode info */
	Print(L"[+] Open UGA mode\r\n");
	err = BS->HandleProtocol(ST->ConsoleOutHandle,&UgaDrawProtocol,(void **)&uga_draw_protocol);
	if(err != EFI_SUCCESS) {
			Print(L"[!] UgaDrawProtocol err = %08X\r\n",err);
			return err;
	}

	err = uga_draw_protocol->GetMode(uga_draw_protocol,&vga_hSize,&vga_vSize,&cDepth,&rRate);
	if(err != EFI_SUCCESS) {
			Print(L"[+] UGA GetMode err = %08X\r\n",err);
			return err;
	}
	Print(L"-> width/height = %d x %d\r\n",vga_hSize,vga_vSize);
	Print(L"-> depth/rate = %d bpp, %d Hz\r\n",cDepth,rRate);

	/* Set graphic mode */
	Print(L"[+] Set graphic mode\r\n");
	err = BS->LocateProtocol(&gEfiConsoleControlProtocolGuid,NULL,(void **)&console_control_protocol);
	if(err != EFI_SUCCESS) {
			Print(L"[!] ConsoleControlProtocol err = %08X\r\n",err);
			return err;
	}
	
	err = console_control_protocol->SetMode(console_control_protocol,EfiConsoleControlScreenGraphics);
	if(err!= EFI_SUCCESS) {
		Print(L"[!] Cannot switch graphic mode EfiConsoleControlScreenGraphics = %08X\r\n",err);
			return err;
	}
	
	/* Build backbuffer */
	err = BS->AllocatePool(EfiLoaderData,vga_hSize*vga_vSize*sizeof(EFI_UGA_PIXEL),(void **)&vga_screen);
	if(err != EFI_SUCCESS) {
			Print(L"[!] AllocatePool err = %08X\r\n",err);
			return err;
	}
	vga_screen_surface.pix = vga_screen;
	vga_screen_surface.width = vga_hSize;
	vga_screen_surface.height = vga_vSize;

	/* Extract bitmap from PE resource */
	vga_background_bmpsize = GetResource(ImageInfo,RT_BITMAP,IDB_SPLASHSCREEN,(UINT8 *)NULL);
	err = BS->AllocatePool(EfiLoaderData,vga_background_bmpsize,(void **)&vga_background_data);
	if(err != EFI_SUCCESS) {
			Print(L"[!] AllocatePool err = %08X\r\n",err);
			return err;
	}
	GetResource(ImageInfo,RT_BITMAP,IDB_SPLASHSCREEN,vga_background_data);
	ConvertBitmapResToBltBuffer(vga_background_data,
		vga_background_bmpsize,
		&vga_background_width,
		&vga_background_height,
		&vga_background_pix);
	vga_background_surface.pix = vga_background_pix;
	vga_background_surface.width = vga_background_width;
	vga_background_surface.height = vga_background_height;

	vga_charset_bmpsize = GetResource(ImageInfo,RT_BITMAP,IDB_CHARSET,(UINT8 *)NULL);
	err = BS->AllocatePool(EfiLoaderData,vga_charset_bmpsize,(void **)&vga_charset_data);
	if(err != EFI_SUCCESS) {
			Print(L"[!] AllocatePool err = %08X\r\n",err);
			return err;
	}
	GetResource(ImageInfo,RT_BITMAP,IDB_CHARSET,vga_charset_data);
	ConvertBitmapResToBltBuffer(vga_charset_data,
		vga_charset_bmpsize,
		&vga_charset_width,
		&vga_charset_height,
		&vga_charset_pix);

	/* Create timer  event */
	BS->CreateEvent(EVT_TIMER,0,(EFI_EVENT_NOTIFY)NULL,NULL,&timerEvent);
	BS->SetTimer(timerEvent,TimerPeriodic,10000);

	/* Free stuffs */
	if(vga_background_data)
		BS->FreePool(vga_background_data);
	if(vga_charset_data)
		BS->FreePool(vga_charset_data);

	return EFI_SUCCESS;
}


/*
 * Clear screen with unique color
 */
void VGAClearScreen()
{
	uga_draw_protocol->Blt(uga_draw_protocol,
		&vga_clearscreen_color,
		EfiUgaVideoFill,
		0,0,0,0,
		vga_hSize,
		vga_vSize,
		0);
}


/* 
 * Copy backbuffer to video screen 
*/
void VGAFlipScreen()
{
	uga_draw_protocol->Blt(uga_draw_protocol,
		vga_screen,
		EfiUgaBltBufferToVideo,
		0,0,0,0,
		vga_hSize,
		vga_vSize,
		0);
}


/*
 * Draw stuff on screen
 */
EFI_STATUS DoVgaLoop() 
{
	UINT32 i;
	UINTN index;
	EFI_EVENT events[2];
	EFI_INPUT_KEY keystroke;

	/* Clear screen */
	VGAClearScreen();

	/* Build events for timer / keystrokes */
	events[0] = ST->ConIn->WaitForKey;
	events[1] = timerEvent;

	InitStarfield();

	DrawString(&vga_background_surface,L"             --+++ DreamBoot +++--",5,360);
	DrawString(&vga_background_surface,L"         + Successfull installation @",5,363+2*16);
	DrawString(&vga_background_surface,L"         + Hook installed on bootmgfw",5,363+4*16);
	DrawString(&vga_background_surface,L"                  Have phun %",5,367+6*16);

	do {
		/* Copy background to backbuffer */
		ZeroMem(vga_screen,vga_hSize*vga_vSize*sizeof(EFI_UGA_PIXEL));
		for(i=0;i<vga_background_height;i++)
		{
			CopyMem(vga_screen+(i*vga_hSize),vga_background_pix+(i*vga_background_width),vga_background_width*sizeof(EFI_UGA_PIXEL));
		}

		UpdateStarfield();
		DrawStarfield(&vga_screen_surface);

		DrawWaveString(&vga_screen_surface,L"Press a key to continue",207,500);

		VGAFlipScreen();

		BS->WaitForEvent(2,events,&index);
		if(index == 0)
		{
			ST->ConIn->ReadKeyStroke(ST->ConIn,&keystroke);
			break;
		}
	}while(1);

	return EFI_SUCCESS;
}


/*
 * Exit UGA mode and go back to text mode
 * Free mem
 */
EFI_STATUS CloseVgaMode() 
{
	EFI_STATUS err = EFI_SUCCESS;

	if(vga_background_pix)
		BS->FreePool(vga_background_pix);
	if(vga_charset_pix)
		BS->FreePool(vga_charset_pix);
	if(vga_screen)
		BS->FreePool(vga_screen);

	if(console_control_protocol)
		err = console_control_protocol->SetMode(console_control_protocol,EfiConsoleControlScreenText);

	return err;
}
