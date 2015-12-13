/***************************************************************************\
* FLASH.C - A flashlight screen saver extension by John Ridges
\***************************************************************************/

#define MAXSIZE 300
#define MAXSPEED 20

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPIPATHS
#define INCL_GPIPRIMITIVES
#define INCL_GPITRANSFORMS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WINRECTANGLES
#define INCL_WINSHELLDATA
#define INCL_WINTIMER
#define INCL_WINWINDOWMGR

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
	int size,speed;
} PROFILEREC;

static ULONG near sqr(ULONG);
static int near rand(void);

static ULONG near seed;
static char name[] = "Flashlight";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 75, 2 };

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
	SHORT i,j;
	RECTL ourrect,parentrect;

	switch(message) {
	case WM_INITDLG:
		WinQueryWindowRect(hwnd,&ourrect);
		WinQueryWindowRect(HWND_DESKTOP,&parentrect);
		WinSetWindowPos(hwnd,NULL,
			(int)(parentrect.xRight-ourrect.xRight+ourrect.xLeft)/2,
			(int)(parentrect.yTop-ourrect.yTop+ourrect.yBottom)/2,
			0,0,SWP_MOVE);
		if (profile.enabled)	WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.size,TRUE);
		WinSetDlgItemShort(hwnd,5,profile.speed,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 30 || i > MAXSIZE) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The size of the spot must be "
					"between 30 and 300",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			WinQueryDlgItemShort(hwnd,5,&j,TRUE);
			if (j < 1 || j > MAXSPEED) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The speed of the spot must be "
					"between 1 and 20",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,5,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,5)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,5));
				return FALSE;
			}
			profile.size = i;
			profile.speed = j;
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
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
	HDC memhdc,screenhdc;
	HPS hps,memhps,screenhps;
	HBITMAP memhbm,screenhbm;
	SIZEL sizl;
	ULONG cxRes,cyRes,aldata[2];
	POINTL pnt,aptl[3];
	RECTL rectup;
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;
	FIXED xvelocity,yvelocity,xposition,yposition,xtemp,ytemp;
	BOOL xdir,ydir,chdir;
	int speed = profile.speed+1<<1;

	hab = WinInitialize(0);
	seed = WinGetCurrentTime(hab);
	dop.pszLogAddress = NULL;
	dop.pszDriverName = "DISPLAY";
	dop.pdriv = NULL;
	dop.pszDataType = NULL;
	screenhdc = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,NULL);
	sizl.cx = sizl.cy = 0;
	screenhps = GpiCreatePS(hab,screenhdc,&sizl,
		PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
	GpiQueryDeviceBitmapFormats(screenhps,2L,aldata);
	bmi.cbFix = sizeof(BITMAPINFOHEADER);
	bmi.cx = (USHORT)(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft);
	bmi.cy = (USHORT)(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom);
	bmi.cPlanes = (USHORT)aldata[0];
	bmi.cBitCount = (USHORT)aldata[1];
	screenhbm = GpiCreateBitmap(screenhps,&bmi,0L,NULL,NULL);
	GpiSetBitmap(screenhps,screenhbm);
	hps = WinGetScreenPS(HWND_DESKTOP);
	aptl[0].x = aptl[2].x = saverinfo->screenrectl.xLeft;
	aptl[0].y = aptl[2].y = saverinfo->screenrectl.yBottom;
	aptl[1].x = saverinfo->screenrectl.xRight;
	aptl[1].y = saverinfo->screenrectl.yTop;
	GpiBitBlt(screenhps,hps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
	WinReleasePS(hps);
	if (saverinfo->closesemaphore) goto abort;
	hps = WinGetPS(saverinfo->screenhwnd);
	WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
	memhdc = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,NULL);
	DevQueryCaps(memhdc,CAPS_VERTICAL_RESOLUTION,1L,&cyRes);
	DevQueryCaps(memhdc,CAPS_HORIZONTAL_RESOLUTION,1L,&cxRes);
	ysize = (int)((profile.size*cyRes+(cxRes>>1))/cxRes);
	sizl.cx = 100+(200*profile.speed+profile.size+199)/profile.size<<1;
	sizl.cy = 100+(200*profile.speed+ysize+199)/ysize<<1;
	memhps = GpiCreatePS(hab,memhdc,&sizl,PU_ARBITRARY|GPIT_MICRO|GPIA_ASSOC);
	bmi.cx = profile.size+speed;
	bmi.cy = ysize+speed;
	WinSetRect(hab,&rectup,0,0,bmi.cx,bmi.cy);
	GpiSetPageViewport(memhps,&rectup);
	memhbm = GpiCreateBitmap(memhps,&bmi,0L,NULL,NULL);
	GpiSetBitmap(memhps,memhbm);
	WinFillRect(memhps,&rectup,CLR_BLACK);
	GpiBeginPath(memhps,1L);
	pnt.x = sizl.cx>>1;
	pnt.y = sizl.cy>>1;
	GpiMove(memhps,&pnt);
	GpiFullArc(memhps,DRO_OUTLINE,MAKEFIXED(100,0));
	GpiEndPath(memhps);
	GpiSetClipPath(memhps,1L,SCP_ALTERNATE|SCP_AND);
	xposition = MAKEFIXED(saverinfo->screenrectl.xLeft,0)+(rand()*
		(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft-profile.size)<<1);
	yposition = MAKEFIXED(saverinfo->screenrectl.yBottom,0)+(rand()*
		(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom-ysize)<<1);
	xdir = rand()&1;
	ydir = rand()&1;
	chdir = TRUE;
	while (!saverinfo->closesemaphore) {
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
			xvelocity = (xdir ? xtemp : -xtemp)*profile.speed;
			xtemp = MAKEFIXED(0,65535)-((ULONG)xtemp*(ULONG)xtemp>>16);
			ytemp = xtemp<<16;
			yvelocity = (ydir ? sqr(ytemp) : -sqr(ytemp))*profile.speed;
			chdir = FALSE;
		}
		aptl[0].x = aptl[0].y = 0;
		aptl[1].x = profile.size+speed;
		aptl[1].y = ysize+speed;
		aptl[2].x = FIXEDINT(xposition)-profile.speed-1;
		aptl[2].y = FIXEDINT(yposition)-profile.speed-1;
		GpiBitBlt(memhps,screenhps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
		aptl[0] = aptl[2];
		aptl[1].x = FIXEDINT(xposition)+profile.size+profile.speed+1;
		aptl[1].y = FIXEDINT(yposition)+ysize+profile.speed+1;
		aptl[2].x = aptl[2].y = 0;
		GpiBitBlt(hps,memhps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
		xposition += xvelocity;
		yposition += yvelocity;
	}
	GpiSetBitmap(memhps,NULL);
	GpiDeleteBitmap(memhbm);
	GpiDestroyPS(memhps);
	DevCloseDC(memhdc);
	WinReleasePS(hps);
abort:
	GpiSetBitmap(screenhps,NULL);
	GpiDeleteBitmap(screenhbm);
	GpiDestroyPS(screenhps);
	DevCloseDC(screenhdc);
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
