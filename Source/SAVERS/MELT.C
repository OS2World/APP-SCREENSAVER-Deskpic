/***************************************************************************\
* MELT.C - A melting screen saver extension by John Ridges
\***************************************************************************/

#define DELAYTIME 20

#define INCL_DOSPROCESS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINRECTANGLES
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
} PROFILEREC;

static int near rand(long);

static ULONG near seed;
static char name[] = "Melting screen";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE };

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
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	int dx,dy;
	HAB hab;
	HPS hps;
	ULONG time;
	RECTL srect;

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	seed = WinGetCurrentTime(hab);
	WinSetRectEmpty(hab,&srect);
	while (!saverinfo->closesemaphore) {
		time = WinGetCurrentTime(hab);
		srect.xLeft += rand(3);
		srect.xRight -= rand(3);
		srect.yBottom += rand(3);
		srect.yTop -= rand(3);
		if (WinIsRectEmpty(hab,&srect)) {
			srect.xLeft = rand(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft)+
				saverinfo->screenrectl.xLeft;
			srect.yBottom = rand(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom)+
				saverinfo->screenrectl.yBottom;
			dx = rand(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft>>2);
			dy = rand(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom>>2);
			srect.xRight = srect.xLeft+dx;
			srect.yTop = srect.yBottom+dy;
			srect.xLeft -= dx;
			srect.yBottom -= dy;
			dx = dy = 0;
			if (rand(2)) dx = rand(2) ? 1 : -1;
			else dy = rand(2) ? 1 : -1;
		}
		WinScrollWindow(saverinfo->screenhwnd,dx,dy,&srect,&srect,NULL,NULL,0);
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static int near rand(x)
long x;
{
	seed = seed*0x343fd+0x269ec3;
	return (int)((HIUSHORT(seed)&0x7fff)*x>>15);
}
