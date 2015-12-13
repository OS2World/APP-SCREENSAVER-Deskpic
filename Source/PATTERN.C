/***************************************************************************\
* PATTERN.C - An example Desktop Picture Reader extension by John Ridges
\***************************************************************************/

#define INCL_BITMAPFILEFORMAT
#define INCL_DOSFILEMGR
#define INCL_GPIBITMAPS
#define INCL_WINSYS

#include <os2.h>
#include <string.h>

char far * far pascal _loadds deskpicextension(HMODULE);
ULONG far pascal _loadds deskpicgetsize(HFILE);
BOOL far pascal _loadds deskpicread(HFILE, SEL, BITMAPINFO far *);
void far pascal _loadds deskpicinfo(char far *, HWND);

static USHORT near cx;   /* Width of screen in pels */
static USHORT near cy;   /* Height of screen in pels */
static BITMAPFILEHEADER near filehead;   /* The icon's header */

char far * far pascal _loadds deskpicextension(module)
HMODULE module;
{
	module;   /* Reference the argument so we don't get compiler warnings */
	return ".ICO";   /* Tell DESKPIC to look for .ICO extensions */
}

ULONG far pascal _loadds deskpicgetsize(hf)
HFILE hf;
{
	USHORT n;
	
	/* Read the icon's header */
	if (DosRead(hf,&filehead,sizeof(BITMAPFILEHEADER),&n) ||
		n != sizeof(BITMAPFILEHEADER)) return 0L;

	/* Check that the icon is big enough and has only one color plane */
	if (filehead.bmp.cx < 16 || filehead.bmp.cy < 16 ||
		filehead.bmp.cPlanes != 1 || filehead.bmp.cBitCount > 8) return 0L;

	/* Get the screen size */
	cx = (USHORT)WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN);
	cy = (USHORT)WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN);

	/* Return the amount of memory needed for the whole screen */
	return (ULONG)cy*((cx*filehead.bmp.cBitCount+7>>3)+3&0xfffc);
}

BOOL far pascal _loadds deskpicread(hf,sel,cbmi)
HFILE hf;
SEL sel;
BITMAPINFO far *cbmi;
{
	USHORT i,n,y;
	ULONG row;
	BYTE huge *abbitmap;

	/* Specify a bitmap the size of the screen */
	cbmi->cbFix = 12;
	cbmi->cx = cx;
	cbmi->cy = cy;
	cbmi->cPlanes = 1;
	cbmi->cBitCount = filehead.bmp.cBitCount;

	/* Read in the icon's color table */
	for (i = 0; i < (USHORT)(1<<filehead.bmp.cBitCount); i++)
		if (DosRead(hf,&cbmi->argbColor[i],sizeof(RGB),&n) || n != sizeof(RGB))
			return TRUE;

	/* Get a pointer to the bitmap data */
	abbitmap = MAKEP(sel,0);

	/* Duplicate the first 16 rows of icon data over every row of the bitmap */
	for (y = 0; y < cy; y++) {

		/* Locate the row in the icon data */
		DosChgFilePtr(hf,(LONG)filehead.offBits+
			(LONG)(y%16)*((filehead.bmp.cx*filehead.bmp.cBitCount+7>>3)+3&0xfffc),
			FILE_BEGIN,&row);

		/* Locate the row in the bitmap */
		row = (ULONG)y*((cx*filehead.bmp.cBitCount+7>>3)+3&0xfffc);

		/* Read the row of icon data */
		for (n = 0; n < filehead.bmp.cBitCount<<1; n++)
			if (DosRead(hf,abbitmap+row+n,1,&i) || i != 1) return TRUE;

		/* Duplicate the data over the rest of the bitmap row */
		for (i = 0; i < (cx*filehead.bmp.cBitCount+7>>3)-n; i++)
			abbitmap[row+n+i] = abbitmap[row+i];
	}

	/* Return success */
	return FALSE;
}

void far pascal _loadds deskpicinfo(filepath,hwnd)
char far *filepath;
HWND hwnd;
{
	char temp[255];

	/* Create and display an "about" message */
	strcpy(temp,"PATTERN.DPR - Generates a desktop pattern from a PM 1.1 "
		"Icon.\n\nby John Ridges\n\nCurrent Icon:  ");
	strcat(temp,filepath);
	WinMessageBox(HWND_DESKTOP,hwnd,temp,"DESKPIC",0,MB_OK|MB_NOICON);
}
