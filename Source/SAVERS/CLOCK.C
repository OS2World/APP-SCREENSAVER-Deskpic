/***************************************************************************\
* CLOCK.C - An clock screen saver extension by John Ridges
\***************************************************************************/

#define MAXSIZE 300
#define DELAYTIME 50

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPITRANSFORMS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
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
	BOOL enabled[5];
	int size;
} PROFILEREC;

void paintclock(HPS,BOOL);
static ULONG near sqr(ULONG);
static int near rand(void);

PROFILEREC profile = { { TRUE, TRUE, TRUE, TRUE, TRUE }, 100 };

static ULONG near seed;
static char name[] = "Clock";
static BOOL gotprofile = FALSE;

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
	*enabledptr = profile.enabled[0];
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
		for (i = 0; i < 5; i++) if (profile.enabled[i])
			WinSendDlgItemMsg(hwnd,i+5,BM_SETCHECK,MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.size,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 3) {
			WinMessageBox(HWND_DESKTOP,hwnd,"The clock screen saver was "
				"inspired by the clock sample program from the Microsoft "
				"Presentation Manager Toolkit.",name,0,MB_OK|MB_NOICON);
			return FALSE;
		}
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 30 || i > MAXSIZE) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The size of the clock must be "
					"between 30 and 300",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			profile.size = i;
			for (i = 0; i < 5; i++) profile.enabled[i] =
				SHORT1FROMMR(WinSendDlgItemMsg(hwnd,i+5,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	int ysize;
	HAB hab;
	HDC memhdc;
	HPS hps,memhps;
	HBITMAP memhbm;
	SIZEL sizl;
	ULONG time,cxRes,cyRes,aldata[2];
	POINTL aptl[3];
	RECTL rectup;
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;
	MATRIXLF mat;
	FIXED xvelocity,yvelocity,xposition,yposition,xtemp,ytemp;
	BOOL xdir,ydir,chdir;
	BOOL first = TRUE;

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
	seed = WinGetCurrentTime(hab);
	dop.pszLogAddress = NULL;
	dop.pszDriverName = "DISPLAY";
	dop.pdriv = NULL;
	dop.pszDataType = NULL;
	memhdc = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,NULL);
	DevQueryCaps(memhdc,CAPS_VERTICAL_RESOLUTION,1L,&cyRes);
	DevQueryCaps(memhdc,CAPS_HORIZONTAL_RESOLUTION,1L,&cxRes);
	ysize = (int)((profile.size*cyRes+(cxRes>>1))/cxRes);
	sizl.cx = 100+(399+profile.size)/profile.size<<1;
	sizl.cy = 100+(399+ysize)/ysize<<1;
	memhps = GpiCreatePS(hab,memhdc,&sizl,PU_ARBITRARY|GPIT_MICRO|GPIA_ASSOC);
	WinSetRect(hab,&rectup,0,0,profile.size+4,ysize+4);
	GpiSetPageViewport(memhps,&rectup);
	GpiQueryDeviceBitmapFormats(memhps,2L,aldata);
	bmi.cbFix = sizeof(BITMAPINFOHEADER);
	bmi.cx = profile.size+4;
	bmi.cy = ysize+4;
	bmi.cPlanes = (USHORT)aldata[0];
	bmi.cBitCount = (USHORT)aldata[1];
	memhbm = GpiCreateBitmap(memhps,&bmi,0L,NULL,NULL);
	GpiSetBitmap(memhps,memhbm);
	WinFillRect(memhps,&rectup,CLR_BLACK);
	mat.fxM11 = mat.fxM22 = MAKEFIXED(1,0);
	mat.fxM12 = mat.fxM21 = 0;
	mat.lM13 = mat.lM23 = 0;
	mat.lM31 = sizl.cx>>1;
	mat.lM32 = sizl.cy>>1;
	mat.lM33 = 1;
	GpiSetDefaultViewMatrix(memhps,9L,&mat,TRANSFORM_REPLACE);
	xposition = MAKEFIXED(saverinfo->screenrectl.xLeft,0)+(rand()*
		(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft-profile.size)<<1);
	yposition = MAKEFIXED(saverinfo->screenrectl.yBottom,0)+(rand()*
		(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom-ysize)<<1);
	xdir = rand()&1;
	ydir = rand()&1;
	chdir = TRUE;
	while (!saverinfo->closesemaphore) {
		time = WinGetCurrentTime(hab);
		if (FIXEDINT(xposition) < (int)saverinfo->screenrectl.xLeft) {
			xposition = (saverinfo->screenrectl.xLeft<<17)-xposition;
			xdir = TRUE;
			ydir = yvelocity > 0;
			chdir = TRUE;
		}
		else if (FIXEDINT(xposition) >= (int)saverinfo->screenrectl.xRight-profile.size) {
			xposition = (saverinfo->screenrectl.xRight-profile.size<<17)-xposition;
			xdir = FALSE;
			ydir = yvelocity > 0;
			chdir = TRUE;
		}
		if (FIXEDINT(yposition) < (int)saverinfo->screenrectl.yBottom) {
			yposition = (saverinfo->screenrectl.yBottom<<17)-yposition;
			xdir = xvelocity > 0;
			ydir = TRUE;
			chdir = TRUE;
		}
		else if (FIXEDINT(yposition) >= (int)saverinfo->screenrectl.yTop-ysize) {
			yposition = (saverinfo->screenrectl.yTop-ysize<<17)-yposition;
			xdir = xvelocity > 0;
			ydir = FALSE;
			chdir = TRUE;
		}
		if (chdir) {
			xtemp = (long)rand()<<1;
			xvelocity = xdir ? xtemp : -xtemp;
			xtemp = MAKEFIXED(0,65535)-((ULONG)xtemp*(ULONG)xtemp>>16);
			ytemp = xtemp<<16;
			yvelocity = ydir ? sqr(ytemp) : -sqr(ytemp);
			chdir = FALSE;
		}
		paintclock(memhps,first);
		first = FALSE;
		aptl[0].x = FIXEDINT(xposition)-2;
		aptl[0].y = FIXEDINT(yposition)-2;
		aptl[1].x = FIXEDINT(xposition)+profile.size+2;
		aptl[1].y = FIXEDINT(yposition)+ysize+2;
		aptl[2].x = aptl[2].y = 0;
		GpiBitBlt(hps,memhps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
		xposition += xvelocity;
		yposition += yvelocity;
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
	GpiSetBitmap(memhps,NULL);
	GpiDeleteBitmap(memhbm);
	GpiDestroyPS(memhps);
	DevCloseDC(memhdc);
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static ULONG near sqr(arg)
ULONG arg;
{
	ULONG sqrt,temp;

	sqrt = 0;
	temp = 1L<<15;
	while (temp) {
		sqrt ^= temp;
		if (sqrt*sqrt > arg) sqrt ^= temp;
		temp >>= 1;
	}
	return sqrt;
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
