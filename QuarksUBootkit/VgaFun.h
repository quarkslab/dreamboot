#pragma once


#include "common.h"
#include "PeCoffLib.h"
#include "resource.h"
#include <math.h>


/* Starfield */
typedef struct {
	int posx,posy;
	int x,y;
	float z;
    UINT32 vitesse;
	EFI_UGA_PIXEL color;
}s_star;

#define N_STARS 150

typedef struct {
	EFI_UGA_PIXEL *pix;
	UINT32 width;
	UINT32 height;
}EFI_SURFACE;

/*
 * Initialize UGA video mode
 */
EFI_STATUS InitVgaMode(EFI_LOADED_IMAGE *ImageInfo);

/*
 * Draw fun stuffs on screen :)
 */
EFI_STATUS DoVgaLoop();

/*
 * Close UGA video mode and go back to text mode
 */
EFI_STATUS CloseVgaMode();


/*
 * Draw string on screen
 */
void DrawString(EFI_SURFACE *surface,CHAR16 *str,UINT32 posx,UINT32 posy);


/*
 * Wait for a key stroke
 */
void WaitForKey();


/*
 * Some windows BMP header stuffs
 */
typedef struct tagBITMAPINFOHEADER {
	UINT32 biSize;
	UINT32 biWidth;
	UINT32 biHeight;
	UINT16 biPlanes;
	UINT16 biBitCount;
	UINT32 biCompression;
	UINT32 biSizeImage;
	UINT32 biXPelsPerMeter;
	UINT32 biYPelsPerMeter;
	UINT32 biClrUsed;
	UINT32 biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagRGBTRIPLE {
	UINT8 rgbtBlue;
	UINT8 rgbtGreen;
	UINT8 rgbtRed;
} RGBTRIPLE;


/* 
 * Console protocol hack 
 * This protocol were not found in EFI 1.1 for graphic mode switching
 * even UGA protocol is there
*/
#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID \
  { 0xf42f7782, 0x12e, 0x4c12, {0x99, 0x56, 0x49, 0xf9, 0x43, 0x4, 0xf7, 0x21} }

typedef struct _EFI_CONSOLE_CONTROL_PROTOCOL   EFI_CONSOLE_CONTROL_PROTOCOL;


typedef enum {
	EfiConsoleControlScreenText,
	EfiConsoleControlScreenGraphics,
	EfiConsoleControlScreenMaxValue
} EFI_CONSOLE_CONTROL_SCREEN_MODE;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  OUT EFI_CONSOLE_CONTROL_SCREEN_MODE   *Mode,
  OUT BOOLEAN                           *GopUgaExists,  OPTIONAL  
  OUT BOOLEAN                           *StdInLocked    OPTIONAL
  )
/*++

  Routine Description:
    Return the current video mode information. Also returns info about existence
    of Graphics Output devices or UGA Draw devices in system, and if the Std In
    device is locked. All the arguments are optional and only returned if a non
    NULL pointer is passed in.

  Arguments:
    This         - Protocol instance pointer.
    Mode         - Are we in text of grahics mode.
    GopUgaExists - TRUE if Console Spliter has found a GOP or UGA device
    StdInLocked  - TRUE if StdIn device is keyboard locked

  Returns:
    EFI_SUCCESS     - Mode information returned.

--*/
;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  IN  EFI_CONSOLE_CONTROL_SCREEN_MODE   Mode
  )
/*++

  Routine Description:
    Set the current mode to either text or graphics. Graphics is
    for Quiet Boot.

  Arguments:
    This  - Protocol instance pointer.
    Mode  - Mode to set the 

  Returns:
    EFI_SUCCESS     - Mode information returned.

--*/
;


typedef
EFI_STATUS
(EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN) (
  IN  EFI_CONSOLE_CONTROL_PROTOCOL      *This,
  IN CHAR16                             *Password
  )
/*++

  Routine Description:
    Lock Std In devices until Password is typed.

  Arguments:
    This     - Protocol instance pointer.
    Password - Password needed to unlock screen. NULL means unlock keyboard

  Returns:
    EFI_SUCCESS      - Mode information returned.
    EFI_DEVICE_ERROR - Std In not locked

--*/
;


struct _EFI_CONSOLE_CONTROL_PROTOCOL {
  EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE           GetMode;
  EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE           SetMode;
  EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN        LockStdIn;
};

