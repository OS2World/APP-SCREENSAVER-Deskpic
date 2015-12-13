/***************************************************************************\
* PUZZLE.C - A screen saver extension by John Ridges
\***************************************************************************/

#define MAXSPEED 10
#define DELAYTIME 25

#define INCL_DOSPROCESS
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
	unsigned int speed;
	unsigned int size;
} PROFILEREC;

static int near rand(int);

static ULONG near seed;
static char name[] = "Puzzle";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 3, 1 };

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
	unsigned int i;
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
		WinSetDlgItemShort(hwnd,4,profile.speed,FALSE);
		WinSendDlgItemMsg(hwnd,5+profile.size,BM_SETCHECK,MPFROMSHORT(1),0);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 1 || i > MAXSPEED) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The speed must be "
					"between 1 and 10",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			profile.speed = i;
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			profile.size = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,5,BM_QUERYCHECKINDEX,0,0));
			WinQueryDlgItemShort(hwnd,4,(PSHORT)&profile.speed,FALSE);
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	int i,xnum,ynum,size,xsize,ysize,xpos,ypos,dir,newxpos,newypos,dx,dy;
	ULONG time;
	HAB hab;
	HPS hps;
	RECTL rect,rectup;
	int oldxpos = -1;
	int oldypos = -1;

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	seed = WinGetCurrentTime(hab);
	xnum = 4<<profile.size;
	ynum = 3<<profile.size;
	xsize = (int)(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft)/xnum;
	ysize = (int)(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom)/ynum;
	rect.yBottom = saverinfo->screenrectl.yBottom;
	rect.yTop = saverinfo->screenrectl.yTop;
	for (i = 1; i <= xnum; i++) {
		rect.xLeft = i*xsize-1;
		if (i != xnum) rect.xRight = rect.xLeft+1;
		else rect.xRight = saverinfo->screenrectl.xRight;
		WinFillRect(hps,&rect,CLR_BLACK);
	}
	rect.xLeft = saverinfo->screenrectl.xLeft;
	rect.xRight = saverinfo->screenrectl.xRight;
	for (i = 1; i <= ynum; i++) {
		rect.yTop = saverinfo->screenrectl.yTop-i*ysize+1;
		if (i != ynum) rect.yBottom = rect.yTop-1;
		else rect.yBottom = saverinfo->screenrectl.yBottom;
		WinFillRect(hps,&rect,CLR_BLACK);
	}
	xpos = rand(xnum);
	ypos = rand(ynum);
	for (i = 0; i < (xsize>>1)+3 && i < (ysize>>1)+3; i += 3) {
		time = WinGetCurrentTime(hab);
		if (saverinfo->closesemaphore) break;
		rect.xLeft = xpos*xsize+(xsize>>1)-i;
		if (rect.xLeft < xpos*xsize) rect.xLeft = xpos*xsize;
		rect.xLeft += saverinfo->screenrectl.xLeft;
		rect.xRight = xpos*xsize+(xsize>>1)+i;
		if (rect.xRight > xpos*xsize+xsize) rect.xRight = xpos*xsize+xsize;
		rect.xRight += saverinfo->screenrectl.xLeft;
		rect.yTop = ypos*ysize+(ysize>>1)-i;
		if (rect.yTop < ypos*ysize) rect.yTop = ypos*ysize;
		rect.yTop = saverinfo->screenrectl.yTop-rect.yTop;
		rect.yBottom = ypos*ysize+(ysize>>1)+i;
		if (rect.yBottom > ypos*ysize+ysize) rect.yBottom = ypos*ysize+ysize;
		rect.yBottom = saverinfo->screenrectl.yTop-rect.yBottom;
		WinFillRect(hps,&rect,CLR_BLACK);
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
	while (!saverinfo->closesemaphore) {
		dir = rand(2) ? 1 : -1;
		if (rand(2)) {
			size = xsize;
			newxpos = xpos-dir;
			newypos = ypos;
			if (newxpos < 0 || newxpos >= xnum) continue;
			dx = profile.speed*dir;
			dy = 0;
			if (dir > 0) {
				rect.xLeft = newxpos*xsize+saverinfo->screenrectl.xLeft;
				rect.xRight = xpos*xsize+xsize+saverinfo->screenrectl.xLeft;
			}
			else {
				rect.xLeft = xpos*xsize+saverinfo->screenrectl.xLeft;
				rect.xRight = newxpos*xsize+xsize+saverinfo->screenrectl.xLeft;
			}
			rect.yTop = saverinfo->screenrectl.yTop-ypos*ysize;
			rect.yBottom = rect.yTop-ysize;
		}
		else {
			size = ysize;
			newxpos = xpos;
			newypos = ypos-dir;
			if (newypos < 0 || newypos >= ynum) continue;
			dx = 0;
			dy = -profile.speed*dir;
			if (dir > 0) {
				rect.yTop = saverinfo->screenrectl.yTop-newypos*ysize;
				rect.yBottom = saverinfo->screenrectl.yTop-ypos*ysize-ysize;
			}
			else {
				rect.yTop = saverinfo->screenrectl.yTop-ypos*ysize;
				rect.yBottom = saverinfo->screenrectl.yTop-newypos*ysize-ysize;
			}
			rect.xLeft = xpos*xsize+saverinfo->screenrectl.xLeft;
			rect.xRight = rect.xLeft+xsize;
		}
		if (newxpos == oldxpos && newypos == oldypos) continue;
		oldxpos = xpos;
		oldypos = ypos;
		xpos = newxpos;
		ypos = newypos;
		dir = 0;
		for (; size > 0; size -= profile.speed) {
			time = WinGetCurrentTime(hab);
			if (saverinfo->closesemaphore) break;
			if (dx > size) dx = size;
			if (dx < -size) dx = -size;
			if (dy > size) dy = size;
			if (dy < -size) dy = -size;
			WinScrollWindow(saverinfo->screenhwnd,dx,dy,&rect,&rect,NULL,&rectup,0);
			if (!dir) WinFillRect(hps,&rectup,CLR_BLACK);
			dir = 1;
			while (time+DELAYTIME > WinGetCurrentTime(hab));
		}
	}
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static int near rand(i)
int i;
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(((seed>>16)&0x7fff)*i>>15);
}
