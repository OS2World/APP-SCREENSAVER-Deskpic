/***************************************************************************\
* Module Name: fade.c
\***************************************************************************/

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS

#include <os2.h>
#include <stdlib.h>

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

void far pascal fadedaemon(void);

extern SAVERBLOCK near saverinfo;
extern ULONG near savesem;

void far pascal fadedaemon()

{
	int shift,taps;
	ULONG mask,xmask,ymask,bits,accu,finish;
	HAB dhab;
	HPS hps;
	POINTL pnt;
	static char extra[] = { 0,0,1,0,0,0,0,1,0,2,1,0,0,1,0 };
	static ULONG feedback[] =
		{ 0x10004, 0x20040, 0x80004, 0x80004, 0x100002, 0x200001, 0x400010,
		0x1000004, 0x1000004, 0x8000004, 0x8000004, 0x8000004, 0x10000004,
		0x40000004, 0x40000004 };

	dhab = WinInitialize(0);
	for (shift = 0; 1L<<shift < saverinfo.screenrectl.xRight; shift++);
	for (taps = 0; 1L<<taps < saverinfo.screenrectl.yTop; taps++);
	ymask = (1L<<taps)-1;
	taps += shift-17;
	if (taps < 0) taps = 0;
	bits = feedback[taps];
	shift += extra[taps];
	xmask = (1L<<shift)-1;
	mask = (1L<<taps+extra[taps]+17)-1;
	finish = accu = mask&((long)rand()|1);
	hps = WinGetPS(saverinfo.screenhwnd);
	GpiSetColor(hps,CLR_BLACK);
	pnt.x = pnt.y = 0;
	GpiSetPel(hps,&pnt);
	do {
		accu = (accu<<1)|((accu&bits) && (accu&bits) != bits);
		pnt.x = accu&xmask;
		if (pnt.x < saverinfo.screenrectl.xRight) {
			pnt.y = (accu>>shift)&ymask;
			if (pnt.y < saverinfo.screenrectl.yTop) GpiSetPel(hps,&pnt);
		}
	} while (!saverinfo.closesemaphore && (accu&mask) != finish);
	WinReleasePS(hps);
	WinTerminate(dhab);
	DosSemWait(&savesem,SEM_INDEFINITE_WAIT);
	while (!saverinfo.closesemaphore);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo.closesemaphore);
}
