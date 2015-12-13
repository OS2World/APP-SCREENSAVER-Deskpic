/***************************************************************************\
* EYES.C - A very silly screen saver extension by John Ridges
\***************************************************************************/

#define MAXEYES 25
#define MAXSPEED 10
#define VIEWPOINT 25
#define BACKGROUND CLR_BLACK

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPIPATHS
#define INCL_GPIPRIMITIVES
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
	FIXED xvelocity,yvelocity;
	FIXED xposition,yposition;
	HDC hdc;
	HPS hps;
	HBITMAP hbm,eyemap;
	BOOL painted;
	BOOL volatile collide[MAXEYES];
} BITMAPREC;

typedef struct {
	BOOL enabled;
	int numeyes;
	BOOL bloodshot;
	int speed;
} PROFILEREC;

static ULONG sqr(ULONG);
static int near rand(void);

static ULONG near seed;
static char name[] = "Eyes";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 2, FALSE, 1 };

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
		if (profile.enabled)
			WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,MPFROMSHORT(1),0);
		if (profile.bloodshot)
			WinSendDlgItemMsg(hwnd,5,BM_SETCHECK,MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.numeyes,TRUE);
		WinSetDlgItemShort(hwnd,6,profile.speed,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 2 || i > MAXEYES) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The number of eyes must be "
					"between 2 and 25",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			WinQueryDlgItemShort(hwnd,6,&j,TRUE);
			if (j < 1 || j > MAXSPEED) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The speed of the eyes must be "
					"between 1 and 10",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,6,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,6)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,6));
				return FALSE;
			}
			profile.numeyes = i;
			profile.speed = j;
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			profile.bloodshot = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,5,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	HAB hab;
	HPS hps;
	int i,j;
	long x,y,d;
	FIXED temp,tempx,tempy;
	SIZEL sizl;
	ULONG aldata[2];
	POINTL pnt,aptl[4];
	SEL bitmapsel;
	RECTL rectup;
	BITMAPREC far *bitmaps;
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;
	BOOL flip = FALSE;
	BOOL firsttime = TRUE;
	static ARCPARAMS arc = { 7, 15, 0, 0 };

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	WinFillRect(hps,&saverinfo->screenrectl,BACKGROUND);
	DosAllocSeg(sizeof(BITMAPREC)*profile.numeyes,&bitmapsel,SEG_NONSHARED);
	bitmaps = MAKEP(bitmapsel,0);
	for (i = 0; i < profile.numeyes; i++) {
		bitmaps[i].xposition = -1L;
		for (j = 0; j < profile.numeyes; j++) bitmaps[i].collide[j] = FALSE;
	}
	seed = WinGetCurrentTime(hab);
	while (!saverinfo->closesemaphore) {
		for (i = 0; i < profile.numeyes; i++) {
			if (bitmaps[i].xposition < 0) {
				dop.pszLogAddress = NULL;
				dop.pszDriverName = "DISPLAY";
				dop.pdriv = NULL;
				dop.pszDataType = NULL;
				bitmaps[i].hdc = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,
					NULL);
				sizl.cx = sizl.cy = 0;
				bitmaps[i].hps = GpiCreatePS(hab,bitmaps[i].hdc,&sizl,
					PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
				GpiQueryDeviceBitmapFormats(bitmaps[i].hps,2L,aldata);
				bmi.cbFix = sizeof(BITMAPINFOHEADER);
				bmi.cx = 48+(profile.speed<<1);
				bmi.cy = 32+(profile.speed<<1);
				bmi.cPlanes = (USHORT)aldata[0];
				bmi.cBitCount = (USHORT)aldata[1];
				bitmaps[i].hbm = GpiCreateBitmap(bitmaps[i].hps,&bmi,0L,NULL,NULL);
				GpiSetBitmap(bitmaps[i].hps,bitmaps[i].hbm);
				bitmaps[i].eyemap = GpiLoadBitmap(bitmaps[i].hps,
					saverinfo->thismodule,1+profile.bloodshot,0L,0L);
				GpiSetArcParams(bitmaps[i].hps,&arc);
				WinSetRect(hab,&rectup,0,0,bmi.cx,bmi.cy);
				WinFillRect(bitmaps[i].hps,&rectup,BACKGROUND);
				GpiBeginPath(bitmaps[i].hps,1L);
				pnt.y = 15+profile.speed;
				for (j = 23; j < 55; j += 16) {
					pnt.x = j+profile.speed;
					GpiMove(bitmaps[i].hps,&pnt);
					GpiFullArc(bitmaps[i].hps,DRO_OUTLINE,MAKEFIXED(1,0));
				}
				GpiEndPath(bitmaps[i].hps);
				GpiSetClipPath(bitmaps[i].hps,1L,SCP_ALTERNATE|SCP_AND);
				tempx = (long)rand()<<1;
				bitmaps[i].xvelocity = (rand()&1 ? tempx : -tempx)*profile.speed;
				tempx = MAKEFIXED(0,65535)-((ULONG)tempx*(ULONG)tempx>>16)-1;
				tempy = tempx<<16;
				bitmaps[i].yvelocity = (rand()&1 ? sqr(tempy) : -sqr(tempy))*
					profile.speed;
				tempx = MAKEFIXED(saverinfo->screenrectl.xLeft,0)+(rand()*
					(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft-32)<<1);
				tempy = MAKEFIXED(saverinfo->screenrectl.yBottom,0)+(rand()*
					(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom-32)<<1);
				bitmaps[i].painted = flip = !flip;
			}
			else {
				tempx = bitmaps[i].xposition+bitmaps[i].xvelocity;
				tempy = bitmaps[i].yposition+bitmaps[i].yvelocity;
				for (j = i+1; j < profile.numeyes; j++)
					if (tempy+MAKEFIXED(32+profile.speed,0) > bitmaps[j].yposition &&
					tempy < bitmaps[j].yposition+MAKEFIXED(32+profile.speed,0) &&
					tempx < bitmaps[j].xposition+MAKEFIXED(32+profile.speed,0) &&
					tempx+MAKEFIXED(32+profile.speed,0) > bitmaps[j].xposition) {
					if (!bitmaps[i].collide[j]) {
						bitmaps[i].collide[j] = TRUE;
						temp = bitmaps[i].xvelocity;
						bitmaps[i].xvelocity = bitmaps[j].xvelocity;
						bitmaps[j].xvelocity = temp;
						temp = bitmaps[i].yvelocity;
						bitmaps[i].yvelocity = bitmaps[j].yvelocity;
						bitmaps[j].yvelocity = temp;
					}
				}
				else bitmaps[i].collide[j] = FALSE;
				if (FIXEDINT(tempx) < (int)saverinfo->screenrectl.xLeft) {
					tempx = (saverinfo->screenrectl.xLeft<<17)-tempx;
					bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
				}
				else if (FIXEDINT(tempx) >= (int)saverinfo->screenrectl.xRight-32) {
					tempx = (saverinfo->screenrectl.xRight-32<<17)-tempx;
					bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
				}
				if (FIXEDINT(tempy) < (int)saverinfo->screenrectl.yBottom) {
					tempy = (saverinfo->screenrectl.yBottom<<17)-tempy;
					bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
				}
				else if (FIXEDINT(tempy) >= (int)saverinfo->screenrectl.yTop-32) {
					tempy = (saverinfo->screenrectl.yTop-32<<17)-tempy;
					bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
				}
				if (bitmaps[i].painted || firsttime) for (j = 16; j < 48; j += 16) {
					x = FIXEDINT(tempx)-24+j-
						FIXEDINT(bitmaps[(i+1)%profile.numeyes].xposition);
					y = FIXEDINT(tempy)-
						FIXEDINT(bitmaps[(i+1)%profile.numeyes].yposition);
					d = sqr(VIEWPOINT*VIEWPOINT+x*x+y*y);
					x = (x*7+(d>>1))/d;
					y = (y*15+(d>>1))/d;
					aptl[0].x = j+profile.speed;
					aptl[0].y = profile.speed;
					aptl[1].x = j+profile.speed+15;
					aptl[1].y = profile.speed+31;
					aptl[2].x = x+8;
					aptl[2].y = y+16;
					aptl[3].x = x+24;
					aptl[3].y = y+48;
					GpiWCBitBlt(bitmaps[i].hps,bitmaps[i].eyemap,4L,aptl,
						ROP_SRCCOPY,BBO_IGNORE);
				}
				bitmaps[i].painted = !bitmaps[i].painted;
				aptl[0].x = FIXEDINT(tempx)-profile.speed;
				aptl[0].y = FIXEDINT(tempy)-profile.speed;
				aptl[1].x = FIXEDINT(tempx)+32+profile.speed;
				aptl[1].y = FIXEDINT(tempy)+32+profile.speed;
				aptl[2].x = 16;
				aptl[2].y = 0;
				GpiBitBlt(hps,bitmaps[i].hps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
			}
			bitmaps[i].xposition = tempx;
			bitmaps[i].yposition = tempy;
		}
		firsttime = FALSE;
	}
	for (i = 0; i < profile.numeyes; i++) if (bitmaps[i].xposition >= 0) {
		GpiDeleteBitmap(bitmaps[i].eyemap);
		GpiSetBitmap(bitmaps[i].hps,NULL);
		GpiDeleteBitmap(bitmaps[i].hbm);
		GpiDestroyPS(bitmaps[i].hps);
		DevCloseDC(bitmaps[i].hdc);
	}
	DosFreeSeg(bitmapsel);
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static ULONG sqr(arg)
ULONG arg;
{
	ULONG sqrt,temp,range;

	sqrt = 0;
	temp = 1L<<15;
	for (range = 1L<<30; range > arg; range >>= 2) temp >>= 1;
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
