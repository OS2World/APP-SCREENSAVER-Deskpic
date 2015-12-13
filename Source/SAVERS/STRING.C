/***************************************************************************\
* STRING.C - A string art screen saver extension by John Ridges
\***************************************************************************/

#define DELAYTIME 2500
#define TEETH 60
#define MINSPIN 6

#define INCL_DOSPROCESS
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
} PROFILEREC;

FIXED far pascal FixMul(FIXED, FIXED);
FIXED far pascal FixDiv(FIXED, FIXED);
static int near intrnd(int);
static FIXED near fixrnd(FIXED);
static FIXED near fixsin(FIXED);
static int near rand(void);

static ULONG near seed;
static char name[] = "String";
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
	int x,theta1,theta2,start,inc,rad1,teeth;
	HAB hab;
	HPS hps;
	POINTL pnt;
	ULONG time;
	FIXED rad2,rad3;
	BOOL going,first;
	static long colors[] = { CLR_WHITE, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
		CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED,
		CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY };

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	seed = WinGetCurrentTime(hab);
	rad1 = (int)(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom)>>1;
	if (rad1 > (int)(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft)>>1)
		rad1 = (int)(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft)>>1;
	going = first = TRUE;
	while (!saverinfo->closesemaphore) {
		if (!going) {
			if (WinGetCurrentTime(hab)-time <= DELAYTIME) continue;
			going = first = TRUE;
		}
		if (first) {
		retry:
			first = FALSE;
			start = theta1 = intrnd(TEETH);
			theta2 = 0;
			inc = 1+intrnd(TEETH/2-1);
			teeth = TEETH/10+intrnd(TEETH*2/3);
			for (x = 1; x <= MINSPIN; x++) if ((TEETH*x/inc)*inc == TEETH*x &&
				(TEETH*x/teeth)*teeth == TEETH*x) goto retry;
			rad2 = FixDiv((FIXED)rad1*teeth,TEETH);
			rad3 = MAKEFIXED(rad1,0)-rad2;
			rad2 = rad2/4+fixrnd(rad2*3/4);
			WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
			GpiSetColor(hps,colors[intrnd(sizeof(colors)/sizeof(long))]);
		}
		pnt.x = (FixMul(fixsin(FixDiv(theta1,TEETH)+16384),rad3)+
			FixMul(fixsin(FixDiv(start,TEETH)+FixDiv(theta2,teeth)+16384),rad2)+32768>>16)+
			(saverinfo->screenrectl.xRight+saverinfo->screenrectl.xLeft>>1);
		pnt.y = (FixMul(fixsin(FixDiv(theta1,TEETH)),rad3)+
			FixMul(fixsin(FixDiv(start,TEETH)+FixDiv(theta2,teeth)),rad2)+32768>>16)+
			(saverinfo->screenrectl.yTop+saverinfo->screenrectl.yBottom>>1);
		GpiMove(hps,&pnt);
		theta1 = (theta1+inc)%TEETH;
		theta2 -= inc;
		while (theta2 < 0) theta2 += teeth;
		pnt.x = (FixMul(fixsin(FixDiv(theta1,TEETH)+16384),rad3)+
			FixMul(fixsin(FixDiv(start,TEETH)+FixDiv(theta2,teeth)+16384),rad2)+32768>>16)+
			(saverinfo->screenrectl.xRight+saverinfo->screenrectl.xLeft>>1);
		pnt.y = (FixMul(fixsin(FixDiv(theta1,TEETH)),rad3)+
			FixMul(fixsin(FixDiv(start,TEETH)+FixDiv(theta2,teeth)),rad2)+32768>>16)+
			(saverinfo->screenrectl.yTop+saverinfo->screenrectl.yBottom>>1);
		GpiLine(hps,&pnt);
		if (!theta2 && theta1 == start) {
			time = WinGetCurrentTime(hab);
			going = FALSE;
		}
	}
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static int near intrnd(arg)
int arg;
{
	return FIXEDINT(fixrnd(MAKEFIXED(arg,0)));
}

static FIXED near fixrnd(arg)
FIXED arg;
{
	return FixMul(arg,(FIXED)rand()<<1);
}

static FIXED near fixsin(arg)
FIXED arg;
{
	int i;
	FIXED r,s;
	static FIXED c[] = { -5018418, 5347884, -2709368, 411775 };

	arg -= arg&0xffff0000;
	if (arg > 49152L) arg -= 65536L;
	else if (arg > 16384L) arg = 32768L-arg;
	s = FixMul(arg,arg);
	r = 2602479;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	return FixMul(arg,r);
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
