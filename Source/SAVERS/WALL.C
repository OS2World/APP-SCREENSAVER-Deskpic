/***************************************************************************\
* WALL.C - A wallpaper screen saver extension by John Ridges
\***************************************************************************/

#define MAXREPS 32

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
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
	int reps;
} PROFILEREC;

static int near rand(void);

static ULONG near seed;
static char name[] = "Wallpaper";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 8 };

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
	SHORT i;
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
		WinSetDlgItemShort(hwnd,4,profile.reps,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 1 || i > MAXREPS) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The number of tiles must be "
					"between 1 and 32",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			profile.reps = i;
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	int i,size,x,y,xpos,ypos;
	HAB hab;
	HPS hps;
	POINTL pnt;
	static long colors[] = { CLR_WHITE, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
		CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED,
		CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY,
		CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK };

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
	seed = WinGetCurrentTime(hab);
	size = ((int)(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft>>1)+
		profile.reps-1)/profile.reps<<1;
	while (!saverinfo->closesemaphore) {
		x = (int)((long)rand()*size>>15);
		y = (int)((long)rand()*size>>15);
		GpiSetColor(hps,colors[(long)rand()*(sizeof(colors)/sizeof(long))>>15]);
		for (ypos = (int)saverinfo->screenrectl.yTop-size;
			ypos > (int)saverinfo->screenrectl.yBottom-size; ypos -= size) {
			if (saverinfo->closesemaphore) break;
			for (xpos = (int)saverinfo->screenrectl.xLeft; xpos <
				(int)saverinfo->screenrectl.xRight;
				xpos += size) for (i = 0; i < 4; i++) {
				if (i&1) pnt.x = xpos+size-x-1;
				else pnt.x = xpos+x;
				if (i&2) pnt.y = ypos+size-y-1;
				else pnt.y = ypos+y;
				GpiSetPel(hps,&pnt);
				if (i&1) pnt.x = xpos+size-y-1;
				else pnt.x = xpos+y;
				if (i&2) pnt.y = ypos+size-x-1;
				else pnt.y = ypos+x;
				GpiSetPel(hps,&pnt);
			}
		}
	}
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
