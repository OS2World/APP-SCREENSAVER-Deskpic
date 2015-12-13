/***************************************************************************\
* SPHERES.C - A screen saver extension by John Ridges
\***************************************************************************/

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPIPATHS
#define INCL_GPIPRIMITIVES
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINSHELLDATA
#define INCL_WINTIMER

#include <os2.h>

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

char far * far pascal _loadds saverstatus(SAVERBLOCK far *, BOOL far *);
MRESULT CALLBACK saverdialog(HWND, USHORT, MPARAM, MPARAM);
void far pascal _loadds saverthread(void);

static SAVERBLOCK far *saverinfo;

typedef struct {
	BOOL enabled;
	unsigned int count;
} PROFILEREC;

static int near rand(void);

static ULONG near seed;
static char name[] = "Spheres";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 25 };

char far * far pascal _loadds saverstatus(initptr,enabledptr)
SAVERBLOCK far *initptr;
BOOL far *enabledptr;
{
	SHORT i;
	
	saverinfo = initptr;
	if (!gotprofile) {
		i = sizeof(PROFILEREC);
		WinQueryProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,&i);
		gotprofile = TRUE;
	}
	*enabledptr = profile.enabled;
	return name;
}

MRESULT CALLBACK saverdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;
{
	RECTL ourrect,parentrect;

	switch(message) {
	case WM_INITDLG:
		WinQueryWindowRect(hwnd,&ourrect);
		WinQueryWindowRect(HWND_DESKTOP,&parentrect);
		WinSetWindowPos(hwnd,NULL,
			(int)(parentrect.xRight-ourrect.xRight+ourrect.xLeft)/2,
			(int)(parentrect.yTop-ourrect.yTop+ourrect.yBottom)/2,
			0,0,SWP_MOVE);
		if (profile.enabled)
			WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.count,FALSE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinQueryDlgItemShort(hwnd,4,(PSHORT)&profile.count,FALSE);
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	long size;
	HAB hab;
	HPS hps;
	HBITMAP bitmap;
	POINTL pos,aptl[4];
	ULONG count = 1;
	static long colors[] = { CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN, CLR_WHITE,
		CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED,
		CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY };

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	aptl[2].x = aptl[2].y = 0;
	aptl[3].x = aptl[3].y = 208;
	seed = WinGetCurrentTime(hab);
	bitmap = GpiLoadBitmap(hps,saverinfo->thismodule,1,0L,0L);
	while (!saverinfo->closesemaphore) {
		GpiSetClipPath(hps,0L,SCP_RESET);
		if (!--count) {
			WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
			count = profile.count;
		}
		GpiSetBackColor(hps,colors[(long)rand()*(sizeof(colors)/sizeof(long))>>15]);
		size = ((long)rand()*79>>15)*2+52;
		pos.x = saverinfo->screenrectl.xLeft+((long)rand()*
			(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft)>>15);
		pos.y = saverinfo->screenrectl.yBottom+((long)rand()*
			(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom)>>15);
		GpiBeginPath(hps,1L);
		GpiMove(hps,&pos);
		GpiFullArc(hps,DRO_OUTLINE,(size-1)<<15);
		GpiEndPath(hps);
		GpiSetClipPath(hps,1L,SCP_ALTERNATE|SCP_AND);
		aptl[0].x = pos.x-(size>>1)+1;
		aptl[0].y = pos.y-(size>>1)+1;
		aptl[1].x = aptl[0].x+size-1;
		aptl[1].y = aptl[0].y+size-1;
		GpiWCBitBlt(hps,bitmap,4L,aptl,ROP_SRCCOPY,BBO_IGNORE);
	}
	GpiDeleteBitmap(bitmap);
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
